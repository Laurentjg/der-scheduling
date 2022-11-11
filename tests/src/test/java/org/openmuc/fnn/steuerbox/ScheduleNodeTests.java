package org.openmuc.fnn.steuerbox;

import com.beanit.iec61850bean.Fc;
import org.junit.jupiter.api.Disabled;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.MethodSource;
import org.openmuc.fnn.steuerbox.models.ScheduleConstants;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;

import static org.openmuc.fnn.steuerbox.AllianderBaseTest.MandatoryOnCondition.ifPresent;

/**
 * Holds tests related to 61850 schedule node behaviour
 */
public class ScheduleNodeTests extends AllianderBaseTest {

    private static final Logger log = LoggerFactory.getLogger(ScheduleNodeTests.class);

    /**
     * Test if the scheduler has the required nodes with correct types as defined in IEC 61850-90-10:2017, table 7 (page
     * 26)
     * <p>
     * This test is not idenependent from our current use case
     **/
    // TODO MZ: fix types of DsaReq, EnaReq
    @ParameterizedTest(name = "hasRequiredSubNodes running {0}")
    @MethodSource("getAllSchedules")
    void hasRequiredSubNodes(ScheduleConstants scheduleConstants) {

        // relevant part of the table is the "non-derived-statistics" (nds) column as there are no derived-statistic

        for (String scheduleName : scheduleConstants.getAllScheduleNames()) {
            log.info("Testing Schedule {}", scheduleName);

            //  Collection of Optional nodes
            Map<String, Fc> optional = new HashMap<>();

            // Collection of mandatory nodes that must be present
            Map<String, Fc> mandatory = new HashMap<>();

            // Collection of 'AtMostOne' nodes
            Map<String, Fc> atMostOne = new HashMap<>();

            //in IEC61850 90-10_2017_end.pdf , table 6, PresConds: MmultiF(), presence under condition
            Collection<MandatoryOnCondition> mMultiF = new LinkedList<>();

            // Collection of Omulti elements (one or several nodes may be present)
            Map<String, Fc> oMulti = new HashMap<>();

            /**
             *  Descriptions (DC)
             */
            optional.put("NamPlt", Fc.DC);

            /**
             *  Status information (ST)
             */
            mandatory.put("SchdSt", Fc.ST);
            optional.put("SchdEntr", Fc.ST);
            atMostOne.put("ValINS", Fc.ST);
            atMostOne.put("ValSPS", Fc.ST);
            atMostOne.put("ValENS", Fc.ST);
            optional.put("ActStrTm", Fc.ST);
            mandatory.put("NxtStrTm", Fc.ST);
            mandatory.put("SchdEnaErr", Fc.ST);
            mandatory.put("Beh", Fc.ST);
            optional.put("Health", Fc.ST);
            optional.put("Mir", Fc.ST);

            /**
             *  Measured and metered values (MX)
             */
            atMostOne.put("ValMV", Fc.MX);

            /**
             * Controls (CO)
             */
            mandatory.put("EnaReq", Fc.CO);
            mandatory.put("DsaReq", Fc.CO);
            optional.put("Mod", Fc.CO);

            /**
             * Settings (SP)
             */
            optional.put("SchdPrio", Fc.SP);
            mandatory.put("NumEntr", Fc.SP);
            mandatory.put("SchdIntv", Fc.SP);
            //at least one element needs to be present if ValMV is present, otherwise forbidden
            mMultiF.add(ifPresent("ValMV").thenMandatory("ValASG001", Fc.SP));
            //at least one element needs to be present if ValINS is present, otherwise forbidden
            mMultiF.add(ifPresent("ValINS").thenMandatory("ValING001", Fc.SP));
            //at least one element needs to be present if ValISPS is present, otherwise forbidden
            mMultiF.add(ifPresent("ValISPS").thenMandatory("ValSPG001", Fc.SP));
            //at least one element needs to be present if ValENS is present, otherwise forbidden
            mMultiF.add(ifPresent("ValENS").thenMandatory("ValENG001", Fc.SP));
            oMulti.put("StrTm", Fc.SP);
            optional.put("EvTrg", Fc.SP);
            //needs to be present if EvTrg is present, otherwise optional
            mMultiF.add(ifPresent("EvTrg").thenMandatory("InSyn", Fc.SP));
            mandatory.put("SchdReuse", Fc.SP);
            oMulti.put("InRef", Fc.SP);

            Collection<String> violations = new LinkedList<>();
            violations.addAll(testOptionalNodes(optional, scheduleName));
            violations.addAll(testMandatoryNodes(mandatory, scheduleName));
            violations.addAll(testAtMostOnceNodes(atMostOne, scheduleName));
            violations.addAll(testMMultiF(mMultiF, scheduleName));
            violations.addAll(testOMulti(oMulti, scheduleName));

            String delimiter = "\n - ";
            String violationsList = delimiter + String.join(delimiter, violations);

            org.junit.jupiter.api.Assertions.assertTrue(violations.isEmpty(),
                    "Found violations of node requirements for schedule " + scheduleName + ": " + violationsList);

        }
    }

