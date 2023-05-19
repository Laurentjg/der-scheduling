#include "der_scheduler_internal.h"
#include <libiec61850/hal_thread.h>

#include "cJSON.h"

#include <stdio.h> //TODO remove - only for debugging

struct sSchedulerStorage {
    char* filename; /* file name of the persistent database */
    cJSON* inMemoryDb; /* in-memory JSON database */
    Semaphore inMemoryDbLock; /* in-memory JSON database access control */
};

SchedulerStorage
SchedulerStorage_init(const char* databaseUri, int numberOfParameters, const char** parameters)
{
    SchedulerStorage self = (SchedulerStorage)calloc(1, sizeof(struct sSchedulerStorage));

    if (self) {
        self->filename = strdup(databaseUri);
        self->inMemoryDb = NULL;
        self->inMemoryDbLock = Semaphore_create(1);
    }

    return self;
}

void
SchedulerStorage_destroy(SchedulerStorage self)
{
    if (self) {
        free(self->filename);

        if (self->inMemoryDb)
            cJSON_Delete(self->inMemoryDb);

        if (self->inMemoryDbLock)
            Semaphore_destroy(self->inMemoryDbLock);

        free(self);
    }
}

static const char*
getStringFromState(ScheduleState state)
{
    switch(state) {
        case SCHD_STATE_NOT_READY:
            return "not ready";
        case SCHD_STATE_START_TIME_REQUIRED:
            return "start time required";
        case SCHD_STATE_READY:
            return "ready";
        case SCHD_STATE_RUNNING:
            return "running";
        case SCHD_STATE_INVALID:
        default:
            return "invalid";
    }
}

static cJSON*
createSchedule(Schedule schedule)
{
    cJSON* scheduleJson = cJSON_CreateObject();

    if (scheduleJson == NULL)
        goto exit_error;

    char schedRef[130];
    ModelNode_getObjectReferenceEx((ModelNode*)schedule->scheduleLn, schedRef, true);

    cJSON* objRef = cJSON_CreateString(schedRef);
    cJSON_AddItemToObject(scheduleJson, "objRef", objRef);

    //TODO add "state"
    cJSON* schdSt = cJSON_CreateString(getStringFromState(Schedule_getState(schedule)));
    cJSON_AddItemToObject(scheduleJson, "state", schdSt);

    cJSON* reuse = cJSON_CreateBool(Schedule_getSchdReuse(schedule));
    cJSON_AddItemToObject(scheduleJson, "reuse", reuse);

    cJSON* prio = cJSON_CreateNumber((double)Schedule_getPrio(schedule));
    cJSON_AddItemToObject(scheduleJson, "prio", prio);

    cJSON* numEntr = cJSON_CreateNumber((double)(Schedule_getNumEntr(schedule)));
    cJSON_AddItemToObject(scheduleJson, "numEntr", numEntr);

    cJSON* schdIntv = cJSON_CreateNumber((double)(Schedule_getSchdIntvInMs(schedule)));
    cJSON_AddItemToObject(scheduleJson, "schdIntv", schdIntv);

    /* save values */
    cJSON* values = cJSON_CreateArray();

    int valueCount = Schedule_getValueCount(schedule);

    for (int idx = 0; idx < valueCount; idx++) {
        MmsValue* mmsValue = Schedule_getValueWithIdx(schedule, idx);

        cJSON* value = NULL;

        if (MmsValue_getType(mmsValue) == MMS_BOOLEAN) {
            value = cJSON_CreateBool(MmsValue_getBoolean(mmsValue));
        }
        else if (MmsValue_getType(mmsValue) == MMS_FLOAT) {
            value = cJSON_CreateNumber((double)MmsValue_toFloat(mmsValue));
        }
        else if (MmsValue_getType(mmsValue) == MMS_INTEGER) {
            value = cJSON_CreateNumber((double)MmsValue_toInt32(mmsValue));
        }

        if (value) {
            cJSON_AddItemToArray(values, value);
        }
    }

    cJSON_AddItemToObject(scheduleJson, "values", values);

    cJSON* startTimes = cJSON_CreateArray();

    LogicalNode* schedLn = schedule->scheduleLn;

    DataObject* dobj = (DataObject*)(schedLn->firstChild);

    while (dobj) 
    {
        if (scheduler_checkIfMultiObjInst(dobj->name, "StrTm")) {

            uint64_t timeVal = 0L;

            DataAttribute* strTm_setTm = (DataAttribute*)ModelNode_getChild((ModelNode*)dobj, "setTm");

            if (strTm_setTm) {
                if (strTm_setTm->mmsValue && MmsValue_getType(strTm_setTm->mmsValue) == MMS_UTC_TIME) {
                    timeVal = MmsValue_getUtcTimeInMs(strTm_setTm->mmsValue);
                }
            }

            cJSON* strTm = cJSON_CreateObject();

            cJSON* startTimeId = cJSON_CreateString(dobj->name);

            cJSON_AddItemToObject(strTm, "id", startTimeId);

            cJSON* startTimeT = cJSON_CreateNumber((double)timeVal);

            cJSON_AddItemToObject(strTm, "t", startTimeT);

            cJSON_AddItemToArray(startTimes, strTm);
        }    

        dobj = (DataObject*)(dobj->sibling);
    }

    cJSON_AddItemToObject(scheduleJson, "startTimes", startTimes);
    
    char* jsonStr = cJSON_PrintUnformatted(scheduleJson);

    printf("\n%s\n", jsonStr);

exit:
    return scheduleJson;

exit_error:
    if (scheduleJson)
        cJSON_Delete(scheduleJson);

    return NULL;
}

