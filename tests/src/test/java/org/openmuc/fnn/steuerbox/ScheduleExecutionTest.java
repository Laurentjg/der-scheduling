package org.openmuc.fnn.steuerbox;

import com.beanit.iec61850bean.ServiceError;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.MethodSource;
import org.openmuc.fnn.steuerbox.models.Requirements;
import org.openmuc.fnn.steuerbox.models.ScheduleConstants;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.time.Duration;
import java.time.Instant;
import java.util.Arrays;
import java.util.List;

import static java.time.Duration.ofSeconds;
import static java.time.Instant.now;

/**
 * Holds tests related to 61850 schedule execution
 */
public class ScheduleExecutionTest extends AllianderBaseTest {

    private static final Logger log = LoggerFactory.getLogger(ScheduleExecutionTest.class);

    @BeforeAll
    public static void configureReserveSchedule() throws ServiceError, IOException {
        dut.writeScheduleValues(
                dut.powerSchedules.getValueAccess().prepareWriting(0, dut.powerSchedules.getReserveSchedule()));
        dut.writeScheduleValues(
                dut.maxPowerSchedules.getValueAccess().prepareWriting(0, dut.maxPowerSchedules.getReserveSchedule()));
        dut.writeScheduleValues(
                dut.onOffSchedules.getValueAccess().prepareWriting(false, dut.onOffSchedules.getReserveSchedule()));
    }

    /**
     * based on 6185-90-10 {@link Requirements#E02} {@link Requirements#S02} {@link Requirements#S05a}{@link
     * Requirements#S05b}{@link Requirements#S05c} {@link Requirements#E01}
     * <p>
     * TODO add section as refernce
     * <p>
     * TODO do the same for all three kinds of schedules
     */
    // TODO MZ: these tests currently fail because after the last schedule has run, the reserve schedule value is not
    //  used but instead the most recent set value remains.
    @ParameterizedTest(name = "test_priorities running {0}")
    @MethodSource("getPowerValueSchedules")
    public void test_priorities(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException, IEC61850MissconfiguredException {

        final Duration interval = ofSeconds(2);

        final Instant testExecutionStart = now();
        final Instant schedulesStart = testExecutionStart.plus(ofSeconds(10));

        //schedule 1:
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(10, 30, 70, 100, 100, 100), scheduleConstants.getScheduleName(1)),
                interval, schedulesStart.plus(interval), 25);

