/**
 ******************************************************************************
 * @addtogroup TauLabsModules Tau Labs Modules
 * @{
 * @addtogroup GSPModule GPS Module
 * @brief Process GPS information
 * @{
 *
 * @file       ubx_cfg.h
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2013
 * @brief      Include file for UBX configuration
 * @see        The GNU Public License (GPL) Version 3
 *
 * Copyright © 2011, 2012, 2013  Bill Nesbitt
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
 */

#include "openpilot.h"
#include "pios_com.h"
#include "modulesettings.h"

/*
 * The format of UBX Packets is documented in the UBX Protocol
 * documentation and is summarized below
 *
 * 1 byte - SYNC_CHAR1 (UBLOX_SYNC1 = 0xB5)
 * 1 byte - SYNC_CHAR2 (UBLOX_SYNC2 = 0x62)
 * 1 byte - CLASS 
 * 1 byte - ID
 * 2 byte - length (little endian)
 * N bytes - payload
 * 2 bytes - checksum
 *
 * The checksum is calculated from the class to the last byte of
 * the payload
 */
#define UBLOX_SYNC1     0xB5
#define UBLOX_SYNC2     0x62

#define UBLOX_NAV_CLASS     0x01
#define UBLOX_RXM_CLASS     0x02
#define UBLOX_CFG_CLASS     0x06
#define UBLOX_MON_CLASS     0x0a
#define UBLOX_AID_CLASS     0x0b
#define UBLOX_TIM_CLASS     0x0d

#define UBLOX_NAV_POSLLH    0x02
#define UBLOX_NAV_STATUS    0x03
#define UBLOX_NAV_DOP       0x04
#define UBLOX_NAV_VELNED    0x12
#define UBLOX_NAV_TIMEUTC   0x21
#define UBLOX_NAV_SBAS      0x32
#define UBLOX_NAV_SVINFO    0x30

#define UBLOX_AID_REQ       0x00

#define UBLOX_RXM_RAW       0x10
#define UBLOX_RXM_SFRB      0x11

#define UBLOX_MON_VER       0x04
#define UBLOX_MON_HW        0x09

#define UBLOX_TIM_TP        0x01

#define UBLOX_CFG_MSG       0x01
#define UBLOX_CFG_TP        0x07
#define UBLOX_CFG_RTATE     0x08
#define UBLOX_CFG_SBAS      0x16
#define UBLOX_CFG_NAV5      0x24

#define UBLOX_SBAS_AUTO     0x00000000
#define UBLOX_SBAS_WAAS     0x0004E004
#define UBLOX_SBAS_EGNOS    0x00000851
#define UBLOX_SBAS_MSAS     0x00020200
#define UBLOX_SBAS_GAGAN    0x00000108

#define UBLOX_MAX_PAYLOAD   384
#define UBLOX_WAIT_MS       20

uint8_t ubloxTxCK_A, ubloxTxCK_B;

static void ubx_cfg_send_checksummed(uintptr_t gps_port, const uint8_t *dat, uint16_t len);

//! Reset the TX checksum calculation
static void ubloxTxChecksumReset(void) {
    ubloxTxCK_A = 0;
    ubloxTxCK_B = 0;
}

//! Update the checksum calculation
static void ubloxTxChecksum(uint8_t c) {
    ubloxTxCK_A += c;
    ubloxTxCK_B += ubloxTxCK_A;
}

//! Enable the selected UBX message at the specified rate
static void ubx_cfg_enable_message(uintptr_t gps_port,
    uint8_t c, uint8_t i, uint8_t rate) {

    const uint8_t msg[] = {
        UBLOX_CFG_CLASS,       // CFG
        UBLOX_CFG_MSG,         // MSG
        0x03,                  // length lsb
        0x00,                  // length msb
        c,                     // class
        i,                     // id
        rate,                  // rate
    };
    ubx_cfg_send_checksummed(gps_port, msg, sizeof(msg));
}

//! Set the rate of all messages
static void ubx_cfg_set_rate(uintptr_t gps_port, uint16_t ms) {

    const uint8_t msg[] = {
        UBLOX_CFG_CLASS,       // CFG
        UBLOX_CFG_RTATE,       // RTATE
        0x06,                  // length lsb
        0x00,                  // length msb
        ms,                    // rate lsb
        ms >> 8,               // rate msb
        0x01,                  // cycles
        0x01,                  // timeref 1 = GPS time
    };
    ubx_cfg_send_checksummed(gps_port, msg, sizeof(msg));
}

