# Device information

Everything worth checking on a drive before moving it: how the master's boot of the node went, its identity, whether PDOs flow and both ends agree on their layout, the state, active and past faults, supply, temperature and motor data. Read-only: it never enables the motor or changes a parameter - the right first program for a new drive.

## Key points

- `GetBootStatus()` after `WaitUntilReady()`: a node whose configuration download failed still answers SDO.
- `CheckPdoMapping()` turns a silent layout mismatch into a list of differences.
- `RefreshAll()` reads several signals concurrently; `IsAllGood()` checks them all.
- `Configurator::Refresh(MotorConfigs&)` reads the motor data, including the rated torque the drive computes.

## Step by step

1. **Bus and device.** `CanBus` gets the interface, the master DCF and the master's node-ID; the `Epos4` is declared before `Start()` so the master routes its PDOs.
2. **`WaitUntilReady()`** blocks until the node answers an SDO request (5 s at most).
3. **Boot status.** The boot completes shortly after the first SDO answer, so the program polls `GetBootStatus().count` for up to 2 s. `'L'` (*already operational*) is advisory; any other letter means the configuration download did not complete.
4. **Identity** - four SDO reads of `0x1018`, plus `0x1000`, `0x1008`, `0x2100`, `0x1F56`, `0x1F57` - rendered with the hardware name and the firmware file name.
5. **PDOs.** `IsPdoActive()` turns true on the first TPDO; `CheckPdoMapping()` reads the eight PDO channels over SDO and compares them with the master's concise DCF.
6. **State, faults, history.** `IsFaulted()` triggers `DescribeLastError()`; the history (`0x1003`) survives a fault reset.
7. **Power.** `RefreshAll()` reads supply, temperature and current concurrently.
8. **Motor data.** `Refresh(MotorConfigs&)` reads `0x6402`, `0x3001`, `0x3002` and the computed rated torque `0x6076`.

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
