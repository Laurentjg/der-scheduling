#ifndef _DER_SCHEDULER_STORAGE_H
#define _DER_SCHEDULER_STORAGE_H

#include "der_scheduler_internal.h"

typedef struct sSchedulerStorage* SchedulerStorage;

SchedulerStorage
SchedulerStorage_init(const char* databaseUri, int numberOfParameters, const char** parameters);

void
SchedulerStorage_destroy(SchedulerStorage self);

bool
SchedulerStorage_saveSchedule(SchedulerStorage self, Schedule schedule);

bool
SchedulerStorage_restoreSchedule(SchedulerStorage self, Schedule schedule);

bool
SchedulerStorage_saveScheduleController(SchedulerStorage self, ScheduleController controller);

bool
SchedulerStorage_restoreScheduleController(SchedulerStorage self, ScheduleController controller);

#endif /* _DER_SCHEDULER_STORAGE_H */