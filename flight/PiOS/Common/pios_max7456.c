/* Imported from MultiOSD (https://github.com/UncleRus/MultiOSD/)
 * Altered for use on STM32 Flight controllers by dRonin
 * Copyright (C) dRonin 2016
 */


/*
 * This file is part of MultiOSD <https://github.com/UncleRus/MultiOSD>
 *
 * MultiOSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pios.h>

#ifdef PIOS_INCLUDE_MAX7456
#include <pios_delay.h>
#include <pios_thread.h>

#include "pios_max7456.h"

#define MAX7456_DEFAULT_BRIGHTNESS 0x00

// MAX7456 reg read addresses
#define MAX7456_REG_STAT  0x20 // 0xa0 Status
#define MAX7456_REG_DMDO  0x30 // 0xb0 Display Memory Data Out
#define MAX7456_REG_CMDO  0x40 // 0xc0 Character Memory Data Out

// MAX7456 reg write addresses
#define MAX7456_REG_VM0   0x00
#define MAX7456_REG_VM1   0x01
#define MAX7456_REG_DMM   0x04
#define MAX7456_REG_DMAH  0x05
#define MAX7456_REG_DMAL  0x06
#define MAX7456_REG_DMDI  0x07
#define MAX7456_REG_OSDM  0x0c // not used. Is to set mix
#define MAX7456_REG_OSDBL 0x6c // black level

// MAX7456 reg write addresses to recording NVM process
#define MAX7456_REG_CMM   0x08
#define MAX7456_REG_CMAH  0x09
#define MAX7456_REG_CMAL  0x0a
#define MAX7456_REG_CMDI  0x0b

#define MAX7456_MASK_PAL  0x40 // PAL mask 01000000
#define MAX7456_MASK_NTCS 0x00 // NTSC mask 00000000

#define bis(var, bit) (var & BV (bit))

#define BV(bit) (1 << (bit))

///////////////////////////////////////////////////////////////////////////////

struct max7456_dev_s {
#define MAX7456_MAGIC 0x36353437	/* '7456' */
	uint32_t magic;
	uint32_t spi_id;
	uint32_t slave_id;

	uint8_t mode, right, bottom, hcenter, vcenter;

	uint8_t mask;
	bool opened;
};

/* Max7456 says 100ns period (10MHz) is OK.  But it may be off-board in
 * some circumstances, so let's not push our luck.
 */
#define MAX7456_SPI_SPEED 6000000

static inline void chip_select(max7456_dev_t dev)
{
	PIOS_SPI_ClaimBus(dev->spi_id);

	PIOS_SPI_RC_PinSet(dev->spi_id, dev->slave_id, false);

	PIOS_SPI_SetClockSpeed(dev->spi_id, MAX7456_SPI_SPEED);
}

static inline void chip_unselect(max7456_dev_t dev)
{
	PIOS_SPI_RC_PinSet(dev->spi_id, dev->slave_id, true);

	PIOS_SPI_ReleaseBus(dev->spi_id);
}

// Write VM0[3] = 1 to enable the display of the OSD image.
// mode | autosync | vsync on | enable OSD
#define enable_osd(dev) do { write_register (dev, MAX7456_REG_VM0, dev->mask | 0x0c); } while (0)
#define disable_osd(dev) do { write_register (dev, MAX7456_REG_VM0, 0); } while (0)

void PIOS_MAX7456_wait_vsync ()
{
	// XXX needs interrupt / semaphore mechanism for this role.
	//loop_until_bit_is_clear (MAX7456_VSYNC_PIN, MAX7456_VSYNC_BIT);
}

static uint8_t read_register (max7456_dev_t dev, uint8_t reg)
{
	PIOS_SPI_TransferByte (dev->spi_id, reg | 0x80);
	return PIOS_SPI_TransferByte (dev->spi_id, 0xff);
}

static void write_register (max7456_dev_t dev, uint8_t reg, uint8_t val)
{
	PIOS_SPI_TransferByte (dev->spi_id, reg);
	PIOS_SPI_TransferByte (dev->spi_id, val);
}

