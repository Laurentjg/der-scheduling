#include "der_scheduler.h"

#include <libiec61850/hal_thread.h>

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

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

typedef enum {
    SCHD_TYPE_UNKNOWN = 0,
    SCHD_TYPE_INS = 1,
    SCHD_TYPE_SPS = 2,
    SCHD_TYPE_ENS = 3,
    SCHD_TYPE_MV = 4
} ScheduleTargetType;

struct sSchedule {
    LogicalNode* scheduleLn;
    ScheduleTargetType targetType;

    DataObject* enaReq;
    DataObject* dsaReq;
    DataObject* schdSt;
    DataObject* val;

    DataObject* evTrg;

    uint64_t nextStartTime;

    uint64_t startTime; /* start time of current schedule execution */
    int entryDurationInMs; /* duration of schedule entry in ms */
    int numberOfScheduleEntries; /* number of valid schedule entries */
    int currentEntryIdx;

    IedServer server;
    IedModel* model;

    Thread thread;
    bool alive;
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

static bool checkIfMultiObjInst(const char* name, const char* multiName)
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

static bool checkIfStrTm(const char* name)
{
    return checkIfMultiObjInst(name, "StrTm");
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
schedule_setState(Schedule self, ScheduleState newState)
{
    DataAttribute* schdSt_stVal = (DataAttribute*)ModelNode_getChild((ModelNode*)self->schdSt, "stVal");
    DataAttribute* schdSt_q = (DataAttribute*)ModelNode_getChild((ModelNode*)self->schdSt, "q");
    DataAttribute* schdSt_t = (DataAttribute*)ModelNode_getChild((ModelNode*)self->schdSt, "t");

   // IedServer_lockDataModel(self->server);

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

   // IedServer_unlockDataModel(self->server);
}

static void
schedule_udpateState(Schedule self, ScheduleState newState)
{
    ScheduleState currentState = schedule_getState(self);

    if (currentState != newState) {
        schedule_setState(self, newState);
    }
}

static void updateIntStatusValue(IedServer server, DataObject* dobj, int32_t value, uint64_t timestamp)
{
    DataAttribute* stVal = (DataAttribute*)ModelNode_getChild((ModelNode*)dobj, "stVal");

    if (stVal) {
        if (stVal->mmsValue) {
            if (MmsValue_getType(stVal->mmsValue) == MMS_INTEGER) {

                DataAttribute* t = (DataAttribute*)ModelNode_getChild((ModelNode*)dobj, "t");

                IedServer_lockDataModel(server);

                if (t) {
                    IedServer_updateUTCTimeAttributeValue(server, t, timestamp);
                }

                IedServer_updateInt32AttributeValue(server, stVal, value);

                IedServer_unlockDataModel(server);
            }
        }
    }
}

static void
schedule_updateScheduleEnableError(Schedule self, ScheduleEnablingError err)
{
    DataObject* schdEnaErr = (DataObject*)ModelNode_getChild((ModelNode*)self->scheduleLn, "SchdEnaErr");

    if (schdEnaErr) {
        updateIntStatusValue(self->server, schdEnaErr, (int32_t)err, Hal_getTimeInMs());
    }
}

static void
schedule_updateNxtStrTm(Schedule self, uint64_t nextStartTime)
{
    DataObject* nxtStrTm = (DataObject*)ModelNode_getChild((ModelNode*)self->scheduleLn, "NxtStrTm");

    if (nxtStrTm) {

        DataAttribute* stVal = (DataAttribute*)ModelNode_getChild((ModelNode*)nxtStrTm, "stVal");
        DataAttribute* q = (DataAttribute*)ModelNode_getChild((ModelNode*)nxtStrTm, "q");
        DataAttribute* t = (DataAttribute*)ModelNode_getChild((ModelNode*)nxtStrTm, "t");

        if (stVal && q && t) {
            Timestamp ts;
            Timestamp_clearFlags(&ts);
            Timestamp_setTimeInMilliseconds(&ts, nextStartTime);

            IedServer_lockDataModel(self->server);

            IedServer_updateTimestampAttributeValue(self->server, stVal, &ts);

            if (nextStartTime != 0)
                IedServer_updateQuality(self->server, q, (Quality)QUALITY_VALIDITY_GOOD);
            else
                IedServer_updateQuality(self->server, q, (Quality)QUALITY_VALIDITY_INVALID);

            Timestamp_setTimeInMilliseconds(&ts, Hal_getTimeInMs());

            IedServer_updateTimestampAttributeValue(self->server, t, &ts);

            IedServer_unlockDataModel(self->server);
        }
    }
}

static uint64_t
schedule_getNextStartTime(Schedule self)
{
    uint64_t nextStartTime = 0;

    uint64_t currentTime = Hal_getTimeInMs();

    LinkedList dataObjects = ModelNode_getChildren((ModelNode*)self->scheduleLn);

    LinkedList doElem = LinkedList_getNext(dataObjects);

    while (doElem) {
        DataObject* dObj = (DataObject*)LinkedList_getData(doElem);

        // check that data object name is "StrTmXXX"
        if (checkIfStrTm(dObj->name)) {

            DataAttribute* setTm = (DataAttribute*)ModelNode_getChild((ModelNode*)dObj, "setTm");

            if (setTm->mmsValue) {
                uint64_t strTmVal = MmsValue_getUtcTimeInMs(setTm->mmsValue);

                if (strTmVal > currentTime) {

                    if (nextStartTime == 0) {
                        nextStartTime = strTmVal;
                    }
                    else {
                        if (strTmVal <= nextStartTime) {
                            nextStartTime = strTmVal;
                        }
                    }
                }
            }
        }

        doElem = LinkedList_getNext(doElem);
    }

    LinkedList_destroyStatic(dataObjects);

    return nextStartTime;
}

static int
schedule_getNumberOfScheduleEntries(Schedule self)
{
    int scheduleEntryCount = 0;

    char* multiObjStr = NULL;

    if (self->targetType == SCHD_TYPE_MV) {
        multiObjStr = "ValASG";
    }
    else if (self->targetType == SCHD_TYPE_ENS) {
        multiObjStr = "ValING";
    }
    else if (self->targetType == SCHD_TYPE_INS) {
        multiObjStr = "ValING";
    }
    else if (self->targetType == SCHD_TYPE_SPS) {
        multiObjStr = "ValSPG";
    }
    else {
        return 0;
    }

    LinkedList dataObjects = ModelNode_getChildren((ModelNode*)self->scheduleLn);

    LinkedList doElem = LinkedList_getNext(dataObjects);

    while (doElem) {
        DataObject* dObj = (DataObject*)LinkedList_getData(doElem);

        if (checkIfMultiObjInst(dObj->name, multiObjStr)) {
            printf("Found schedule instance %s\n", dObj->name);
            scheduleEntryCount++;
        }

        doElem = LinkedList_getNext(doElem);
    }

    LinkedList_destroyStatic(dataObjects);

    printf("INFO: Schedule has %i elements\n", scheduleEntryCount);

    return scheduleEntryCount;
}

static DataAttribute*
schedule_getCurrentValueAttribute(Schedule self)
{
    DataAttribute* valueAttr = NULL;

    char* objNameStr = NULL;

    if (self->targetType == SCHD_TYPE_MV) {
        objNameStr = "ValMV";
    }
    else if (self->targetType == SCHD_TYPE_ENS) {
        objNameStr = "ValENS";
    }
    else if (self->targetType == SCHD_TYPE_INS) {
        objNameStr = "ValINS";
    }
    else if (self->targetType == SCHD_TYPE_SPS) {
        objNameStr = "ValSPS";
    }
    else {
        return NULL;
    }

    valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, objNameStr);

    if (valueAttr) {
        
        if (self->targetType == SCHD_TYPE_MV) {
            valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)valueAttr, "mag.f");

            if (valueAttr == NULL)
                valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)valueAttr, "mag.i");
        }
        else {
            valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)valueAttr, "stVal");
        }
     
        return valueAttr;
    }

    return NULL;
}

