package org.openmuc.fnn.steuerbox;

import com.beanit.iec61850bean.ServiceError;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.MethodSource;
import org.openmuc.fnn.steuerbox.models.Requirements;
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
import java.util.List;

import static java.time.Duration.ofSeconds;
import static java.time.Instant.now;

/**
 * Holds tests related to 61850 schedule execution
 */
public class ScheduleExecutionTest extends AllianderBaseTest {

    private static final Logger log = LoggerFactory.getLogger(ScheduleExecutionTest.class);

    /**
     * based on 6185-90-10 {@link Requirements#E02} {@link Requirements#S02} {@link Requirements#S05a}{@link
     * Requirements#S05b}{@link Requirements#S05c} {@link Requirements#E01}
     * <p>
     * TODO add section as refernce
     * <p>
     * TODO do the same for all three kinds of schedules
     */
    @ParameterizedTest(name = "test_priorities running {0}")
    @MethodSource("getScheduleTypes")
    public void test_priorities(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {

        final Duration interval = ofSeconds(2);

        final Instant testExecutionStart = now();
        final Instant schedulesStart = testExecutionStart.plus(ofSeconds(10));

        //schedule 1:
        dut.writeAndEnableSchedule(Arrays.asList(10, 30, 70, 100, 100, 100), interval,
                scheduleConstants.getScheduleName(1), schedulesStart.plus(interval), 25);

        // schedule 2:
        dut.writeAndEnableSchedule(Arrays.asList(11, 31, 71, 99, 99, 99), interval,
                scheduleConstants.getScheduleName(2), schedulesStart.plus(interval.multipliedBy(5)), 40);

        // schedule 3:
        dut.writeAndEnableSchedule(Arrays.asList(12, 32, 72, 98, 98), interval, scheduleConstants.getScheduleName(3),
                schedulesStart.plus(interval.multipliedBy(9)), 60);

        //schedule 4, ends after 44s:
        dut.writeAndEnableSchedule(Arrays.asList(13, 33, 73, 97, 97, 97, 97, 97, 97, 97), interval,
                scheduleConstants.getScheduleName(4), schedulesStart.plus(interval.multipliedBy(13)), 70);

        //schedule 5, ends after 42s
        dut.writeAndEnableSchedule(Arrays.asList(70, 70, 70, 70, 70), interval, scheduleConstants.getScheduleName(5),
                schedulesStart.plus(interval.multipliedBy(17)), 100);

        //schedule 6,
        dut.writeAndEnableSchedule(Arrays.asList(90), interval, scheduleConstants.getScheduleName(6),
                schedulesStart.plus(interval.multipliedBy(18)), 120);

        //schedule 7,

        dut.writeAndEnableSchedule(Arrays.asList(90), interval, scheduleConstants.getScheduleName(7),
                schedulesStart.plus(interval.multipliedBy(20)), 120);

        //schedule 8:

        dut.writeAndEnableSchedule(Arrays.asList(10), interval, scheduleConstants.getScheduleName(8),
                schedulesStart.plus(interval.multipliedBy(22)), 80);

        //schedule 9
        dut.writeAndEnableSchedule(Arrays.asList(80), interval, scheduleConstants.getScheduleName(9),
                schedulesStart.plus(interval.multipliedBy(23)), 20);

        //schedule 10
        dut.writeAndEnableSchedule(Arrays.asList(100), interval, scheduleConstants.getScheduleName(10),
                schedulesStart.plus(interval.multipliedBy(24)), 11);

        log.debug("Test setup took {}", Duration.between(testExecutionStart, now()));

        // TODO: read system reserve schedule
        float sysResValue = 0f;
        List<Float> expectedValues = Arrays.asList(sysResValue, 10f, 30f, 70f, 100f, 11f, 31f, 71f, 99f, 12f, 32f, 72f,
                98f, 13f, 33f, 73f, 97f, 70f, 90f, 70f, 90f, 70f, 10f, 80f, 100f, sysResValue);

        List<Float> actualValues = dut.monitor(schedulesStart.plus(interval.dividedBy(2)), interval.multipliedBy(26),
                interval, scheduleConstants);
        log.info("expected values {}", expectedValues);
        log.info("observed values {}", actualValues);

        assertValuesMatch(expectedValues, actualValues, 0.01);
    }

    @ParameterizedTest(name = "testSchedulePriosWithTwoSchedules running {0}")
    @MethodSource("getScheduleTypes")
    public void testSchedulePriosWithTwoSchedules(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {

        // FIXME: read system reserve
        float sysresValue = 0;//dut.readSysResScheduleReturnConstantPower();
        // add some time to configure, then truncate to seconds to make everything more
        // nicely to look at
        Duration configurationBuffer = ofSeconds(10);
        Instant start = now().plus(configurationBuffer).truncatedTo(ChronoUnit.SECONDS);

        Duration stepwidth = ofSeconds(4);

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

    @ParameterizedTest(name = "testRisingPriorities running {0}")
    @MethodSource("getScheduleTypes")
    public void testRisingPriorities(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException, ParserConfigurationException,
            IEC61850MissconfiguredException, SAXException {

        //Delayed start of all schedules to have enough time for data transfer, more schedules->more delay
        final Instant testExecutionStart = now().plus(ofSeconds(10));
        //final Instant reportingStart = testExecutionStart.plus(Duration.ofSeconds(8));
        final Instant schedulesStart = testExecutionStart.plus(ofSeconds(4));

        //Duration.ofSeconds() is the duration each value of Array.asList is executed
        //schedule 1: start after 2s, duration: 12s, priority: 15
        dut.writeAndEnableSchedule(Arrays.asList(10, 10, 10, 10, 10, 10), ofSeconds(4),
                scheduleConstants.getScheduleName(1), schedulesStart, 15);

        // schedule 2: start after 4s, duration 10s, priority 20
        dut.writeAndEnableSchedule(Arrays.asList(40, 40, 40, 40, 40), ofSeconds(4),
                scheduleConstants.getScheduleName(2), schedulesStart.plus(ofSeconds(4)), 20);

        // schedule 3: start after 6s, duration 8s, priority 30
        dut.writeAndEnableSchedule(Arrays.asList(70, 70, 70, 70), ofSeconds(4), scheduleConstants.getScheduleName(3),
                schedulesStart.plus(ofSeconds(8)), 30);

        //schedule 4: start after 8s, duration 6s, priority 40
        dut.writeAndEnableSchedule(Arrays.asList(100, 100, 100), ofSeconds(4), scheduleConstants.getScheduleName(4),
                schedulesStart.plus(ofSeconds(12)), 40);

        List<Float> expectedValues = Arrays.asList(0f, 10f, 40f, 70.0f, 100f, 100f, 100f, 0f);

        Instant monitoringStart = testExecutionStart.plus(ofSeconds(2));
        List<Float> actualValues = dut.monitor(monitoringStart, ofSeconds(32), ofSeconds(4), scheduleConstants);

        log.info("expected values {}", expectedValues);
        log.info("observed values {}", actualValues);

        assertValuesMatch(expectedValues, actualValues, 0.01);

        //disable all schedules in the end for them to be ready for further tests
        for (int i = 1; i <= 4; i++) {
            dut.disableSchedule(scheduleConstants.getScheduleName(i));
        }
    }

    @ParameterizedTest(name = "testSamePrios running {0}")
    @MethodSource("getScheduleTypes")
    public void testSamePrios(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {

        final Instant testExecutionStart = now().plus(ofSeconds(10));
        final Instant schedulesStart = testExecutionStart.plus(ofSeconds(4));

        //schedule 5, start after 2s, duration 12s, Prio 40
        dut.writeAndEnableSchedule(Arrays.asList(10, 10, 10, 10, 10, 10), ofSeconds(4),
                scheduleConstants.getScheduleName(5), schedulesStart, 40);
        //schedule 1, start after 4s, duration 4s, Prio 40
        dut.writeAndEnableSchedule(Arrays.asList(70, 100), ofSeconds(4), scheduleConstants.getScheduleName(1),
                schedulesStart.plus(ofSeconds(4)), 40);
        //schedule 2, start after 8s, duration 4s, Prio 40
        dut.writeAndEnableSchedule(Arrays.asList(70, 70), ofSeconds(4), scheduleConstants.getScheduleName(2),
                schedulesStart.plus(ofSeconds(12)), 40);
        //schedule 3, start after 14s, duration 2s, Prio 60
        dut.writeAndEnableSchedule(Arrays.asList(100), ofSeconds(4), scheduleConstants.getScheduleName(3),
                schedulesStart.plus(ofSeconds(24)), 60);

        List<Float> expectedValues = Arrays.asList(0f, 10f, 70f, 100f, 70f, 70f, 10f, 100f, 0f);

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
    @MethodSource("getScheduleTypes")
    public void samePriorityAndStart(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {
        //test of schedules with same prio and same start
        //according to IEC61850 outcome is coincidence

        final Instant testExcecutionStart = now();
        final Instant schedulesStart = testExcecutionStart.plus(ofSeconds(4));

        //Schedule 1, start after 2s, duration 8s, Prio 50
        dut.writeAndEnableSchedule(Arrays.asList(10, 10, 10, 10), ofSeconds(4), scheduleConstants.getScheduleName(1),
                schedulesStart, 50);

        //Schedule 2, start after 2sec, duration 10s, Prio 50
        dut.writeAndEnableSchedule(Arrays.asList(40, 40, 40, 40, 40), ofSeconds(4),
                scheduleConstants.getScheduleName(2), schedulesStart, 50);

        List<Float> actualValues = dut.monitor(testExcecutionStart.plus(ofSeconds(4)), ofSeconds(20), ofSeconds(2),
                scheduleConstants);

        //disable all schedules in the end for them to be ready for further tests
        for (int i = 1; i <= 10; i++) {
            dut.disableSchedule(scheduleConstants.getScheduleName(i));
        }

        // TODO: use some actual asserts after expected behaviour has been clarified with MZ
    }

}