        // schedule 2:
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(11, 31, 71, 99, 99, 99), scheduleConstants.getScheduleName(2)), interval,
                schedulesStart.plus(interval.multipliedBy(5)), 40);

        // schedule 3:
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(12, 32, 72, 98, 98), scheduleConstants.getScheduleName(3)), interval,
                schedulesStart.plus(interval.multipliedBy(9)), 60);

        //schedule 4, ends after 44s:
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(13, 33, 73, 97, 97, 97, 97, 97, 97, 97),
                                scheduleConstants.getScheduleName(4)), interval, schedulesStart.plus(interval.multipliedBy(13)),
                70);

        //schedule 5, ends after 42s
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(70, 70, 70, 70, 70), scheduleConstants.getScheduleName(5)), interval,
                schedulesStart.plus(interval.multipliedBy(17)), 100);

        //schedule 6,
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(90), scheduleConstants.getScheduleName(6)), interval,
                schedulesStart.plus(interval.multipliedBy(18)), 120);

        //schedule 7,

        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(90), scheduleConstants.getScheduleName(7)), interval,
                schedulesStart.plus(interval.multipliedBy(20)), 120);

        //schedule 8:
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(10), scheduleConstants.getScheduleName(8)), interval,
                schedulesStart.plus(interval.multipliedBy(22)), 80);

        //schedule 9
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(80), scheduleConstants.getScheduleName(9)), interval,
                schedulesStart.plus(interval.multipliedBy(23)), 20);

        //schedule 10
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(100), scheduleConstants.getScheduleName(10)), interval,
                schedulesStart.plus(interval.multipliedBy(24)), 11);

        log.debug("Test setup took {}", Duration.between(testExecutionStart, now()));

        float sysResValue = (float) dut.readConstantPowerFromSysResScheduleFromModelNode(
                scheduleConstants.getValueAccess(), scheduleConstants.getReserveSchedule());

        List<Float> expectedValues = Arrays.asList(sysResValue, 10f, 30f, 70f, 100f, 11f, 31f, 71f, 99f, 12f, 32f, 72f,
                98f, 13f, 33f, 73f, 97f, 70f, 90f, 70f, 90f, 70f, 10f, 80f, 100f, sysResValue);

        List<Float> actualValues = dut.monitor(schedulesStart.plus(interval.dividedBy(2)), interval.multipliedBy(26),
                interval, scheduleConstants);

        log.info("expected values {}", expectedValues);
        log.info("observed values {}", actualValues);

        assertValuesMatch(expectedValues, actualValues, 0.01);
    }

    // TODO implement a test similar to the above for OnOff schedules
    public void test_prioritiesOnOffSchedules() throws ServiceError, IOException {

        final Duration interval = ofSeconds(2);

        final Instant testExecutionStart = now();
        final Instant schedulesStart = testExecutionStart.plus(ofSeconds(10));

        // wie test_priorities nur mit 0/1 werten. idealerweise ist immer nur 1 schedule 1 oder 0, sodass man sieht dass das scheduling hier funktioniert

        // TODO: dut.monitor(...) does not work with OnOff schedules yet

        // TODO: implement test
    }

    // TODO MZ: what do we expect here? (I would implement the test such that it matches what you implemented)
    @ParameterizedTest(name = "testSamePrios running {0}")
    @MethodSource("getPowerValueSchedules")
    public void testSamePrios(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException, IEC61850MissconfiguredException {

        final Instant testExecutionStart = now().plus(ofSeconds(10));
        final Instant schedulesStart = testExecutionStart.plus(ofSeconds(4));

        //schedule 5, start after 2s, duration 12s, Prio 40
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(10, 10, 10, 10, 10, 10), scheduleConstants.getScheduleName(5)),
                ofSeconds(4), schedulesStart, 40);
        //schedule 1, start after 4s, duration 4s, Prio 40
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(70, 100), scheduleConstants.getScheduleName(1)), ofSeconds(4),
                schedulesStart.plus(ofSeconds(4)), 40);
        //schedule 2, start after 8s, duration 4s, Prio 40
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(70, 70), scheduleConstants.getScheduleName(2)), ofSeconds(4),
                schedulesStart.plus(ofSeconds(12)), 40);
        //schedule 3, start after 14s, duration 2s, Prio 60
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(100), scheduleConstants.getScheduleName(3)), ofSeconds(4),
                schedulesStart.plus(ofSeconds(24)), 60);

        float sysResValue = (float) dut.readConstantPowerFromSysResScheduleFromModelNode(
                scheduleConstants.getValueAccess(), scheduleConstants.getReserveSchedule());
        List<Float> expectedValues = Arrays.asList(sysResValue, 10f, 70f, 100f, 70f, 70f, 10f, 100f, sysResValue);

        Instant monitoringStart = testExecutionStart.plus(ofSeconds(2));
        List<Float> actualValues = dut.monitor(monitoringStart, ofSeconds(36), ofSeconds(4), scheduleConstants);
        log.info("expected values {}", expectedValues);
        log.info("observed values {}", actualValues);
        assertValuesMatch(expectedValues, actualValues, 0.01);

        //disable all schedules in the end for them to be ready for further tests
        for (int i = 1; i <= 5; i++) {
            dut.disableSchedule(scheduleConstants.getScheduleName(i));
        }
    }

    @ParameterizedTest(name = "samePriorityAndStart running {0}")
    @MethodSource("getPowerValueSchedules")
    public void samePriorityAndStart(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {
        //test of schedules with same prio and same start
        //according to IEC61850 outcome is coincidence

        final Instant testExcecutionStart = now();
        final Instant schedulesStart = testExcecutionStart.plus(ofSeconds(4));

        //Schedule 1, start after 2s, duration 8s, Prio 50
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(10, 10, 10, 10), scheduleConstants.getScheduleName(1)), ofSeconds(4),
                schedulesStart, 50);

        //Schedule 2, start after 2sec, duration 10s, Prio 50
        dut.writeAndEnableSchedule(scheduleConstants.getValueAccess()
                        .prepareWriting(Arrays.asList(40, 40, 40, 40, 40), scheduleConstants.getScheduleName(2)), ofSeconds(4),
                schedulesStart, 50);

        List<Float> actualValues = dut.monitor(testExcecutionStart.plus(ofSeconds(4)), ofSeconds(20), ofSeconds(2),
                scheduleConstants);

        //disable all schedules in the end for them to be ready for further tests
        for (int i = 1; i <= 10; i++) {
            dut.disableSchedule(scheduleConstants.getScheduleName(i));
        }

        // TODO: use some actual asserts after expected behaviour has been clarified with MZ (seems like not defined in 61850-90-10)
    }

}
