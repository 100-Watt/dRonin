/**
 ******************************************************************************
 * @addtogroup OpenPilotModules OpenPilot Modules
 * @{
 * @addtogroup PathPlanner Path Planner Module
 * @brief Executes a series of waypoints
 * @{
 *
 * @file       pathplanner.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2012.
 * @brief      Executes a series of waypoints
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
#include "paths.h"

#include "flightstatus.h"
#include "pathdesired.h"
#include "pathplannersettings.h"
#include "pathstatus.h"
#include "positionactual.h"
#include "waypoint.h"
#include "waypointactive.h"

// Private constants
#define STACK_SIZE_BYTES 1024
#define TASK_PRIORITY (tskIDLE_PRIORITY+1)
#define MAX_QUEUE_SIZE 2

// Private types

// Private variables
static xTaskHandle taskHandle;
static xQueueHandle queue;
static PathPlannerSettingsData pathPlannerSettings;
static WaypointActiveData waypointActive;
static WaypointData waypoint;
static bool path_status_updated;

// Private functions
static void advanceWaypoint();
static void checkTerminationCondition();
static void activateWaypoint();

static void pathPlannerTask(void *parameters);
static void settingsUpdated(UAVObjEvent * ev);
static void waypointsUpdated(UAVObjEvent * ev);
static void pathStatusUpdated(UAVObjEvent * ev);
static void createPathBox();
static void createPathLogo();

static int32_t active_waypoint = -1;
/**
 * Module initialization
 */
int32_t PathPlannerStart()
{
	taskHandle = NULL;

	// Start VM thread
	xTaskCreate(pathPlannerTask, (signed char *)"PathPlanner", STACK_SIZE_BYTES/4, NULL, TASK_PRIORITY, &taskHandle);
	TaskMonitorAdd(TASKINFO_RUNNING_PATHPLANNER, taskHandle);

	return 0;
}

/**
 * Module initialization
 */
int32_t PathPlannerInitialize()
{
	taskHandle = NULL;

	PathPlannerSettingsInitialize();
	WaypointInitialize();
	WaypointActiveInitialize();
	
	// Create object queue
	queue = xQueueCreate(MAX_QUEUE_SIZE, sizeof(UAVObjEvent));

	return 0;
}

MODULE_INITCALL(PathPlannerInitialize, PathPlannerStart)

/**
 * Module task
 */
static void pathPlannerTask(void *parameters)
{
	PathPlannerSettingsConnectCallback(settingsUpdated);
	settingsUpdated(PathPlannerSettingsHandle());

	WaypointConnectCallback(waypointsUpdated);
	WaypointActiveConnectCallback(waypointsUpdated);

	PathStatusConnectCallback(pathStatusUpdated);

	// If the PathStatus isn't available no follower is running and we should abort
	if (PathStatusHandle() == NULL || !TaskMonitorQueryRunning(TASKINFO_RUNNING_PATHFOLLOWER)) {
		AlarmsSet(SYSTEMALARMS_ALARM_PATHPLANNER, SYSTEMALARMS_ALARM_CRITICAL);
		while(1)
			vTaskDelay(100);
	}
	AlarmsClear(SYSTEMALARMS_ALARM_PATHPLANNER);

	FlightStatusData flightStatus;
	
	// Main thread loop
	bool pathplanner_active = false;
	path_status_updated = false;

	while (1)
	{

		vTaskDelay(20);

		// When not running the path planner short circuit and wait
		FlightStatusGet(&flightStatus);
		if (flightStatus.FlightMode != FLIGHTSTATUS_FLIGHTMODE_PATHPLANNER) {
			pathplanner_active = false;
			continue;
		}
		
		if(pathplanner_active == false) {
			// This triggers callback to update variable
			WaypointActiveGet(&waypointActive);
			waypointActive.Index = 0;
			WaypointActiveSet(&waypointActive);

			pathplanner_active = true;
			continue;
		}

		// This method determines if we have achieved the goal of the active
		// waypoint
		if (path_status_updated)
			checkTerminationCondition();

	}
}

/**
 * On changed waypoints or active waypoint update position desired
 * if we are in charge
 */
static void waypointsUpdated(UAVObjEvent * ev)
{
	FlightStatusData flightStatus;
	FlightStatusGet(&flightStatus);
	if (flightStatus.FlightMode != FLIGHTSTATUS_FLIGHTMODE_PATHPLANNER)
		return;
	
	WaypointActiveGet(&waypointActive);
	if(active_waypoint != waypointActive.Index)
		activateWaypoint(waypointActive.Index);
}

/**
 * When the PathStatus is updated indicate a new one is available to consume
 */
static void pathStatusUpdated(UAVObjEvent * ev)
{
	path_status_updated = true;
}

