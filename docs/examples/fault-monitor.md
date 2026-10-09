# Fault monitor

React to faults as they happen: an emergency callback flags the main loop, which polls telemetry at 2 Hz, describes any fault with its cause and recovery, and clears it. The loop a base station status panel runs. (The `0x8220` in the run below is an EMCY the simulator sends at boot.)

## Key points

- The callback runs on the CANopen thread: it only sets an atomic flag.
- `IsWarning()` filters codes the drive keeps running through.
- `ClearsPosition()` says when homing is lost; `ClearFault()` sends the NMT reset a lost heartbeat needs.

## Running it

```bash
ros2 run eposlib_examples fault_monitor \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2 [seconds]
```

## Source

```cpp title="examples/src/fault_monitor.cpp"
--8<-- "src/fault_monitor.cpp"
```

## Output against epos4_sim

```text
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
EMCY: 0x8220 unknown error (not in the chapter 7 table; newer firmware?)
  emcy reg : generic, communication
48.0 V  35.0 degC  I2t 0 %  last EMCY 0x8220
0x0000 No Error
  register : no error flags
  reaction : none
  live reg : generic, communication
clearing... ok
48.0 V  35.0 degC  I2t 0 %  last EMCY 0x8220
48.0 V  35.0 degC  I2t 0 %  last EMCY 0x8220
48.0 V  35.0 degC  I2t 0 %  last EMCY 0x8220
48.0 V  35.0 degC  I2t 0 %  last EMCY 0x8220
48.0 V  35.0 degC  I2t 0 %  last EMCY 0x8220
```
