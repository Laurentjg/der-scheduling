package org.openmuc.fnn.steuerbox.models;

/**
 * These requirements are meant to be linked from the JavaDoc of unit tests covering them. This way, requirements
 * *should* be linked easily to their tests and vice versa.
 * <p>
 * All requirements are taken from the main github at https://github.com/alliander-opensource/der-scheduling/blob/main/REQUIREMENTS.md
 * .
 * <p>
 * Adding the description as JavaDoc helps to quickly identify what a requirement stands for.
 * <p>
 * TODO: add new requirements
 */
public enum Requirements {

    /**
     * Basic configuration of DER default shall be stored inside a SCL File
     */
    C01,

    /**
     * reserve schedules shall be stored inside a SCL File
     */
    C01a,

    /**
     * number of available schedules (maximum 10 schedules per logical Node, less can be configured) shall be stored
     * inside a SCL File
     */
    C01b,

    /**
     * number of devices to be controlled (limited to 1 for evaluation, extension shall be foreseen) shall be stored
     * inside a SCL File
     */
    C01c,

    /**
     * Parameters of schedules other than the reserve schedule are to be stored in a key-value manner
     */
    C02,

    /**
     * Parameters of schedules other than the reserve schedule are persisted to a separate file (e.g. as JSON)
     */
    C03,

    /**
     * Changes in schedules are persisted into the file as soon as the schedule is validated
     */
    C04,

    /**
     * A interface to CoMPAS is not required.
     */
    C05,

    /**
     * A simple interface to notify about changes in SCL file is to be defined.
     */
    C05a,

    /**
     * A simple interface to notify about changes in key-value
     */
    C05b,

    /**
     * Map Logical Nodes and Common Data Classes to configuration values, allow reading and writing according to
     * 61850-90-10
     */
    LN01,

    /**
     * The control functions of the DER are modeled and implemented independently
     */
    LN02,

    /**
     * Absolute active power is controlled in 10 logical nodes ActPow_FSCH01 .. ActPow_FSCH10, with schedule controller
     * ActPow_FSCC and actuator interface ActPow_GGIO
     */
    LN02a,

    /**
     * Maximum power is controlled in 10 logical nodes WGn_FSCH01 .. WGn_FSCH10, with schedule controller WGn_FSCC and
     * actuator interface WGn_GGIO
     */
    LN02b,

    /**
     * On/Off state of the DER is controlled in 10 logical nodes for the schedules, with schedule controller and an
     * actuator interface. The names are to be defined.
     */
    LN02b1,

    /**
     * The scheduling nodes and their children are to be modeled according to 61850-90-10
     */
    LN03,

    /**
     * A set of 10 schedules per logical node must be supported
     */
    S01,

    /**
     * Support time based schedules
     */
    S02,

    /**
     * Triggers other than time based may be supported
     */
    S03,

    /**
     * Periodical schedules may be supported
     */
    S04,

    /**
     * Support of absolute schedules of absolute power setpoints
     */
    S05a,

    /**
     * Support of absolute schedules of maximum power setpoints
     */
    S05b,

    /**
     * Support of schedules to turn DER on and off
     */
    S05c,

    /**
     * Validation of Schedules according to 61850-90-10
     */
    S08,

    /**
     * Schedules for maximum generation values and absolute power values are modeled in the same way (using the same
     * DTO)
     */
    S09,

    /**
     * Each schedule must store a sequence of max. 100 values
     */
    S10,

    /**
     * Execution of schedules must regard schedule priority according to 61850-10
     */
    E01,

    /**
     * Resolution time base: >= 1 second
     */
    E02;

}
