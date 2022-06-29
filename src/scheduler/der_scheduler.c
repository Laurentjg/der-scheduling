#include "der_scheduler.h"

#include <string.h>
#include <stdio.h>

typedef enum {
    SCHD_STATE_INVALID = 0,
    SCHD_STATE_NOT_READY = 1,
    SCHD_STATE_START_TIME_REQUIRED = 2,
    SCHD_STATE_READY = 3,
    SCHD_STATE_RUNNING = 4
} ScheduleState;

typedef enum {
    SCHD_ENA_ERR_NONE = 1,
    SCHD_ENA_ERR_MISSING_VALID_NUMENTR = 2,
    SCHD_ENA_ERR_MISSING_VALID_SCHDINTV = 3,
    SCHD_ENA_ERR_MISSING_VALID_SCHEDULE_VALUES = 4,
    SCHD_ENA_ERR_UNCONSISTENT_VALUES_CDC = 5,
    SCHD_ENA_ERR_MISSING_VALID_STRTM = 6,
    SCHD_ENA_ERR_OTHER = 99
} ScheduleEnablingError;

struct sSchedule {
    LogicalNode* scheduleLn;
    DataObject* enaReq;
    DataObject* dsaReq;
    DataObject* schdSt;
    IedServer server;
};

struct sScheduleController {
    Schedule activeSchedule;
    LinkedList schedules;

    LogicalNode* controllerLn;
    IedServer server;
};

struct sScheduler
{
    IedModel* model;
    IedServer server;
    LinkedList scheduleController;
    LinkedList schedules;
};


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

static ScheduleState
schedule_getState(Schedule self)
{
    ScheduleState state = SCHD_STATE_INVALID;

    if (self->schdSt) {
        DataAttribute* schdSt_stVal = (DataAttribute*)ModelNode_getChild((ModelNode*)self->schdSt, "stVal");

        if (schdSt_stVal != NULL) {
            int stateValue = IedServer_getInt32AttributeValue(self->server, schdSt_stVal);

            if ((stateValue >= SCHD_STATE_NOT_READY) && (stateValue <=  SCHD_STATE_RUNNING)) {
                state = (ScheduleState)stateValue;
            }
            else {
                printf("ERROR: Schedule has invalid state value: %i\n", stateValue);
            }
        }
    }

    return state;
}

static void
schedule_udpateState(Schedule self, ScheduleState newState)
{
    ScheduleState currentState = schedule_getState(self);

    if (currentState != newState) {
        DataAttribute* schdSt_stVal = (DataAttribute*)ModelNode_getChild((ModelNode*)self->schdSt, "stVal");
        DataAttribute* schdSt_q = (DataAttribute*)ModelNode_getChild((ModelNode*)self->schdSt, "q");
        DataAttribute* schdSt_t = (DataAttribute*)ModelNode_getChild((ModelNode*)self->schdSt, "t");

        IedServer_lockDataModel(self->server);

        if (schdSt_t) {
            Timestamp ts;
            Timestamp_clearFlags(&ts);
            Timestamp_setSubsecondPrecision(&ts, 10);
            Timestamp_setTimeInMilliseconds(&ts, Hal_getTimeInMs());
            IedServer_updateTimestampAttributeValue(self->server, schdSt_t, &ts);
        }

        if (schdSt_q) {
            Quality q = 0;
            Quality_setValidity(&q, QUALITY_VALIDITY_GOOD);
            IedServer_updateQuality(self->server, schdSt_q, q);            
        }

        if (schdSt_stVal) {
            IedServer_updateInt32AttributeValue(self->server, schdSt_stVal, newState);
        }

        IedServer_unlockDataModel(self->server);
    }
}

static MmsDataAccessError
schdPrio_writeAccessHandler(DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter)
{
    /* TODO send PRIO_UPDATED event to schedule controller(s) */
    printf("SchPrio.setVal: %i\n", MmsValue_toInt32(value));
}

static ControlHandlerResult 
schedule_controlHandler(ControlAction action, void* parameter, MmsValue* ctlVal, bool test)
{
    Schedule self = (Schedule)parameter; 

    DataObject* ctrlObj = ControlAction_getControlObject(action);

    if (ctrlObj == self->enaReq) {
        if ((test == false) && (MmsValue_getBoolean(ctlVal) == true)) {
            /* TODO check for conditions to enable schedule */

            printf("Enabled schedule\n");
        }
    }
    else if (ctrlObj == self->dsaReq) {
        if ((test == false) && (MmsValue_getBoolean(ctlVal) == true)) {
            printf("Disabled schedule\n");
        }
    }
}

