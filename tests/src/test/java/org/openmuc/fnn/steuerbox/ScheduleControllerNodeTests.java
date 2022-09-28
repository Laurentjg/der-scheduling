package org.openmuc.fnn.steuerbox;

import com.beanit.iec61850bean.Fc;
import com.beanit.iec61850bean.ServiceError;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.MethodSource;
import org.openmuc.fnn.steuerbox.models.ScheduleConstants;

import java.io.IOException;
import java.time.Duration;
import java.time.Instant;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;

/**
 * Holds tests related to 61850 schedule controller node behaviour
 */
public class ScheduleControllerNodeTests extends AllianderBaseTest {

    /**
     * Test if the scheduler has the required nodes with correct types as defined in IEC 61850-90-10:2017, table 6 (page
     * 25)
     **/
    @ParameterizedTest(name = "checkSubnodes running {0}")
    @MethodSource("getScheduleTypes")
    void checkSubnodes(ScheduleConstants scheduleConstants) {

        // relevant part of the table is the "non-derived-statistics" (nds) column

        Map<String, Fc> optional = new HashMap<>();
        Map<String, Fc> atMostOne = new HashMap<>();
        Map<String, Fc> mandatory = new HashMap<>();
        Map<String, Fc> mMulti = new HashMap<>();
        Map<String, Fc> oMulti = new HashMap<>();

        /**
         * Descriptions (DC)
         */
        optional.put("NamPlt", Fc.DC);

        /**
         * Status Information (ST)
         */
        mandatory.put("ActSchdRef", Fc.ST);
        atMostOne.put("ValINS", Fc.ST);
        atMostOne.put("ValSPS", Fc.ST);
        atMostOne.put("ValENS", Fc.ST);
        mandatory.put("Beh", Fc.ST);
        optional.put("Health", Fc.ST);
        optional.put("Mir", Fc.ST);
        /**
         * Measured and metered values (MX)
         */
        atMostOne.put("ValMV", Fc.MX);
        /**
         * Controls
         */
        optional.put("Mod", Fc.CO);
        /**
         * Settings (SP)
         */
        mandatory.put("CtlEnt", Fc.SP);
        for (int n = 1; n <= 10; n++) {
            // n=001..010 is only valid for Alliander tests
            String numberAsStringFilledUpWithZeros = String.format("%03d", n);
            mMulti.put("Schd" + numberAsStringFilledUpWithZeros, Fc.SP);
        }
        oMulti.put("InRef", Fc.SP);

        /***
         * Replace this by usages of test
         */

        String controllerNode = scheduleConstants.getController();
        Collection<String> violations = new LinkedList<>();
        violations.addAll(testOptionalNodes(optional, controllerNode));
        violations.addAll(testAtMostOnceNodes(atMostOne, controllerNode));
        violations.addAll(testMandatoryNodes(mandatory, controllerNode));
        violations.addAll(testMMulti(mMulti, controllerNode));
        violations.addAll(testOMulti(oMulti, controllerNode));

        String delimiter = "\n - ";
        String violationsList = delimiter + String.join(delimiter, violations);

        org.junit.jupiter.api.Assertions.assertTrue(violations.isEmpty(),
                "Found violations of node requirements for schedule controller " + controllerNode + ": "
                        + violationsList + "\n");
    }

    /**
     * {@ link Requirements#LN02a}
     */
    @ParameterizedTest(name = "activeControllerIsUpdated running {0}")
    @MethodSource("getScheduleTypes")
    void activeControllerIsUpdated(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {
        //initial state: no schedule active -> reserve schedule is working
        //test, that ActSchdRef contains a reference of the reserve schedule

        org.junit.jupiter.api.Assertions.assertEquals(scheduleConstants.getReserveSchedule(),
                dut.readActiveSchedule(scheduleConstants.getController()),
                "Expecting no schedules to run in initial state");

        String schedule = scheduleConstants.getScheduleName(1);
        //write and activate a schedule with a higher priority than the reserve schedule
        Instant scheduleStart = Instant.now().plus(Duration.ofSeconds(2));
        dut.writeAndEnableSchedule(Arrays.asList(70), Duration.ofSeconds(2), schedule, scheduleStart, 100);

        // waiting shortly, why so long??
        Thread.sleep(6000);

        //test, that ActSchdRef contains a reference of the active schedule
        org.junit.jupiter.api.Assertions.assertEquals(schedule,
                "FNN_STEUERBOX" + dut.readActiveSchedule(scheduleConstants.getController()));

        // wait until the active schedule finished service
        Thread.sleep(5000);

        //make sure the reserve schedule is active again
        org.junit.jupiter.api.Assertions.assertEquals(scheduleConstants.getReserveSchedule(),
                dut.readActiveSchedule(scheduleConstants.getController()),
                "Did not return to system reserve schedule after execution time");
    }

    /**
     * {@ link Requirements#LN02a}
     */
    @ParameterizedTest(name = "activeControllerIsUpdatedWithScheduleOfHighestPrio running {0}")
    @MethodSource("getScheduleTypes")
    void activeControllerIsUpdatedWithScheduleOfHighestPrio(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {

        org.junit.jupiter.api.Assertions.assertEquals(scheduleConstants.getReserveSchedule(),
                dut.readActiveSchedule(scheduleConstants.getController()),
                "Expecting no schedules to run in initial state");

        Instant start = Instant.now().plus(Duration.ofMillis(500));
        // schedule 1 with low prio
        dut.writeAndEnableSchedule(Arrays.asList(100), Duration.ofSeconds(1), scheduleConstants.getScheduleName(1),
                start, 20);

        // schedule 2 starts a bit after schedule 1 but with higher prio
        dut.writeAndEnableSchedule(Arrays.asList(10), Duration.ofSeconds(1), scheduleConstants.getScheduleName(2),
                start.plus(Duration.ofSeconds(1)), 200);

        Thread.sleep(Duration.between(start, Instant.now()).toMillis() + 200);

        org.junit.jupiter.api.Assertions.assertEquals(dut.readActiveSchedule(scheduleConstants.getController()),
                scheduleConstants.getScheduleName(2));

        Thread.sleep(1000); // FIXME: is this really necessary?
    }

}
