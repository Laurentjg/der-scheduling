package org.openmuc.fnn.steuerbox;

import com.beanit.iec61850bean.BasicDataAttribute;
import com.beanit.iec61850bean.BdaBoolean;
import com.beanit.iec61850bean.BdaFloat32;
import com.beanit.iec61850bean.BdaFloat64;
import com.beanit.iec61850bean.BdaInt16;
import com.beanit.iec61850bean.BdaInt16U;
import com.beanit.iec61850bean.BdaInt32;
import com.beanit.iec61850bean.BdaInt32U;
import com.beanit.iec61850bean.BdaInt64;
import com.beanit.iec61850bean.BdaInt8;
import com.beanit.iec61850bean.BdaInt8U;
import com.beanit.iec61850bean.BdaTimestamp;
import com.beanit.iec61850bean.BdaVisibleString;
import com.beanit.iec61850bean.ClientAssociation;
import com.beanit.iec61850bean.ClientSap;
import com.beanit.iec61850bean.Fc;
import com.beanit.iec61850bean.FcModelNode;
import com.beanit.iec61850bean.ModelNode;
import com.beanit.iec61850bean.ServerModel;
import com.beanit.iec61850bean.ServiceError;
import org.apache.commons.io.IOUtils;
import org.openmuc.fnn.steuerbox.models.ScheduleConstants;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.ParserConfigurationException;
import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.charset.Charset;
import java.time.Duration;
import java.time.Instant;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;

public class IEC61850Utility implements Closeable {

    private final static Logger log = LoggerFactory.getLogger(IEC61850Utility.class);

    private final ClientAssociation association;
    private final ServerModel serverModel;

    protected IEC61850Utility(String host, int port) throws UnknownHostException, IOException, ServiceError {
        this(InetAddress.getByName(host), port);
    }

    protected IEC61850Utility(InetAddress host, int port) throws IOException, ServiceError {
        log.info("Connecting to {}:{}", host, port);
        ClientSap clientSap = new ClientSap();

        this.association = clientSap.associate(host, port, null, null);
        log.debug("loading server model");
        this.serverModel = this.association.retrieveModel();
        log.debug("done loading server model");
    }

    @Override
    public void close() {
        this.association.disconnect();
    }

    public static float readConstantSystemReservePowerFromConfigXml(String xml)
            throws ParserConfigurationException, IOException, SAXException, IEC61850MissconfiguredException {
        DocumentBuilder documentBuilder = Context.getDocumentBuilderFactory().newDocumentBuilder();

        InputStream asStream = IOUtils.toInputStream(xml, Charset.defaultCharset());

        Document configDocument = documentBuilder.parse(asStream);
        configDocument.getDocumentElement().normalize();
        NodeList reserve = configDocument.getElementsByTagName("reserve");

        if (reserve.getLength() != 1) {
            throw new IEC61850MissconfiguredException(
                    "Expected to find exactly one element 'reserve' but found: " + reserve
                            + ". Maybe there is more than one CLS configured?");
        }

        NodeList systemReserveScheduleEntries = reserve.item(0).getChildNodes();
        int valueCount = 0;
        String powerAsString = "";
        for (int i = 0; i < systemReserveScheduleEntries.getLength(); i++) {
            Node item = systemReserveScheduleEntries.item(i);
            // only interested in elements!
            if (Node.ELEMENT_NODE == item.getNodeType()) {
                valueCount++;
                powerAsString = item.getAttributes().getNamedItem("power").getNodeValue();
            }
        }

        if (valueCount != 1) {
            throw new IEC61850MissconfiguredException(
                    "Expected exactly 1 power value in system reserve Schedule but got "
                            + systemReserveScheduleEntries.getLength() + ". Please reconfigure the device.");
        }

        float power = Float.parseFloat(powerAsString);

        return power;
    }

    public float readSysResScheduleReturnConstantPower()
            throws ParserConfigurationException, IOException, SAXException, IEC61850MissconfiguredException,
            ServiceError {
        String xml = readFileVia61850("config.xml", 10_000);
        System.out.println("XML:\n" + xml);
        return readConstantSystemReservePowerFromConfigXml(xml);
    }

    public String readFileVia61850(String fileName, int readTimeoutMillis) throws ServiceError, IOException {
        AtomicBoolean done = new AtomicBoolean(false);
        StringBuffer buffer = new StringBuffer();
        association.getFile(fileName, (fileData, moreFollows) -> {

            buffer.append(new String(fileData));
            done.set(!moreFollows);
            return moreFollows;
        });

        long end = System.currentTimeMillis() + readTimeoutMillis;
        while (!done.get()) {
            if (System.currentTimeMillis() < end) {
                throw new IOException("Timeout exceeded");
            }
            try {
                Thread.sleep(500);
            } catch (InterruptedException e) {
                throw new IOException("interrupted", e);
            }
        }

        return buffer.toString();
    }

