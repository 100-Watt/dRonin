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

#include <openpilot.h>
#include <math.h>
#include <string.h>
#include <misc_math.h>

#include "charosd.h"

#include "flightstatus.h"


/*
 * 012
 * 3 7
 * 456
 */
static void draw_rect (charosd_state_t state, uint8_t l, uint8_t t, uint8_t w, uint8_t h, bool filled, uint8_t attr)
{
	const uint8_t _rect_thin [] = {0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7};
	const uint8_t _rect_fill [] = {0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf};

	if (w < 2 || h < 2) return;

	uint8_t r = w - 1;
	uint8_t b = h - 1;

	const uint8_t *rect = filled ? _rect_fill : _rect_thin;

	char buffer [w + 1];

	for (uint8_t i = 0; i < h; i ++)
	{
		char spacer;
		if (i == 0)
		{
			buffer [0] = rect [0];
			buffer [r] = rect [2];
			spacer = rect [1];
		}
		else if (i == b)
		{
			buffer [0] = rect [4];
			buffer [r] = rect [6];
			spacer = rect [5];
		}
		else
		{
			buffer [0] = rect [3];
			buffer [r] = rect [7];
			spacer = ' ';
		}

		if (w > 2)
			memset (buffer + 1, spacer, w - 2);

		buffer [w] = 0;

		PIOS_MAX7456_puts (state->dev, l, t + i, buffer, attr);
	}
}

#define _ARROWS 0x90

static void draw_arrow (charosd_state_t state, uint8_t x, uint8_t y, uint16_t direction, uint8_t attr)
{
	uint8_t chr = _ARROWS + ((uint8_t) (direction / 360.0 * 16)) * 2;
	PIOS_MAX7456_put (state->dev, x, y, chr, attr);
	PIOS_MAX7456_put (state->dev, x + 1, y, chr + 1, attr);
}

#define ERR_STR "---"


#define terminate_buffer() do { buffer [sizeof (buffer) - 1] = 0; } while (0)

#define STD_UPDATE(__name, n, fmt, ...) static void __name ## _update (charosd_state_t state, uint8_t x, uint8_t y) \
{ \
	char buffer[n]; \
	snprintf (buffer, sizeof (buffer), fmt, __VA_ARGS__); \
	terminate_buffer (); \
	PIOS_MAX7456_puts (state->dev, x, y, buffer, 0); \
}

#define STD_PANEL(__name, bs, fmt, ...) \
	STD_UPDATE (__name, bs, fmt, __VA_ARGS__); 


/* Alt */
STD_PANEL (ALTITUDE, 8, "\x85%d\x8d", (int16_t) round (-state->telemetry.actual.down));

/* Climb */
#define _PAN_CLIMB_SYMB 0x03

static void CLIMB_update (charosd_state_t state, uint8_t x, uint8_t y)
{
	int8_t c = round (/*XXX:telemetry::stable::climb*/ 0 * 10);
	uint8_t s;
	char buffer[8];

	if (c >= 20) s = _PAN_CLIMB_SYMB + 5;
	else if (c >= 10) s = _PAN_CLIMB_SYMB + 4;
	else if (c >= 0) s = _PAN_CLIMB_SYMB + 3;
	else if (c <= -20) s = _PAN_CLIMB_SYMB + 2;
	else if (c <= -10) s = _PAN_CLIMB_SYMB + 1;
	else s = _PAN_CLIMB_SYMB;
	snprintf (buffer, sizeof (buffer), "%c%.1f\x8c", s, /*XXX:telemetry::stable::climb*/ 0.0);
	terminate_buffer ();
	PIOS_MAX7456_puts (state->dev, x, y, buffer, 0);
}

/* FlightMode */

static void FLIGHTMODE_update (charosd_state_t state, uint8_t x, uint8_t y)
{
	const char *mode = "INIT";

	switch (state->telemetry.flight_status.mode) {
	case FLIGHTSTATUS_FLIGHTMODE_MANUAL:
		mode = "MAN";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_ACRO:
		mode = "ACRO";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_ACROPLUS:
		mode = "ACR+";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_LEVELING:
		mode = "LVL";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_HORIZON:
		mode = "HRZN";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_AXISLOCK:
		mode = "ALCK";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_VIRTUALBAR:
		mode = "VBAR";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_STABILIZED1:
		mode = "ST1";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_STABILIZED2:
		mode = "ST2";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_STABILIZED3:
		mode = "ST3";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_AUTOTUNE:
		mode = "TUNE";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_ALTITUDEHOLD:
		mode = "AHLD";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_POSITIONHOLD:
		mode = "PHLD";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_RETURNTOHOME:
		mode = "RTH";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_PATHPLANNER:
		mode = "PLAN";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_FAILSAFE:
		mode = "FAIL";
		break;
	case FLIGHTSTATUS_FLIGHTMODE_TABLETCONTROL:
		// There are many sub modes here that could be filled in.
		mode = "TAB";
		break;
	}

	draw_rect (state, x, y, 6, 3, 0, 0);
	PIOS_MAX7456_puts (state->dev, x + 1, y + 1, mode, 0);
}

