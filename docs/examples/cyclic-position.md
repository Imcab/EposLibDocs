# Cyclic position

Follow a trajectory computed on the master - a sine wave of 20 degrees around the starting point, at the gearbox output - with the drive interpolating between setpoints. The pattern under any trajectory follower.

## Key points

- `EnterCyclicPositionMode()` seeds the target with the actual position, so the first SYNC holds the axis.
- Angles are staged as `units::angle::degree_t`; `GetMechanism().ToAngle()` converts the feedback back.
- The actual position trails the target by the two-to-three SYNC latency of the cyclic path.

## Step by step

1. **`EnterCyclicPositionMode()`** seeds the target with the actual position, so the first SYNC holds the axis exactly where it is.
2. **The origin** is converted to degrees at the output with `GetMechanism().ToAngle()`.
3. **The loop** stages `origin + 20° sin(2π 0.5 t)` as a `units::angle::degree_t`; `StageTargetPosition()` converts it to quadcounts arithmetically.
4. **Back to the origin** for 300 ms, then exit and disable.

The actual position lags the target by the two-to-three SYNC latency of the cyclic path - visible in the plot below.

<figure markdown="span">
  ![Cyclic position](../assets/plots/step_position.svg)
  <figcaption>The sine, recorded with step_response against epos4_sim.</figcaption>
</figure>

## Running it

```bash
ros2 run eposlib_examples cyclic_position \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2 [amplitude-deg]
```

## Source

```cpp title="examples/src/cyclic_position.cpp"
--8<-- "src/cyclic_position.cpp"
```

## Output against epos4_sim

```text
  t=0.00s  target=   0.00 deg  actual=   0.00 deg
  t=0.25s  target=  14.15 deg  actual=  11.77 deg
  t=0.50s  target=  20.00 deg  actual=  19.52 deg
  t=0.75s  target=  14.14 deg  actual=  16.18 deg
  t=1.00s  target=  -0.00 deg  actual=   3.12 deg
  t=1.25s  target= -14.15 deg  actual= -11.77 deg
  t=1.50s  target= -20.00 deg  actual= -19.52 deg
  t=1.75s  target= -14.14 deg  actual= -16.54 deg
  t=2.00s  target=   0.00 deg  actual=  -3.12 deg
  t=2.25s  target=  14.15 deg  actual=  11.76 deg
  t=2.50s  target=  20.00 deg  actual=  19.65 deg
  t=2.75s  target=  14.14 deg  actual=  16.54 deg
  t=3.00s  target=  -0.00 deg  actual=   3.74 deg
  t=3.25s  target= -14.15 deg  actual= -11.76 deg
  t=3.50s  target= -20.00 deg  actual= -19.52 deg
  t=3.75s  target= -14.14 deg  actual= -16.53 deg
```
