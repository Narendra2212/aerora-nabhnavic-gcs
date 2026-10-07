# 50 — Hardware/Software Interface Baseline

## Primary FC

`STM32H743VITx` is the primary MCU observed in the schematic and is also the target device in the Simulink model.

## Secondary safety/security MCU

`STM32G071CBTx` is present in the schematic and is associated with the watchdog interface. Its exact safety/security partition must be formally defined.

## Sensors

Observed:
- ICM-42688-P
- ICM-20948
- two BMP390L references
- GPS interface
- optical flow interface
- LiDAR interface
- temperature/current sensing

## Communications

Observed hardware/interface evidence:
- E22-900M22S LoRa module
- RC receiver UART interface
- companion UART interface
- CAN expansion
- USB-C

## Storage/security

Observed:
- MicroSD
- W25Q128JVE flash
- ATECC608A secure element

## Power monitoring

Observed:
- INA226 current/power monitoring
- switching regulators
- multiple regulated rails
- dual tether/battery input connectors
- tether operating input is specified as 480 V DC at the product level

## Actuation

The PCB exposes six motor signal labels `M1_SIG` through `M6_SIG`, consistent with the current hexacopter architecture.

The Simulink model contains a hexacopter mixer and PWM output structures, plus payload actuator structures.

## GPU/companion interface

The schematic exposes companion UART and power/I/O. The firmware model receives GPU/autonomy commands through a MAVLink subsystem. The exact GPU module, serial protocol configuration, baud rate and physical connector assignment must be frozen in the interface control document before implementation.

## GCS impact

The GCS shall represent the above hardware as capabilities and interfaces rather than hard-coded assumptions so future Aerora products can reuse the platform.
