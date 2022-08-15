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
ScheduleController_create(LogicalNode* fsccLn, Scheduler scheduler)
{
    ScheduleController self = (ScheduleController)calloc(1, sizeof(struct sScheduleController));

    if (self) {
        self->controllerLn = fsccLn;
        self->server = scheduler->server;
        self->scheduler = scheduler;
        self->schedules = LinkedList_create();
    }

    return self;
}

void
ScheduleController_destroy(ScheduleController self)
{
    if (self) {

        LinkedList_destroyStatic(self->schedules);

        free(self);
    }
}

static MmsDataAccessError
schd_setSrcRef_writeAccessHandler(DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter)
{
    ScheduleController self = (ScheduleController)parameter;

    const char* scheduleRef = MmsValue_toString(value);
    printf("INFO:   -> schedule: %s\n", scheduleRef);

    Schedule sched = Scheduler_getScheduleByObjRef(self->scheduler, scheduleRef);

    if (sched) {
        printf("INFO:       -> schedule found\n");
    }
    else {
        printf("ERROR: schedule %s not found\n", scheduleRef);
        return DATA_ACCESS_ERROR_OBJECT_VALUE_INVALID;
    }

    if (LinkedList_contains(self->schedules, sched)) {
        printf("ERROR: schedule %s already conntected with schedule controller\n", scheduleRef);
        return DATA_ACCESS_ERROR_OBJECT_VALUE_INVALID;
    }

    if (dataAttribute->mmsValue) {
        const char* oldScheduleRef = MmsValue_toString(dataAttribute->mmsValue);

        Schedule oldSchedule = Scheduler_getScheduleByObjRef(self->scheduler, oldScheduleRef);

        if (oldSchedule) {
            printf("WARNING: disconnect schedule %s from schedule controller\n", oldScheduleRef);

            //TODO how to handle the situation when multiple Schd have the same reference?

            //TODO remove listener??? (or remove automatically when called from unknown schedule?)

            LinkedList_remove(self->schedules, oldSchedule);
        }
    }

    printf("INFO: connect schedule %s to schedule controller\n", scheduleRef);

    LinkedList_add(self->schedules, sched);

    Schedule_setListeningController(sched, self);
    
    return DATA_ACCESS_ERROR_SUCCESS;
}

void
ScheduleController_initialize(ScheduleController self)
{
    // create list of known schedules 

    LinkedList dataObjects = ModelNode_getChildren((ModelNode*)self->controllerLn);

    LinkedList doElem = LinkedList_getNext(dataObjects);

    while (doElem) {
        DataObject* dObj = (DataObject*)LinkedList_getData(doElem);

        if (scheduler_checkIfMultiObjInst(dObj->name, "Schd")) {
            DataAttribute* schd_setSrcRef = (DataAttribute*)ModelNode_getChild((ModelNode*)dObj, "setSrcRef");

            if (schd_setSrcRef) {
                if (schd_setSrcRef->type == IEC61850_VISIBLE_STRING_129) {
                    printf("INFO: %s.setSrcRef found\n", dObj->name);
                
                    if (schd_setSrcRef->mmsValue) {
                        const char* scheduleRef = MmsValue_toString(schd_setSrcRef->mmsValue);
                        printf("INFO:   -> schedule: %s\n", scheduleRef);

                        Schedule sched = Scheduler_getScheduleByObjRef(self->scheduler, scheduleRef);

                        printf("INFO:       -> schedule found\n");

                        LinkedList_add(self->schedules, sched);

                        Schedule_setListeningController(sched, self);
                    }

                    IedServer_handleWriteAccess(self->server, schd_setSrcRef, schd_setSrcRef_writeAccessHandler, self);
                }
                else {
                    printf("ERROR: %s.setSrcRef has wrong type\n", dObj->name);
                }
            }
            else {
                printf("ERROR: %s has no attribute setSrcRef\n", dObj->name);
            }
        }

        doElem = LinkedList_getNext(doElem);
    }

    LinkedList_destroyStatic(dataObjects);


}