static inline void set_mode (max7456_dev_t dev, uint8_t value)
{
	dev->mode = value;
	if (value == MAX7456_MODE_NTSC)
	{
		dev->mask = MAX7456_MASK_NTCS;
		dev->right = MAX7456_NTSC_COLUMNS - 1;
		dev->bottom = MAX7456_NTSC_ROWS - 1;
		dev->hcenter = MAX7456_NTSC_HCENTER;
		dev->vcenter = MAX7456_NTSC_VCENTER;
		return;
	}
	dev->mask = MAX7456_MASK_PAL;
	dev->right = MAX7456_PAL_COLUMNS - 1;
	dev->bottom = MAX7456_PAL_ROWS - 1;
	dev->hcenter = MAX7456_PAL_HCENTER;
	dev->vcenter = MAX7456_PAL_VCENTER;
}

static inline void detect_mode (max7456_dev_t dev)
{
	chip_select (dev);
	// read STAT and auto detect video mode PAL/NTSC
	uint8_t stat = read_register (dev, MAX7456_REG_STAT);
	chip_unselect (dev);

	if (bis (stat, 0))
	{
		set_mode (dev, MAX7456_MODE_PAL);
		return;
	}
	if (bis (stat, 1))
	{
		set_mode (dev, MAX7456_MODE_NTSC);
		return;
	}

	// Give up / guess PAL
	set_mode (dev, MAX7456_MODE_NTSC); // XXX
}

int PIOS_MAX7456_init (max7456_dev_t *dev_out,
		uint32_t spi_handle, uint32_t slave_idx)
{
	// Prepare pins
	//sbi (MAX7456_SELECT_DDR, MAX7456_SELECT_BIT);
	//cbi (MAX7456_VSYNC_DDR, MAX7456_VSYNC_BIT);
	//sbi (MAX7456_VSYNC_PORT, MAX7456_VSYNC_BIT); // pull-up

	// Reset
	
	max7456_dev_t dev = PIOS_malloc(sizeof(*dev));

	bzero(dev, sizeof(*dev));
	dev->magic = MAX7456_MAGIC;
	dev->spi_id = spi_handle;
	dev->slave_id = slave_idx;

	chip_select (dev);
	write_register (dev, MAX7456_REG_VM0, BV (1));
	chip_unselect (dev);

	PIOS_DELAY_WaituS (100);

	chip_select (dev);
	while (read_register (dev, MAX7456_REG_VM0) & BV (1));
	chip_unselect (dev);

	// Detect video mode
	detect_mode (dev);

	chip_select (dev);
	/*
	Write OSDBL[4] = 0 to enable automatic OSD black
	level control. This ensures the correct OSD image
	brightness. This register contains 4 factory-preset
	bits [3:0] that must not be changed.
	*/
	write_register (dev, MAX7456_REG_OSDBL, read_register (dev, MAX7456_REG_OSDBL) & 0xef);

	write_register (dev, MAX7456_REG_VM1, 0x7f);

	enable_osd (dev);

	// set all rows to the same character brightness black/white level
	uint8_t brightness = MAX7456_DEFAULT_BRIGHTNESS;
	for (uint8_t r = 0; r < 16; ++ r)
		write_register (dev, 0x10 + r, brightness);

	chip_unselect (dev);

	*dev_out = dev;

	return 0;
}

void PIOS_MAX7456_clear (max7456_dev_t dev)
{
	if (dev->opened) PIOS_MAX7456_close (dev);
	chip_select (dev);
	write_register (dev, MAX7456_REG_DMM, 0x04);
	chip_unselect (dev);
	PIOS_DELAY_WaituS (30);
}

void PIOS_MAX7456_upload_char (max7456_dev_t dev, uint8_t char_index,
		uint8_t data [])
{
	chip_select (dev);
	disable_osd (dev);

	PIOS_DELAY_WaituS (10);

	// Write CMAH[7:0] = xxH to select the character (0–255) to be written
	write_register (dev, MAX7456_REG_CMAH, char_index);

	for (uint8_t i = 0; i < 54; i ++)
	{
		// Write CMAL[7:0] = xxH to select the 4-pixel byte (0–53) in the character to be written
		write_register (dev, MAX7456_REG_CMAL, i);
		// Write CMDI[7:0] = xxH to set the pixel values of the selected part of the character
		write_register (dev, MAX7456_REG_CMDI, data [i]);
	}

	// Write CMM[7:0] = 1010xxxx to write to the NVM array from the shadow RAM
	write_register (dev, MAX7456_REG_CMM, 0xa0);

	/*
	The character memory is busy for approximately 12ms during this operation.
	STAT[5] can be read to verify that the NVM writing process is complete.
	*/
	while (read_register (dev, MAX7456_REG_STAT) & 0x20) {
		chip_unselect (dev);
		PIOS_Thread_Sleep(1);
		chip_select (dev);
	}


	enable_osd (dev);
	chip_unselect (dev);
}

