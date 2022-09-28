package org.openmuc.fnn.steuerbox.models;

import com.beanit.iec61850bean.ServiceError;
import org.openmuc.fnn.steuerbox.IEC61850Utility;

import java.io.IOException;

public class AllianderDER extends IEC61850Utility {

    public AllianderDER(String host, int port) throws ServiceError, IOException {
        super(host, port);
    }

    public static AllianderDER getWithDefaultSettings() throws ServiceError, IOException {
        return new AllianderDER("127.0.0.1", 102);
    }

    public final ScheduleConstants powerSchedules = ScheduleConstants.fromScheduleNames(
            "DER_Scheduler_Control/ActPow_GGIO",//
            "DER_Scheduler_Control/ActPow_FSCC",//
            "DER_Scheduler_Control/ActPow_SysRes_FSCH",//
            "DER_Scheduler_Control/ActPow_FSCH01", //
            "DER_Scheduler_Control/ActPow_FSCH02", //
            "DER_Scheduler_Control/ActPow_FSCH03", //
            "DER_Scheduler_Control/ActPow_FSCH04", //
            "DER_Scheduler_Control/ActPow_FSCH05", //
            "DER_Scheduler_Control/ActPow_FSCH06", //
            "DER_Scheduler_Control/ActPow_FSCH07", //
            "DER_Scheduler_Control/ActPow_FSCH08", //
            "DER_Scheduler_Control/ActPow_FSCH09", //
            "DER_Scheduler_Control/ActPow_FSCH10");

    public final ScheduleConstants maxPowerSchedules = ScheduleConstants.fromScheduleNames(
            "DER_Scheduler_Control/MaxPow_GGIO",//
            "DER_Scheduler_Control/MaxPow_FSCC",//
            "DER_Scheduler_Control/MaxPow_SysRes_FSCH",//
            "DER_Scheduler_Control/MaxPow_FSCH01", //
            "DER_Scheduler_Control/MaxPow_FSCH02", //
            "DER_Scheduler_Control/MaxPow_FSCH03", //
            "DER_Scheduler_Control/MaxPow_FSCH04", //
            "DER_Scheduler_Control/MaxPow_FSCH05", //
            "DER_Scheduler_Control/MaxPow_FSCH06", //
            "DER_Scheduler_Control/MaxPow_FSCH07", //
            "DER_Scheduler_Control/MaxPow_FSCH08", //
            "DER_Scheduler_Control/MaxPow_FSCH09", //
            "DER_Scheduler_Control/MaxPow_FSCH10");

    public final ScheduleConstants onOffSchedules = ScheduleConstants.fromScheduleNames(
            "DER_Scheduler_Control/OnOffPow_GGIO",//
            "DER_Scheduler_Control/OnOffPow_FSCC",//
            "DER_Scheduler_Control/OnOffPow_SysRes_FSCH",//
            "DER_Scheduler_Control/OnOffPow_FSCH01", //
            "DER_Scheduler_Control/OnOffPow_FSCH02", //
            "DER_Scheduler_Control/OnOffPow_FSCH03", //
            "DER_Scheduler_Control/OnOffPow_FSCH04", //
            "DER_Scheduler_Control/OnOffPow_FSCH05", //
            "DER_Scheduler_Control/OnOffPow_FSCH06", //
            "DER_Scheduler_Control/OnOffPow_FSCH07", //
            "DER_Scheduler_Control/OnOffPow_FSCH08", //
            "DER_Scheduler_Control/OnOffPow_FSCH09", //
            "DER_Scheduler_Control/OnOffPow_FSCH10");
}