/**
 * This method checks the current position against the active waypoint
 * to determine if it has been reached
 */
static void checkTerminationCondition()
{
	PathStatusData pathStatus;
	PathStatusGet(&pathStatus);
	path_status_updated = false;

	if (pathStatus.Status == PATHSTATUS_STATUS_COMPLETED)
		advanceWaypoint();
}

/**
 * Increment the waypoint index which triggers the active waypoint method
 */
static void advanceWaypoint()
{
	WaypointActiveGet(&waypointActive);
	waypointActive.Index++;
	WaypointActiveSet(&waypointActive);

	// Invalidate any pending path status updates
	path_status_updated = false;
}

/**
 * This method is called when a new waypoint is activated
 */
static void activateWaypoint(int idx)
{	
	active_waypoint = idx;

	if (idx > 0) {
		WaypointData prevWaypoint;
		WaypointInstGet(idx - 1, &prevWaypoint);
	}

	WaypointInstGet(idx, &waypoint);

	PathDesiredData pathDesired;

	pathDesired.End[PATHDESIRED_END_NORTH] = waypoint.Position[WAYPOINT_POSITION_NORTH];
	pathDesired.End[PATHDESIRED_END_EAST] = waypoint.Position[WAYPOINT_POSITION_EAST];
	pathDesired.End[PATHDESIRED_END_DOWN] = waypoint.Position[WAYPOINT_POSITION_DOWN];
	pathDesired.ModeParameters = waypoint.ModeParameters;

	// Use this to ensure the cases match up (catastrophic if not) and to cover any cases
	// that don't make sense to come from the path planner
	switch(waypoint.Mode) {
		case WAYPOINT_MODE_FLYVECTOR:
			pathDesired.Mode = PATHDESIRED_MODE_FLYVECTOR;
			break;
		case WAYPOINT_MODE_FLYENDPOINT:
			pathDesired.Mode = PATHDESIRED_MODE_FLYENDPOINT;
			break;
		case WAYPOINT_MODE_FLYCIRCLELEFT:
			pathDesired.Mode = PATHDESIRED_MODE_FLYCIRCLELEFT;
			break;
		case WAYPOINT_MODE_FLYCIRCLERIGHT:
			pathDesired.Mode = PATHDESIRED_MODE_FLYCIRCLERIGHT;
			break;
		default:
			AlarmsSet(SYSTEMALARMS_ALARM_PATHPLANNER, SYSTEMALARMS_ALARM_ERROR);
			return;
	}

	pathDesired.EndingVelocity = waypoint.Velocity;

	if(waypointActive.Index == 0) {
		// For first waypoint, get current position as start point
		PositionActualData positionActual;
		PositionActualGet(&positionActual);
		
		pathDesired.Start[PATHDESIRED_START_NORTH] = positionActual.North;
		pathDesired.Start[PATHDESIRED_START_EAST] = positionActual.East;
		pathDesired.Start[PATHDESIRED_START_DOWN] = positionActual.Down - 1;
		pathDesired.StartingVelocity = waypoint.Velocity;
	} else {
		// Get previous waypoint as start point
		WaypointData waypointPrev;
		WaypointInstGet(waypointActive.Index - 1, &waypointPrev);
		
		pathDesired.Start[PATHDESIRED_END_NORTH] = waypointPrev.Position[WAYPOINT_POSITION_NORTH];
		pathDesired.Start[PATHDESIRED_END_EAST] = waypointPrev.Position[WAYPOINT_POSITION_EAST];
		pathDesired.Start[PATHDESIRED_END_DOWN] = waypointPrev.Position[WAYPOINT_POSITION_DOWN];
		pathDesired.StartingVelocity = waypointPrev.Velocity;
	}

	PathDesiredSet(&pathDesired);

	// Invalidate any pending path status updates
	path_status_updated = false;
}

void settingsUpdated(UAVObjEvent * ev) {
	uint8_t preprogrammedPath = pathPlannerSettings.PreprogrammedPath;
	PathPlannerSettingsGet(&pathPlannerSettings);
	if (pathPlannerSettings.PreprogrammedPath != preprogrammedPath) {
		switch(pathPlannerSettings.PreprogrammedPath) {
			case PATHPLANNERSETTINGS_PREPROGRAMMEDPATH_NONE:
				break;
			case PATHPLANNERSETTINGS_PREPROGRAMMEDPATH_10M_BOX:
				createPathBox();
				break;
			case PATHPLANNERSETTINGS_PREPROGRAMMEDPATH_LOGO:
				createPathLogo();
				break;
				
		}
	}
}