static DataAttribute*
schedule_getCurrentValueSubAttribute(Schedule self, const char* attrName)
{
    DataAttribute* valueAttr = NULL;

    char* objNameStr = NULL;

    if (self->targetType == SCHD_TYPE_MV) {
        objNameStr = "ValMV";
    }
    else if (self->targetType == SCHD_TYPE_ENS) {
        objNameStr = "ValENS";
    }
    else if (self->targetType == SCHD_TYPE_INS) {
        objNameStr = "ValINS";
    }
    else if (self->targetType == SCHD_TYPE_SPS) {
        objNameStr = "ValSPS";
    }
    else {
        return NULL;
    }

    valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, objNameStr);

    if (valueAttr) {
        
        valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)valueAttr, attrName);
     
        return valueAttr;
    }

    return NULL;
}

/**
 * @brief Get the schedule attribute with index idx (e.g. idx=3 => "ValASG1" or "ValASG01", ...)
 * 
 * @param self 
 * @param idx 
 * @return DataAttribute* 
 */
static DataAttribute*
schedule_getScheduleValueAttribute(Schedule self, int idx)
{
    DataAttribute* valueAttr = NULL;

    char attrNameBuf[100];

    char* multiObjStr = NULL;

    if (self->targetType == SCHD_TYPE_MV) {
        multiObjStr = "ValASG";
    }
    else if (self->targetType == SCHD_TYPE_ENS) {
        multiObjStr = "ValING";
    }
    else if (self->targetType == SCHD_TYPE_INS) {
        multiObjStr = "ValING";
    }
    else if (self->targetType == SCHD_TYPE_SPS) {
        multiObjStr = "ValSPG";
    }
    else {
        return NULL;
    }

    sprintf(attrNameBuf, "%s%i", multiObjStr, idx);
    valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, attrNameBuf);

    if (valueAttr == NULL) {
        sprintf(attrNameBuf, "%s%02i", multiObjStr, idx);
        valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, attrNameBuf);
    }

    if (valueAttr == NULL) {
        sprintf(attrNameBuf, "%s%03i", multiObjStr, idx);
        valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, attrNameBuf);
    }

    if (valueAttr == NULL) {
        sprintf(attrNameBuf, "%s%04i", multiObjStr, idx);
        valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, attrNameBuf);
    };

    if (valueAttr) {
        
        if (self->targetType == SCHD_TYPE_MV) {
            valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)valueAttr, "setMag.f");

            if (valueAttr == NULL)
                valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)valueAttr, "setMag.i");
        }
        else {
            valueAttr = (DataAttribute*)ModelNode_getChild((ModelNode*)valueAttr, "setVal");
        }
     
        return valueAttr;
    }

    return NULL;
}

