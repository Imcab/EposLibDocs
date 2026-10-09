# Requirements

## Drives

EposLib drives **maxon EPOS4** positioning controllers over **CANopen**. The object
indices, bit layouts and error tables are transcribed from the *EPOS4 Firmware
Specification, edition 2026-07* and the *EPOS4 Communication Guide, edition 2026-04*.

| | |
|---|---|
| Controller family | EPOS4 (Module, Compact, Disk, Micro variants) |
| Fieldbus | CANopen, CiA 301 / CiA 402 |
| Reference device | EPOS4 Module/Compact 50/15, the EDS shipped in the repository |
| Firmware tested | `0x0170` (`EPOS4_0170h_6552h_0000h_0000h`) |

Some objects only exist on some variants (the high-speed outputs, the brake voltages, the
STO card) or on newer firmware (the halt option code, `0x605D`). The library marks those
fields as hardware- or firmware-dependent: a `Refresh()` leaves them unset when absent
instead of failing, and the page of each feature says where it applies. The hardware codes
the library recognises are listed in [Hardware](../reference/hardware.md).

!!! note "Other CiA 402 drives"
    The state machine, the operating modes and everything under `od::cia402` are defined by
    the device profile, not by maxon. Configuration groups, error tables and anything under
    `od::maxon` are EPOS4-specific.

## CAN interface

Any adapter with a **SocketCAN** driver on Linux works: the library opens the interface by
name (`can0`, `can1`, `vcan0`). Common choices:

- USB adapters: PEAK PCAN-USB, Kvaser Leaf, CANable (candleLight firmware), Innomaker USB2CAN.
- On-board controllers: NVIDIA Jetson (Orin, Xavier) have a CAN controller exposed as `can0`
  once its transceiver is wired and the `mttcan` driver is loaded.
- `vcan`, the kernel's virtual CAN, for [simulation](simulation.md).

The example networks run at **1 Mbit/s**, the EPOS4 default.

## Software

| | Version |
|---|---|
| OS | Linux (SocketCAN). Ubuntu 22.04 is the reference. |
| Compiler | C++17 (GCC 9+ or Clang 10+) |
| CMake | 3.18 or newer |
| Lely CANopen | the libraries and `dcfgen`; `ros-humble-lely-core-libraries` provides both |
| ros2units | header-only units, [Imcab/ros2units](https://github.com/Imcab/ros2units) |
| GoogleTest | only to build the tests |
| ROS 2 | optional. Humble is the reference; EposLib does not use ROS, it only builds under `colcon`. |

!!! warning "Mixing ROS distributions"
    ROS 2 distributions do not talk to each other reliably. If your team runs **Humble** and
    your laptop runs Jazzy, build and run inside a Humble container -
    [Installation](installation.md#docker) shows one - rather than mixing the two on one
    network.
