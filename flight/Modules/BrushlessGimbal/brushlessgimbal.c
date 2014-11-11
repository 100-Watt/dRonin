/**
 ******************************************************************************
 * @addtogroup TauLabsModules Tau Labs Modules
 * @{
 * @addtogroup Gimbal output module
 * @brief Output the desired speeds
 * @{
 *
 * @file       brushlessgimbal.c
 * @author     Tau Labs, http://github.com/TauLabs Copyright (C) 2013-2014
 * @brief      Drives the gimbal outputs
 *
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
 */


#include "openpilot.h"
#include "misc_math.h"

#include "actuatordesired.h"
#include "brushlessgimbalsettings.h"
#include "cameradesired.h"
#include "gyros.h"
#include "pios_thread.h"
#include "pios_queue.h"

// Private constants
#define MAX_QUEUE_SIZE 2

#if defined(PIOS_BRUSHLESSGIMBAL_STACK_SIZE)
#define STACK_SIZE_BYTES PIOS_BRUSHLESSGIMBAL_STACK_SIZE
#else
#define STACK_SIZE_BYTES 1312
#endif

#define TASK_PRIORITY PIOS_THREAD_PRIO_HIGHEST
#define FAILSAFE_TIMEOUT_MS 100

// Private types

// Private variables
static struct pios_queue *queue;
static struct pios_thread *taskHandle;

// Private functions
static void brushlessGimbalTask(void* parameters);

/**
 * @brief Module initialization
 * @return 0
 */
static int32_t BrushlessGimbalStart()
{
	// Start main task
	taskHandle = PIOS_Thread_Create(brushlessGimbalTask, "BrushlessGimbal", STACK_SIZE_BYTES, NULL, TASK_PRIORITY);
	TaskMonitorAdd(TASKINFO_RUNNING_ACTUATOR, taskHandle);
	PIOS_WDG_RegisterFlag(PIOS_WDG_ACTUATOR);

	return 0;
}

/**
 * @brief Module initialization
 * @return 0
 */
static int32_t BrushlessGimbalInitialize()
{
	// Listen for ActuatorDesired updates (Primary input to this module)
	ActuatorDesiredInitialize();
	queue = PIOS_Queue_Create(MAX_QUEUE_SIZE, sizeof(UAVObjEvent));
	ActuatorDesiredConnectQueue(queue);

	BrushlessGimbalSettingsInitialize();

	return 0;
}
MODULE_INITCALL(BrushlessGimbalInitialize, BrushlessGimbalStart)

/**
 * @brief Gimbal output control task
 */
static void brushlessGimbalTask(void* parameters)
{
	UAVObjEvent ev;

	TIM2->CNT = 0;
	TIM3->CNT = 0;
	TIM15->CNT = 0;
	TIM17->CNT = 0;

	bool armed = false;
	bool previous_armed = false;
	while (1) {
		PIOS_WDG_UpdateFlag(PIOS_WDG_ACTUATOR);

		// Wait until the ActuatorDesired object is updated
		PIOS_Queue_Receive(queue, &ev, 1);

		previous_armed = armed;
		armed |= PIOS_Thread_Systime() > 10000;

		if (armed && !previous_armed) {
			PIOS_Brushless_SetUpdateRate(60000);
		}

		if (!armed)
			continue;

		CameraDesiredData cameraDesired;
		CameraDesiredGet(&cameraDesired);

		BrushlessGimbalSettingsData settings;
		BrushlessGimbalSettingsGet(&settings);

		bool locked = false;

		// If the frame is tilted by a large amount, lock the gimbal
		locked |= fabsf(cameraDesired.Roll) > settings.MaxAngle[BRUSHLESSGIMBALSETTINGS_MAXANGLE_ROLL] ||
		          fabsf(cameraDesired.Pitch) > settings.MaxAngle[BRUSHLESSGIMBALSETTINGS_MAXANGLE_PITCH];

		// If the difference between the setpoint and the frame is too large, lock the gimbal
		float pitch_setpoint = -cameraDesired.Declination;
		locked |= circular_modulus_deg(cameraDesired.Pitch - pitch_setpoint) > settings.MaxAngle[BRUSHLESSGIMBALSETTINGS_MAXANGLE_PITCH];

		PIOS_Brushless_Lock(locked);

		if (!locked) {

			ActuatorDesiredData actuatorDesired;
			ActuatorDesiredGet(&actuatorDesired);

			PIOS_Brushless_SetScale(settings.PowerScale[0], settings.PowerScale[1], settings.PowerScale[2]);
			PIOS_Brushless_SetMaxAcceleration(settings.SlewLimit[0], settings.SlewLimit[1], settings.SlewLimit[2]);

			PIOS_Brushless_SetSpeed(0, actuatorDesired.Roll * settings.MaxDPS[BRUSHLESSGIMBALSETTINGS_MAXDPS_ROLL], 0.001f);
			PIOS_Brushless_SetSpeed(1, actuatorDesired.Pitch  * settings.MaxDPS[BRUSHLESSGIMBALSETTINGS_MAXDPS_PITCH], 0.001f);

			// Use the gyros to set a damping term.  This creates a phase offset of the integrated
			// driving position to make the control pull against any momentum.  Essentially the main
			// output to the driver (above) is a velocity signal which the driver takes care of
			// integrating to create a position.  The current rate of roll creates a shift in that
			// position (without changing the integrated position).
			// This idea was taken from https://code.google.com/p/brushless-gimbal/
			GyrosData gyros;
			GyrosGet(&gyros);
			PIOS_Brushless_SetPhaseLag(0, -gyros.x * settings.Damping[BRUSHLESSGIMBALSETTINGS_DAMPING_ROLL]);
			PIOS_Brushless_SetPhaseLag(1, -gyros.y * settings.Damping[BRUSHLESSGIMBALSETTINGS_DAMPING_PITCH]);
		}

	}
}

/**
 * @}
 * @}
 */
