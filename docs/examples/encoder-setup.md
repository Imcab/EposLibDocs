# Encoder setup

Inspect the feedback configuration: which sensor is in each slot, the encoder's pulses per revolution, the resolution, and the position in degrees at the output. Read-only. The simulator does not compute the main sensor resolution, so the run below falls back to computing it from the encoder pulses - the same fallback that is useful on hardware.

## Key points

- `Encoder::Refresh()` reads each configuration group in any power state.
- `QuadCountsPerRevolution()`: 500 pulses are 2000 quadcounts.
- `SetMechanismFromDevice()` takes the resolution from the drive; only the gear ratio is yours.

## Running it

```bash
ros2 run eposlib_examples encoder_setup \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2 [gear-ratio]
```

## Source

```cpp title="examples/src/encoder_setup.cpp"
--8<-- "src/encoder_setup.cpp"
```

## Output against epos4_sim

```text
sensor 1: 0x01  sensor 2: 0x00  sensor 3: 0x10
main sensor resolution: 0 qc/rev
encoder 1: 500 pulses/rev = 2000 qc/rev
resolution from the encoder pulses: 2000 qc/rev
output angle: 0.000 deg
output velocity: 0.000 rpm
```
