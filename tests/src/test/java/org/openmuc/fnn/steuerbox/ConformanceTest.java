package org.openmuc.fnn.steuerbox;

import com.beanit.iec61850bean.Fc;
import com.beanit.iec61850bean.ServiceError;
import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Disabled;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.MethodSource;
import org.openmuc.fnn.steuerbox.models.ScheduleConstants;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.xml.sax.SAXException;

import javax.xml.parsers.ParserConfigurationException;
import java.io.IOException;
import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

// TODO: link these to requirements, then move it into SchedulingTest
public class ConformanceTest extends AllianderBaseTest {

    private static final Logger log = LoggerFactory.getLogger(ConformanceTest.class);

    @BeforeEach
    public void makeSureTimeSet() {
        Assertions.assertTrue(dut.isTimeSet());
        log.warn("MAKE SURE THAT THE DATE/TIME IS SET PROPERLY; OTHERWISE THESE TESTS WILL NOT WORK");
    }

    @BeforeAll
    public static void disableAllRunningSchedules() {
        getScheduleTypes().forEach(scheduleType -> {
            scheduleType.getAllScheduleNames().forEach(schedule -> {
                try {
                    dut.disableSchedules(schedule);
                } catch (Exception e) {
                    Assertions.fail("error, could not disable schedule " + schedule);
                    log.error("error, could not disable schedule " + schedule, e);
                }
            });
        });
        log.debug("Disabled all schedules during init");
    }