/* ArmedFlag */
static void ARMEDFLAG_update (charosd_state_t state, uint8_t x, uint8_t y)
{
	uint8_t attr = state->telemetry.flight_status.arm_status == FLIGHTSTATUS_ARMED_ARMED ? 0 : MAX7456_ATTR_INVERT;
	draw_rect (state, x, y, 3, 3, true, attr);
	PIOS_MAX7456_put (state->dev, x + 1, y + 1, 0xe0, attr);
}

#define TIME_SYM 0xfa

static void FLIGHTTIME_update (charosd_state_t state, uint8_t x, uint8_t y) {
       char buffer[10];
       uint32_t time = state->telemetry.system.flight_time;
       int min, sec;

       uint16_t hours = (time / 3600000); // hours
       if (hours == 0) {
               min = time / 60000;
               sec = (time / 1000) - 60 * min;
               sprintf (buffer, "%c%02d:%02d", TIME_SYM, (int)min, (int)sec);
       } else {
               min = time / 60000 - 60 * hours;
               sec = (time / 1000) - 60 * min - 3600 * hours;
               sprintf (buffer, "%c%02d:%02d:%02d", TIME_SYM, (int)hours, (int)min, (int)sec);
       }

       terminate_buffer ();

       PIOS_MAX7456_puts (state->dev, x + 1, y + 1, buffer, 0);
}

/* Roll */
STD_PANEL (ROLL, 7, "\xb2%d\xb0", (int16_t) state->telemetry.actual.roll);

/* Pitch */
STD_PANEL (PITCH, 7, "\xb1%d\xb0", (int16_t) state->telemetry.actual.pitch);

/* GPS */

#define _PAN_GPS_2D 0x01
#define _PAN_GPS_3D 0x02

static void GPS_update (charosd_state_t state, uint8_t x, uint8_t y)
{
	char buffer[4];

	snprintf(buffer, sizeof (buffer), "%d", state->telemetry.gps_position.Satellites); 
	terminate_buffer ();
	bool err = state->telemetry.gps_position.Status < GPSPOSITION_STATUS_FIX2D;
	PIOS_MAX7456_puts (state->dev, x, y, "\x10\x11", err ? MAX7456_ATTR_INVERT : 0);
	PIOS_MAX7456_put (state->dev, x + 2, y, state->telemetry.gps_position.Status < GPSPOSITION_STATUS_FIX3D ? _PAN_GPS_2D : _PAN_GPS_3D,
		err ? MAX7456_ATTR_INVERT : (state->telemetry.gps_position.Status < GPSPOSITION_STATUS_FIX2D ? MAX7456_ATTR_BLINK : 0));
	if (err) PIOS_MAX7456_puts (state->dev, x + 3, y, ERR_STR, MAX7456_ATTR_INVERT);
	else PIOS_MAX7456_puts (state->dev, x + 3, y, buffer, 0);
}

/* Lat */
STD_PANEL (LATITUDE, 11, "\x83%02.6f", (double)state->telemetry.gps_position.Latitude / 10000000.0);

/* Lon */
STD_PANEL (LONGITUDE, 11, "\x84%02.6f", (double)state->telemetry.gps_position.Longitude / 10000000.0);

#if 0
namespace horizon
{

#define PANEL_HORIZON_WIDTH 14
#define PANEL_HORIZON_HEIGHT 5

#define _PAN_HORZ_LINE 0x16
#define _PAN_HORZ_TOP 0x0e

#define _PAN_HORZ_CHAR_LINES 18
#define _PAN_HORZ_VRES 9
#define _PAN_HORZ_INT_WIDTH (PANEL_HORIZON_WIDTH - 2)
#define _PAN_HORZ_LINES (PANEL_HORIZON_HEIGHT * _PAN_HORZ_VRES)
#define _PAN_HORZ_TOTAL_LINES (PANEL_HORIZON_HEIGHT * _PAN_HORZ_CHAR_LINES)

	PANEL_NAME ("Horizon");

	const char _line [PANEL_HORIZON_WIDTH + 1]   = "\xb8            \xb9";
	const char _center [PANEL_HORIZON_WIDTH + 1] = "\xc8            \xc9";
	char buffer [PANEL_HORIZON_HEIGHT][PANEL_HORIZON_WIDTH + 1];

	float __attribute__ ((noinline)) deg2rad (float deg)
	{
		return deg * 0.017453293;
	}

