package org.openmuc.fnn.steuerbox;

import com.beanit.iec61850bean.ServiceError;
import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Disabled;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.MethodSource;
import org.openmuc.fnn.steuerbox.models.Requirements;
import org.openmuc.fnn.steuerbox.models.ScheduleConstants;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;

/**
 * Tests suiting the requirements in https://github.com/alliander-opensource/der-scheduling/blob/main/REQUIREMENTS.md
 * <p>
 * Systemoverview available at https://github.com/alliander-opensource/der-scheduling/blob/main/images/system-overview.drawio.png
 * <p>
 * General tests related to 61850 are to be found in {@link ScheduleExecutionTest}
 */

public class AllianderTests extends AllianderBaseTest {

    private static final Logger log = LoggerFactory.getLogger(AllianderTests.class);

    /**
     * @throws ServiceError
     * @throws IOException
     *         {@link org.openmuc.fnn.steuerbox.models.Requirements#C01a}
     */
    // TODO: clearify with MZ/Alliander if this requirement is actually needed; then update + enable test
    @Disabled
    @Test
    void requirement_C01_a_activePowerReserveSchedule() throws ServiceError, IOException {
        String sclFileName = "config.xml";
        String sclFile = dut.readFileVia61850(sclFileName, 10_000);

        // TODO: checks, implement once the format is fixed
        // has 100 values
        // ...
    }

    @ParameterizedTest(name = "tenSchedulesAreSupportedPerType running {0}")
    @MethodSource("getAllSchedules")
    void tenSchedulesAreSupportedPerType(ScheduleConstants scheduleConstants) {
        int j = 0;
        for (String scheduleName : scheduleConstants.getAllScheduleNames()) {
            boolean ScheduleExistsOrNot = dut.nodeExists(scheduleName);
            Assertions.assertTrue(ScheduleExistsOrNot, "Schedule " + scheduleName + " does not exist");
            if (ScheduleExistsOrNot)
                j++;
        }
        log.info("There are {} existing schedules at this logical node", j);
    }

    /**
     * Covers {@link Requirements#S10}
     */
    @ParameterizedTest(name = "scheduleSupports100values running {0}")
    @MethodSource("getAllSchedules")
    void scheduleSupports100values(ScheduleConstants scheduleConstants) {
        for (String scheduleName : scheduleConstants.getAllScheduleNames()) {
            String valNodeName;
            if (scheduleConstants.getScheduleType() == ScheduleConstants.ScheduleType.SPG)
                valNodeName = ".ValSPG";
            else
                valNodeName = ".ValASG";
            for (int i = 1; i <= 100; i++) {
                String numberAsStringFilledUpWithZeros = String.format("%03d", i);
                String valueConfigurationNode = scheduleName + valNodeName + numberAsStringFilledUpWithZeros;
                boolean valueExists = dut.nodeExists(valueConfigurationNode);
                Assertions.assertTrue(valueExists, "Missing node " + valueConfigurationNode);
            }
        }
    }

    @ParameterizedTest(name = "scheduleSupportsTimebasedScheduling running {0}")
    @MethodSource("getAllSchedules")
    void scheduleSupportsTimebasedScheduling(ScheduleConstants scheduleConstants) {
        for (int i = 1; i <= 10; i++) {
            String aTimerNode = scheduleConstants.getScheduleName(i) + ".StrTm01";
            Assertions.assertTrue(dut.nodeExists(aTimerNode),
                    "Node " + aTimerNode + " does not exist. Time based scheduling is not possible without this node");
        }
    }

    @ParameterizedTest(name = "allExpectedSchedulesExist running {0}")
    @MethodSource("getAllSchedules")
    void allExpectedSchedulesExist(ScheduleConstants scheduleConstants) {
        for (String scheduleName : scheduleConstants.getAllScheduleNames()) {
            org.assertj.core.api.Assertions.assertThat(dut.nodeExists(scheduleName))
                    .describedAs("Expected schedule " + scheduleName + " to exist but did not find it")
                    .isTrue();
        }
    }
}
