package org.openmuc.fnn.steuerbox;

import com.beanit.iec61850bean.ModelNode;
import com.beanit.iec61850bean.ServiceError;
import org.assertj.core.api.Assertions;
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

/**
 * Holds general tests related to 61850 scheduling
 * <p>
 * * TODO: general tests for 61850 scheduling
 * * - scheduling, look into 90-10
 * * - DER, look into 90-7
 * * - check 7-420, too
 */
public class SchedulingTest extends AllianderBaseTest {

    private static final Logger log = LoggerFactory.getLogger(ConformanceTest.class);

    @ParameterizedTest
    @MethodSource("getScheduleTypes")
    void allExpectedSchedulesExist(ScheduleConstants scheduleConstants) {
        for (String scheduleName : scheduleConstants.getAllScheduleNames()) {
            Assertions.assertThat(dut.nodeExists(scheduleName))
                    .describedAs("Expected schedule " + scheduleName + " to exist but did not find it")
                    .isTrue();
        }
    }

    /**
     * {@link Requirements#S10}
     */
    @ParameterizedTest
    @MethodSource("getScheduleTypes")
    void schedulesCanStore100Values(ScheduleConstants scheduleConstants) {
        for (String scheduleName : scheduleConstants.getAllScheduleNames()) {
            ModelNode onOffSchedule = dut.getCachedServerModel().findModelNode(scheduleName, null);
            long booleanValuesOfSchedule = onOffSchedule.getChildren().stream()//
                    .filter(node -> node.getName().startsWith("Val"))// we have all Values now
                    .filter(valueNodes -> valueNodes.getName().contains("ASG")).count();

            int expectedCount = 100;
            Assertions.assertThat(expectedCount)
                    .describedAs("Schdedule " + scheduleName + " does not have " + expectedCount + " as expected")
                    .isEqualTo(booleanValuesOfSchedule);
        }
    }

    /**
     * based on 6185-90-10 {@link Requirements#E02} {@link Requirements#S02} {@link Requirements#S05a}{@link
     * Requirements#S05b}{@link Requirements#S05c} {@link Requirements#E01}
     * <p>
     * TODO add section as refernce
     * <p>
     * TODO do the same for all three kinds of schedules
     */
    @ParameterizedTest
    @MethodSource("getScheduleTypes")
    public void test_priorities(ScheduleConstants scheduleConstants)
            throws ServiceError, IOException, InterruptedException {

        final Instant testExecutionStart = Instant.now();
        final Instant schedulesStart = testExecutionStart.plus(Duration.ofSeconds(30));

        //schedule 1:
        dut.writeAndEnableSchedule(Arrays.asList(10, 30, 70, 100, 100, 100), Duration.ofSeconds(2),
                scheduleConstants.getScheduleName(1), schedulesStart.plus(Duration.ofSeconds(2)), 25);

        // schedule 2:
        dut.writeAndEnableSchedule(Arrays.asList(11, 31, 71, 99, 99, 99), Duration.ofSeconds(2),
                scheduleConstants.getScheduleName(2), schedulesStart.plus(Duration.ofSeconds(10)), 40);

        // schedule 3:
        dut.writeAndEnableSchedule(Arrays.asList(12, 32, 72, 98, 98), Duration.ofSeconds(2),
                scheduleConstants.getScheduleName(3), schedulesStart.plus(Duration.ofSeconds(18)), 60);

        //schedule 4, ends after 44s:
        dut.writeAndEnableSchedule(Arrays.asList(13, 33, 73, 97, 97, 97, 97, 97, 97, 97), Duration.ofSeconds(2),
                scheduleConstants.getScheduleName(4), schedulesStart.plus(Duration.ofSeconds(26)), 60);

        //schedule 5, ends after 42s
        dut.writeAndEnableSchedule(Arrays.asList(70, 70, 70, 70, 70), Duration.ofSeconds(2),
                scheduleConstants.getScheduleName(5), schedulesStart.plus(Duration.ofSeconds(34)), 100);

        //schedule 6,
        dut.writeAndEnableSchedule(Arrays.asList(90), Duration.ofSeconds(2), scheduleConstants.getScheduleName(6),
                schedulesStart.plus(Duration.ofSeconds(36)), 120);

        //schedule 7,

        dut.writeAndEnableSchedule(Arrays.asList(90), Duration.ofSeconds(2), scheduleConstants.getScheduleName(7),
                schedulesStart.plus(Duration.ofSeconds(40)), 120);

        //schedule 8:

        dut.writeAndEnableSchedule(Arrays.asList(10), Duration.ofSeconds(2), scheduleConstants.getScheduleName(8),
                schedulesStart.plus(Duration.ofSeconds(44)), 80);

        //schedule 9
        dut.writeAndEnableSchedule(Arrays.asList(80), Duration.ofSeconds(2), scheduleConstants.getScheduleName(9),
                schedulesStart.plus(Duration.ofSeconds(46)), 20);

        //schedule 10
        dut.writeAndEnableSchedule(Arrays.asList(100), Duration.ofSeconds(2), scheduleConstants.getScheduleName(10),
                schedulesStart.plus(Duration.ofSeconds(48)), 10);

        List<Float> expectedValues = Arrays.asList(0f, 10f, 30f, 70f, 100f, 11f, 31f, 71f, 99f, 12f, 32f, 72f, 98f, 13f,
                33f, 73f, 97f, 70f, 90f, 70f, 90f, 70f, 10f, 80f, 100f, 0f);

        List<Float> actualValues = dut.monitor(schedulesStart, Duration.ofSeconds(48), Duration.ofSeconds(2),
                scheduleConstants);
        log.info("expected values {}", expectedValues);
        log.info("observed values {}", actualValues);

        assertValuesMatch(expectedValues, actualValues, 0.01);
    }
}
