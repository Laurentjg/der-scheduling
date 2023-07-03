#include "der_scheduler_internal.h"
#include <libiec61850/hal_thread.h>

#include "cJSON.h"

#include <stdio.h> //TODO remove - only for debugging

struct sSchedulerStorage {
    char* filename; /* file name of the persistent database */
    cJSON* inMemoryDb; /* in-memory JSON database */
    Semaphore inMemoryDbLock; /* in-memory JSON database access control */
};

static char*
readFileToBuffer(const char* filename)
{
    char* buffer = NULL;

    FILE* file = fopen(filename, "r");

    if (file) {
        if (fseek(file, 0L, SEEK_END) == 0) {

            long fileSize = ftell(file);

            buffer = calloc(1, fileSize + 1);

            if (fseek(file, 0L, SEEK_SET) != 0) {
                printf("ERROR: cannot seek to start of file\n");
                free(buffer);
                return NULL;
            }

            size_t readBytes = fread(buffer, 1, fileSize, file);

            if (readBytes != fileSize) {
                printf("ERROR: failed to read file\n");
                free(buffer);
                return NULL;
            }

            buffer[readBytes] = 0; /* add string terminator */
        }

        fclose(file);
    }

    return buffer;
}

static bool
writeStringToFile(const char* filename, const char* str)
{
    FILE* file = fopen(filename, "w");

    if (file) {
        int ret = fprintf(file, "%s", str);

        fclose(file);

        if (ret < 0) {
            printf("ERROR: failed to write to file\n");

            return false;
        }
    }
    else {
        printf("ERROR: failed to open file for writing\n");

        return false;
    }

    return true;
}

static char*
getFilenameForSchedule(Schedule schedule, char* buffer)
{
    char objRef[130];

    ModelNode_getObjectReferenceEx((ModelNode*)schedule->scheduleLn, objRef, false);

    printf("objRef  : %s\n", objRef);

    /* convert "." to "_" and "/" to "_" */
    int pos = 0;
    int outPos = 0;

    while (objRef[pos] != 0) {
        if (objRef[pos] == '.') {
            buffer[outPos++] = '_';
        }
        else if (objRef[pos] == '/') {
            buffer[outPos++] = '_';
            buffer[outPos++] = '_';
        }
        else {
            buffer[outPos++] = objRef[pos];
        }

        pos++;
    }

    buffer[outPos++] = '.';
    buffer[outPos++] = 'j';
    buffer[outPos++] = 's';
    buffer[outPos++] = 'o';
    buffer[outPos++] = 'n';
    buffer[outPos++] = 0;

    printf("filename: %s\n", buffer);
}

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

static ScheduleState
getStateFromString(const char* stateStr)
{
    if (!strcmp(stateStr, "not ready"))
        return SCHD_STATE_NOT_READY;
    if (!strcmp(stateStr, "start time required"))
        return SCHD_STATE_START_TIME_REQUIRED;
    if (!strcmp(stateStr, "ready"))
        return SCHD_STATE_READY;
    if (!strcmp(stateStr, "running"))
        return SCHD_STATE_RUNNING;
    else
        return SCHD_STATE_INVALID;
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
    printf("Save schedule:\n");

    char filename[200];
    getFilenameForSchedule(schedule, filename);

    cJSON* scheduleJson = createSchedule(schedule);

    if (scheduleJson) {
        char* jsonStr = cJSON_PrintUnformatted(scheduleJson);

        cJSON_Delete(scheduleJson);

        if (jsonStr) {
            writeStringToFile(filename, jsonStr);

            free(jsonStr);
        }
        else {
            printf("ERROR: failed to write schedule to file %s\n", filename);

            return false;
        }
    }
    else {
        printf("ERROR: Failed to convert schedule to json\n");

        return false;
    }

    return true;
}