void PIOS_MAX7456_download_char (max7456_dev_t dev, uint8_t char_index,
		uint8_t data [])
{
	chip_select (dev);
	disable_osd (dev);

	PIOS_DELAY_WaituS(10);

	// Write CMAH[7:0] = xxH to select the character (0–255) to be read
	write_register (dev, MAX7456_REG_CMAH, char_index);

	// Write CMM[7:0] = 0101xxxx to read the character data from the NVM to the shadow RAM
	write_register (dev, MAX7456_REG_CMM, 0x50);
	/*
	 * The character memory is busy for approximately 12ms during this operation.
	 * The Character Memory Mode register is cleared and STAT[5] is reset to 0 after
	 * the write operation has been completed.
	 * 
	 * note: MPL: I don't see this being required in the datasheet for
	 * read operations.  But it doesnt hurt.
	 */
	while (read_register (dev, MAX7456_REG_STAT) & 0x20) {
		chip_unselect (dev);
		PIOS_Thread_Sleep(1);
		chip_select (dev);
	}

	for (uint8_t i = 0; i < 54; i ++)
	{
		// Write CMAL[7:0] = xxH to select the 4-pixel byte (0–53) in the character to be written
		write_register (dev, MAX7456_REG_CMAL, i);
		// Write CMDI[7:0] = xxH to set the pixel values of the selected part of the character
		data [i] = read_register (dev, MAX7456_REG_CMDO);
	}

	enable_osd (dev);
	chip_unselect (dev);
}

static inline void set_offset (max7456_dev_t dev, uint8_t col, uint8_t row)
{
	uint16_t offset = (row * 30 + col) & 0x1ff;
	write_register (dev, MAX7456_REG_DMAH, offset >> 8);
	write_register (dev, MAX7456_REG_DMAL, (uint8_t) offset);
}

void PIOS_MAX7456_put (max7456_dev_t dev, 
		uint8_t col, uint8_t row, uint8_t chr, uint8_t attr)
{
	PIOS_Assert(!dev->opened);

	chip_select (dev);
	set_offset (dev, col, row);
	write_register (dev, MAX7456_REG_DMM, (attr & 0x07) << 3);
	write_register (dev, MAX7456_REG_DMDI, chr);
	chip_unselect (dev);
}

void PIOS_MAX7456_open (max7456_dev_t dev, uint8_t col, uint8_t row, uint8_t attr)
{
	PIOS_Assert(!dev->opened);

	dev->opened = true;

	chip_select (dev);
	set_offset (dev, col > dev->right ? 0 : col, row > dev->bottom ? 0 : row);

	// 16 bits operating mode, char attributes, autoincrement
	write_register (dev, MAX7456_REG_DMM, ((attr & 0x07) << 3) | 0x01);
}

void PIOS_MAX7456_close (max7456_dev_t dev)
{
	PIOS_Assert(dev->opened);

	// terminate autoincrement mode
	write_register (dev, MAX7456_REG_DMDI, 0xff);
	chip_unselect (dev);
	dev->opened = false;
}

// 0xff terminates autoincrement mode
#define valid_char(c) (c == 0xff ? 0x00 : c)

void PIOS_MAX7456_puts (max7456_dev_t dev, uint8_t col, uint8_t row, const char *s, uint8_t attr)
{
	PIOS_MAX7456_open (dev, col, row, attr);
	while (*s)
	{
		write_register (dev, MAX7456_REG_DMDI, valid_char (*s));
		s ++;
	}
	PIOS_MAX7456_close (dev);
}

void PIOS_MAX7456_get_extents(max7456_dev_t dev, 
		uint8_t *mode, uint8_t *right, uint8_t *bottom,
		uint8_t *hcenter, uint8_t *vcenter)
{
	if (mode) {
		*mode=dev->mode;
	}

	if (right) {
		*right=dev->right;
	}

	if (bottom) {
		*bottom=dev->bottom;
	}

	if (hcenter) {
		*hcenter=dev->hcenter;
	}

	if (vcenter) {
		*vcenter=dev->vcenter;
	}
}

#endif /* PIOS_INCLUDE_MAX7456 */
