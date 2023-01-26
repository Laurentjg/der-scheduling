package org.openmuc.fnn.steuerbox.scheduling;

import org.openmuc.fnn.steuerbox.IEC61850Utility;

import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.function.BiFunction;

/**
 * Represents different types of schedules regarding the values they represent, or in other words what type of GGIO (for
 * now analogue values, boolean values) they control.
 */
public enum ScheduleType {
    /**
     * Used to schedule analogue values (that are mapped as floats) such as power schedules
     */
    ASG((device, schedule) -> ValueAccess.asgAccess(device, schedule)) {
        @Override
        public ScheduleDefinitions<Float> withScheduleDefinitions(IEC61850Utility device, String ggio,
                String controller, String reserveSchedule, String... schedules) {
            return ASG.createScheduleDefinitionsFrom(device, ggio, controller, reserveSchedule, 0f, schedules);
        }
    },
    /**
     * Used to schedule boolean values, such as OnOff schedules
     */
    SPG((device, schedule) -> ValueAccess.spgAccess(device, schedule)) {
        @Override
        public ScheduleDefinitions<Boolean> withScheduleDefinitions(IEC61850Utility device, String ggio,
                String controller, String reserveSchedule, String... schedules) {
            return SPG.createScheduleDefinitionsFrom(device, ggio, controller, reserveSchedule, false, schedules);
        }
    };

    private BiFunction<IEC61850Utility, ScheduleDefinitions, ValueAccess> valueAccessFunction;

    ScheduleType(BiFunction<IEC61850Utility, ScheduleDefinitions, ValueAccess> valueAccessFunction) {
        this.valueAccessFunction = valueAccessFunction;
    }

    public abstract <X> ScheduleDefinitions<X> withScheduleDefinitions(IEC61850Utility device, String ggio,
            String controller, String reserveSchedule, String... schedules);

    private <T> ScheduleDefinitions<T> createScheduleDefinitionsFrom(IEC61850Utility device, String ggio,
            String controller, String reserveSchedule, T defaultValue, String... schedules) {

        List<String> scheduleNames = Arrays.asList(schedules);

        return new ScheduleDefinitions() {
            @Override
            public String getScheduleName(int scheduleNumber) {
                if (scheduleNumber <= 0 || scheduleNumber > scheduleNames.size()) {
                    throw new IllegalArgumentException("Schedule number must be between 1 and " + scheduleNames.size());
                }
                return scheduleNames.get(scheduleNumber - 1);
            }

            @Override
            public int getScheduleNumber(String scheduleName) throws IllegalArgumentException {
                for (int i = 0; i < scheduleNames.size(); i++) {
                    if (scheduleNames.get(i).equals(scheduleName)) {
                        int scheduleNumber = i + 1;
                        return scheduleNumber;
                    }
                }
                throw new IllegalArgumentException("No schedule with name '" + scheduleName + "' configured");
            }

            @Override
            public String getControlledGGIO() {
                return ggio;
            }

            @Override
            public String getGGIOValueReference() {
                return getControlledGGIO() + getValueAccess().getGGIOValueSuffix();
            }

            @Override
            public Collection<String> getAllScheduleNames() {
                return scheduleNames;
            }

            @Override
            public ValueAccess getValueAccess() {
                return valueAccessFunction.apply(device, this);
            }

            @Override
            public org.openmuc.fnn.steuerbox.scheduling.ScheduleType getScheduleType() {
                return org.openmuc.fnn.steuerbox.scheduling.ScheduleType.this;
            }

            @Override
            public T getDefaultValue() {
                return defaultValue;
            }

            @Override
            public String getController() {
                return controller;
            }

            @Override
            public String getReserveSchedule() {
                return reserveSchedule;
            }

            @Override
            public String toString() {
                return ScheduleDefinitions.class.getSimpleName() + " with GGIO " + getControlledGGIO();
            }
        };
    }
}
