# Profile position

Point-to-point moves with the drive generating the trapezoidal ramp: an absolute move in quadcounts, a relative move given as an angle at the output with its own speed, and back to the start.

## Key points

- The profile is configured once with `MotionProfileConfigs`; requests only override what differs.
- `SetMechanism()` makes `WithPosition(90_deg)` mean 90 degrees at the gearbox output.
- `SetControl()` returns once the drive has accepted the target; waiting for *target reached* is separate.
- *Target reached* can still be set from the previous move: wait for it to drop first.

## Running it

```bash
ros2 run eposlib_examples profile_position \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2
```

## Source

```cpp title="examples/src/profile_position.cpp"
--8<-- "src/profile_position.cpp"
```

## Output against epos4_sim

```text
move 1 done at 10000 qc
move 2 done at 60000 qc
back at 0 qc
```