    /**
     * {@ link Requirements#LN02a}
     */
    @ParameterizedTest
    @MethodSource("getScheduleTypes")
    void activeControllerIsUpdated(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {
        //initial state: no schedule active -> reserve schedule is working
        //test, that ActSchdRef contains a reference of the reserve schedule

        Assertions.assertEquals(scheduleConstants.getReserveSchedule(), scheduleConstants.getController(),
                "Expecting no schedules to run in initial state");

        String schedule = scheduleConstants.getScheduleName(1);
        //write and activate a schedule with a higher priority than the reserve schedule
        Instant scheduleStart = Instant.now().plus(Duration.ofSeconds(2));
        dut.writeAndEnableSchedule(Arrays.asList(70), Duration.ofSeconds(2), schedule, scheduleStart, 100);

        // waiting shortly, why so long??
        Thread.sleep(6000);

        //test, that ActSchdRef contains a reference of the active schedule
        Assertions.assertEquals(schedule, "FNN_STEUERBOX" + dut.readActiveSchedule(scheduleConstants.getController()));

        // wait until the active schedule finished service
        Thread.sleep(5000);

        //make sure the reserve schedule is active again
        Assertions.assertEquals(scheduleConstants.getReserveSchedule(),
                dut.readActiveSchedule(scheduleConstants.getController()),
                "Did not return to system reserve schedule after execution time");
    }

    /**
     * {@ link Requirements#LN02a}
     */
    @ParameterizedTest
    @MethodSource("getScheduleTypes")
    void activeControllerIsUpdatedWithScheduleOfHighestPrio(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {

        Assertions.assertEquals(scheduleConstants.getReserveSchedule(),
                dut.readActiveSchedule(scheduleConstants.getController()),
                "Expecting no schedules to run in initial state");
        ;

        Instant start = Instant.now().plus(Duration.ofMillis(500));
        // schedule 1 with low prio
        dut.writeAndEnableSchedule(Arrays.asList(100), Duration.ofSeconds(1), scheduleConstants.getScheduleName(1),
                start, 20);

        // schedule 2 starts a bit after schedule 1 but with higher prio
        dut.writeAndEnableSchedule(Arrays.asList(10), Duration.ofSeconds(1), scheduleConstants.getScheduleName(2),
                start.plus(Duration.ofSeconds(1)), 200);

        Thread.sleep(Duration.between(start, Instant.now()).toMillis() + 200);

        Assertions.assertEquals(dut.readActiveSchedule(scheduleConstants.getController()),
                scheduleConstants.getScheduleName(2));

        Thread.sleep(1000); // FIXME: is this really necessary?
    }

    @ParameterizedTest
    @MethodSource("getScheduleTypes")
    public void testSchedulePriosWithTwoSchedules(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {

        // FIXME: read system reserve
        float sysresValue = 0;//dut.readSysResScheduleReturnConstantPower();
        // add some time to configure, then truncate to seconds to make everything more
        // nicely to look at
        Duration configurationBuffer = Duration.ofSeconds(10);
        Instant start = Instant.now().plus(configurationBuffer).truncatedTo(ChronoUnit.SECONDS);

        Duration stepwidth = Duration.ofSeconds(4);

        Instant schedule1Start = start.plus(stepwidth);
        Instant schedule2Start = schedule1Start.plus(stepwidth);

        float schaltprogramValue1 = 20;
        float schaltprogramValue2 = 50;

        dut.writeAndEnableSchedule(
                Arrays.asList(schaltprogramValue1, schaltprogramValue2, schaltprogramValue1, schaltprogramValue2),
                stepwidth, scheduleConstants.getScheduleName(1), schedule1Start, 100);

        float notbefehlValue = 77;
        dut.writeAndEnableSchedule(Arrays.asList(notbefehlValue), stepwidth, scheduleConstants.getScheduleName(2),
                schedule2Start, 200);

        List<Float> expectedValues = Arrays.asList(sysresValue, schaltprogramValue1, notbefehlValue,
                schaltprogramValue1, schaltprogramValue2, sysresValue);

        // start monitoring half a stepwith after start such that it
        Duration halfStepwidth = stepwidth.dividedBy(2);
        Instant monitoringStart = start.plus(halfStepwidth);
        List<Float> actualValues = dut.monitor(monitoringStart, stepwidth.multipliedBy(6), stepwidth,
                scheduleConstants);

        log.info("expected values {}", expectedValues);
        log.info("observed values {}", actualValues);

        assertValuesMatch(expectedValues, actualValues, 0.01);

        log.info("schedule prio test passed :)");

        //disable used schedules in the end for them to be ready for further tests
        dut.disableSchedule(scheduleConstants.getScheduleName(1));
        dut.disableSchedule(scheduleConstants.getScheduleName(2));
    }

    @Disabled
    @ParameterizedTest
    @MethodSource("getScheduleTypes")
    public void testRisingPriorities(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException, ParserConfigurationException,
            IEC61850MissconfiguredException, SAXException {

        //Delayed start of all schedules to have enough time for data transfer, more schedules->more delay
        final Instant testExecutionStart = Instant.now().plus(Duration.ofSeconds(10));
        //final Instant reportingStart = testExecutionStart.plus(Duration.ofSeconds(8));
        final Instant schedulesStart = testExecutionStart.plus(Duration.ofSeconds(4));

        //Duration.ofSeconds() is the duration each value of Array.asList is executed
        //schedule 1: start after 2s, duration: 12s, priority: 15
        dut.writeAndEnableSchedule(Arrays.asList(10, 10, 10, 10, 10, 10), Duration.ofSeconds(4),
                scheduleConstants.getScheduleName(1), schedulesStart, 15);

        // schedule 2: start after 4s, duration 10s, priority 20
        dut.writeAndEnableSchedule(Arrays.asList(40, 40, 40, 40, 40), Duration.ofSeconds(4),
                scheduleConstants.getScheduleName(2), schedulesStart.plus(Duration.ofSeconds(4)), 20);

        // schedule 3: start after 6s, duration 8s, priority 30
        dut.writeAndEnableSchedule(Arrays.asList(70, 70, 70, 70), Duration.ofSeconds(4),
                scheduleConstants.getScheduleName(3), schedulesStart.plus(Duration.ofSeconds(8)), 30);

        //schedule 4: start after 8s, duration 6s, priority 40
        dut.writeAndEnableSchedule(Arrays.asList(100, 100, 100), Duration.ofSeconds(4),
                scheduleConstants.getScheduleName(4), schedulesStart.plus(Duration.ofSeconds(12)), 40);

        List<Float> expectedValues = Arrays.asList(0f, 10f, 40f, 70.0f, 100f, 100f, 100f, 0f);

        Instant monitoringStart = testExecutionStart.plus(Duration.ofSeconds(2));
        List<Float> actualValues = dut.monitor(monitoringStart, Duration.ofSeconds(32), Duration.ofSeconds(4),
                scheduleConstants);

        log.info("expected values {}", expectedValues);
        log.info("observed values {}", actualValues);

        assertValuesMatch(expectedValues, actualValues, 0.01);

        //disable all schedules in the end for them to be ready for further tests
        for (int i = 1; i <= 10; i++) {
            dut.disableSchedule(scheduleConstants.getScheduleName(i));
        }
    }

    @Disabled
    @ParameterizedTest
    @MethodSource("getScheduleTypes")
    public void testSamePrios(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {

        final Instant testExecutionStart = Instant.now().plus(Duration.ofSeconds(10));
        final Instant schedulesStart = testExecutionStart.plus(Duration.ofSeconds(4));

        //schedule 5, start after 2s, duration 12s, Prio 40
        dut.writeAndEnableSchedule(Arrays.asList(10, 10, 10, 10, 10, 10), Duration.ofSeconds(4),
                scheduleConstants.getScheduleName(5), schedulesStart, 40);
        //schedule 1, start after 4s, duration 4s, Prio 40
        dut.writeAndEnableSchedule(Arrays.asList(70, 100), Duration.ofSeconds(4), scheduleConstants.getScheduleName(1),
                schedulesStart.plus(Duration.ofSeconds(4)), 40);
        //schedule 2, start after 8s, duration 4s, Prio 40
        dut.writeAndEnableSchedule(Arrays.asList(70, 70), Duration.ofSeconds(4), scheduleConstants.getScheduleName(2),
                schedulesStart.plus(Duration.ofSeconds(12)), 40);
        //schedule 3, start after 14s, duration 2s, Prio 60
        dut.writeAndEnableSchedule(Arrays.asList(100), Duration.ofSeconds(4), scheduleConstants.getScheduleName(3),
                schedulesStart.plus(Duration.ofSeconds(24)), 60);

        List<Float> expectedValues = Arrays.asList(0f, 10f, 70f, 100f, 70f, 70f, 10f, 100f, 0f);

        Instant monitoringStart = testExecutionStart.plus(Duration.ofSeconds(2));
        List<Float> actualValues = dut.monitor(monitoringStart, Duration.ofSeconds(36), Duration.ofSeconds(4),
                scheduleConstants);
        log.info("expected values {}", expectedValues);
        log.info("observed values {}", actualValues);
        assertValuesMatch(expectedValues, actualValues, 0.01);

        //disable all schedules in the end for them to be ready for further tests
        for (int i = 1; i <= 10; i++) {
            dut.disableSchedule(scheduleConstants.getScheduleName(i));
        }
    }

    @Disabled
    @ParameterizedTest
    @MethodSource("getScheduleTypes")
    public void samePriorityAndStart(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {
        //test of schedules with same prio and same start
        //according to IEC61850 outcome is coincidence

        final Instant testExcecutionStart = Instant.now();
        final Instant schedulesStart = testExcecutionStart.plus(Duration.ofSeconds(4));

        //Schedule 1, start after 2s, duration 8s, Prio 50
        dut.writeAndEnableSchedule(Arrays.asList(10, 10, 10, 10), Duration.ofSeconds(4),
                scheduleConstants.getScheduleName(1), schedulesStart, 50);

        //Schedule 2, start after 2sec, duration 10s, Prio 50
        dut.writeAndEnableSchedule(Arrays.asList(40, 40, 40, 40, 40), Duration.ofSeconds(4),
                scheduleConstants.getScheduleName(2), schedulesStart, 50);

        List<Float> actualValues = dut.monitor(testExcecutionStart.plus(Duration.ofSeconds(4)), Duration.ofSeconds(20),
                Duration.ofSeconds(2), scheduleConstants);

        //disable all schedules in the end for them to be ready for further tests
        for (int i = 1; i <= 10; i++) {
            dut.disableSchedule(scheduleConstants.getScheduleName(i));
        }

        // TODO: use some actual asserts after expected behaviour has been clarified with MZ
    }

    /**
     * Test if the scheduler has the required nodes with correct types as defined in IEC 61850-90-10:2017, table 6 (page
     * 25)
     **/
    @ParameterizedTest
    @MethodSource("getScheduleTypes")
    void scheduleControllerNodeHasRequiredNodes(ScheduleConstants scheduleConstants) {

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

        Assertions.assertTrue(violations.isEmpty(),
                "Found violations of node requirements for schedule controller " + controllerNode + ": "
                        + violationsList + "\n");
    }

}
