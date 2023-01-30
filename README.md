# DER-scheduling

This project aims to create an Open source scheduling stack for distributed energy resources (DER) control according to IEC 61850.
The main functionalities will be to control a DERs regarding:
- absolute power output
- maximal power output
- turning them on or off

This project consists of:
 - a set of [requirements](REQUIREMENTS.md)
 - a C implementation of IEC61850 scheduling based upon MZ Automations [IEC 61850 Protocol Library](https://www.mz-automation.de/communication-protocols/iec-61850-protocol-library/)
 - a [system overview](images/system-overview.drawio.png) of the C implementation
 - a set of integration / conformance tests written in java, see the [conformace test readme](conformance-tests/README.md)

TODO: during merge with develop_mz branch, include the [building section](https://github.com/alliander-opensource/der-scheduling/tree/develop_mz#building)

This is a collaboration of [Alliander](alliander.com), [Fraunhofer ISE](https://www.ise.fraunhofer.de/) and [MZ Automations](https://www.mz-automation.de). 
