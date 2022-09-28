# Alliander rfg+ conformance tests

This project is aiming for testing the scheduling functions of an existing implementation against
the [requirements](https://github.com/alliander-opensource/der-scheduling/blob/main/REQUIREMENTS.md).

To link
the [tests](https://gitlab.cc-asp.fraunhofer.de/sgc_industry/alliander-rfg-conformance-tests/-/blob/main/src/test/java/org/openmuc/fnn/steuerbox/)
to their requirements, the requirements are being copied
into [a java class](https://gitlab.cc-asp.fraunhofer.de/sgc_industry/alliander-rfg-conformance-tests/-/blob/main/src/main/java/org/openmuc/fnn/steuerbox/models/Requirements.java)
such that they can be referenced in the javadoc of the tests. A java IDE (
e.g. [intelliJ](https://www.jetbrains.com/idea/)) will render this in a way that references can be followed
bidirectionally.

## Running the tests

The tests will later on run with JUnit, a common testing framework for java. They can be started by
calling `./gradlew test` for linux systems or `gradlew.bat test` in Windows.

# Test setup

Tested runtime environment is openjdk 11. The test assumes the test device is running with ip 192.168.16.200 and
accessible on port

102. [This is currently hard coded.](https://gitlab.cc-asp.fraunhofer.de/sgc_industry/alliander-rfg-conformance-tests/-/blob/93ff7732ccc7006702e81218b3b275a2ba7a9994/src/main/java/org/openmuc/fnn/steuerbox/models/AllianderDER.java)

## Test results

[Allure](https://github.com/allure-framework) will be used to create readable test reports with more information.

Also, the testResults folder contains the logs of all tests, these might be useful for debugging.

## Getting started

For a first impression, it probably makes sense to have a look in the very
incomplete [requirements](https://gitlab.cc-asp.fraunhofer.de/sgc_industry/alliander-rfg-conformance-tests/-/blob/main/src/main/java/org/openmuc/fnn/steuerbox/models/Requirements.java)
and the general IEC
61850 [SchedulingTests](https://gitlab.cc-asp.fraunhofer.de/sgc_industry/alliander-rfg-conformance-tests/-/blob/main/src/test/java/org/openmuc/fnn/steuerbox/SchedulingTest.java)
and more
specific [AllianderTests](https://gitlab.cc-asp.fraunhofer.de/sgc_industry/alliander-rfg-conformance-tests/-/blob/main/src/test/java/org/openmuc/fnn/steuerbox/AllianderTests.java)
.