static bool
getScheduleData(SchedulerStorage self, Schedule schedule, const char* scheduleJsonStr)
{
    char schedRef[130];

    ModelNode_getObjectReferenceEx((ModelNode*)schedule->scheduleLn, schedRef, true);

    printf("Restore schedule data for %s\n", schedRef);

    char filename[200];

    getFilenameForSchedule(schedule, filename);

    cJSON* json = cJSON_Parse(scheduleJsonStr);

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

    //printf("  objRef: %s\n", objRef->valuestring);

    cJSON* state = cJSON_GetObjectItem(json, "state");

    if (state == NULL) {
        printf("JSON-DB(ERROR): No state in schedule\n");

        cJSON_Delete(json);

        return false;
    }

    if (cJSON_IsString(state) == false) {
        printf("JSON-DB(ERROR): state has invalid type\n");

        cJSON_Delete(json);

        return false;
    }

    ScheduleState schedState = getStateFromString(state->valuestring);

    //printf("  state: %s(%i)\n", state->valuestring, schedState);

    Schedule_setState(schedule, schedState);

    cJSON* reuse = cJSON_GetObjectItem(json, "reuse");

    if (reuse == NULL) {
        printf("JSON-DB(ERROR): No reuse in schedule\n");

        cJSON_Delete(json);

        return false;
    }

    if (cJSON_IsBool(reuse) == false) {
        printf("JSON-DB(ERROR): reuse has invalid type\n");

        cJSON_Delete(json);

        return false;
    }
    
    //printf("  reuse: %i\n", reuse->valueint);

    Schedule_setSchdReuse(schedule, (bool)reuse->valueint);
    
    cJSON* prio = cJSON_GetObjectItem(json, "prio");

    if (prio == NULL) {
        printf("JSON-DB(ERROR): No prio in schedule\n");

        cJSON_Delete(json);

        return false;
    }

    if (cJSON_IsNumber(prio) == false) {
        printf("JSON-DB(ERROR): prio has invalid type\n");

        cJSON_Delete(json);

        return false;
    }

    //printf("  prio: %i\n", prio->valueint);

    Schedule_setPrio(schedule, prio->valueint);

    cJSON* numEntr = cJSON_GetObjectItem(json, "numEntr");

    if (numEntr == NULL) {
        printf("JSON-DB(ERROR): No numEntr in schedule\n");

        cJSON_Delete(json);

        return false;
    }

    if (cJSON_IsNumber(numEntr) == false) {
        printf("JSON-DB(ERROR): numEntr has invalid type\n");

        cJSON_Delete(json);

        return false;
    }

    //printf("  numEntr: %i\n", numEntr->valueint);

    Schedule_setNumEntr(schedule, numEntr->valueint);

    cJSON* schdIntv = cJSON_GetObjectItem(json, "schdIntv");

    if (schdIntv == NULL) {
        printf("JSON-DB(ERROR): No schdIntv in schedule\n");

        cJSON_Delete(json);

        return false;
    }

    if (cJSON_IsNumber(schdIntv) == false) {
        printf("JSON-DB(ERROR): schdIntv has invalid type\n");

        cJSON_Delete(json);

        return false;
    }

    //printf("  schdIntv: %i\n", schdIntv->valueint);

    Schedule_setSchIntvInMs(schedule, schdIntv->valueint);

    cJSON* values = cJSON_GetObjectItem(json, "values");

    if (values == NULL) {
        printf("JSON-DB(ERROR): No values in schedule\n");

        cJSON_Delete(json);

        return false;
    }

    int valueCount = cJSON_GetArraySize(values);

    printf("  found %i values\n", valueCount);

    for (int i = 0; i < valueCount; i++) {
        cJSON* value = cJSON_GetArrayItem(values, i);
       
        MmsValue* mmsValue = Schedule_getValueWithIdx(schedule, i);

        if (mmsValue) {
            if (MmsValue_getType(mmsValue) == MMS_BOOLEAN) {
                MmsValue_setBoolean(mmsValue, value->valueint);
                //printf("    values[%i]: %s\n", i, value->valueint != 0 ? "true" : "false");
            }
            else if (MmsValue_getType(mmsValue) == MMS_FLOAT) {
                MmsValue_setFloat(mmsValue, (float)value->valuedouble);
                //printf("    values[%i]: %f\n", i, (float)value->valuedouble);
            }
            else if (MmsValue_getType(mmsValue) == MMS_INTEGER) {
                MmsValue_setInt32(mmsValue, value->valueint);
                //printf("    values[%i]: %i\n", i, value->valueint);
            }
        }
        else {
            printf("   value with index %i not found in schedule!\n", i);
        }
    }

    cJSON* startTimes = cJSON_GetObjectItem(json, "startTimes");

    if (startTimes) {
        int startTimeCount = cJSON_GetArraySize(startTimes);

        printf("  found %i start times:\n", startTimeCount);

        for (int i = 0; i < startTimeCount; i++) {
            cJSON* startTime = cJSON_GetArrayItem(startTimes, i);

            if (startTime) {
                cJSON* id = cJSON_GetObjectItem(startTime, "id");

                if (id) {
                    cJSON* t = cJSON_GetObjectItem(startTime, "t");

                    int tValue = t ? t->valueint : 0;

                   // printf("    %s: %i\n", id->valuestring, tValue);
                }
                else {
                    printf("     ERROR: start time %i has no id\n", i);
                }
            }
        }
    }

    //TODO read other schedule components!

    cJSON_Delete(json);

    return true;
}

bool
SchedulerStorage_restoreSchedule(SchedulerStorage self, Schedule schedule)
{
    cJSON* scheduleJson = NULL;

    char filename[200];
    getFilenameForSchedule(schedule, filename);

    char* buffer = readFileToBuffer(filename);

    if (buffer == NULL) {
        scheduleJson = createSchedule(schedule);

        char* jsonStr = cJSON_PrintUnformatted(scheduleJson);

        if (jsonStr) {
            writeStringToFile(filename, jsonStr);

            free(jsonStr);
        }
        else {
            printf("ERROR: failed to write schedule to file %s\n", filename);
        }
    }
    else {
        scheduleJson = cJSON_Parse(buffer);
        free(buffer);
    }

    if (scheduleJson) {
        char* jsonStr = cJSON_Print(scheduleJson);

        getScheduleData(self, schedule, jsonStr);

        cJSON_Delete(scheduleJson);
    }

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
