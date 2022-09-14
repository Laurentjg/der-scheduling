#include "der_scheduler.h"

#include <signal.h>
#include <stdio.h>

#include <libiec61850/hal_thread.h>

static bool running = false;

static void
sigint_handler(int signalId)
{
    running = false;
}

static void
scheduler_TargetValueChanged(void* parameter, const char* targetValueObjRef, MmsValue* value, Quality quality, uint64_t timestampMs)
{
    char mmsValueBuf[200];
    if (value) {
       MmsValue_printToBuffer(value, mmsValueBuf, 200);

       printf("Target value changed: %s: %s\n", targetValueObjRef, mmsValueBuf);

       //TODO call bash script for demo?
    }
}

int
main(int argc, char** argv)
{
    IedModel* model = ConfigFileParser_createModelFromConfigFileEx("model.cfg");

    if (model) {
        IedServer server = IedServer_create(model);

        Scheduler sched = Scheduler_create(model, server);

        Scheduler_parseModel(sched);

        Scheduler_setTargetValueHandler(sched, scheduler_TargetValueChanged, server);

        IedServer_start(server, 102);

        if (IedServer_isRunning(server)) {

            running = true;

            while (running) {
                Thread_sleep(100);
            }            
        }
        else {
            printf("ERROR: Cannot start server\n");
        }

        Scheduler_destroy(sched);
        IedServer_destroy(server);
        IedModel_destroy(model);
    }
    else {
        printf("ERROR: Failed to load data model\n");
    }

}