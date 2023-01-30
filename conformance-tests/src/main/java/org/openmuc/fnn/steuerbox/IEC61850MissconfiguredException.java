package org.openmuc.fnn.steuerbox;

/**
 * Is thrown if a IEC61850 device is configured in a way that is not expected
 */
public class IEC61850MissconfiguredException extends Exception {
    public IEC61850MissconfiguredException(String message) {
        super(message);
    }
}
