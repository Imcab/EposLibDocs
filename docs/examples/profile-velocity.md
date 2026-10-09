# Profile velocity

Spin at a velocity, with the drive ramping up and down. The ramp time is velocity / acceleration. `epos4_sim` does not model Profile Velocity Mode, so this example is only compiled; run it on hardware.

## Key points

- There is no handshake in PVM: the target velocity takes effect as soon as it arrives.
- `IsAtZeroSpeed()` is Statusword bit 12 under PVM.
- Ctrl+C stops the loop and ramps back to zero before disabling.

## Step by step

1. **Enable**, then **`SetControl(ProfileVelocity)`**: mode 3, the acceleration and deceleration overrides, and the target velocity into RPDO2. There is no handshake in PVM.
2. **Monitor** velocity and averaged current every 200 ms - the current shows what holding that speed costs.
3. **Stop on the ramp:** a target of 0, then wait for *speed is zero* (Statusword bit 12 in PVM).
4. **Disable.**

The ramp takes `rpm / acceleration` seconds each way. See [Motion profiles](../epos4/motion-profiles.md).

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
