# Profile position

Point-to-point moves with the drive generating the trapezoidal ramp: an absolute move in quadcounts, a relative move given as an angle at the output with its own speed, and back to the start.

## Key points

- The profile is configured once with `MotionProfileConfigs`; requests only override what differs.
- `SetMechanism()` makes `WithPosition(90_deg)` mean 90 degrees at the gearbox output.
- `SetControl()` returns once the drive has accepted the target; waiting for *target reached* is separate.
- *Target reached* can still be set from the previous move: wait for it to drop first.

## Step by step

1. **Mechanism before start.** `SetMechanism(2000, 1/100)` makes quantities convertible; it touches nothing on the bus.
2. **Profile once.** `Apply(MotionProfileConfigs)` writes `0x6081`, `0x6083`, `0x6084`; every later move inherits them.
3. **Enable** walks the state machine to *Operation enabled* (three Controlword commands).
4. **Move 1** - `SetControl(ProfilePosition{}.WithPosition(start + 10000))`: switches to mode 1 if needed, puts the target in RPDO1, and performs the setpoint handshake. It returns when the drive has *accepted* the target.
5. **`WaitForTarget()`** waits for *target reached* to drop (it may still be set from before) and then to rise, watching for a fault meanwhile.
6. **Move 2** is relative and in degrees at the output: 90° through 1:100 is 25 motor turns = 50 000 qc, at a 1500 rpm override (written to `0x6081` before the handshake).
7. **Back to the start, disable.**

On hardware, watch `GetFollowingError()` during the moves: it should stay well inside `LimitConfigs::followingErrorWindow`.

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