static MmsDataAccessError
strTm_writeAccessHandler(DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter)
{
    Schedule self = (Schedule)parameter;

    uint64_t newStrTm = MmsValue_getUtcTimeInMs(value);

    char objRefBuf[130];

    ModelNode_getObjectReference((ModelNode*) dataAttribute, objRefBuf);

    // check if the time is valid (is in the future)
    if (newStrTm > Hal_getTimeInMs()) {
        //TODO check if the schedule is in the correct state?

        if (schedule_getState(self) == SCHD_STATE_READY) {
            //self->nextStartTime = schedule_getNextStartTime(self);

            //schedule_updateNxtStrTm(self, newStrTm);
        }

        printf("INFO: Write access to %s -> value accepted\n", objRefBuf);

        return DATA_ACCESS_ERROR_SUCCESS;
    }
    else {
        printf("WARN: Write access to %s -> invalid value\n", objRefBuf);

        return DATA_ACCESS_ERROR_OBJECT_VALUE_INVALID;
    }
}

static MmsDataAccessError
schdPrio_writeAccessHandler(DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter)
{
    Schedule self = (Schedule)parameter;

    /* TODO send PRIO_UPDATED event to schedule controller(s) */
    printf("SchPrio.setVal: %i\n", MmsValue_toInt32(value));

    return DATA_ACCESS_ERROR_SUCCESS;
}

static bool
isEventDriven(Schedule self)
{
    bool eventDriven = false;

    if (self->evTrg) {
        DataAttribute* setVal = (DataAttribute*)ModelNode_getChild((ModelNode*)self->evTrg, "setVal");

        if (setVal) {
            if (setVal->mmsValue) {
                eventDriven = MmsValue_getBoolean(setVal->mmsValue);
            }
        }
    }

    return eventDriven;
}

