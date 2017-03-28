/**
 ******************************************************************************
 * @file       pios_crossfire.c
 * @author     dRonin, http://dRonin.org/, Copyright (C) 2017
 * @addtogroup PIOS PIOS Core hardware abstraction layer
 * @{
 * @addtogroup PIOS_Crossfire PiOS TBS Crossfire receiver driver
 * @{
 * @brief Receives and decodes CRSF protocol receiver packets
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>
 *
 * Additional note on redistribution: The copyright and license notices above
 * must be maintained in each individual source file that is a derivative work
 * of this source file; otherwise redistribution is prohibited.
 */

#include "pios_crossfire.h"
#include "pios_crc.h"

#ifdef PIOS_INCLUDE_CROSSFIRE

// There's 16 channels in the RC channel payload, the Crossfire currently
// only uses 12. Asked Perna a while ago to always send RSSI and LQ on
// 15 and 16, said was nice idea, but never showed up in change logs.
#define PIOS_CROSSFIRE_CHANNELS		16

// Addresses with one-based index, because they're used after a pointer
// increase only.
#define CRSF_LEN_IDX				2
#define CRSF_HEADER_IDX				3

// Lengths of the type and CRC fields.
#define CRSF_ADDRESS_LEN			1
#define CRSF_LENGTH_LEN				1
#define CRSF_CRC_LEN				1
#define CRSF_TYPE_LEN				1

// Maximum payload in the protocol can be 32 bytes.
// Should figure out whether that includes type and CRC, or not.
#define CRSF_MAX_PAYLOAD			32

// Frame types. We care about RC data only for now.
// Nothing else gets sent over the link yet, anyway.
#define CRSF_FRAME_RCCHANNELS		0x16

// Payload sizes
// RC channel payload has 16 channels at 11 bits = 176 bits, 22 bytes.
#define CRSF_PAYLOAD_RCCHANNELS		22

struct crsf_frame_t {
	// Module address. Not sure what's the point, assuming provision for
	// a bus type deal. Not used.
	uint8_t	dev_addr;

	// Length of the payload following this field. Length includes CRC and
	// type field in addition to payload.
	uint8_t	length;

	// Frame type
	uint8_t type;

	// The payload plus CRC appendix.
	uint8_t payload[CRSF_MAX_PAYLOAD + CRSF_CRC_LEN];
};

#define CRSF_CRCFIELD(frame)		(frame.payload[frame.length - CRSF_CRC_LEN - CRSF_TYPE_LEN])

// Macro referring to the maximum frame size.
#define CRSF_MAX_FRAMELEN sizeof(struct crsf_frame_t)

// Internal use. Random number.
#define PIOS_CROSSFIRE_MAGIC		0xcdf19cf5 

/**
 * @brief TBS Crossfire receiver driver internal state data
 */
struct pios_crossfire_dev {
	uint32_t magic;
	uint8_t buf_pos;
	uint16_t rx_timer;
	uint16_t failsafe_timer;
	uint8_t bytes_expected;

	uint16_t channel_data[PIOS_CROSSFIRE_CHANNELS];

	union {
		struct crsf_frame_t frame;
		uint8_t rx_buf[sizeof(struct crsf_frame_t)];
	} u;
};

/**
 * @brief Allocates a driver instance
 * @retval pios_crossfire_dev pointer on success, NULL on failure
 */
static struct pios_crossfire_dev *PIOS_Crossfire_Alloc(void);
/**
 * @brief Validate a driver instance
 * @param[in] dev device driver instance pointer
 * @retval true on success, false on failure
 */
static bool PIOS_Crossfire_Validate(const struct pios_crossfire_dev *dev);
/**
 * @brief Read a channel from the last received frame
 * @param[in] id Driver instance
 * @param[in] channel 0-based channel index
 * @retval raw channel value, or error value (see pios_rcvr.h)
 */
static int32_t PIOS_Crossfire_Read(uintptr_t id, uint8_t channel);
/**
 * @brief Set all channels in the last frame buffer to a given value
 * @param[in] dev Driver instance
 * @param[in] value channel value
 */