    public void writeAndEnableSchedule(List<? extends Number> values, Duration interval, String scheduleName,
            Instant start, int prio) throws ServiceError, IOException {

        if (values.size() < 1) {
            throw new IllegalArgumentException("At least one value required.");
        }

        Long intervalInSeconds = interval.getSeconds();

        if (intervalInSeconds < 1) {
            throw new IllegalArgumentException("interval must be larger than one second");
        }

        int index = 1;
        for (Number value : values) {
            String valueBasicDataAttribute = String.format("%s.ValASG%03d.setMag.f", scheduleName, index++);
            log.debug("Writing {} to {}", value, valueBasicDataAttribute);
            setDataValues(valueBasicDataAttribute, null, value.toString());
        }

        setDataValues(scheduleName + ".NumEntr.setVal", null, String.valueOf(values.size()));

        log.info("setting {} start to {}", scheduleName, start);
        setDataValues(scheduleName + ".StrTm01.setTm", null, Long.toString(start.toEpochMilli()));
        setDataValues(scheduleName + ".SchdIntv.setVal", null, intervalInSeconds.toString());
        setDataValues(scheduleName + ".SchdPrio.setVal", null, Long.toString(prio));

        BasicDataAttribute disableOp = findAndAssignValue(scheduleName + ".DsaReq.Oper.ctlVal", Fc.CO, "false");
        BasicDataAttribute enableOp = findAndAssignValue(scheduleName + ".EnaReq.Oper.ctlVal", Fc.CO, "true");

        operate((FcModelNode) disableOp.getParent().getParent());
        operate((FcModelNode) enableOp.getParent().getParent());

    }

    public void disableSchedule(String scheduleNames) throws ServiceError, IOException {

        BasicDataAttribute disableOp = findAndAssignValue(scheduleNames + ".DsaReq.Oper.ctlVal", Fc.CO, "true");

        operate((FcModelNode) disableOp.getParent().getParent());
    }

    protected void operate(FcModelNode node) throws ServiceError, IOException {
        try {
            association.operate(node);
        } catch (ServiceError e) {
            throw new ServiceError(e.getErrorCode(), "Unable to operate " + node.getReference().toString(), e);
        }
    }

    /**
     * Reads the main power, that should be controlled by our schedule
     */
    public float readAnalogOutFromGGIO(String controlledEntitiy) throws ServiceError, IOException {
        BdaFloat32 activePower = (BdaFloat32) serverModel.findModelNode(controlledEntitiy + ".AnOut1.mxVal.f", null);
        association.getDataValues((FcModelNode) activePower);
        float mainPowerValue = activePower.getFloat();
        log.debug("Got output value {} at {} UTC", mainPowerValue, Instant.now());
        return mainPowerValue;
    }

    /**
     * Monitors the main power of the 61850 DUT by polling in monitoring interval over the duration of
     * monitoringDuration.
     * <p>
     * The result list contains a list of polled values. The first value is the polled value at the start.
     */
    public List<Float> monitor(Instant start, Duration monitoringDuration, Duration monitoringInterval,
            ScheduleConstants constants) throws InterruptedException {

        log.info("setting up monitoring");
        if (monitoringDuration.minus(Duration.ofSeconds(1)).isNegative()) {
            throw new RuntimeException("Duration is too small, needs to be at least 1s");
        }

        if (start.plus(monitoringDuration).getEpochSecond() < Instant.now().getEpochSecond()) {
            throw new RuntimeException("Nothing to monitor, start is in the past");
        }

        AtomicLong counter = new AtomicLong(monitoringDuration.getSeconds() / monitoringInterval.getSeconds());

        ScheduledExecutorService service = new ScheduledThreadPoolExecutor(1);

        List<Float> monitoredValues = new LinkedList<>();
        Runnable monitoringTask = () -> {
            try {
                if (counter.getAndDecrement() > 0) {
                    float value = readAnalogOutFromGGIO(constants.getControlledGGIO());
                    monitoredValues.add(value);
                }
                else {
                    service.shutdown();
                }
            } catch (ServiceError | IOException e) {
                log.error("Unable to read value", e);
            }
        };

        long millisUntilStart = Duration.between(Instant.now(), start).toMillis();
        service.scheduleWithFixedDelay(monitoringTask, millisUntilStart, monitoringInterval.toMillis(),
                TimeUnit.MILLISECONDS);

        service.awaitTermination(monitoringDuration.getSeconds() + millisUntilStart / 1000 + 5, TimeUnit.SECONDS);

        return monitoredValues;
    }