	void update ()
	{
		for (uint8_t i = 0; i < PANEL_HORIZON_HEIGHT; i ++)
		{
			memcpy_P (buffer [i], i == PANEL_HORIZON_HEIGHT / 2 ? _center : _line, PANEL_HORIZON_WIDTH);
			buffer [i][PANEL_HORIZON_WIDTH] = 0;
		}

		// code below from minoposd
		int16_t pitch_line = tan (deg2rad (/*XXX:telemetry::attitude::pitch*/ 0)) * _PAN_HORZ_LINES;
		float roll = tan (deg2rad (/*XXX:telemetry::attitude::roll*/ 0));
		for (uint8_t col = 1; col <= _PAN_HORZ_INT_WIDTH; col ++)
		{
			// center X point at middle of each column
			float middle = col * _PAN_HORZ_INT_WIDTH - (_PAN_HORZ_INT_WIDTH * _PAN_HORZ_INT_WIDTH / 2) - _PAN_HORZ_INT_WIDTH / 2;
			// calculating hit point on Y plus offset to eliminate negative values
			int16_t hit = roll * middle + pitch_line + _PAN_HORZ_LINES;
			if (hit > 0 && hit < _PAN_HORZ_TOTAL_LINES)
			{
				int8_t row = PANEL_HORIZON_HEIGHT - ((hit - 1) / _PAN_HORZ_CHAR_LINES);
				int8_t subval = (hit - (_PAN_HORZ_TOTAL_LINES - row * _PAN_HORZ_CHAR_LINES + 1)) * _PAN_HORZ_VRES / _PAN_HORZ_CHAR_LINES;
				if (subval == _PAN_HORZ_VRES - 1)
					buffer [row - 2][col] = _PAN_HORZ_TOP;
				buffer [row - 1][col] = _PAN_HORZ_LINE + subval;
			}
		}
	}

	void draw (uint8_t x, uint8_t y)
	{
		for (uint8_t i = 0; i < PANEL_HORIZON_HEIGHT; i ++)
			PIOS_MAX7456_puts (state->dev, x, y + i, buffer [i]);
	}

}  // namespace horizon
#endif

/* Throttle */
STD_PANEL (THROTTLE, 7, "\x87%d%%", (int)MAX(0, state->telemetry.manual.throttle*100));

/* GroundSpeed */
STD_PANEL (GROUNDSPEED, 7, "\x80%d\x81", (int16_t) (/*XXX:telemetry::stable::groundspeed*/ 0 * 3.6));

STD_PANEL (BATTERYVOLT, 8, "%.2f\x8e", (double)state->telemetry.battery.Voltage);

/* BatCurrent */
STD_PANEL (BATTERYCURRENT, 8, "%.2f\x8f", (double)state->telemetry.battery.Current);

/* BatConsumed */
STD_PANEL (BATTERYCONSUMED, 8, "\xfb%u\x82", (uint16_t) state->telemetry.battery.ConsumedEnergy);

/* RSSIFlag */
static void RSSIFLAG_update (charosd_state_t state, uint8_t x, uint8_t y)
{
	if (state->telemetry.manual.rssi < 50) {
		PIOS_MAX7456_put (state->dev, x, y, 0xb4, MAX7456_ATTR_BLINK);
	}
}

#if 0
/* HomeDistance */
DECLARE_BUF (8);

uint8_t attr, i_attr; /* XXX */
static void update ()
{
	attr = /*XXX:telemetry::home::state*/ 0 == /*XXX:telemetry::home::NO*/ 0_FIX ? MAX7456_ATTR_INVERT : 0;
	i_attr = /*XXX:telemetry::home::state*/ 0 != /*XXX:telemetry::home::FIXED*/ 0 ? MAX7456_ATTR_INVERT : 0;
	if (i_attr)
	{
		snprintf_P (buffer, sizeof (buffer), "%S",
			/*XXX:telemetry::home::state*/ 0 == /*XXX:telemetry::home::NO*/ 0_FIX ? ERR_STR : "\x09\x09\x09\x8d");
		return;
	}
	if (/*XXX:telemetry::home::distance*/ 0 >= 10000)
		 snprintf_P (buffer, sizeof (buffer), "%.1f\x8b", /*XXX:telemetry::home::distance*/ 0 / 1000);
	else snprintf_P (buffer, sizeof (buffer), "%u\x8d", (uint16_t) /*XXX:telemetry::home::distance*/ 0);
}

static void draw (uint8_t x, uint8_t y)
{
	PIOS_MAX7456_put (state->dev, x, y, 0x12, i_attr);
	PIOS_MAX7456_puts (state->dev, x + 1, y, buffer, attr);
}

#endif 

/* HomeDirection */
#define _PAN_HD_ARROWS 0x90

