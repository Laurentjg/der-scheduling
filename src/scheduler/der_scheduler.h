#include <libiec61850/iec61850_server.h>

typedef struct sSchedule* Schedule;

typedef struct sScheduler* Scheduler;

typedef struct sScheduleController* ScheduleController;

Scheduler
Scheduler_create(IedModel* model, IedServer server);

/**
 * @brief Callback to receive notifications on target value changes
 * 
 * @param parameter user provided parameter that is passed to the callback
 * @param targetValueObjRef object reference of the target value (relative to the LN)
 * @param value the updated value of the target
 * @param quality the quality of the target value
 * @param timestampMs the timestamp of the target value (in ms since epoch)
 */
typedef void
(*Scheduler_TargetValueChanged)(void* parameter, const char* targetValueObjRef, MmsValue* value, Quality quality, uint64_t timestampMs);

/**
 * @brief 
 * 
 * @param self 
 * @param handler 
 * @param parameter 
 */
void
Scheduler_setTargetValueHandler(Scheduler self, Scheduler_TargetValueChanged handler, void* parameter);

/**
 * @brief Get the current target value of a schedule controller
 * 
 * @param self 
 * @param controllerRef object reference of the schedule controller (LDInst/LN)
 * @param targetRef user provided buffer to write the current target reference (at least 130 characters)
 * @return MmsValue* current target value or NULL if no target value exists
 */
MmsValue*
Scheduler_getTargetValue(Scheduler self, const char* controllerRef, char* targetRef);

void
Scheduler_destroy(Scheduler self);

void
Scheduler_parseModel(Scheduler self);