//! Configure the navigation mode and minimum fix
static void ubx_cfg_set_mode(uintptr_t gps_port) {

    const uint8_t msg[] = {
        UBLOX_CFG_CLASS,       // CFG
        UBLOX_CFG_NAV5,        // NAV5 mode
        0x24,                  // length lsb - 36 bytes
        0x00,                  // length msb
        0b0000101,             // mask LSB (fixMode, dyn)
        0x00,                  // mask MSB (reserved)
        0x06,                  // dynamic model (6 - airborne < 1g)
        0x02,                  // fixmode (2 - 3D only)
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,       // padded with 32 zeros
    };
    ubx_cfg_send_checksummed(gps_port, msg, sizeof(msg));
}

//! Configure the timepulse output pin
static void ubx_cfg_set_timepulse(uintptr_t gps_port) {
    const uint8_t TP_POLARITY = 1;
    const uint32_t int_us = 1000000;
    const uint32_t len_us = 100000;

    const uint8_t msg[] = {
        UBLOX_CFG_CLASS,       // CFG
        UBLOX_CFG_TP,          // TP
        0x14,                  // length lsb
        0x00,                  // length msb
        int_us & 0xff,
        int_us >> 8 & 0xff,   // interval (us)
        int_us >> 16 & 0xff,
        int_us >> 24 & 0xff,
        len_us & 0xff,
        len_us >> 8 & 0xff,     // length (us)
        len_us >> 16 & 0xff,
        len_us >> 24 & 0xff,
        TP_POLARITY,           // polarity - zero off
        0x01,                  // 1 - GPS time
        0x00,                  // bitmask
        0x00,                  // reserved
        0x00, 0x00,            // antenna delay
        0x00, 0x00,            // rf group delay
        0, 0, 0, 0             // user delay
    };
    ubx_cfg_send_checksummed(gps_port, msg, sizeof(msg));
}

//! Enable or disable SBAS satellites
static void ubx_cfg_set_sbas(uintptr_t gps_port, uint8_t enable) {
    // second bit of mode field is diffCorr
    enable = (enable > 0);

    const uint8_t msg[] = {
        UBLOX_CFG_CLASS, // CFG
        UBLOX_CFG_SBAS,  // SBAS
        0x08,            // length lsb
        0x00,            // length msb
        enable,          // enable flag
        0b011,           // mode
        3,               // # SBAS tracking channels
        0,
        UBLOX_SBAS_AUTO,
        UBLOX_SBAS_AUTO >> 4,
        UBLOX_SBAS_AUTO >> 8,
        UBLOX_SBAS_AUTO >> 12,
    };

    ubx_cfg_send_checksummed(gps_port, msg, sizeof(msg));
}

static void ubx_cfg_poll_version(uintptr_t gps_port) {
    const uint8_t msg[] = {UBLOX_MON_CLASS, UBLOX_MON_VER};
    ubx_cfg_send_checksummed(gps_port, msg, sizeof(msg));
}

static void ubx_cfg_version_specific(uintptr_t gps_port, uint8_t ver) {
    if (ver > 6) {
        // 10Hz for ver 7+
        ubx_cfg_set_rate(gps_port, (uint16_t)100);
        // SBAS screwed up on v7 modules w/ v1 firmware
        ubx_cfg_set_sbas(gps_port, 0); // disable SBAS
    } else {
        // 5Hz
        ubx_cfg_set_rate(gps_port, (uint16_t)200);
    }
}

//! Send a stream of data followed by checksum
static void ubx_cfg_send_checksummed(uintptr_t gps_port,
    const uint8_t *dat, uint16_t len)
{
    // Calculate checksum
    ubloxTxChecksumReset();
    for (uint16_t i = 0; i < len; i++) {
        ubloxTxChecksum(dat[i]);
    }

    // Send buffer followed by checksum
    PIOS_COM_SendChar(gps_port, UBLOX_SYNC1);
    PIOS_COM_SendChar(gps_port, UBLOX_SYNC2);
    PIOS_COM_SendBuffer(gps_port, dat, len);
    PIOS_COM_SendChar(gps_port, ubloxTxCK_A);
    PIOS_COM_SendChar(gps_port, ubloxTxCK_B);

    vTaskDelay(TICKS2MS(UBLOX_WAIT_MS));
}