    /**
     * Test that SchdEntr is present and is updated by the currently running schedule as described in  IEC
     * 61850-90-10:2017, table 7 (page 26)
     **/
    @Disabled
    @ParameterizedTest(name = "SchdEntrIsUpdatedWithCurrentlyRunningScheduleIfPresent running {0}")
    @MethodSource("getAllSchedules")
    void SchdEntrIsUpdatedWithCurrentlyRunningScheduleIfPresent(ScheduleConstants scheduleConstants) {
        // test if SchdEntr is available

        // if no, log and exit

        // if present:
        // contains 0 because no schedule is running

        // start schedule

        // contains a value now

        /**
         * From table:
         * The current schedule entry of a running schedule.
         * This is the Data-Instance-ID of the data object
         * ValXXX (e.g. ValASG). As long as the schedule is
         * not running the value shall be 0.
         */
        org.junit.jupiter.api.Assertions.fail("not implemented");
    }

    /**
     * Test that ValINS is not present (because it is not relevant for our use case  thus we cannot/do not want to test
     * the expected behaviour). IEC 61850-90-10:2017, table 7 (page 26)
     **/
    @ParameterizedTest(name = "ValINSIsUpdatedWithCurrentlyRunningScheduleIfPresent running {0}")
    @MethodSource("getAllSchedules")
    void ValINSIsUpdatedWithCurrentlyRunningScheduleIfPresent(ScheduleConstants scheduleConstants) {
        testOptionalNodeNotPresent(scheduleConstants, "ValINS");
    }

    /**
     * Test that ValSPS is not present (because it is not relevant for our use case  thus we cannot/do not want to test
     * the expected behaviour). IEC 61850-90-10:2017, table 7 (page 26)
     **/
    @ParameterizedTest(name = "ValSpsIsUpdatedWithCurrentlyRunningScheduleIfPresent running {0}")
    @MethodSource("getPowerValueSchedules")
    void ValSpsIsUpdatedWithCurrentlyRunningScheduleIfPresent(ScheduleConstants scheduleConstants) {
        testOptionalNodeNotPresent(scheduleConstants, "ValSPS");
        // TODO implement for OnOff schedules
    }

    /**
     * Test that ValENS is not present (because it is not relevant for our use case  thus we cannot/do not want to test
     * the expected behaviour). IEC 61850-90-10:2017, table 7 (page 26)
     **/
    @ParameterizedTest(name = "ValEnsIsUpdatedWithCurrentlyRunningScheduleIfPresent running {0}")
    @MethodSource("getAllSchedules")
    void ValEnsIsUpdatedWithCurrentlyRunningScheduleIfPresent(ScheduleConstants scheduleConstants) {
        testOptionalNodeNotPresent(scheduleConstants, "ValENS");
    }

