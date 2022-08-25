# DER-scheduling

## Introduction

This project aims to create an Open source scheduling stack for distributed energy resources (DER) control according to IEC 61850-90-10.

The main functionalities will be to control a DERs regarding:
- absolute power output
- maximal power output
- turning them on or off

Currently, the project is in planning phase collecting [requirements](https://github.com/alliander-opensource/der-scheduling/blob/main/REQUIREMENTS.md) and creating a [system overview](https://raw.githubusercontent.com/alliander-opensource/der-scheduling/main/images/system-overview.drawio.png).


This is a collaboration of [Alliander](alliander.com), [Fraunhofer ISE](https://www.ise.fraunhofer.de/) and [MZ Automation](https://www.mz-automation.de). 

The work is based upon MZ Automation's [IEC 61850 Protocol Library](https://www.mz-automation.de/communication-protocols/iec-61850-protocol-library/).

## Building

Building this code requires a C toolchain and CMake installed.

The C code is developed and tested with Ubuntu 20.04. The following instructions are assuming that you are using a similar Linux environment. Otherwise the required steps might differ.

1. First you have to download, build, and install the latest version of [libiec61850](https://github.com/mz-automation/libiec61850) (1.5.2).

You can skip this step when libiec61850 is already installed on your PC

.. code-block:: console

  $ git clone git@github.com:mz-automation/libiec61850.git
  $ cd libiec61850
  $ mkdir build
  $ cd build
  $ cmake ..
  $ make
  $ sudo make install

2. Building the scheduler code

.. code-block:: console

  $ mkdir build
  $ cd build
  $ cmake ..
  $ make