static void createPathBox()
{
	WaypointCreateInstance();
	WaypointCreateInstance();
	WaypointCreateInstance();
	WaypointCreateInstance();
	WaypointCreateInstance();
	
	// Draw O
	WaypointData waypoint;
	waypoint.Velocity = 5; // Since for now this isn't directional just set a mag
	waypoint.Mode = WAYPOINT_MODE_FLYVECTOR;

	waypoint.Position[0] = 0;
	waypoint.Position[1] = 0;
	waypoint.Position[2] = -10;
	WaypointInstSet(0, &waypoint);

	waypoint.Position[0] = 25;
	waypoint.Position[1] = 25;
	waypoint.Position[2] = -10;
	WaypointInstSet(1, &waypoint);

	waypoint.Position[0] = -25;
	waypoint.Position[1] = 25;
	waypoint.Mode = WAYPOINT_MODE_FLYCIRCLERIGHT;
	waypoint.ModeParameters = 35;
	WaypointInstSet(2, &waypoint);

	waypoint.Position[0] = -25;
	waypoint.Position[1] = -25;
	WaypointInstSet(3, &waypoint);

	waypoint.Position[0] = 25;
	waypoint.Position[1] = -25;
	WaypointInstSet(4, &waypoint);

	waypoint.Position[0] = 25;
	waypoint.Position[1] = 25;
	WaypointInstSet(5, &waypoint);

	waypoint.Position[0] = 0;
	waypoint.Position[1] = 0;
	waypoint.Mode = WAYPOINT_MODE_FLYVECTOR;
	WaypointInstSet(6, &waypoint);
}

static void createPathLogo()
{
	float scale = 1;
	
	// Draw O
	WaypointData waypoint;
	waypoint.Velocity = 5; // Since for now this isn't directional just set a mag
	for(uint32_t i = 0; i < 20; i++) {
		waypoint.Position[1] = scale * 30 * cos(i / 19.0 * 2 * M_PI);
		waypoint.Position[0] = scale * 50 * sin(i / 19.0 * 2 * M_PI);
		waypoint.Position[2] = -50;
		waypoint.Mode = WAYPOINT_MODE_FLYVECTOR;
		WaypointCreateInstance();
	}
	
	// Draw P
	for(uint32_t i = 20; i < 35; i++) {
		waypoint.Position[1] = scale * (55 + 20 * cos(i / 10.0 * M_PI - M_PI / 2));
		waypoint.Position[0] = scale * (25 + 25 * sin(i / 10.0 * M_PI - M_PI / 2));
		waypoint.Position[2] = -50;
		waypoint.Mode = WAYPOINT_MODE_FLYVECTOR;
		WaypointCreateInstance();
	}
		
	waypoint.Position[1] = scale * 35;
	waypoint.Position[0] = scale * -50;
	waypoint.Position[2] = -50;
	waypoint.Mode = WAYPOINT_MODE_FLYVECTOR;
	WaypointCreateInstance();
	WaypointInstSet(35, &waypoint);
	
	// Draw Box
	waypoint.Position[1] = scale * 35;
	waypoint.Position[0] = scale * -60;
	waypoint.Position[2] = -30;
	waypoint.Mode = WAYPOINT_MODE_FLYVECTOR;
	WaypointCreateInstance();
	WaypointInstSet(36, &waypoint);

	waypoint.Position[1] = scale * 85;
	waypoint.Position[0] = scale * -60;
	waypoint.Position[2] = -30;
	waypoint.Mode = WAYPOINT_MODE_FLYVECTOR;
	WaypointCreateInstance();
	WaypointInstSet(37, &waypoint);

	waypoint.Position[1] = scale * 85;
	waypoint.Position[0] = scale * 60;
	waypoint.Position[2] = -30;
	waypoint.Mode = WAYPOINT_MODE_FLYVECTOR;
	WaypointCreateInstance();
	WaypointInstSet(38, &waypoint);
	
	waypoint.Position[1] = scale * -40;
	waypoint.Position[0] = scale * 60;
	waypoint.Position[2] = -30;
	waypoint.Mode = WAYPOINT_MODE_FLYVECTOR;
	WaypointCreateInstance();
	WaypointInstSet(39, &waypoint);

	waypoint.Position[1] = scale * -40;
	waypoint.Position[0] = scale * -60;
	waypoint.Position[2] = -30;
	waypoint.Mode = WAYPOINT_MODE_FLYVECTOR;
	WaypointCreateInstance();
	WaypointInstSet(40, &waypoint);

	waypoint.Position[1] = scale * 35;
	waypoint.Position[0] = scale * -60;
	waypoint.Position[2] = -30;
	waypoint.Mode = WAYPOINT_MODE_FLYVECTOR;
	WaypointCreateInstance();
	WaypointInstSet(41, &waypoint);

}

/**
 * @}
 * @}
 */
