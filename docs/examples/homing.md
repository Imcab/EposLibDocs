# Homing

Give an incremental encoder an absolute zero: map the switches to their inputs, configure the homing speeds, home on the negative limit switch, then declare a position without moving with `SetPosition()`.

## Key points

- `GetSupportedHomingMethods()` lists what the firmware implements.
- `Home()` refuses a method whose switch is not mapped, before anything moves.
- `IsPositionReferenced()` confirms the reference.
- `SetPosition(1000)` uses homing method 37 and restores the homing configuration afterwards.

## Running it

```bash
ros2 run eposlib_examples homing \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2
```

## Source

```cpp title="examples/src/homing.cpp"
--8<-- "src/homing.cpp"
```

## Output against epos4_sim

```text
supported: 37 34 33 27 23 18 17 11 7 2 1 -1 -2 -3 -4
negative limit active: no
homing attained, position 0 qc, referenced: yes
position is now 1000 qc
```
