#include <libiec61850/iec61850_server.h>

typedef struct sSchedule* Schedule;

typedef struct sScheduler* Scheduler;

typedef struct sScheduleController* ScheduleController;

Scheduler
Scheduler_create(IedModel* model, IedServer server);

void
Scheduler_parseModel(Scheduler self);