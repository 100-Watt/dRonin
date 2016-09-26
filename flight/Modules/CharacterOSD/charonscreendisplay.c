/**
 ******************************************************************************
 * @addtogroup Tau Labs Modules
 * @{
 * @addtogroup CharacterOSD OSD Module
 * @brief Process OSD information
 * @{
 *
 * @file       characterosd.c
 * @author     dRonin, http://dronin.org Copyright (C) 2016
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

#include <openpilot.h>
#include <pios_board_info.h>
#include "pios_thread.h"
#include "misc_math.h"
#include "pios_modules.h"
#include "pios_sensors.h"

#include "modulesettings.h"
#include "pios_video.h"

#include "physical_constants.h"

#include "accels.h"
#include "accessorydesired.h"
#include "airspeedactual.h"
#include "attitudeactual.h"
#include "baroaltitude.h"
#include "flightstatus.h"
#include "flightbatterystate.h"
#include "flightbatterysettings.h"
#include "flightstats.h"
#include "gpsposition.h"
#include "positionactual.h"
#include "gpstime.h"
#include "gpssatellites.h"
#include "gpsvelocity.h"
#include "homelocation.h"
#include "magnetometer.h"
#include "manualcontrolcommand.h"
#include "modulesettings.h"
#include "stabilizationsettings.h"
#include "stateestimation.h"
#include "systemalarms.h"
#include "systemstats.h"
#include "tabletinfo.h"
#include "taskinfo.h"
#include "velocityactual.h"
#include "vtxinfo.h"
#include "waypoint.h"
#include "waypointactive.h"

#include "charosd.h"

#define STACK_SIZE_BYTES 3072
#define TASK_PRIORITY PIOS_THREAD_PRIO_LOW

#define SPLASH_TIME_MS (5*1000)

bool module_enabled;

static void panel_draw (charosd_state_t state, uint8_t panel, uint8_t x, uint8_t y)
{
	if (panel <= CHARONSCREENDISPLAYSETTINGS_PANELTYPE_MAXOPTVAL) {
		if (panels[panel].update) {
			panels [panel].update(state, x, y);
		}
	}
}

static void screen_draw (charosd_state_t state, CharOnScreenDisplaySettingsData *page)
{
	PIOS_MAX7456_clear (state->dev);

	for (uint8_t i = 0; i < CHARONSCREENDISPLAYSETTINGS_PANELTYPE_NUMELEM;
		       i++) {
		panel_draw(state, page->PanelType[i], page->X[i], page->Y[i]);
	}
}

static const uint8_t charosd_font_data[] = {
#include "charosd-font.h"
};

/* 12 x 18 x 2bpp */
#define FONT_CHAR_SIZE ((12 * 18 * 2) / 8)

static void program_characters(charosd_state_t state)
{
	/* Check on font data, replacing chars as appropriate. */
	for (int i = 0; i < 256; i++) {
		const uint8_t *this_char =
			& charosd_font_data[i * FONT_CHAR_SIZE];
		/* Every 8 characters, take a break, let other lowprio
		 * tasks run */
		if ((i & 0x7) == 0) {
			PIOS_Thread_Sleep(2);
		}

		uint8_t nvm_char[FONT_CHAR_SIZE];

		PIOS_MAX7456_download_char(state->dev, i, nvm_char);

		/* Only do this when necessary, because it's slow and wears
		 * NVRAM. */
		if (memcmp(nvm_char, this_char, FONT_CHAR_SIZE)) {
			PIOS_MAX7456_upload_char(state->dev, i,
					this_char);
		}
	}
}

static void update_telemetry(telemetry_t *telemetry)
{
	if (FlightBatteryStateHandle()) {
		FlightBatteryStateGet(&telemetry->battery);
	}
	SystemStatsFlightTimeGet(&telemetry->system.flight_time);
	AttitudeActualRollGet(&telemetry->actual.roll);
	AttitudeActualPitchGet(&telemetry->actual.pitch);
	PositionActualDownGet(&telemetry->actual.down);
	GPSPositionGet(&telemetry->gps_position);
	ManualControlCommandRssiGet(&telemetry->manual.rssi);
	ManualControlCommandThrottleGet(&telemetry->manual.throttle);
	FlightStatusArmedGet(&telemetry->flight_status.arm_status);
	FlightStatusFlightModeGet(&telemetry->flight_status.mode);
}

static void splash_screen(charosd_state_t state)
{
	const char *welcome_msg = "Welcome to dRonin";

	SystemAlarmsData alarm;
	SystemAlarmsGet(&alarm);
	const char *boot_reason = AlarmBootReason(alarm.RebootCause);
	PIOS_MAX7456_puts(state->dev, CENTER_STR(welcome_msg), 4, welcome_msg, 0);
	PIOS_MAX7456_puts(state->dev, CENTER_STR(boot_reason), 6, boot_reason, 0);

	PIOS_Thread_Sleep(SPLASH_TIME_MS);
}

/**
 * Main osd task. It does not return.
 */
static void CharOnScreenDisplayTask(void *parameters)
{
	(void) parameters;

	charosd_state_t state;

	state = PIOS_malloc(sizeof(*state));
	state->dev = pios_max7456_id;

	program_characters(state);

	splash_screen(state);

	while (1) {
		update_telemetry(&state->telemetry);

		CharOnScreenDisplaySettingsData page;
		CharOnScreenDisplaySettingsGet(&page);

		state->custom_text = (char*)page.CustomText;

		screen_draw(state, &page);

		PIOS_MAX7456_wait_vsync(state->dev);
	}
}

/**
 * Initialize the osd module
 */
int32_t CharOnScreenDisplayInitialize(void)
{
	CharOnScreenDisplaySettingsInitialize();

	if (pios_max7456_id) {	
		module_enabled = true;
	}

	return 0;
}

int32_t CharOnScreenDisplayStart(void)
{
	if (module_enabled) {
		struct pios_thread *taskHandle;

		taskHandle = PIOS_Thread_Create(CharOnScreenDisplayTask, "OnScreenDisplay", STACK_SIZE_BYTES, NULL, TASK_PRIORITY);
		TaskMonitorAdd(TASKINFO_RUNNING_ONSCREENDISPLAY, taskHandle);

		return 0;
	}
	return -1;
}

MODULE_INITCALL(CharOnScreenDisplayInitialize, CharOnScreenDisplayStart);