static bool 
checkSyncInput(Schedule self)
{
    bool checkResult = false;

    DataAttribute* inSyn_setSrcRef = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "InSyn.setSrcRef");

    if (inSyn_setSrcRef) {
        const char* srcRef = MmsValue_toString(inSyn_setSrcRef->mmsValue);
        
        printf("INFO: InSyn set to (%s)\n", srcRef);

        //TODO check if object reference is valid and a possible trigger signal

        ModelNode* triggerSignal = IedModel_getModelNodeByShortObjectReference(self->model, srcRef);

        if (triggerSignal) {
            //TODO check if the trigger signal is a boolean or SPS (has "stVal" of type boolean)

            if (triggerSignal->modelType == DataAttributeModelType) {
                DataAttribute* triggerDa = (DataAttribute*)triggerSignal;

                if (triggerDa->type == IEC61850_BOOLEAN) {
                    printf("INFO: Trigger signal is valid\n");
                    checkResult = true;
                }
            }
        }
        else {
            printf("ERROR: Trigger signal %s not found\n", srcRef);
        }
    }

    return checkResult;
}

static bool isTimeTriggered(Schedule self)
{
    // check if schedule has a StrTm object
    bool hasStrTm = false;

    LinkedList dataObjects = ModelNode_getChildren((ModelNode*)self->scheduleLn);

    LinkedList doElem = LinkedList_getNext(dataObjects);

    while (doElem) {
        DataObject* dObj = (DataObject*)LinkedList_getData(doElem);

        // check that data object name is "StrTmXXX"
        if (checkIfStrTm(dObj->name)) {
            hasStrTm = true;
            break;
        }

        doElem = LinkedList_getNext(doElem);
    }

    LinkedList_destroyStatic(dataObjects);

    return hasStrTm;
}

static bool isPeriodic(Schedule self)
{

}

static void
schedule_installWriteAccessHandlersForStrTm(Schedule self)
{
    LinkedList dataObjects = ModelNode_getChildren((ModelNode*)self->scheduleLn);

    LinkedList doElem = LinkedList_getNext(dataObjects);

    while (doElem) {
        DataObject* dObj = (DataObject*)LinkedList_getData(doElem);

        // check that data object name is "StrTmXXX"
        if (checkIfStrTm(dObj->name)) {

            DataAttribute* setTm = (DataAttribute*)ModelNode_getChild((ModelNode*)dObj, "setTm");

            if (setTm) {
                IedServer_handleWriteAccess(self->server, setTm, strTm_writeAccessHandler, self);            
            }
        }

        doElem = LinkedList_getNext(doElem);
    }

    LinkedList_destroyStatic(dataObjects);
}

static uint64_t 
getStartTime(DataObject* strTm)
{
    uint64_t strTmVal = 0;

    DataAttribute* setTm = (DataAttribute*)ModelNode_getChild((ModelNode*)strTm, "setTm");

    if (setTm) {
        if (setTm->mmsValue) {
            strTmVal = MmsValue_getUtcTimeInMs(setTm->mmsValue);
        }
    }

    return strTmVal;
}

static bool checkForValidStartTimes(Schedule self)
{
    bool hasValidStartTimes = false;
    //TODO check data objects for StrTm objects

    uint64_t currentTime = Hal_getTimeInMs();

    LinkedList dataObjects = ModelNode_getChildren((ModelNode*)self->scheduleLn);

    LinkedList doElem = LinkedList_getNext(dataObjects);

    while (doElem) {
        DataObject* dObj = (DataObject*)LinkedList_getData(doElem);

        // check that data object name is "StrTmXXX"
        if (checkIfStrTm(dObj->name)) {
            printf("INFO: Found start time: %s\n", dObj->name);

            if (getStartTime(dObj) > currentTime) {
                hasValidStartTimes = true;
            }
            else {
                printf("     start time is in the past!\n");
            }
        }

        doElem = LinkedList_getNext(doElem);
    }

    LinkedList_destroyStatic(dataObjects);

    return hasValidStartTimes;
}

static uint64_t
getSchdIntvValueInMs(Schedule self)
{
    uint64_t interval = 0;

    int multiplierEnumValue = 0;

    int baseTime = 1; /* 1 second */

    double multiPl = 1.0;

    DataAttribute* unit = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "SchdIntv.units.SIUnit");

    if (unit) {
        if ((unit->mmsValue) && (MmsValue_getType(unit->mmsValue) == MMS_INTEGER)) {
            int unitEnumValue = MmsValue_toInt32(unit->mmsValue);

            if (unitEnumValue == 4) { /* second */
                baseTime = 1;
            }
            else if (unitEnumValue == 84) { /* hour */
                baseTime = 3600;
            }
            else if (unitEnumValue == 85) { /* minute */
                baseTime = 60;
            }
            else {
                printf("ERROR: invalid unit %i (has to be a time unit)\n", unitEnumValue);
            }
        }
    }

    DataAttribute* multiplier = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "SchdIntv.units.multiplier");

    if (multiplier) {
        if ((multiplier->mmsValue) && (MmsValue_getType(multiplier->mmsValue) == MMS_INTEGER)) {
            multiplierEnumValue = MmsValue_toInt32(multiplier->mmsValue);

            multiPl = pow(10, multiplierEnumValue);
        }
    }

    DataAttribute* schdIntv = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "SchdIntv.setVal");

    if (schdIntv) {
        if ((schdIntv->mmsValue) && (MmsValue_getType(schdIntv->mmsValue) == MMS_INTEGER)) {
            int schedIntvValue = MmsValue_toInt32(schdIntv->mmsValue);

            double value = (schedIntvValue * baseTime * 1000) * multiPl;

            interval = (uint64_t)value;
        }
    }

    return interval;
}

