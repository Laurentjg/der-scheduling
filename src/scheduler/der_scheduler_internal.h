#include "der_scheduler.h"

#include <libiec61850/hal_thread.h>

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

    LinkedList knownScheduleControllers; /* list of ScheduleControllers to inform on state/value change events */
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
ScheduleController_create(LogicalNode* fsccLn, IedServer server);

void
scheduleController_schedulePrioUpdated(ScheduleController self, Schedule sched, int newPrio);

int
Schedule_getPrio(Schedule self);

bool
Schedule_isRunning(Schedule self);

void
Schedule_destroy(Schedule self);
