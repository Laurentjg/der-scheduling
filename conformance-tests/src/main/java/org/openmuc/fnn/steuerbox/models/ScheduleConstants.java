package org.openmuc.fnn.steuerbox.models;

import org.openmuc.fnn.steuerbox.IEC61850Utility;

import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.function.Function;

/**
 * Contains 61850 references to essential nodes of a schedule
 */
public interface ScheduleConstants {
    IEC61850Utility.ValueAccess getValueAccess();

    String getScheduleName(int scheduleNumber);

    String getControlledGGIO();

    String getController();

    String getReserveSchedule();

    Collection<String> getAllScheduleNames();

    ScheduleType getScheduleType();

    enum ScheduleType {
        /**
         * power schedules
         * <p>
         * TODO: better doc!
         */
        ASG(device -> IEC61850Utility.asgAccess(device)),
        /**
         * OnOff schedules
         * <p>
         * TODO: better doc!
         */
        SPG(device -> IEC61850Utility.spgAccess(device));

        private Function<IEC61850Utility, IEC61850Utility.ValueAccess> valueAccessFunction;

        ScheduleType(Function<IEC61850Utility, IEC61850Utility.ValueAccess> valueAccessFunction) {
            this.valueAccessFunction = valueAccessFunction;
        }

        ScheduleConstants withScheduleNames(IEC61850Utility device, String ggio, String controller,
                String reserveSchedule, String... schedules) {

            List<String> scheduleNames = Arrays.asList(schedules);

            return new ScheduleConstants() {
                @Override
                public String getScheduleName(int scheduleNumber) {
                    if (scheduleNumber <= 0 || scheduleNumber > scheduleNames.size()) {
                        throw new IllegalArgumentException(
                                "Schedule number must be between 1 and " + scheduleNames.size());
                    }
                    return scheduleNames.get(scheduleNumber - 1);
                }

                @Override
                public String getControlledGGIO() {
                    return ggio;
                }

                @Override
                public Collection<String> getAllScheduleNames() {
                    return scheduleNames;
                }

                @Override
                public IEC61850Utility.ValueAccess getValueAccess() {
                    return valueAccessFunction.apply(device);
                }

                @Override
                public ScheduleType getScheduleType() {
                    return ScheduleType.this;
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
                    return ScheduleConstants.class.getSimpleName() + " with GGIO " + getControlledGGIO();
                }
            };
        }
    }
}
