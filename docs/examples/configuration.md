# Configuration

Read a drive's entire configuration, change three fields, read them back, and save. Only the fields that are set are written - the gains and motor data read at the start are untouched.

## Key points

- `Refresh(Epos4Configuration&)` keeps going past objects absent on this hardware.
- `Apply()` validates first and refuses «Power Disable» objects while powered.
- `Save()` makes the change survive a power cycle; without `--save` it does not.

## Running it

```bash
ros2 run eposlib_examples configuration \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2 [--save]
```

## Source

```cpp title="examples/src/configuration.cpp"
--8<-- "src/configuration.cpp"
```

## Output against epos4_sim

```text
before:
  limits.maxMotorSpeed         50000 rpm
  motionProfile.profileAcceleration 10000 rpm/s
  velocityControl.p            20000 
  velocityControl.i            500000 
  motor.nominalCurrent         15000 mA
after:
  limits.maxMotorSpeed         4000 rpm
  motionProfile.profileAcceleration 3000 rpm/s
save: ok
```