    protected BasicDataAttribute setDataValues(String objectReference, Fc fc, String value)
            throws ServiceError, IOException {
        log.debug("Setting {} to {}", objectReference, value);
        BasicDataAttribute bda = findAndAssignValue(objectReference, fc, value);
        try {
            association.setDataValues(bda);
            return bda;
        } catch (ServiceError se) {
            throw new ServiceError(se.getErrorCode(),
                    String.format("Unable to set '%s' to '%s'", objectReference, value), se);
        }
    }

    protected BasicDataAttribute findAndAssignValue(String objectReference, Fc fc, String value) {
        ModelNode node = serverModel.findModelNode(objectReference, fc);
        if (node == null) {
            throw new RuntimeException("Could not find node with name " + objectReference);
        }
        if (!(node instanceof BasicDataAttribute)) {
            throw new RuntimeException(
                    String.format("Unable to assign a value to node '%s' with type '%s'.", node.getName(),
                            node.getClass().getSimpleName()));
        }
        else {
            BasicDataAttribute attribute = (BasicDataAttribute) node;
            setBda(value, attribute);
            return attribute;
        }
    }

    private static void setBda(String valueString, BasicDataAttribute modelNode) {
        if (modelNode instanceof BdaFloat32) {
            float value = Float.parseFloat(valueString);
            ((BdaFloat32) modelNode).setFloat(value);
        }
        else if (modelNode instanceof BdaFloat64) {
            double value = Float.parseFloat(valueString);
            ((BdaFloat64) modelNode).setDouble(value);
        }
        else if (modelNode instanceof BdaInt8) {
            byte value = Byte.parseByte(valueString);
            ((BdaInt8) modelNode).setValue(value);
        }
        else if (modelNode instanceof BdaInt8U) {
            short value = Short.parseShort(valueString);
            ((BdaInt8U) modelNode).setValue(value);
        }
        else if (modelNode instanceof BdaInt16) {
            short value = Short.parseShort(valueString);
            ((BdaInt16) modelNode).setValue(value);
        }
        else if (modelNode instanceof BdaInt16U) {
            int value = Integer.parseInt(valueString);
            ((BdaInt16U) modelNode).setValue(value);
        }
        else if (modelNode instanceof BdaInt32) {
            int value = Integer.parseInt(valueString);
            ((BdaInt32) modelNode).setValue(value);
        }
        else if (modelNode instanceof BdaInt32U) {
            long value = Long.parseLong(valueString);
            ((BdaInt32U) modelNode).setValue(value);
        }
        else if (modelNode instanceof BdaInt64) {
            long value = Long.parseLong(valueString);
            ((BdaInt64) modelNode).setValue(value);
        }
        else if (modelNode instanceof BdaBoolean) {
            boolean value = Boolean.parseBoolean(valueString);
            ((BdaBoolean) modelNode).setValue(value);
        }
        else if (modelNode instanceof BdaVisibleString) {
            ((BdaVisibleString) modelNode).setValue(valueString);
        }
        else if (modelNode instanceof BdaTimestamp) {
            ((BdaTimestamp) modelNode).setInstant(Instant.ofEpochMilli(Long.parseLong(valueString)));
        }
        else {
            throw new IllegalArgumentException();
        }
    }

    public boolean isTimeSet() {
        // TODO: actually check in test device, otherwise scheduling tests will all fail!
        log.warn("MAKE SURE THAT THE DATE/TIME IS SET PROPERLY; OTHERWISE THESE TESTS WILL NOT WORK");
        return true;
    }

    public ServerModel getCachedServerModel() {
        return serverModel;
    }

    /**
     * Reads the active schedule reference from the schedule controller
     */
    public String readActiveSchedule(String scheduleController) throws ServiceError, IOException {
        BdaVisibleString activeSchedule = (BdaVisibleString) serverModel.findModelNode(
                scheduleController + ".ActSchdRef.stVal", null);
        association.getDataValues((FcModelNode) activeSchedule);
        String activeScheduleString = activeSchedule.getValueString();
        log.debug("Got active Schedule {} at {} UTC", activeScheduleString, Instant.now());
        return activeScheduleString;
    }

    public void disableSchedules(String scheduleNames) throws ServiceError, IOException {
        BasicDataAttribute disableOp = findAndAssignValue(scheduleNames + ".DsaReq.Oper.ctlVal", Fc.CO, "true");
        operate((FcModelNode) disableOp.getParent().getParent());
    }

    public boolean nodeExists(String nodeName) {
        ModelNode node = getNode(nodeName);
        // node is null if it does not exist
        return node != null;
    }

    public ModelNode getNode(String nodeName) {
        return serverModel.findModelNode(nodeName, null);
    }
}
