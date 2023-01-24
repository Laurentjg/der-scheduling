# REQUIREMENTS

Basic schedule funtionality shall be supported, as depicted below:

![Scheduling-components-overview](images/61850-90-7_6.3.2_Schedules_relevant_parts.png)

# Configuration

[C01]: Basic configuration of DER default shall be stored inside a SCL File

[C01-a]: Reserve Schedules shall be stored inside a SCL File

[C01-b]: Number of available schedules (maximum 10 schedules per logical Node, less can be configured) shall be stored inside a SCL File

[C01-c]: Number of devices to be controlled (limited to 1 for evaluation, extension shall be foreseen) shall be stored inside a SCL File

[C02]: Parameters of schedules other than the reserve schedule are to be stored in a key-value manner

[C03]: Parameters of schedules other than the reserve schedule are persisted to a separate file (e.g. as JSON)

[C04]: Changes in schedules are persisted into the file as soon as the schedule is validated

[C05]: A interface to CoMPAS is not required.

[C05-a]: A simple interface to notify about changes in SCL file is to be defined.

[C05-b]: A simple interface to notify about changes in key-value

# Logical Nodes
[LN01]: Map Logical Nodes and Common Data Classes to configuration values, allow reading and writing according to 61850-90-10

[LN02]: The control functions of the DER are modeled and implemented independently

[LN02-a]: Absolute active power is controlled in 10 logical nodes ActPow_FSCH01 .. ActPow_FSCH10, with schedule controller ActPow_FSCC1 and actuator interface ActPow_GGIO1

[LN02-b]: Maximum power is controlled in 10 logical nodes MaxPow_FSCH01 .. MaxPow_FSCH10, with schedule controller MaxPow_FSCC1 and actuator interface MaxPow_GGIO1

[LN02-c]: On/Off state of the DER is controlled 10 logical nodes OnOff_FSCH01 .. OnOff_FSCH10, with schedule controller OnOff_FSCC1 and actuator interface OnOff_GGIO1

[LN03]: The scheduling nodes and their children are to be modeled according to 61850-90-10 

# Scheduling
[S01]: A set of 10 schedules per logical node must be supported

[S02]: Support time based schedules

[S02-a]: All dates are in UTC

[S03]: Triggers other than time based may be supported

[S04]: Periodical schedules must be supported

[S05a]: Support of absolute schedules of absolute power setpoints

[S05b]: Support of absolute schedules of maximum power setpoints

[S05c]: Support of schedules to turn DER on and off

[S08]: Validation of Schedules according to 61850-90-10

[S09]: Schedules for maximum generation values and absolute power values are modeled in the same way (using the same DTO)

[S10]: Each schedule must store a sequence of max. 100 values

[S11]: There are three Reserve Schedules: Active Power Reserve Schedule, Maximum Power Reserve Schedule and On/Off Reserve Schedule

[S12]: Each Reserve Schedule holds 100 values

[S13]: Reserve Schedules are always active and cannot be deactivated

[S14]: Reserve Schedules have the lowest priority (10). This cannot be changed.

[S15]: Reserve Schedules have a fixed start date of 01.01.1970 00:00:01 (UTC). This cannot be changed.

[S16]: Reserve Schedules are set to cyclic execution. This cannot be changed.

[S17]: If two schedules are configured with the same pirority and start time, the schedule that comes first in the schedule controller's schedule reference is executed.

# Schedule execution
[E01]: Execution of schedules must regard schedule priority according to 61850-10

[E02]: Resolution time base: >= 1 second

# Interfaces

[I01]: The Active power Actuator Interface contains the current value (a double) and the source schedule priority (an integer)

[I02]: The Maximum Power Actuator Interface contains the current value (a double) and the source schedule priority (an integer)

[I03]: The On/Off Actuator Interface contains the current value (a double) and the source schedule priority (an integer). The mapping is 0 - off / 1 - on.
