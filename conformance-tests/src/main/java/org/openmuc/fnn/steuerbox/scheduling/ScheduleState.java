package org.openmuc.fnn.steuerbox.scheduling;

/**
 * Schedule states defined in IEC61850-90-10 ed 2017, Table 8 (page 28)
 */
public enum ScheduleState {
    /**
     * Fallback state for error cases
     */
    UNKNOWN(0),
    NOT_READY(1),
    START_TIME_REQUIRED(2),
    READY(3),
    RUNNING(4);

    private final int value;

    private ScheduleState(int value) {
        this.value = value;
    }

    public static ScheduleState parse(String valueString) {
        try {
            Integer value = Integer.parseInt(valueString);
            for (ScheduleState state : ScheduleState.values()) {
                if (state.value == value) {
                    return state;
                }
            }
        } catch (Exception e) {
            return UNKNOWN;
        }
        return UNKNOWN;
    }
}
