#include "der_scheduler_internal.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

bool 
scheduler_checkIfMultiObjInst(const char* name, const char* multiName)
{
    bool isInstance = false;

    int multiNameSize = strlen(multiName);
    int nameSize = strlen(name);

    if (nameSize >= multiNameSize) {

        if (memcmp(name, multiName, multiNameSize) == 0) {
            bool isNumber = true;

            int i;

            for (i = multiNameSize; i < nameSize; i++) {
                if (isdigit(name[i]) == false) {
                    isNumber = false;
                    break;
                }
            }

            if (isNumber) {
                isInstance = true;
            }
        }
    }

    return isInstance;
}

Scheduler
Scheduler_create(IedModel* model, IedServer server)
{  
    Scheduler self = (Scheduler)calloc(1, sizeof(struct sScheduler));

    if (self) {
        self->model = model;
        self->server = server;
        self->scheduleController = LinkedList_create();
        self->schedules = LinkedList_create();
    }

    return self;
}

void
Scheduler_destroy(Scheduler self)
{
    if (self)
    {
        LinkedList_destroyDeep(self->scheduleController, (LinkedListValueDeleteFunction)ScheduleController_destroy);

        LinkedList_destroyDeep(self->schedules, (LinkedListValueDeleteFunction)Schedule_destroy);

        free(self);
    }
}

void
Scheduler_enableScheduleControl(Scheduler self, const char* scheduleRef, bool enable)
{
    Schedule schedule = Scheduler_getScheduleByObjRef(self, scheduleRef);

    if (schedule) {
        Schedule_enableScheduleControl(schedule, enable);
    }
    else {
        printf("WARN: Schedule %s not found\n", scheduleRef);
    }
}

bool
Scheduler_enableSchedule(Scheduler self, const char* scheduleRef, bool enable)
{
    Schedule schedule = Scheduler_getScheduleByObjRef(self, scheduleRef);

    if (schedule) {
        return Schedule_enableSchedule(schedule, enable);
    }
    else {
        printf("WARN: Schedule %s not found\n", scheduleRef);

        return false;
    }
}

void
Scheduler_enableWriteAccessToParameter(Scheduler self, const char* scheduleRef, Scheduler_ScheduleParameter parameter, bool enable)
{
    Schedule schedule = Scheduler_getScheduleByObjRef(self, scheduleRef);

    if (schedule) {
        switch (parameter)
        {
        case SCHED_PARAM_SCHD_PRIO:
            Schedule_enableWriteAccessToSchdPrio(schedule, enable);
            break;

        case SCHED_PARAM_STR_TM:
            Schedule_enableWriteAccessToStrTm(schedule, enable);
            break;

        case SCHED_PARAM_SCHD_REUSE:
            Schedule_enableWriteAccessToSchdReuse(schedule, enable);
            break;
        
        default:
            printf("WARN: Unknown parameter %i\n", parameter);
            break;
        }
    }
    else {
        printf("WARN: Schedule %s not found\n", scheduleRef);
    }
}

void
Scheduler_setTargetValueHandler(Scheduler self, Scheduler_TargetValueChanged handler, void* parameter)
{
    self->targetValueHandler = handler;
    self->targetValueHandlerParameter = parameter;
}

void
scheduler_targetValueChanged(Scheduler self, DataAttribute* targetAttr, MmsValue* value, Quality quality, uint64_t timestampMs)
{
    if (self->targetValueHandler) {
        char targetValueObjRef[130];

        ModelNode_getObjectReferenceEx((ModelNode*)targetAttr, targetValueObjRef, true);

        self->targetValueHandler(self->targetValueHandlerParameter, targetValueObjRef, value, quality, timestampMs);
    }
}

static void
scheduler_initializeScheduleControllers(Scheduler self)
{
    LinkedList schedCtrlElem = LinkedList_getNext(self->scheduleController);

    while (schedCtrlElem) {

        ScheduleController controller = (ScheduleController)LinkedList_getData(schedCtrlElem);

        ScheduleController_initialize(controller);

        schedCtrlElem = LinkedList_getNext(schedCtrlElem);
    }
}

void
Scheduler_parseModel(Scheduler self)
{
    if (self->model) {
        /* search data model for FSCC instances */

        int i = 0;

        for (i = 0; i < IedModel_getLogicalDeviceCount(self->model); i++) {
            LogicalDevice* ld = IedModel_getDeviceByIndex(self->model, i);

            if (ld) {
                printf("INFO: Found LD: %s\n", ld->name);

                LogicalNode* ln = (LogicalNode*)ld->firstChild;

                while (ln) {
                    /* check if LN name contains "FSCC" */

                    if (strstr(ln->name, "FSCC")) {

                        /* check for other indications DO "ActSchdRef", DO "CtlEnt", DO "ValXX", DO "SchdXX" */
                        bool isScheduleController = true;

                        ModelNode* actSchdRef = ModelNode_getChild((ModelNode*)ln, "ActSchdRef");

                        if (actSchdRef == NULL) {
                            printf("ERROR: ActSchdRef not found in LN %s -> skip LN\n", ln->name);
                            isScheduleController = false;
                        }

                        ModelNode* ctlEnt = ModelNode_getChild((ModelNode*)ln, "CtlEnt");

                        if (ctlEnt == NULL) {
                            printf("ERROR: CtlEnt not found in LN %s -> skip LN\n", ln->name);
                            isScheduleController = false;
                        }

                        if (isScheduleController) {
                            printf("INFO: Found schedule controller: %s/%s\n", ld->name, ln->name);

                            ScheduleController controller = ScheduleController_create(ln, self);

                            if (controller)
                                LinkedList_add(self->scheduleController, controller);
                        }
                    }

                    if (strstr(ln->name, "FSCH")) {

                        Schedule sched = Schedule_create(ln, self->server, self->model);

                        if (sched) {
                            LinkedList_add(self->schedules, sched);
                        }
                        else {
                            printf("ERROR: Invalid schedule: %s/%s -> ignored\n", ld->name, ln->name);
                        }
                    }

                    ln = (LogicalNode*)ln->sibling;
                }

            }
        }

        scheduler_initializeScheduleControllers(self);
    }
}

Schedule
Scheduler_getScheduleByObjRef(Scheduler self, const char* objRef)
{
    Schedule matchingSchedule = NULL;

    if (objRef && objRef[0] != 0) {

        bool withoutIedName = false;

        if (objRef[0] == '@') {
            withoutIedName = true;
            objRef = objRef + 1;
        }

        LinkedList scheduleElem = LinkedList_getNext(self->schedules);

        while (scheduleElem) {
            Schedule schedule = (Schedule)LinkedList_getData(scheduleElem);

            char scheduleObjRefBuf[130];

            ModelNode_getObjectReferenceEx((ModelNode*)schedule->scheduleLn, scheduleObjRefBuf, withoutIedName);

            if (!strcmp(objRef, scheduleObjRefBuf)) {
                matchingSchedule = schedule;
                break;
            }

            scheduleElem = LinkedList_getNext(scheduleElem);
        }
    }

    return matchingSchedule;
}