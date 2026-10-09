# Telemetry logger

Record a drive's health - supply, temperature, current, I2t, PWM duty cycle, state, last EMCY - to CSV at a fixed rate. What a rover logs on every drive during a run, to know afterwards why a joint stopped. Read-only.

## Key points

- One `RefreshAll()` per row: every value of the row comes from the same moment.
- A value whose read failed is left empty, never filled with a stale number.
- `fflush` after each row: the file survives a crash or a pulled battery.

## Step by step

1. **Seven signals** refreshed together each row with `RefreshAll()`.
2. **A failed read is an empty field**, so a gap in the log is visible as a gap.
3. **The last EMCY** comes from the lock-free cache, which still answers when the node has gone silent.
4. **`fflush` every row.**

## Running it

```bash
ros2 run eposlib_examples telemetry_logger \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2 run.csv 60 5
```

## Source

```cpp title="examples/src/telemetry_logger.cpp"
--8<-- "src/telemetry_logger.cpp"
```

## Output against epos4_sim (run.csv)

```text
t,supply_V,stage_degC,current_A,i2t_motor_pct,i2t_stage_pct,pwm_permille,state,last_emcy
0.00,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
0.20,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
0.40,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
0.60,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
0.80,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
1.00,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
1.20,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
1.40,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
1.60,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
1.80,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
2.00,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
2.20,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
2.40,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
2.60,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
2.80,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
3.00,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
3.20,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
3.40,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
3.60,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
3.80,48.000000,35.000000,0.050000,0,0,0,Switch on disabled,0x8220
```
