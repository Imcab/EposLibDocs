# Device information

Everything worth checking on a drive before moving it: how the master's boot of the node went, its identity, whether PDOs flow and both ends agree on their layout, the state, active and past faults, supply, temperature and motor data. Read-only: it never enables the motor or changes a parameter - the right first program for a new drive.

## Key points

- `GetBootStatus()` after `WaitUntilReady()`: a node whose configuration download failed still answers SDO.
- `CheckPdoMapping()` turns a silent layout mismatch into a list of differences.
- `RefreshAll()` reads several signals concurrently; `IsAllGood()` checks them all.
- `Configurator::Refresh(MotorConfigs&)` reads the motor data, including the rated torque the drive computes.

## Running it

```bash
ros2 run eposlib_examples device_info \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2
```

## Source

```cpp title="examples/src/device_info.cpp"
--8<-- "src/device_info.cpp"
```

## Output against epos4_sim

```text
boot     : ok ('L' NMT slave was initially operational.)
identity : EPOS4 Module/Compact 50/15, firmware EPOS4_0170h_6552h_0000h_0000h, serial 12345678
pdo      : active
mapping  : drive and master agree
state    : Switch on disabled
mode     : None
history  : 0x8220 0x8220 0x8220 0x8220 0x8220
supply   : 48.0 V
stage    : 35.0 degC
current  : 0.050 A
nominal  : 15.000 A
Kt       : 0.00 mNm/A
rated    : 0.0000 Nm
position : 0 qc
velocity : 0 rpm
```
