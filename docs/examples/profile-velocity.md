# Profile velocity

Spin at a velocity, with the drive ramping up and down. The ramp time is velocity / acceleration. `epos4_sim` does not model Profile Velocity Mode, so this example is only compiled; run it on hardware.

## Key points

- There is no handshake in PVM: the target velocity takes effect as soon as it arrives.
- `IsAtZeroSpeed()` is Statusword bit 12 under PVM.
- Ctrl+C stops the loop and ramps back to zero before disabling.

## Running it

```bash
ros2 run eposlib_examples profile_velocity \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2 [rpm] [seconds]
```

## Source

```cpp title="examples/src/profile_velocity.cpp"
--8<-- "src/profile_velocity.cpp"
```

## Output

Not run in simulation: `epos4_sim` does not model Profile Velocity Mode.
