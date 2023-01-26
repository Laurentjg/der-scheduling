package de.fhg.ise.testtool.utils.annotations.label;

import org.openmuc.fnn.steuerbox.models.Requirement;

import java.lang.annotation.ElementType;
import java.lang.annotation.Target;

/**
 * Custom Label for test methods to link to requirements
 */
@Target({ ElementType.METHOD })
public @interface Requirements {
    Requirement[] value() default Requirement.NONE;

    String description() default "";
}
