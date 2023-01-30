package org.openmuc.fnn.steuerbox.scheduling;

/**
 * Representation of error codes ("ScheduleEnablingErrorKind") defined in IEC61850-90-10 ed 2017, Table 9 (page 28)
 */
public enum ScheduleEnablingErrorKind {
    NONE(1),
    MISSING_VALID_NUMENTR(2),
    MISSING_VALID_SCHDINTV(3),
    MISSING_VALID_SCHEDULE_VALUES(4);

    private final int value;

    ScheduleEnablingErrorKind(int value) {
        this.value = value;
    }

    public static ScheduleEnablingErrorKind parse(String valueString) {
        try {
            Integer value = Integer.parseInt(valueString);
            for (ScheduleEnablingErrorKind errorKind : ScheduleEnablingErrorKind.values()) {
                if (errorKind.value == value) {
                    return errorKind;
                }
            }
        } catch (Exception e) {
            return NONE;
        }
        return NONE;
    }
}