static void HOMEDIRECTION_update (charosd_state_t state, uint8_t x, uint8_t y)
{
	if (/*XXX:telemetry::home::state*/ 0 == /*XXX:telemetry::home::FIXED*/ 0)
		draw_arrow (state, x, y, /*XXX:telemetry::home::direction*/ 0, 0);
}

/* Callsign */
STD_PANEL(CALLSIGN, 11, "%s", state->custom_text);

/* Temperature */
STD_PANEL (TEMPERATURE, 9, "\xfd%.1f\x8a", /*XXX:telemetry::environment::temperature*/ 0.0);

/* RSSI */
static void RSSI_update (charosd_state_t state, uint8_t x, uint8_t y)
{
	const char _l0 [] = "\xe5\xe8\xe8";
	const char _l1 [] = "\xe2\xe8\xe8";
	const char _l2 [] = "\xe2\xe6\xe8";
	const char _l3 [] = "\xe2\xe3\xe8";
	const char _l4 [] = "\xe2\xe3\xe7";
	const char _l5 [] = "\xe2\xe3\xe4";

	const char * const levels [] = { _l0, _l1, _l2, _l3, _l4, _l5 };

	uint8_t level = round (state->telemetry.manual.rssi / 20.0);
	if (level == 0 && state->telemetry.manual.rssi > 0) level = 1;
	if (level > 5) level = 5;

	PIOS_MAX7456_puts (state->dev, x, y, levels[level], 0);
}

#if 0
/* Compass */
DECLARE_BUF (12);

uint8_t attr, arrow; /* XXX */

static void update ()
{
	const uint8_t ruler [] = {
		0xc2, 0xc0, 0xc1, 0xc0,
		0xc4, 0xc0, 0xc1, 0xc0,
		0xc3, 0xc0, 0xc1, 0xc0,
		0xc5, 0xc0, 0xc1, 0xc0,
	};

	const int8_t ruler_size = sizeof (ruler);

	int16_t offset = round (/*XXX:telemetry::stable::heading*/ 0 * ruler_size / 360.0) - (sizeof (buffer) - 1) / 2;
	if (offset < 0) offset += ruler_size;
	for (uint8_t i = 0; i < sizeof (buffer) - 1; i ++)
	{
		buffer [i] = pgm_read_byte (&ruler [offset]);
		if (++ offset >= ruler_size) offset = 0;
	}
	terminate_buffer ();
	switch (/*XXX:telemetry::stable::heading*/ 0_source)
	{
		case /*XXX:telemetry::stable::DISABLED:*/ 0
			attr = MAX7456_ATTR_INVERT;
			arrow = 0xc6;
			break;
		case /*XXX:telemetry::stable::GPS:*/ 0
			attr = 0;
			arrow = 0xb6;
			break;
		case /*XXX:telemetry::stable::INTERNAL*/ 0_MAG:
			attr = 0;
			arrow = 0xc7;
			break;
		default:
			attr = 0;
			arrow = 0xc6;
	}
}

static void compass_draw (uint8_t x, uint8_t y)
{
	PIOS_MAX7456_put (state->dev, x + (sizeof (buffer) - 1) / 2, y, arrow, attr);
	PIOS_MAX7456_puts (state->dev, x, y + 1, buffer, attr);
}
#endif

/* Airspeed */
STD_PANEL (AIRSPEED, 7, "\x88%d\x81", (int16_t) (/*XXX:telemetry::stable::airspeed*/ 0 * 3.6));

#define declare_panel(__name) [CHARONSCREENDISPLAYSETTINGS_PANELTYPE_ ## __name] = { __name ## _update }

const panel_t panels [CHARONSCREENDISPLAYSETTINGS_PANELTYPE_MAXOPTVAL+1] = {
	declare_panel (AIRSPEED),
	declare_panel (ALTITUDE),
	declare_panel (ARMEDFLAG),
	declare_panel (BATTERYVOLT),
	declare_panel (BATTERYCURRENT),
	declare_panel (BATTERYCONSUMED),
	declare_panel (CALLSIGN),
	declare_panel (CLIMB),
//	declare_panel (COMPASS),
	declare_panel (FLIGHTMODE),
	declare_panel (FLIGHTTIME),
	declare_panel (GROUNDSPEED),
	declare_panel (GPS),
//	declare_panel (home_distance),
	declare_panel (HOMEDIRECTION),
//	declare_panel (horizon),
	declare_panel (LATITUDE),
	declare_panel (LONGITUDE),
	declare_panel (PITCH),
	declare_panel (ROLL),
	declare_panel (RSSIFLAG),
	declare_panel (RSSI),
	declare_panel (TEMPERATURE),
	declare_panel (THROTTLE),
};

const uint8_t count = sizeof (panels) / sizeof (panel_t);