Schedule
Schedule_create(LogicalNode* schedLn, IedServer server)
{
    Schedule self = NULL;

    /* check for other indications DO "ActSchdRef", DO "CtlEnt", DO "ValXX", DO "SchdXX" */
    bool isSchedule = true;

    DataAttribute* schdPrio_setVal = NULL;


    ModelNode* schdSt = ModelNode_getChild((ModelNode*)schedLn, "SchdSt");

    if (schdSt == NULL) {
        printf("SchdSt not found in LN %s -> skip LN\n", schedLn->name);
        isSchedule = false;
    }

    ModelNode* nxtStrTm = ModelNode_getChild((ModelNode*)schedLn, "NxtStrTm");

    if (nxtStrTm == NULL) {
        printf("NxtStrTm not found in LN %s -> skip LN\n", schedLn->name);
        isSchedule = false;
    }

    ModelNode* schdPrio = ModelNode_getChild((ModelNode*)schedLn, "SchdPrio");

    if (schdPrio == NULL) {
        printf("SchdPrio not found in LN %s -> skip LN\n", schedLn->name);
        isSchedule = false;
    }
    else {
        schdPrio_setVal = (DataAttribute*)ModelNode_getChild(schdPrio, "setVal");

        if (schdPrio_setVal == NULL) {
            printf("SchdPrio.setVal not found in LN %s -> skip LN\n", schedLn->name);
            isSchedule = false;
        }
    }

    ModelNode* enaReq = ModelNode_getChild((ModelNode*)schedLn, "EnaReq");

    if (enaReq == NULL) {
        printf("EnaReq not found in LN %s -> skip LN\n", schedLn->name);
        isSchedule = false;
    }

    ModelNode* dsaReq = ModelNode_getChild((ModelNode*)schedLn, "DsaReq");

    if (enaReq == NULL) {
        printf("DsaReq not found in LN %s -> skip LN\n", schedLn->name);
        isSchedule = false;
    }

    if (isSchedule) {
        printf("Found schedule: %s/%s\n", schedLn->parent->name, schedLn->name);
    }

    if (isSchedule) {

        self = (Schedule)calloc(1, sizeof(struct sSchedule));

        if (self) {
            self->scheduleLn = schedLn;
            self->server = server;
            self->enaReq = (DataObject*)enaReq;
            self->dsaReq = (DataObject*)dsaReq;
            self->schdSt = (DataObject*)schdSt;

            IedServer_handleWriteAccess(self->server, schdPrio_setVal, schdPrio_writeAccessHandler, self);

            IedServer_setControlHandler(self->server, self->dsaReq, schedule_controlHandler, self);
            IedServer_setControlHandler(self->server, self->enaReq, schedule_controlHandler, self);

            schedule_udpateState(self, SCHD_STATE_NOT_READY);
        }
    }

    return self;
}

void
Schedule_updateParameters(Schedule self)
{
    /* update internal parameters by the values of the data model */

    
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
    if (self) {
        //TODO release resources
        free(self);
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
                printf("Found LD: %s\n", ld->name);

                LogicalNode* ln = (LogicalNode*)ld->firstChild;

                while (ln) {
                    /* check if LN name contains "FSCC" */

                    if (strstr(ln->name, "FSCC")) {
                        /* check for other indications DO "ActSchdRef", DO "CtlEnt", DO "ValXX", DO "SchdXX" */
                        bool isScheduleController = true;

                        ModelNode* actSchdRef = ModelNode_getChild((ModelNode*)ln, "ActSchdRef");

                        if (actSchdRef == NULL) {
                            printf("ActSchdRef not found in LN %s -> skip LN\n", ln->name);
                            isScheduleController = false;
                        }

                        ModelNode* ctlEnt = ModelNode_getChild((ModelNode*)ln, "CtlEnt");

                        if (ctlEnt == NULL) {
                            printf("CtlEnt not found in LN %s -> skip LN\n", ln->name);
                            isScheduleController = false;
                        }

                        if (isScheduleController) {
                            printf("Found schedule controller: %s/%s\n", ld->name, ln->name);

                            ScheduleController controller = ScheduleController_create(ln, self->server);

                            if (controller)
                                LinkedList_add(self->scheduleController, controller);
                        }
                    }

                    if (strstr(ln->name, "FSCH")) {

                        Schedule sched = Schedule_create(ln, self->server);

                        if (sched)
                            LinkedList_add(self->schedules, sched);
                    }


                    ln = (LogicalNode*)ln->sibling;
                }

            }
        }
    }
}