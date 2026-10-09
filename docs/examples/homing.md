# Homing

Give an incremental encoder an absolute zero: map the switches to their inputs, configure the homing speeds, home on the negative limit switch, then declare a position without moving with `SetPosition()`.

## Key points

- `GetSupportedHomingMethods()` lists what the firmware implements.
- `Home()` refuses a method whose switch is not mapped, before anything moves.
- `IsPositionReferenced()` confirms the reference.
- `SetPosition(1000)` uses homing method 37 and restores the homing configuration afterwards.

## Step by step

1. **Supported methods** from `0x60E3`.
2. **Map the switches** (`DigitalInputConfigs`): without a mapped switch `Home()` refuses the method rather than search blindly.
3. **Configure the run** (`HomingConfigs`): search speeds, acceleration, offset move, home position.
4. **Polarity check:** a limit switch already active before the run usually means an inverted polarity.
5. **`Home()`** blocks until *homing attained* + *target reached*, or *homing error*, or the timeout.
6. **`SetPosition(1000)`** declares the current position to be 1000 qc without moving, through method 37, and restores the homing configuration.

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