static bool
saveScheduleData(SchedulerStorage self, Schedule schedule)
{
    char schedRef[130];

    ModelNode_getObjectReferenceEx((ModelNode*)schedule->scheduleLn, schedRef, true);

    printf("Save schedule data for %s\n", schedRef);

    cJSON* scheduleJson = cJSON_CreateObject();

    if (scheduleJson == NULL)
    {
        return false;
    }

    cJSON* objRef = cJSON_CreateString(schedRef);

    cJSON_AddItemToObject(scheduleJson, "objRef", objRef);

    char* jsonStr = cJSON_Print(scheduleJson);

    printf("\n%s\n", jsonStr);
}

bool
SchedulerStorage_saveSchedule(SchedulerStorage self, Schedule schedule)
{
    //TODO implement me

    return true;
}

static bool
getScheduleData(SchedulerStorage self, Schedule schedule)
{
    const char* scheduleDataJson = "{\"objRef\": \"LLN0\", \"status\": \"running\"}";

    char schedRef[130];

    ModelNode_getObjectReferenceEx((ModelNode*)schedule->scheduleLn, schedRef, true);

    printf("Restore schedule data for %s\n", schedRef);

    cJSON* json = cJSON_Parse(scheduleDataJson);

    if (json == NULL)
    {
        printf("Failed to lookup schedule\n");

        return false;
    }

    cJSON* objRef = cJSON_GetObjectItem(json, "objRef");

    if (objRef == NULL) {
        printf("JSON-DB(ERROR): No objRef in schedule\n");

        cJSON_Delete(json);

        return false;
    }

    if (cJSON_IsString(objRef) == false) {
        printf("JSON-DB(ERROR): objRef has invalid type\n");

        cJSON_Delete(json);

        return false;
    }

    printf("objRef: %s\n", objRef->valuestring);

    cJSON_Delete(json);

    return true;
}

bool
SchedulerStorage_restoreSchedule(SchedulerStorage self, Schedule schedule)
{
    getScheduleData(self, schedule);

    createSchedule(schedule);

    return true;
}

bool
SchedulerStorage_saveScheduleController(SchedulerStorage self, ScheduleController controller)
{
    //TODO implement me

    return true;
}

bool
SchedulerStorage_restoreScheduleController(SchedulerStorage self, ScheduleController controller)
{
    //TODO implement me

    return true;
}
