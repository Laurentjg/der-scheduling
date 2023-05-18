#include "der_scheduler_internal.h"
#include "hal_thread.h"

#include "cJSON.h"

#include <stdio.h> //TODO remove - only for debugging

struct sSchedulerStorage {
    char* filename; /* file name of the persistent database */
    cJSON* inMemoryDb; /* in-memory JSON database */
    Semaphore inMemoryDbLock; /* in-memroy JSON database access control */
};

SchedulerStorage
SchedulerStorage_init(const char* databaseUri, int numberOfParameters, const char** parameters)
{
    SchedulerStorage self = (SchedulerStorage)calloc(1, sizeof(struct sSchedulerStorage));

    if (self) {
        self->filename = strdup(databaseUri);
        self->inMemoryDb = NULL;
        self->inMemoryDbLock = Semaphore_create();
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

static bool
saveScheduleData(SchedulerStorage self, Schedule schedule)
{
    char schedRef[130];

    ModelNode_getObjectReferenceEx(schedule->scheduleLn, schedRef, true);

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

    ModelNode_getObjectReferenceEx(schedule->scheduleLn, schedRef, true);

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

    saveScheduleData(self, schedule);

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
