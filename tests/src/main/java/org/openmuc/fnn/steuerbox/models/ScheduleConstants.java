package org.openmuc.fnn.steuerbox.models;

import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * Contains 61850 references to essential nodes of a schedule
 */
public interface ScheduleConstants {
    String getScheduleName(int scheduleNumber);

    String getControlledGGIO();

    String getController();

    String getReserveSchedule();

    Collection<String> getAllScheduleNames();

    static ScheduleConstants fromScheduleNames(String ggio, String controller, String reserveSchedule,
            String... schedules) {

        List<String> scheduleNames = Arrays.asList(schedules);

        return new ScheduleConstants() {
            @Override
            public String getScheduleName(int scheduleNumber) {
                if (scheduleNumber <= 0 || scheduleNumber > scheduleNames.size()) {
                    throw new IllegalArgumentException("Schedule number must be between 1 and " + scheduleNames.size());
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