void ubx_cfg_send_configuration(uintptr_t gps_port)
{
    vTaskDelay(MS2TICKS(UBLOX_WAIT_MS));

    ubx_cfg_set_timepulse(gps_port);

    ubx_cfg_enable_message(gps_port, UBLOX_NAV_CLASS, UBLOX_NAV_VELNED, 1);	// NAV VELNED
    ubx_cfg_enable_message(gps_port, UBLOX_NAV_CLASS, UBLOX_NAV_POSLLH, 1);	// NAV POSLLH
    ubx_cfg_enable_message(gps_port, UBLOX_NAV_CLASS, UBLOX_NAV_STATUS, 1); // NAV STATUS
    ubx_cfg_enable_message(gps_port, UBLOX_NAV_CLASS, UBLOX_NAV_TIMEUTC, 5);    // NAV TIMEUTC
    ubx_cfg_enable_message(gps_port, UBLOX_NAV_CLASS, UBLOX_NAV_DOP, 5);    // NAV DOP

#ifdef GPS_DO_RTK
    ubx_cfg_enable_message(gps_port, UBLOX_TIM_CLASS, UBLOX_TIM_TP, 1);   // TIM TP
    ubx_cfg_enable_message(gps_port, UBLOX_AID_CLASS, UBLOX_AID_REQ, 1);  // AID REQ

    ubx_cfg_enable_message(gps_port, UBLOX_RXM_CLASS, UBLOX_RXM_RAW, 1);	// RXM RAW
    ubx_cfg_enable_message(gps_port, UBLOX_RXM_CLASS, UBLOX_RXM_SFRB, 1);	// RXM SFRB
#endif
#ifdef GPS_DEBUG
    ubx_cfg_enable_message(gps_port, UBLOX_NAV_CLASS, UBLOX_NAV_SVINFO, 1);	// NAV SVINFO
    ubx_cfg_enable_message(gps_port, UBLOX_NAV_CLASS, UBLOX_NAV_SBAS, 1);	// NAV SBAS
    ubx_cfg_enable_message(gps_port, UBLOX_MON_CLASS, UBLOX_MON_HW, 1);	// MON HW
#endif

    ubx_cfg_set_mode(gps_port);						// 3D, airborne
    ubx_cfg_poll_version(gps_port); 

    // Hardcoded version. The poll version method should fetch the
    // data but we need to link to that.
    ubx_cfg_version_specific(gps_port, 6);
}

//! Set the output baudrate to 230400
void ubx_cfg_set_baudrate(uintptr_t gps_port, ModuleSettingsGPSSpeedOptions baud_rate)
{
    // UBX,41 msg
    // 1 - portID
    // 0007 - input protocol (all)
    // 0001 - output protocol (ubx only)
    // 230400 - baudrate
    // 0 - no attempt to autobaud
    // 0x18 - baudrate
    const char * msg_2400 = "$PUBX,41,1,0007,0001,2400,0*1B\n";
    const char * msg_4800 = "$PUBX,41,1,0007,0001,4800,0*11\n";
    const char * msg_9600 = "$PUBX,41,1,0007,0001,9600,0*12\n";
    const char * msg_19200 = "$PUBX,41,1,0007,0001,19200,0*27\n";
    const char * msg_38400 = "$PUBX,41,1,0007,0001,38400,0*22\n";
    const char * msg_57600 = "$PUBX,41,1,0007,0001,57600,0*29\n";
    const char * msg_115200 = "$PUBX,41,1,0007,0001,115200,0*1A\n";

    const char *msg = "";
    uint32_t baud = 57600;
    switch (baud_rate) {
    case MODULESETTINGS_GPSSPEED_2400:
        msg = msg_2400;
        baud = 2400;
        break;
    case MODULESETTINGS_GPSSPEED_4800:
        msg = msg_4800;
        baud = 4800;
        break;
    case MODULESETTINGS_GPSSPEED_9600:
        msg = msg_9600;
        baud = 9600;
        break;
    case MODULESETTINGS_GPSSPEED_19200:
        msg = msg_19200;
        baud = 19200;
        break;
    case MODULESETTINGS_GPSSPEED_38400:
        msg = msg_38400;
        baud = 38400;
        break;
    case MODULESETTINGS_GPSSPEED_57600:
        msg = msg_57600;
        baud = 57600;
        break;
    case MODULESETTINGS_GPSSPEED_115200:
        msg = msg_115200;
        baud = 115200;
        break;
    }
    
    // Attempt to set baud rate to desired value from a number of
    // common rates
    const uint32_t baud_rates[] = {4800, 9600, 19200, 38400, 57600, 115200};
    for (uint32_t i = 0; i < NELEMENTS(baud_rates); i++) {
        PIOS_COM_ChangeBaud(gps_port, baud_rates[i]);
        vTaskDelay(TICKS2MS(UBLOX_WAIT_MS));
        PIOS_COM_SendString(gps_port, msg);
        vTaskDelay(TICKS2MS(UBLOX_WAIT_MS));
    }

    // Set to proper baud rate
    PIOS_COM_ChangeBaud(gps_port, baud);
}

/**
 * @}
 * @}
 */