static void PIOS_Crossfire_SetAllChannels(struct pios_crossfire_dev *dev, uint16_t value);
/**
 * @brief Serial receive callback
 * @param[in] context Driver instance handle
 * @param[in] buf Receive buffer
 * @param[in] buf_len Number of bytes available
 * @param[out] headroom Number of bytes remaining in internal buffer
 * @param[out] task_woken Did we awake a task?
 * @retval Number of bytes consumed from the buffer
 */
static uint16_t PIOS_Crossfire_Receive(uintptr_t context, uint8_t *buf, uint16_t buf_len,
		uint16_t *headroom, bool *task_woken);
/**
 * @brief Reset the internal receive buffer state
 * @param[in] dev device driver instance pointer
 */
static void PIOS_Crossfire_ResetBuffer(struct pios_crossfire_dev *dev);
/**
 * @brief Unpack a frame from the internal receive buffer to the channel buffer
 * @param[in] dev device driver instance pointer
 */
static void PIOS_Crossfire_UnpackFrame(struct pios_crossfire_dev *dev);
/**
 * @brief RTC tick callback
 * @param[in] context Driver instance handle
 */
static void PIOS_Crossfire_Supervisor(uintptr_t context);

// public
const struct pios_rcvr_driver pios_crossfire_rcvr_driver = {
	.read = PIOS_Crossfire_Read,
};


static struct pios_crossfire_dev *PIOS_Crossfire_Alloc(void)
{
	struct pios_crossfire_dev *dev = PIOS_malloc_no_dma(sizeof(struct pios_crossfire_dev));
	if (!dev)
		return NULL;

	memset(dev, 0, sizeof(*dev));
	dev->magic = PIOS_CROSSFIRE_MAGIC;

	return dev;
}

static bool PIOS_Crossfire_Validate(const struct pios_crossfire_dev *dev)
{
	return dev && dev->magic == PIOS_CROSSFIRE_MAGIC;
}

int PIOS_Crossfire_Init(uintptr_t *crsf_id, const struct pios_com_driver *driver,
		uintptr_t uart_id)
{
	struct pios_crossfire_dev *dev = PIOS_Crossfire_Alloc();

	if (!dev)
		return -1;

	*crsf_id = (uintptr_t)dev;

	PIOS_Crossfire_ResetBuffer(dev);
	PIOS_Crossfire_SetAllChannels(dev, PIOS_RCVR_INVALID);

	if (!PIOS_RTC_RegisterTickCallback(PIOS_Crossfire_Supervisor, *crsf_id))
		PIOS_Assert(0);

	(driver->bind_rx_cb)(uart_id, PIOS_Crossfire_Receive, *crsf_id);

	return 0;
}

static int32_t PIOS_Crossfire_Read(uintptr_t context, uint8_t channel)
{
	if (channel > PIOS_CROSSFIRE_CHANNELS)
		return PIOS_RCVR_INVALID;

	struct pios_crossfire_dev *dev = (struct pios_crossfire_dev *)context;
	if (!PIOS_Crossfire_Validate(dev))
		return PIOS_RCVR_NODRIVER;

	return dev->channel_data[channel];
}

static void PIOS_Crossfire_SetAllChannels(struct pios_crossfire_dev *dev, uint16_t value)
{
	for (int i = 0; i < PIOS_CROSSFIRE_CHANNELS; i++)
		dev->channel_data[i] = value;
}

