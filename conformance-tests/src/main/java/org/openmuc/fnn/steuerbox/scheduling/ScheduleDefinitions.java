package org.openmuc.fnn.steuerbox.scheduling;

import java.time.Duration;
import java.time.Instant;
import java.util.Collection;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * Contains 61850 references to essential nodes of a schedule
 */
public interface ScheduleDefinitions<T> {

    /**
     * Returns access to schedule values such that these can be modified on the  61850 device
     */
    ValueAccess<T> getValueAccess();

    /**
     * Prepare a schedule to be
     */
    default PreparedSchedule prepareSchedule(Collection<T> values, int scheduleNumber, Duration interval, Instant start,
            int prio) {
        ValueAccess<T> valueAccess = getValueAccess();
        return valueAccess.prepareSchedule(values, scheduleNumber, interval, start, prio);
    }

    /**
     * Get the schedule name from the number
     *
     * @throws IllegalArgumentException
     *         if no schedule with that number is known
     */
    String getScheduleName(int scheduleNumber) throws IllegalArgumentException;

    /**
     * Get the number of a schedule from its name.
     *
     * @throws IllegalArgumentException
     *         if no scheudle with that name is known
     */
    int getScheduleNumber(String scheduleName) throws IllegalArgumentException;

    String getControlledGGIO();

    String getGGIOValueReference();

    String getController();

    String getReserveSchedule();

    Collection<String> getAllScheduleNames();

    ScheduleType getScheduleType();

    T getDefaultValue();

    default Collection<T> getDefaultValues(int numberOfValues) {
        if (numberOfValues < 1) {
            throw new IllegalArgumentException(
                    "numberOfValues (the size of the returned collection) needs to be at least 1");
        }
        final T defaultValue = getDefaultValue();
        return Stream.generate(() -> defaultValue).limit(numberOfValues).collect(Collectors.toList());
    }
}