static bool
performGenericScheduleValidityChecks(Schedule self)
{
    /* check if NumEntr is valid */

    DataAttribute* numEntr = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "NumEntr.setVal");

    bool numEntrValid = false;

    if (numEntr) {
        if ((numEntr->mmsValue) && (MmsValue_getType(numEntr->mmsValue) == MMS_INTEGER)) {

            int numEntrVal = MmsValue_toInt32(numEntr->mmsValue);

            if (numEntrVal > 0) {

                if (numEntrVal <= schedule_getNumberOfScheduleEntries(self)) {

                    numEntrValid = true;

                    int i;

                    for (i = 1; i <= numEntrVal; i++) {
                        if (schedule_getScheduleValueAttribute(self, i) == NULL) {
                            numEntrValid = false;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    if (numEntrValid == false) {
        schedule_updateScheduleEnableError(self, SCHD_ENA_ERR_MISSING_VALID_NUMENTR);

        return false;
    }

    printf("INFO: NumEntr is valid\n");

    /* check if SchdIntv is valied */

    DataAttribute* schdIntv = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "SchdIntv.setVal");

    bool schdIntvValid = false;

    if (schdIntv) {
        if ((schdIntv->mmsValue) && (MmsValue_getType(schdIntv->mmsValue) == MMS_INTEGER)) {
            int schdIntvValue = MmsValue_toInt32(schdIntv->mmsValue);

            if (schdIntvValue > 0) {
                schdIntvValid = true;
            }
        }
    }

    if (schdIntvValid == false) {
        schedule_updateScheduleEnableError(self, SCHD_ENA_ERR_MISSING_VALID_SCHDINTV);

        return false;
    }

    printf("INFO: SchdIntv interval: %lu ms\n", getSchdIntvValueInMs(self));

    printf("INFO: SchdIntv is valid\n");

    return true;
 }

static bool
enabledSchedule(Schedule self)
{
    ScheduleState newState = SCHD_STATE_NOT_READY;
    /* TODO check for conditions to enable schedule */

    //TODO check if a Start time (StrTm) is defined or an external trigger option is set (EvTeg == true}

    if (performGenericScheduleValidityChecks(self)) {

        if (isEventDriven(self)) {
            if (checkSyncInput(self)) {
                printf("INFO: valid trigger info set\n");
                newState = SCHD_STATE_READY;
            }
        }
        else if(isTimeTriggered(self)) {

            if (checkForValidStartTimes(self)) {
                printf("INFO: valid schedules found\n");
                newState = SCHD_STATE_READY;
            }
            else {
                schedule_updateScheduleEnableError(self, SCHD_ENA_ERR_MISSING_VALID_STRTM);
            }
        }
    }

    schedule_udpateState(self, newState);

    if (newState == SCHD_STATE_READY) {
        uint64_t nextStartTime = schedule_getNextStartTime(self);

        schedule_updateNxtStrTm(self, nextStartTime);

        schedule_updateScheduleEnableError(self, SCHD_ENA_ERR_NONE);

        return true;
    }
    else
        return false;
}

static void
disableSchedule(Schedule self)
{
    ScheduleState newState = SCHD_STATE_NOT_READY;

    schedule_udpateState(self, newState);
}

static ControlHandlerResult 
schedule_controlHandler(ControlAction action, void* parameter, MmsValue* ctlVal, bool test)
{
    Schedule self = (Schedule)parameter; 

    DataObject* ctrlObj = ControlAction_getControlObject(action);

    if (ctrlObj == self->enaReq) {
        if ((test == false) && (MmsValue_getBoolean(ctlVal) == true)) {

            if (enabledSchedule(self)) {
                printf("Enabled schedule\n");
            }
            else {
                //TODO figure out how a negative answer can be sent?
                printf("Cannot enable schedule\n");
            }


        }
    }
    else if (ctrlObj == self->dsaReq) {
        if ((test == false) && (MmsValue_getBoolean(ctlVal) == true)) {

            disableSchedule(self);

            printf("Disabled schedule\n");
        }
    }
}

static int
schedule_getCurrentIdx(Schedule self, uint64_t currentTime)
{
    int currentIdx = (currentTime - self->startTime) / self->entryDurationInMs;

    if (currentIdx > self->numberOfScheduleEntries) 
        currentIdx = -1;

    return currentIdx;
}

static void
schedule_updateCurrentValue(Schedule self, uint64_t currentTime, MmsValue* value)
{
    DataAttribute* currentValAttr = schedule_getCurrentValueAttribute(self);

    if (currentValAttr) {
        DataAttribute* q = schedule_getCurrentValueSubAttribute(self, "q");
        DataAttribute* t = schedule_getCurrentValueSubAttribute(self, "t");

        IedServer_lockDataModel(self->server);

        IedServer_updateAttributeValue(self->server, currentValAttr, value);

        if (t) {
            //TODO change to IedServer_updateTimestampAttributeValue 
            IedServer_updateUTCTimeAttributeValue(self->server, t, currentTime);
        }

        if (q) {
            IedServer_updateQuality(self->server, q, QUALITY_VALIDITY_GOOD);
        }

        IedServer_unlockDataModel(self->server);
    }
}

static void
schedule_updateSchdEntr(Schedule self, uint64_t currentTime, int idx)
{
    DataAttribute* schdEntr_stVal = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "SchdEntr.stVal");

    if (schdEntr_stVal) {
        DataAttribute* schdEntr_t = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "SchdEntr.t");

        IedServer_lockDataModel(self->server);

        if (schdEntr_t) {
            //TODO change to IedServer_updateTimestampAttributeValue 
            IedServer_updateUTCTimeAttributeValue(self->server, schdEntr_t, currentTime);
        }

        IedServer_updateInt32AttributeValue(self->server, schdEntr_stVal, idx);

        IedServer_unlockDataModel(self->server);
    }
}

static void*
schedule_thread(void* parameter)
{
    Schedule self = (Schedule)parameter;

    while (self->alive) {

        ScheduleState state = schedule_getState(self);

        uint64_t currentTime = Hal_getTimeInMs();

        ScheduleState newState = state;

        if (state == SCHD_STATE_READY) {

            printf("INFO: state: ready\n");
            printf("INFO:     current time: %lu\n", currentTime);
            printf("INFO:     next start  : %lu\n", self->nextStartTime);

            if (self->nextStartTime == 0) {
                self->nextStartTime = schedule_getNextStartTime(self);
            }
            
            if ((self->nextStartTime != 0) && (currentTime > self->nextStartTime)) {

                self->startTime = self->nextStartTime;
                self->entryDurationInMs = getSchdIntvValueInMs(self);
                self->numberOfScheduleEntries = schedule_getNumberOfScheduleEntries(self);
                self->currentEntryIdx = -2;

                /* calculate current index */
                int currentIdx = schedule_getCurrentIdx(self, currentTime);
                

                //TODO update ActStrTm
                //TODO update NextStrTm

                self->nextStartTime = schedule_getNextStartTime(self);

                if (self->nextStartTime == 0) {
                    //TODO change to other state?
                }

                schedule_updateNxtStrTm(self, self->nextStartTime);

                newState = SCHD_STATE_RUNNING;
                printf("INFO: Schedule switchted to running state\n");
            }
        }
        else if (state == SCHD_STATE_RUNNING) {

            int currentIdx = schedule_getCurrentIdx(self, currentTime);

            printf("INFO: state: ready (idx: %i/%i)\n", currentIdx, self->currentEntryIdx);

            if (currentIdx != self->currentEntryIdx) {
                DataAttribute* valueAttr = schedule_getScheduleValueAttribute(self, currentIdx + 1);

                if (valueAttr) {
                    char objRef[130];

                    ModelNode_getObjectReferenceEx((ModelNode*)valueAttr, objRef, true);

                    printf("Found schdule entry: %s\n", objRef);

                    MmsValue* val = valueAttr->mmsValue;

                    printf("  val: %p\n", val);

                    if (val) {
                        char valBuf[256];

                        MmsValue_printToBuffer(val, valBuf, 256);

                        printf("INFO: schedule value [%i]: %s\n", currentIdx, valBuf);

                        // update ValMV, ValINS, ValSPS, ValENS
                        schedule_updateCurrentValue(self, currentTime, val);
            
                        schedule_updateSchdEntr(self, currentTime, currentIdx + 1);
                    }
                }
                else {
                    printf("ERROR: no schedule entry found for idx: %i\n", currentIdx + 1);
                }

                self->currentEntryIdx = currentIdx;
            }
            else {

                if (currentIdx == -1) {
                    printf("INFO: schedule ended\n");

                    //TODO check for next state

                    newState = SCHD_STATE_NOT_READY;
                }

            }
        }

        if (newState != state) {
            schedule_setState(self, newState);
        }        

        Thread_sleep(100);
    }
}

Schedule
Schedule_create(LogicalNode* schedLn, IedServer server, IedModel* model)
{
    Schedule self = NULL;

    /* check for other indications DO "ActSchdRef", DO "CtlEnt", DO "ValXX", DO "SchdXX" */
    bool isSchedule = true;

    ScheduleTargetType targetType = SCHD_TYPE_UNKNOWN;

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

    ModelNode* scheduleValue = ModelNode_getChild((ModelNode*)schedLn, "ValMV");

    if (scheduleValue) {
        targetType = SCHD_TYPE_MV;
    }

    scheduleValue = ModelNode_getChild((ModelNode*)schedLn, "ValINS");

    if (scheduleValue) {
        targetType = SCHD_TYPE_INS;
    }

    scheduleValue = ModelNode_getChild((ModelNode*)schedLn, "ValSPS");

    if (scheduleValue) {
        targetType = SCHD_TYPE_SPS;
    }

    scheduleValue = ModelNode_getChild((ModelNode*)schedLn, "ValENS");

    if (scheduleValue) {
        targetType = SCHD_TYPE_ENS;
    }

    if (targetType == SCHD_TYPE_UNKNOWN) {
        printf("ERROR: Found schedule %s/%s but with unknown target type!\n", schedLn->parent->name, schedLn->name);
    }

    if (isSchedule) {
        printf("INFO: Found schedule: %s/%s\n", schedLn->parent->name, schedLn->name);
    }

    if (isSchedule) {

        self = (Schedule)calloc(1, sizeof(struct sSchedule));

        if (self) {
            self->scheduleLn = schedLn;
            self->server = server;
            self->model = model;
            self->enaReq = (DataObject*)enaReq;
            self->dsaReq = (DataObject*)dsaReq;
            self->schdSt = (DataObject*)schdSt;
            self->val = (DataObject*)scheduleValue;

            self->evTrg = (DataObject*)ModelNode_getChild((ModelNode*)schedLn, "EvTrg");

            if (self->evTrg) {
                DataAttribute* inSyn_setSrcRef = (DataAttribute*)ModelNode_getChild((ModelNode*)schedLn, "InSyn.setSrcRef");

                if (inSyn_setSrcRef == NULL) {
                    printf("ERROR: Found EvTrg but InSyn.setSrcRef not present\n");
                }
            }

            self->targetType = targetType;

            IedServer_handleWriteAccess(self->server, schdPrio_setVal, schdPrio_writeAccessHandler, self);

            schedule_installWriteAccessHandlersForStrTm(self);

            IedServer_setControlHandler(self->server, self->dsaReq, schedule_controlHandler, self);
            IedServer_setControlHandler(self->server, self->enaReq, schedule_controlHandler, self);

            schedule_setState(self, SCHD_STATE_NOT_READY);

            schedule_updateNxtStrTm(self, 0);

            self->thread = Thread_create(schedule_thread, self, false);

            self->alive = true;

            Thread_start(self->thread);
        }
    }

    return self;
}

void
Schedule_destroy(Schedule self)
{
    if (self) {
        self->alive = false;
        Thread_destroy(self->thread);

        free(self);
    }
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

                        Schedule sched = Schedule_create(ln, self->server, self->model);

                        if (sched)
                            LinkedList_add(self->schedules, sched);
                    }


                    ln = (LogicalNode*)ln->sibling;
                }

            }
        }
    }
}