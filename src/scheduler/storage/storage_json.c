#include "schedule_storage.h"

struct sSchedulerStorage {
    char* filename;
};

SchedulerStorage
SchedulerStorage_init(const char* databaseUri, int numberOfParameters, const char** parameters)
{
    SchedulerStorage self = (SchedulerStorage)calloc(1, sizeof(struct sSchedulerStorage));

    if (self) {
        self->filename = strdup(databaseUri);
    }

    return self;
}

void
SchedulerStorage_destroy(SchedulerStorage self)
{
    if (self) {
        free(self->filename);

        free(self);
    }
}

bool
SchedulerStorage_saveSchedule(SchedulerStorage self, Schedule schedule)
{
    //TODO implement me

    return true;
}

bool
SchedulerStorage_restoreSchedule(SchedulerStorage self, Schedule schedule)
{
    //TODO implement me

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
