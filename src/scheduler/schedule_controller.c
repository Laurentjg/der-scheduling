#include "der_scheduler_internal.h"

#include <stdio.h>

static void
scheduleController_updateActSchdRef(ScheduleController self, Schedule schedule)
{
    DataObject* actSchdRef = (DataObject*)ModelNode_getChild((ModelNode*)self->controllerLn, "ActSchdRef");

    if (actSchdRef) {
        DataAttribute* actSchdRef_stVal = (DataAttribute*)ModelNode_getChild((ModelNode*)actSchdRef, "stVal");
        DataAttribute* actSchdRef_t = (DataAttribute*)ModelNode_getChild((ModelNode*)actSchdRef, "t");
        DataAttribute* actSchdRef_q = (DataAttribute*)ModelNode_getChild((ModelNode*)actSchdRef, "q");

        IedServer_lockDataModel(self->server);

        if (schedule) {
            if (actSchdRef_stVal) {

                char objRefBuf[130];

                char* objRef = ModelNode_getObjectReference((ModelNode*)schedule->scheduleLn, objRefBuf);

                IedServer_updateVisibleStringAttributeValue(self->server, actSchdRef_stVal, objRef);
            }

            if (actSchdRef_q)
                IedServer_updateQuality(self->server, actSchdRef_q, QUALITY_VALIDITY_GOOD);
        }
        else {
            if (actSchdRef_stVal)
                IedServer_updateVisibleStringAttributeValue(self->server, actSchdRef_stVal, "");

            if (actSchdRef_q)
                IedServer_updateQuality(self->server, actSchdRef_q, QUALITY_VALIDITY_INVALID);
        }

        if (actSchdRef_t) {
            Timestamp ts;
            Timestamp_clearFlags(&ts);
            Timestamp_setTimeInMilliseconds(&ts, Hal_getTimeInMs());

            IedServer_updateTimestampAttributeValue(self->server, actSchdRef_t, &ts);
        }

        IedServer_unlockDataModel(self->server);
    }
}

static Schedule
scheduleController_getActiveSchedule(ScheduleController self)
{
    Schedule activeSchedule = NULL;

    LinkedList schedElem = LinkedList_getNext(self->schedules);

    while (schedElem) {
        Schedule schedule = (Schedule)LinkedList_getData(schedElem);

        if (Schedule_isRunning(schedule)) {
            if (activeSchedule == NULL) {
                activeSchedule == schedule;
            }
            else {
                if (Schedule_getPrio(schedule) > Schedule_getPrio(activeSchedule)) {
                    activeSchedule = schedule;
                }
            }
        }

        schedElem = LinkedList_getNext(schedElem);
    }

    return activeSchedule;
}

/* functions called by Schedule */

/**
 * @brief Schedule informs the controller that its priority was updated
 * 
 * @param self 
 * @param sched 
 * @param newPrio 
 */
void
scheduleController_schedulePrioUpdated(ScheduleController self, Schedule sched, int newPrio)
{
    Schedule activeSchedule = scheduleController_getActiveSchedule(self);

    if (activeSchedule) {
        if (activeSchedule != self->activeSchedule) {
            
            // change active schedule
            self->activeSchedule = activeSchedule;

            printf("INFO: Active schedule changed due to priority update\n");
            scheduleController_updateActSchdRef(self, self->activeSchedule);
        }
    }
    else {
        // there is no running schedule
        scheduleController_updateActSchdRef(self, NULL);
    }
}

/**
 * @brief Schedule informs the controller that its state was updated
 * 
 * @param self 
 * @param sched 
 * @param newState 
 */
void
scheduleController_scheduleStateUpdated(ScheduleController self, Schedule sched, ScheduleState newState)
{

}

/**
 * @brief Schedule informs the controller that its scheduled value was updated
 * 
 * @param self 
 * @param sched 
 */
void
scheduleController_scheduleValueUpdated(ScheduleController self, Schedule sched)
{
    // check if the schedule is the actve schedule

    if (sched == self->activeSchedule) {

    }
    else {
        //ignore new value
    }
}

ScheduleController
ScheduleController_create(LogicalNode* fsccLn, IedServer server)
{
    ScheduleController self = (ScheduleController)calloc(1, sizeof(struct sScheduleController));

    if (self) {
        self->controllerLn = fsccLn;
        self->server = server;
    }

    return self;
}

