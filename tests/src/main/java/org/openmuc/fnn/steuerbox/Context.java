package org.openmuc.fnn.steuerbox;

import javax.xml.parsers.DocumentBuilderFactory;

/**
 * Holds shared objects
 */
public class Context {

    private static final DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();

    public static DocumentBuilderFactory getDocumentBuilderFactory() {
        return factory;
    }
}