static uint16_t PIOS_Crossfire_Receive(uintptr_t context, uint8_t *buf, uint16_t buf_len,
		uint16_t *headroom, bool *task_woken)
{
	struct pios_crossfire_dev *dev = (struct pios_crossfire_dev *)context;

	if (!PIOS_Crossfire_Validate(dev))
		goto out_fail;

	for (int i = 0; i < buf_len; i++) {
		// Ignore any incoming stuff until we're expecting new data.
		if(dev->buf_pos >= dev->bytes_expected)
			continue;

		dev->u.rx_buf[dev->buf_pos++] = buf[i];

		if(dev->buf_pos == CRSF_LEN_IDX) {
			// Read length field and adjust. Denotes payload, plus type field, plus CRC field.
			dev->bytes_expected = CRSF_ADDRESS_LEN + CRSF_LENGTH_LEN + dev->u.frame.length;

			// If length field isn't plausible, ignore rest of the data.
			if(dev->bytes_expected >= CRSF_MAX_FRAMELEN)
				dev->bytes_expected = 0;
		}

		if (dev->buf_pos == dev->bytes_expected) {
			// Frame complete, decode.
			PIOS_Crossfire_UnpackFrame(dev);
			PIOS_RCVR_ActiveFromISR();
		}
	}

	dev->rx_timer = 0;

	// Keep pumping data.
	*headroom = CRSF_MAX_FRAMELEN;
	*task_woken = false;
	return buf_len;

out_fail:
	*headroom = 0;
	*task_woken = false;
	return 0;
}

static void PIOS_Crossfire_ResetBuffer(struct pios_crossfire_dev *dev)
{
	dev->buf_pos = 0;
	dev->bytes_expected = CRSF_MAX_FRAMELEN;
}

static void PIOS_Crossfire_UnpackFrame(struct pios_crossfire_dev *dev)
{
	// Pull CRC from type field and payload.
	uint8_t crc = PIOS_CRC_updateCRC_TBS(0, &dev->u.frame.type, dev->u.frame.length - CRSF_CRC_LEN);

	if(crc == CRSF_CRCFIELD(dev->u.frame)) {

		// Apparently only RC channel payload gets sent for now.
		if(dev->u.frame.type == CRSF_FRAME_RCCHANNELS) {
			// From pios_sbus.c
			uint8_t *s = dev->u.frame.payload;
			uint16_t *d = dev->channel_data;

			#define F(v,s) (((v) >> (s)) & 0x7ff)

			/* unroll channels 1-8 */
			d[0] = F(s[0] | s[1] << 8, 0);
			d[1] = F(s[1] | s[2] << 8, 3);
			d[2] = F(s[2] | s[3] << 8 | s[4] << 16, 6);
			d[3] = F(s[4] | s[5] << 8, 1);
			d[4] = F(s[5] | s[6] << 8, 4);
			d[5] = F(s[6] | s[7] << 8 | s[8] << 16, 7);
			d[6] = F(s[8] | s[9] << 8, 2);
			d[7] = F(s[9] | s[10] << 8, 5);

			/* unroll channels 9-16 */
			d[8] = F(s[11] | s[12] << 8, 0);
			d[9] = F(s[12] | s[13] << 8, 3);
			d[10] = F(s[13] | s[14] << 8 | s[15] << 16, 6);
			d[11] = F(s[15] | s[16] << 8, 1);
			d[12] = F(s[16] | s[17] << 8, 4);
			d[13] = F(s[17] | s[18] << 8 | s[19] << 16, 7);
			d[14] = F(s[19] | s[20] << 8, 2);
			d[15] = F(s[20] | s[21] << 8, 5);
		}

		// Consider link proper anyway, even if the packet
		// didn't deliver RC data.
		dev->failsafe_timer = 0;
	}

	PIOS_Crossfire_ResetBuffer(dev);
}

static void PIOS_Crossfire_Supervisor(uintptr_t context)
{
	struct pios_crossfire_dev *dev = (struct pios_crossfire_dev *)context;
	PIOS_Assert(PIOS_Crossfire_Validate(dev));

	// An RC channel type packet is 26 bytes, at 420kbit with stop bit, that's 557us.
	// Maximum packet size is 36 bytes, that's 772us.
	// Minimum frame timing is supposed to be 4ms, typical is however 6.67ms (150hz).
	// So if more than 1.6ms passed without communication, safe to say that a new
	// packet is inbound.
	if (++dev->rx_timer > 1)
		PIOS_Crossfire_ResetBuffer(dev);

	// Failsafe after 50ms.
	if (++dev->failsafe_timer > 32)
		PIOS_Crossfire_SetAllChannels(dev, PIOS_RCVR_TIMEOUT);
}

#endif // PIOS_INCLUDE_CROSSFIRE

 /**
  * @}
  * @}
  */
