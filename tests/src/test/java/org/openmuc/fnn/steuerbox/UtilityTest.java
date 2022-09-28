package org.openmuc.fnn.steuerbox;

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import static org.openmuc.fnn.steuerbox.AllianderTests.extractNumberFromLastNodeName;

public class UtilityTest {

    @Test
    void numberExtractionWorks() {
        Assertions.assertEquals(
                extractNumberFromLastNodeName("device4.schedule76.whatever.in.between.number123").get().intValue(),
                123);
    }

    @Test
    void numberExtractionDoesNotThrow() {
        Assertions.assertTrue(extractNumberFromLastNodeName("").isEmpty());
        Assertions.assertTrue(extractNumberFromLastNodeName("nonumberinhere").isEmpty());
    }
}
