/**
 ******************************************************************************
 * @addtogroup TauLabsModules Tau Labs Modules
 * @{ 
 * @addtogroup VTXConfig Module
 * @{ 
 *
 * @file       tbs_smartaudio.c
 * @author     dRonin, http://dRonin.org/, Copyright (C) 2016
 *
 * @brief      This module configures the video transmitter
 * @see        The GNU Public License (GPL) Version 3
 *
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
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Additional note on redistribution: The copyright and license notices above
 * must be maintained in each individual source file that is a derivative work
 * of this source file; otherwise redistribution is prohibited.
 */

#include "openpilot.h"
#include "pios_crc.h"

#include "vtxinfo.h"

#define NUM_TBS_CH 40

const uint16_t TBS_CH[NUM_TBS_CH] = {
	5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725, // band A
	5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866, // band B
	5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945, // band E
	5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880, // Airwave
	5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917  // Raceband
};



typedef struct {
	uint8_t command;
	uint8_t length;
	uint8_t channel;
	uint8_t pwr_level;
	uint8_t operation_mode;
	uint8_t freq0;
	uint8_t freq1;
	uint8_t crc;
} __attribute__((packed)) GET_INFO;

typedef struct {
	uint8_t command;
	uint8_t length;
	uint8_t channel;
	uint8_t dummy;
	uint8_t crc;
} __attribute__((packed)) SET_CHANNEL;


typedef struct {
	uint8_t command;
	uint8_t length;
	uint8_t pwr;
	uint8_t dummy;
	uint8_t crc;
} __attribute__((packed)) SET_POWER;


int32_t tbsvtx_rx_msg(uintptr_t usart_id, uint8_t n_bytes, uint8_t *buff, uint16_t timeout)
{
	uint32_t start = PIOS_Thread_Systime();
	uint8_t c;
	uint8_t bytes_rx = 0;

	// Wait for second byte of start sequence. Should be 0x55 but is received as 0xd5 by STM32
	do {
		PIOS_COM_ReceiveBuffer(usart_id, &c, 1, 2);
		if (PIOS_Thread_Systime() - start > timeout) {
			return - 1;
		}
	} while (c != 0xd5);

	while(bytes_rx < n_bytes) {
		if (PIOS_COM_ReceiveBuffer(usart_id, &c, 1, 2) > 0) {
			buff[bytes_rx++] = c;
		}
		if (PIOS_Thread_Systime() - start > timeout) {
			return - 1;
		}
	}

	// check CRC
	uint8_t crc = PIOS_CRC_updateCRC_TBS(0, buff, n_bytes - 1);

	if (crc != buff[n_bytes - 1]) {
		return - 2;
	}

	return 0;
}


int32_t tbsvtx_get_state(uintptr_t usart_id, VTXInfoData *info)
{
	// Send dummy 00 byte before, so line goes high
	uint8_t msg[5] = {0x00, 0xAA, 0x55, 0x03, 0x00};
	PIOS_COM_SendBuffer(usart_id, msg, 5);

	GET_INFO info_msg;
	if (tbsvtx_rx_msg(usart_id, sizeof(info_msg), (uint8_t*)&info_msg, 200) < 0) {
		return -1;
	}

	// Do some additional sanity checks
	if ((info_msg.command != 0x01) || (info_msg.length != 6) || (info_msg.channel >= NUM_TBS_CH)) {
		return -2;
	}

	info->Frequency = TBS_CH[info_msg.channel];
	switch(info_msg.pwr_level) {
		case 0x07:
			info->Power = 25;
			break;
		case 0x10:
			info->Power = 200;
			break;
		case 0x19:
			info->Power = 500;
			break;
		case 0x28:
			info->Power = 800;
			break;
		default:
			info->Power = 25;
	}

	info->Model = VTXINFO_MODEL_TBSUNIFYPRO5G8;
	info->SupporedPowerLevels[0] = VTXINFO_SUPPOREDPOWERLEVELS_25MW;
	info->SupporedPowerLevels[1] = VTXINFO_SUPPOREDPOWERLEVELS_200MW;
	info->SupporedPowerLevels[2] = VTXINFO_SUPPOREDPOWERLEVELS_500MW;
	info->SupporedPowerLevels[3] = VTXINFO_SUPPOREDPOWERLEVELS_800MW;
	info->SupporedPowerLevels[4] = VTXINFO_SUPPOREDPOWERLEVELS_NONE;

	info->SupportedBands[0] = VTXINFO_SUPPORTEDBANDS_5G8BANDA;
	info->SupportedBands[1] = VTXINFO_SUPPORTEDBANDS_5G8BANDB;
	info->SupportedBands[2] = VTXINFO_SUPPORTEDBANDS_5G8BANDE;
	info->SupportedBands[3] = VTXINFO_SUPPORTEDBANDS_5G8AIRWAVE;
	info->SupportedBands[4] = VTXINFO_SUPPORTEDBANDS_5G8RACEBAND;
	info->SupportedBands[5] = VTXINFO_SUPPORTEDBANDS_NONE;

	memcpy(info->RxBytes, (void*)&info_msg, 8);

	return 0;
}

int32_t tbsvtx_set_freq(uintptr_t usart_id, uint16_t frequency)
{
	// find the channel
	uint8_t channel = 255;
	for (int i=0; i<NUM_TBS_CH; i++) {
		if (TBS_CH[i] == frequency) {
			channel = i;
			break;
		}
	}
	if (channel == 255) {
		// invalid frequency
		return -1;
	}

	uint8_t set_ch_msg[7] = {0x00, 0xAA, 0x55, 0x07, 0x01, 0x00, 0x00};
	set_ch_msg[5] = channel;
	PIOS_COM_SendBuffer(usart_id, set_ch_msg, 7);

	SET_CHANNEL set_ch_resp;
	if (tbsvtx_rx_msg(usart_id, sizeof(set_ch_resp), (uint8_t*)&set_ch_resp, 200) < 0) {
		return -2;
	}

	// Do some sanity checks
	if ((set_ch_resp.command != 0x03) || (set_ch_resp.length != 0x03) || (set_ch_resp.channel != channel)) {
		return -3;
	}

	return 0;
}

int32_t tbsvtx_set_power(uintptr_t usart_id, uint16_t power)
{
	uint8_t power_byte;
	switch (power) {
		case 25:
			power_byte = 0x07;
			break;
		case 200:
			power_byte = 0x10;
			break;
		case 500:
			power_byte = 0x19;
			break;
		case 800:
			power_byte = 0x28;
			break;
		default:
			return -1;
	}

	uint8_t set_pwr_msg[7] = {0x00, 0xAA, 0x55, 0x05, 0x01, 0x07, 0x00};
	set_pwr_msg[5] = power_byte;
	PIOS_COM_SendBuffer(usart_id, set_pwr_msg, 7);

	SET_POWER set_pwr_resp;
	if (tbsvtx_rx_msg(usart_id, sizeof(set_pwr_resp), (uint8_t*)&set_pwr_resp, 200) < 0) {
		return -2;
	}

	// Do some sanity checks
	if ((set_pwr_resp.command != 0x02) || (set_pwr_resp.length != 0x03) || (set_pwr_resp.pwr != power_byte)) {
		return -3;
	}

	return 0;
}