    /**
     * Test that ActStrTm is updated according to IEC 61850-90-10:2017, table 7 (page 26)
     * <p>
     * See "Explanation" column of ActStrTm, NxtStrTm and ValMV. This test summarizes the requirements such that each
     * schedule needs to run only once. By doing so, the total test duration is reduced.
     **/
    @Disabled
    @ParameterizedTest(name = "ActStrTm_and_NxtStrTm_ValMV_areUpdatedProperly running {0}")
    @MethodSource("getAllSchedules")
    void ActStrTm_and_NxtStrTm_ValMV_areUpdatedProperly(ScheduleConstants scheduleConstants) {
        /**
         * ActStrTm:
         * The time when the schedule started running. If the
         * schedule is not running, the quality of the value is
         * set to invalid.
         */

        /**
         * NxtStrTm:
         * The next time when the schedule is
         * planned/intented to start or re-start running. If no
         * schedules are planned running, the quality of the
         * value is set to invalid.
         */

        /**
         * ValMV:
         * Current value determined by the schedule. As long
         * as the schedule is not running the quality of the
         * value shall be set to invalid. The unit of this data
         * shall be the same as the unit of the data object(s)
         * ValASG.
         */

        // TODO:
        // disable schedule and make sure ActStrTm & NxtStrTm quality is set to invalid

        // enable scheudle and make sure the NxtStrTm date is set here

        //wait until running and make sure ActStrTm is at executtion start and NxtStrTm is set to invalid

        org.junit.jupiter.api.Assertions.fail("not implemented");
    }

    /**
     * Test that SchdEnaErr holds a reasonable error code after provoking errors whilest enabling schedules. See IEC
     * 61850-90-10:2017, table 7 (page 26)
     **/
    @Disabled
    @ParameterizedTest(name = "EnaReq_operating running {0}")
    @MethodSource("getAllSchedules")
    void EnaReq_operating(ScheduleConstants scheduleConstants) {
        /**
         * (controllable) Operating with value true initiates
         * enable transition request according to the state
         * diagram; operating with value false is ignored. The
         * change of its status value is a local issue.
         */
        // TODO: see above

    }

    /**
     * Test that SchdEnaErr holds a reasonable error code after provoking errors whilest enabling schedules. See IEC
     * 61850-90-10:2017, table 7 (page 26)
     **/
    @Disabled
    @ParameterizedTest(name = "DsaReq_operating running {0}")
    @MethodSource("getAllSchedules")
    void DsaReq_operating(ScheduleConstants scheduleConstants) {
        /**
         * (controllable) Operating with value true initiates
         * disable transition request according to the state diagram; operating with value false is ignored. The
         * change of its status value is a local issue.
         */
        // TODO: see above

        org.junit.jupiter.api.Assertions.fail("not implemented");
    }

    /**
     * Test that SchdEnaErr holds a reasonable error code after provoking errors whilest enabling schedules. See IEC
     * 61850-90-10:2017, table 7 (page 26)
     **/
    @Disabled
    @ParameterizedTest(name = "SchdEnaErrHoldsErrorCode running {0}")
    @MethodSource("getAllSchedules")
    void SchdEnaErrHoldsErrorCode(ScheduleConstants scheduleConstants) {
        // TODO: implement with e.g. one example

        org.junit.jupiter.api.Assertions.fail("not implemented");
    }

    /**
     * Test that NumEntr can only be set to values > 0  and values <= the number of  instantiated Val[ASG|ING|SPG|ENG]'s
     * as stated in IEC 61850-90-10:2017, table 7 (page 26)
     **/
    @Disabled
    @ParameterizedTest(name = "NumEntr_range running {0}")
    @MethodSource("getAllSchedules")
    void NumEntr_range(ScheduleConstants scheduleConstants) {
        // TODO: test that it cannot be set to 0, -1 and 101

        org.junit.jupiter.api.Assertions.fail("not implemented");
    }

    /**
     * ############################### UTILITIES AND HELPERS ##################################
     */

    /**
     * Tests that a node is not present in the model. This is to facilitate testing as we can simply leave out nodes
     * that are not relevant for our use case.
     */
    private void testOptionalNodeNotPresent(ScheduleConstants scheduleConstants, String nodeName) {
        scheduleConstants.getAllScheduleNames().forEach(schedule -> {
            org.junit.jupiter.api.Assertions.assertFalse(dut.nodeExists(schedule + "." + nodeName),
                    "Optional node " + nodeName
                            + " not relevant for this use case, so it should be left out (behavior cannot be tested).");
        });
    }
}
