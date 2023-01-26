package org.openmuc.fnn.steuerbox.scheduling;

import com.beanit.iec61850bean.ServiceError;
import org.openmuc.fnn.steuerbox.IEC61850Utility;

import java.io.IOException;
import java.time.Duration;
import java.time.Instant;

/**
 * A schedule, ready to be written to the IEC 61850 device
 */
public interface PreparedSchedule {
    /**
     * Writes and enables the schedule on the device
     */
    void writeAndEnable(IEC61850Utility utility) throws ServiceError, IOException;

    /**
     * Representation of schedule values ready to be written to the 61850 server. Implementations of {@link
     * PreparedScheduleValues} are available in {@link ValueAccess}.
     */
    interface PreparedScheduleValues {
        void writeValues() throws ServiceError, IOException;

        int size();

        String getScheduleName();

        PreparedSchedule asSchedule(Duration interval, Instant start, int prio);
    }
}