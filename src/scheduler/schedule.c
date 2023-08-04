#include <stdio.h>
#include <math.h>

#include "der_scheduler_internal.h"

void
Schedule_setListeningController(Schedule self, ScheduleController controller)
{
    if (LinkedList_contains(self->knownScheduleControllers, controller) == false) {
        LinkedList_add(self->knownScheduleControllers, controller);
    }
}

static bool checkIfStrTm(const char* name)
{
    return scheduler_checkIfMultiObjInst(name, "StrTm");
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
checkIfTimeTriggeredAndPeriodic(Schedule self)
{
    self->isTimeTriggerd = false;
    self->isPeriodic = false;

    /* check if schedule has a StrTm object */

    LinkedList dataObjects = ModelNode_getChildren((ModelNode*)self->scheduleLn);

    LinkedList doElem = LinkedList_getNext(dataObjects);

    while (doElem) {
        DataObject* dObj = (DataObject*)LinkedList_getData(doElem);

        /* check that data object name is "StrTmXXX" */
        if (checkIfStrTm(dObj->name)) {
            self->isTimeTriggerd = true;

            /* check if "StrTm" has a "setVal" element */
            ModelNode* setCal = ModelNode_getChild((ModelNode*)dObj, "setCal");

            if (setCal) {
                self->isPeriodic = true;
                break;
            }
        }

        doElem = LinkedList_getNext(doElem);
    }

    LinkedList_destroyStatic(dataObjects);
}

static void
schedule_setState(Schedule self, ScheduleState newState)
{
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

    if (self->storage) {
        SchedulerStorage_saveSchedule(self->storage, self);
    }

    /* send PRIO_UPDATED event to schedule controller(s) */

    LinkedList controllerElem = LinkedList_getNext(self->knownScheduleControllers);

    while (controllerElem) {
        ScheduleController controller = (ScheduleController)LinkedList_getData(controllerElem);

        scheduleController_scheduleStateUpdated(controller, self, newState);

        controllerElem = LinkedList_getNext(controllerElem);
    }
}

static void
schedule_udpateState(Schedule self, ScheduleState newState)
{
    ScheduleState currentState = schedule_getState(self);

    if (currentState != newState) {
        schedule_setState(self, newState);
    }
}

static void
updateIntStatusValue(IedServer server, DataObject* dobj, int32_t value, uint64_t timestamp)
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
updateTimeStatus(Schedule self, uint64_t startTime, const char* objName)
{
    DataObject* timeObj = (DataObject*)ModelNode_getChild((ModelNode*)self->scheduleLn, objName);

    if (timeObj) {

        DataAttribute* stVal = (DataAttribute*)ModelNode_getChild((ModelNode*)timeObj, "stVal");
        DataAttribute* q = (DataAttribute*)ModelNode_getChild((ModelNode*)timeObj, "q");
        DataAttribute* t = (DataAttribute*)ModelNode_getChild((ModelNode*)timeObj, "t");

        if (stVal && q && t) {
            Timestamp ts;
            Timestamp_clearFlags(&ts);
            Timestamp_setTimeInMilliseconds(&ts, startTime);

            IedServer_lockDataModel(self->server);

            IedServer_updateTimestampAttributeValue(self->server, stVal, &ts);

            if (startTime != 0)
                IedServer_updateQuality(self->server, q, (Quality)QUALITY_VALIDITY_GOOD);
            else
                IedServer_updateQuality(self->server, q, (Quality)QUALITY_VALIDITY_INVALID);

            Timestamp_setTimeInMilliseconds(&ts, Hal_getTimeInMs());

            IedServer_updateTimestampAttributeValue(self->server, t, &ts);

            IedServer_unlockDataModel(self->server);
        }
    }
}

static void
schedule_updateActStrTm(Schedule self, uint64_t actStartTime)
{
    updateTimeStatus(self, actStartTime, "ActStrTm");
}

static void
schedule_updateNxtStrTm(Schedule self, uint64_t nextStartTime)
{
    updateTimeStatus(self, nextStartTime, "NxtStrTm");
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

LinkedList
Schedule_getStartTimes(Schedule self)
{
    LogicalNode* schedLn = self->scheduleLn;

    DataObject* dobj = (DataObject*)(schedLn->firstChild);

    while (dobj) {

    }
}

/**
 * StrXX value has to be set to "00" when active
 */
static void
eraseStartTime(Schedule self, uint64_t startTime)
{
    LinkedList dataObjects = ModelNode_getChildren((ModelNode*)self->scheduleLn);

    LinkedList doElem = LinkedList_getNext(dataObjects);

    while (doElem) {
        DataObject* dObj = (DataObject*)LinkedList_getData(doElem);

        // check that data object name is "StrTmXXX"
        if (checkIfStrTm(dObj->name)) {

            DataAttribute* setTm = (DataAttribute*)ModelNode_getChild((ModelNode*)dObj, "setTm");

            if (setTm->mmsValue) {
                uint64_t strTmVal = MmsValue_getUtcTimeInMs(setTm->mmsValue);

                if (strTmVal == startTime) {
                    IedServer_updateUTCTimeAttributeValue(self->server, setTm, 0);
                }
            }
        }

        doElem = LinkedList_getNext(doElem);
    }

    LinkedList_destroyStatic(dataObjects);
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
        multiObjStr = "ValENG";
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

        if (scheduler_checkIfMultiObjInst(dObj->name, multiObjStr)) {
           // printf("Found schedule instance %s\n", dObj->name);
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
        multiObjStr = "ValENG";
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

    if (self->allowWriteToStrTm)
    {
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

            IedServer_updateAttributeValue(self->server, dataAttribute, value);

            if (self->storage) {
                SchedulerStorage_saveSchedule(self->storage, self);
            }

            return DATA_ACCESS_ERROR_SUCCESS_NO_UPDATE;
        }
        else {
            printf("WARN: Write access to %s -> invalid value (is: %lu current: %lu\n", objRefBuf, newStrTm, Hal_getTimeInMs());

            return DATA_ACCESS_ERROR_OBJECT_VALUE_INVALID;
        }
    }
    else {
        return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
    }
}

static MmsDataAccessError
schdPrio_writeAccessHandler(DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter)
{
    Schedule self = (Schedule)parameter;

    if (self->allowWriteToSchdPrio) {
        int prio = MmsValue_toInt32(value);

        IedServer_updateAttributeValue(self->server, dataAttribute, value);

        /* send PRIO_UPDATED event to schedule controller(s) */

        LinkedList controllerElem = LinkedList_getNext(self->knownScheduleControllers);

        while (controllerElem) {
            ScheduleController controller = (ScheduleController)LinkedList_getData(controllerElem);

            scheduleController_schedulePrioUpdated(controller, self, prio);

            controllerElem = LinkedList_getNext(controllerElem);
        }

        if (self->storage) {
            SchedulerStorage_saveSchedule(self->storage, self);
        }

        return DATA_ACCESS_ERROR_SUCCESS_NO_UPDATE;
    }
    else {
        return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
    }
}

static MmsDataAccessError
schedule_genericWriteAccessHandler(DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter)
{
    Schedule self = (Schedule)parameter;

    IedServer_updateAttributeValue(self->server, dataAttribute, value);

    if (self->storage) {
        SchedulerStorage_saveSchedule(self->storage, self);
    }

    return DATA_ACCESS_ERROR_SUCCESS_NO_UPDATE;
}

static MmsDataAccessError
schdReuse_writeAccessHandler(DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter)
{
    Schedule self = (Schedule)parameter;

    if (self->allowWriteToSchdReuse) {

        IedServer_updateAttributeValue(self->server, dataAttribute, value);

        if (self->storage) {
            SchedulerStorage_saveSchedule(self->storage, self);
        }

        return DATA_ACCESS_ERROR_SUCCESS_NO_UPDATE;
    }
    else {
        return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
    }
}

static MmsDataAccessError
schdIntv_writeAccessHandler(DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter)
{
    Schedule self = (Schedule)parameter;

    printf("write access to schdIntv\n");

    if (self->allowWriteToSchdIntv) {

        IedServer_updateAttributeValue(self->server, dataAttribute, value);

        if (self->storage) {
            SchedulerStorage_saveSchedule(self->storage, self);
        }

        return DATA_ACCESS_ERROR_SUCCESS_NO_UPDATE;
    }
    else {
        return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
    }
}

static MmsDataAccessError
schdValue_writeAccessHandler(DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter)
{
    Schedule self = (Schedule)parameter;

    IedServer_updateAttributeValue(self->server, dataAttribute, value);

    if (self->storage) {
        SchedulerStorage_saveSchedule(self->storage, self);
    }

    return DATA_ACCESS_ERROR_SUCCESS_NO_UPDATE;
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

static bool
isTimeTriggered(Schedule self)
{
    return self->isTimeTriggerd;
}

static bool
isPeriodic(Schedule self)
{
    return self->isPeriodic;
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

static void
schedule_installWriteAccessHandlersForValues(Schedule self)
{
    int idx = 1;

    while (true) {
        DataAttribute* da = schedule_getScheduleValueAttribute(self, idx);

        if (da) {
            char objRef[130];

            ModelNode_getObjectReference((ModelNode*)da, objRef);

            IedServer_handleWriteAccessForComplexAttribute(self->server, da, schdValue_writeAccessHandler, self);

            idx++;
        }
        else {
            printf("schedue write attribute not found for index %i\n", idx);

            break;
        }
    }
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

static bool
hasSetTm(DataObject* dobj)
{
    ModelNode* setTm = ModelNode_getChild((ModelNode*)dobj, "setTm");

    if (setTm) {
        //TODO check type?
        return true;
    }
    else {
        return false;
    }
}

static bool
hasSetCal(DataObject* dobj)
{
    ModelNode* setCal = ModelNode_getChild((ModelNode*)dobj, "setCal");

    if (setCal) {
        //TODO check type?
        return true;
    }
    else {
        return false;
    }
}

static void
setSchdIntvValueInMs(Schedule self, uint64_t interval)
{
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

            double value = (double)interval / ((double)baseTime * 1000.0 * multiPl);

            MmsValue_setInt32(schdIntv->mmsValue, (int)value);
        }
    }
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

static int
schedule_getNumEntrValue(Schedule self)
{
    int numEntrVal = -1;

    DataAttribute* numEntr = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "NumEntr.setVal");

    if (numEntr) {
        if ((numEntr->mmsValue) && (MmsValue_getType(numEntr->mmsValue) == MMS_INTEGER)) {
            numEntrVal = MmsValue_toInt32(numEntr->mmsValue);
        }
    }
    
    return numEntrVal;
}

static bool
checkForValidStartTimes(Schedule self)
{
    bool hasValidStartTimes = false;

    uint64_t currentTime = Hal_getTimeInMs();

    LinkedList dataObjects = ModelNode_getChildren((ModelNode*)self->scheduleLn);

    LinkedList doElem = LinkedList_getNext(dataObjects);

    uint64_t scheduleDurationMs = 0;
    
    int numEntryVal = schedule_getNumEntrValue(self);

    if (numEntryVal > 0) {
        scheduleDurationMs = getSchdIntvValueInMs(self) * numEntryVal;
    }
    
    while (doElem) {
        DataObject* dObj = (DataObject*)LinkedList_getData(doElem);

        // check that data object name is "StrTmXXX"
        if (checkIfStrTm(dObj->name)) {
            printf("INFO: Found start time: %s\n", dObj->name);

            if (isPeriodic(self)) {
                if (hasSetCal(dObj)) {
                    if (hasSetTm(dObj)) {
                        if (getStartTime(dObj) < currentTime) {
                            hasValidStartTimes = true;
                        }
                    }
                    else {
                        hasValidStartTimes = true;
                    }
                }
                else {
                    printf("DEBUG: start time of periodic schedule is missing setCal -> ignore\n");
                }
            }
            else {
                if (getStartTime(dObj) + scheduleDurationMs > currentTime) {
                    hasValidStartTimes = true;
                }
                else {
                    printf("     start time is in the past and consumed!\n");
                }
            }
        }

        doElem = LinkedList_getNext(doElem);
    }

    LinkedList_destroyStatic(dataObjects);

    return hasValidStartTimes;
}

static bool
performGenericScheduleValidityChecks(Schedule self)
{
    /* check if NumEntr is valid */
    int numEntrVal = schedule_getNumEntrValue(self);

    bool numEntrValid = false;

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
    
    self->numberOfScheduleEntries = numEntrVal;

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

static CheckHandlerResult
schedule_performCheckHandler(ControlAction action, void* parameter, MmsValue* ctlVal, bool test, bool interlockCheck)
{
    CheckHandlerResult result = CONTROL_OBJECT_UNDEFINED;

    Schedule self = (Schedule)parameter; 

    DataObject* ctrlObj = ControlAction_getControlObject(action);

    char scheduleRef[130];
    ModelNode_getObjectReference((ModelNode*)self->scheduleLn, scheduleRef);

    if (ctrlObj == self->enaReq) {
        if ((test == false) && (MmsValue_getBoolean(ctlVal) == true)) {
            if (self->allowRemoteControl) {
                //TODO perform check if schedule is valid
                result = CONTROL_ACCEPTED;
            }
            else {
                result = CONTROL_OBJECT_ACCESS_DENIED;
            }
        }
        else {
            result = CONTROL_ACCEPTED;
        }
    }
    else if (ctrlObj == self->dsaReq) {
        if ((test == false) && (MmsValue_getBoolean(ctlVal) == true)) {
            if (self->allowRemoteControl) {
                result = CONTROL_ACCEPTED;
            }
            else {
                result = CONTROL_OBJECT_ACCESS_DENIED;
            }
        }
        else {
            result = CONTROL_ACCEPTED;
        }
    }

    printf("Controlhandler (%s) -> %i\n", scheduleRef, result);

    return result;
}

static ControlHandlerResult 
schedule_controlHandler(ControlAction action, void* parameter, MmsValue* ctlVal, bool test)
{
    Schedule self = (Schedule)parameter; 

    DataObject* ctrlObj = ControlAction_getControlObject(action);

    char scheduleRef[130];
    ModelNode_getObjectReference((ModelNode*)self->scheduleLn, scheduleRef);

    if (ctrlObj == self->enaReq) {
        if ((test == false) && (MmsValue_getBoolean(ctlVal) == true)) {

            if (enabledSchedule(self)) {
                printf("INFO: Enabled schedule %s\n", scheduleRef);
            }
            else {
                //TODO figure out how a negative answer can be sent?
                printf("WARN: Cannot enable schedule %s\n", scheduleRef);
            }
        }
    }
    else if (ctrlObj == self->dsaReq) {
        if ((test == false) && (MmsValue_getBoolean(ctlVal) == true)) {

            disableSchedule(self);

            printf("INFO: Disabled schedule %s\n", scheduleRef);
        }
    }

    return CONTROL_RESULT_OK;
}

static int
schedule_getCurrentIdx(Schedule self, uint64_t currentTime)
{
    int currentIdx = (currentTime - self->startTime) / self->entryDurationInMs;

    if (currentIdx >= self->numberOfScheduleEntries) 
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

static void
notifyControllers(Schedule self, MmsValue* val, uint64_t currentTime)
{
    /* send new value to schedule controller(s) */

    LinkedList controllerElem = LinkedList_getNext(self->knownScheduleControllers);

    while (controllerElem) {
        ScheduleController controller = (ScheduleController)LinkedList_getData(controllerElem);

        scheduleController_scheduleValueUpdated(controller, self, val, currentTime);

        controllerElem = LinkedList_getNext(controllerElem);
    }
}

MmsValue*
Schedule_getCurrentValue(Schedule self)
{
    MmsValue* currentValue = NULL;

    int currentIdx = schedule_getCurrentIdx(self, Hal_getTimeInMs());

    if (currentIdx != -1) {

        DataAttribute* valueAttr = schedule_getScheduleValueAttribute(self, currentIdx + 1);

        if (valueAttr) {
            currentValue = valueAttr->mmsValue;
        }
    }

    return currentValue;
}

MmsValue*
Schedule_getValueWithIdx(Schedule self, int idx)
{
    MmsValue* value = NULL;

    DataAttribute* valueAttr = schedule_getScheduleValueAttribute(self, idx + 1);

    if (valueAttr) {
        value = valueAttr->mmsValue;
    }

    return value;
}

static void*
schedule_thread(void* parameter)
{
    Schedule self = (Schedule)parameter;

    char scheduleRef[130];
    ModelNode_getObjectReference((ModelNode*)self->scheduleLn, scheduleRef);

    schedule_updateActStrTm(self, 0);

    while (self->alive) {

        ScheduleState state = schedule_getState(self);

        uint64_t currentTime = Hal_getTimeInMs();

        ScheduleState newState = state;

        if (state == SCHD_STATE_READY) {

            if (self->nextStartTime == 0) {
                self->nextStartTime = schedule_getNextStartTime(self);
            }
            
            if ((self->nextStartTime != 0) && (currentTime > self->nextStartTime)) {

                self->startTime = self->nextStartTime;
                self->entryDurationInMs = getSchdIntvValueInMs(self);
                self->numberOfScheduleEntries = schedule_getNumEntrValue(self);
                self->currentEntryIdx = -2;

                /* calculate current index */
                int currentIdx = schedule_getCurrentIdx(self, currentTime);
                
                /* update ActStrTm */
                schedule_updateActStrTm(self, self->startTime);

                eraseStartTime(self, self->startTime);

                self->nextStartTime = schedule_getNextStartTime(self);

                schedule_updateNxtStrTm(self, self->nextStartTime);

                newState = SCHD_STATE_RUNNING;
                printf("INFO: Schedule %s switchted to running state\n", scheduleRef);

            }
        }
        else if (state == SCHD_STATE_RUNNING) {

            int currentIdx = schedule_getCurrentIdx(self, currentTime);

            if ((currentIdx != -1) && (currentIdx != self->currentEntryIdx)) {
                DataAttribute* valueAttr = schedule_getScheduleValueAttribute(self, currentIdx + 1);

                if (valueAttr) {
                    char objRef[130];

                    ModelNode_getObjectReferenceEx((ModelNode*)valueAttr, objRef, true);

                    MmsValue* val = valueAttr->mmsValue;

                    if (val) {
                        char valBuf[256];

                        MmsValue_printToBuffer(val, valBuf, 256);

                        printf("INFO: schedule %s - value %s [%i]: %s\n", scheduleRef, objRef, currentIdx, valBuf);

                        // update ValMV, ValINS, ValSPS, ValENS
                        schedule_updateCurrentValue(self, currentTime, val);
            
                        schedule_updateSchdEntr(self, currentTime, currentIdx + 1);

                        notifyControllers(self, val, currentTime);
                    }
                }

                self->currentEntryIdx = currentIdx;
            }
            else {

                if (currentIdx == -1) {
                    printf("INFO: schedule %s ended\n", scheduleRef);

                    //TODO check for next state
                    self->nextStartTime = schedule_getNextStartTime(self);
                   
                    if (self->nextStartTime) {
                        // update ActStrTime = 0(invalid)

                        schedule_updateNxtStrTm(self, self->nextStartTime);

                        newState = SCHD_STATE_READY;
                    }
                    else {
                        schedule_updateNxtStrTm(self, 0);

                        newState = SCHD_STATE_NOT_READY;
                    }

                    schedule_updateActStrTm(self, 0);           
                }

            }
        }

        if (newState != state) {
            printf("INFO: schedule %s switch from state %i to state %i\n", scheduleRef, state, newState);
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

    DataAttribute* schdPrio_setVal = (DataAttribute*)ModelNode_getChild((ModelNode*)schedLn, "SchdPrio.setVal");

    if (schdPrio_setVal == NULL) {
        printf("SchdPrio.setVal not found in LN %s -> skip LN\n", schedLn->name);
        isSchedule = false;
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

    ModelNode* schdResue_setVal = ModelNode_getChild((ModelNode*)schedLn, "SchdReuse.setVal");

    if (targetType == SCHD_TYPE_UNKNOWN) {
        printf("ERROR: Found schedule %s/%s but with unknown target type!\n", schedLn->parent->name, schedLn->name);
    }

    if (isSchedule) {
        printf("INFO: Found schedule: %s/%s\n", schedLn->parent->name, schedLn->name);
    }

    if (isSchedule) {

        LinkedList knownScheduleControllers = LinkedList_create();

        self = (Schedule)calloc(1, sizeof(struct sSchedule));

        if (self && knownScheduleControllers) {
            self->storage = NULL;
            self->scheduleLn = schedLn;
            self->server = server;
            self->model = model;
            self->enaReq = (DataObject*)enaReq;
            self->dsaReq = (DataObject*)dsaReq;
            self->schdSt = (DataObject*)schdSt;
            self->val = (DataObject*)scheduleValue;
            self->knownScheduleControllers = knownScheduleControllers;

            self->evTrg = (DataObject*)ModelNode_getChild((ModelNode*)schedLn, "EvTrg");

            if (self->evTrg) {
                DataAttribute* inSyn_setSrcRef = (DataAttribute*)ModelNode_getChild((ModelNode*)schedLn, "InSyn.setSrcRef");

                if (inSyn_setSrcRef == NULL) {
                    printf("ERROR: Found EvTrg but InSyn.setSrcRef not present\n");
                }
            }

            checkIfTimeTriggeredAndPeriodic(self);

            self->targetType = targetType;

            IedServer_handleWriteAccess(self->server, schdPrio_setVal, schdPrio_writeAccessHandler, self);

            if (schdResue_setVal) {
                IedServer_handleWriteAccess(self->server, (DataAttribute*)schdResue_setVal, schdReuse_writeAccessHandler, self);
            }

            DataAttribute* schdIntv_setVal = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "SchdIntv.setVal");

            if (schdIntv_setVal) {
                IedServer_handleWriteAccess(self->server, schdIntv_setVal, schdIntv_writeAccessHandler, self);
            }
    
            DataAttribute* numEntr_setVal = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "NumEntr.setVal");

            if (numEntr_setVal) {
                IedServer_handleWriteAccess(self->server, numEntr_setVal, schedule_genericWriteAccessHandler, self);
            }

            schedule_installWriteAccessHandlersForStrTm(self);

            schedule_installWriteAccessHandlersForValues(self);

            IedServer_setPerformCheckHandler(self->server, self->dsaReq, schedule_performCheckHandler, self);
            IedServer_setPerformCheckHandler(self->server, self->enaReq, schedule_performCheckHandler, self);

            IedServer_setControlHandler(self->server, self->dsaReq, schedule_controlHandler, self);
            IedServer_setControlHandler(self->server, self->enaReq, schedule_controlHandler, self);

            schedule_setState(self, SCHD_STATE_NOT_READY);

            schedule_updateNxtStrTm(self, 0);

            self->thread = Thread_create(schedule_thread, self, false);

            self->alive = true;

            self->allowRemoteControl = true;
            self->allowWriteToSchdPrio = true;
            self->allowWriteToStrTm = true;
            self->allowWriteToSchdReuse = true;
            self->allowWriteToSchdIntv = true;

            Thread_start(self->thread);
        }
        else {
            if (knownScheduleControllers)
                LinkedList_destroyStatic(knownScheduleControllers);

            if (self)
                Schedule_destroy(self);
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

int
Schedule_getPrio(Schedule self)
{
    int prio = 0;

    DataAttribute* schdPrio_setVal = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "SchdPrio.setVal");

    if (schdPrio_setVal) {
        if (schdPrio_setVal->mmsValue && MmsValue_getType(schdPrio_setVal->mmsValue) == MMS_INTEGER) {
            prio = MmsValue_toInt32(schdPrio_setVal->mmsValue);
        }
    }

    return prio;
}

void
Schedule_setPrio(Schedule self, int value)
{
    DataAttribute* schdPrio_setVal = (DataAttribute*)ModelNode_getChild((ModelNode*)self->scheduleLn, "SchdPrio.setVal");

    if (schdPrio_setVal && schdPrio_setVal->mmsValue) {
        MmsValue_setInt32(schdPrio_setVal->mmsValue, value);
    }
    else {
        printf("Schedule_setPrio: failed\n");
    }
}

bool
Schedule_getSchdReuse(Schedule self)
{
    bool retVal = false;

    DataAttribute* schdReuse_setVal = (DataAttribute*)ModelNode_getChild((ModelNode*)(self->scheduleLn), "SchdReuse.setVal");

    if (schdReuse_setVal) {
        if (schdReuse_setVal->mmsValue && MmsValue_getType(schdReuse_setVal->mmsValue) == MMS_BOOLEAN) {
            retVal = MmsValue_getBoolean(schdReuse_setVal->mmsValue);
        }
    }

    return retVal;
}

void
Schedule_setSchdReuse(Schedule self, bool reuse)
{
    DataAttribute* schdReuse_setVal = (DataAttribute*)ModelNode_getChild((ModelNode*)(self->scheduleLn), "SchdReuse.setVal");

    if (schdReuse_setVal && schdReuse_setVal->mmsValue) {
        MmsValue_setBoolean(schdReuse_setVal->mmsValue, reuse);
    }
}

int
Schedule_getNumEntr(Schedule self)
{
    int numEntry = 0;

    DataAttribute* numEntry_setVal = (DataAttribute*)ModelNode_getChild((ModelNode*)(self->scheduleLn), "NumEntr.setVal");

    if (numEntry_setVal) {
        if (numEntry_setVal->mmsValue && MmsValue_getType(numEntry_setVal->mmsValue) == MMS_INTEGER) {
            numEntry = MmsValue_toInt32(numEntry_setVal->mmsValue);
        }
    }

    return numEntry;
}

void
Schedule_setNumEntr(Schedule self, int numEntry)
{
     DataAttribute* numEntry_setVal = (DataAttribute*)ModelNode_getChild((ModelNode*)(self->scheduleLn), "NumEntr.setVal");

    if (numEntry_setVal && numEntry_setVal->mmsValue) {
        MmsValue_setInt32(numEntry_setVal->mmsValue, numEntry);
    }
}

int
Schedule_getValueCount(Schedule self)
{
    return schedule_getNumberOfScheduleEntries(self);
}

int
Schedule_getSchdIntvInMs(Schedule self)
{
    return getSchdIntvValueInMs(self);
}

void
Schedule_setSchIntvInMs(Schedule self, int value)
{
    setSchdIntvValueInMs(self, value);

    self->entryDurationInMs = value;
}

bool
Schedule_isRunning(Schedule self)
{
    if (schedule_getState(self) == SCHD_STATE_RUNNING) {
        return true;
    }
    else {
        return false;
    }
}

void
Schedule_enableScheduleControl(Schedule self, bool enable)
{
    self->allowRemoteControl = enable;
}

bool
Schedule_enableSchedule(Schedule self, bool enable)
{
    if (enable) {
        return enabledSchedule(self);
    }
    else {
        if (schedule_getState(self) != SCHD_STATE_NOT_READY) {
            disableSchedule(self);
        }

        return true;
    }
}

ScheduleState
Schedule_getState(Schedule self)
{
    return schedule_getState(self);
}

void
Schedule_setReuse(Schedule self, bool resue)
{
    self->reuse = true;
}

void
Schedule_setState(Schedule self, ScheduleState state)
{
    schedule_udpateState(self, state);
}

void
Schedule_enableWriteAccessToSchdPrio(Schedule self, bool enable)
{
    self->allowWriteToSchdPrio = enable;
}

void
Schedule_enableWriteAccessToStrTm(Schedule self, bool enable)
{
    self->allowWriteToStrTm = enable;
}

void
Schedule_enableWriteAccessToSchdReuse(Schedule self, bool enable)
{
    self->allowWriteToSchdReuse = enable;
}
