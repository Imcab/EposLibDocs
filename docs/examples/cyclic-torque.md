# Cyclic torque

A torque step through Cyclic Synchronous Torque, given in newton metres, with a velocity limit that drops the torque to zero. The run below used a simulator whose rated torque was set to 0.45 N m; 0.05 N m is 111 thousandths of it.

## Key points

- The rated torque is checked first: if it is 0 the motor data is missing and N m has no meaning.
- `EnterCyclicTorqueMode()` starts at the torque the axis already produces, and reads the rated torque once.
- `StageTargetTorque(0.05_Nm)` converts without bus access.
- On a free shaft any torque above friction accelerates the motor until its no-load speed - guard it.

## Step by step

1. **Rated torque first.** Every CST value is relative to `0x6076`; a 0 means the motor data is missing and the program refuses to run.
2. **`EnterCyclicTorqueMode()`** seeds the published torque with the torque the axis produces now and reads the rated torque once.
3. **The loop** stages `0.05_Nm` - converted to thousandths with the rated torque read at entry, no bus access - and checks two things every cycle: the velocity limit and `IsCyclicHealthy()`.
4. **On exit** it stages 0 for five SYNC periods so the zero reaches the drive, then leaves the cyclic mode and disables.

On a free shaft the motor accelerates to its no-load speed; on a blocked one the torque is held. See [Motor and thermal model](../epos4/motor-and-thermal.md).

<figure markdown="span">
  ![Cyclic torque](../assets/plots/step_torque.svg)
  <figcaption>A torque step recorded with step_response against epos4_sim (rated torque 0.45 N m).</figcaption>
</figure>

## Running it

```bash
ros2 run eposlib_examples cyclic_torque \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2 [Nm] [max-rpm]
```

## Source

```cpp title="examples/src/cyclic_torque.cpp"
--8<-- "src/cyclic_torque.cpp"
```

## Output against epos4_sim

```text
rated torque 0.4500 Nm, commanding 0.0500 Nm
  t=0.00s  torque=    0 per mille  velocity=     0 rpm
  t=0.20s  torque=  111 per mille  velocity=   184 rpm
  t=0.40s  torque=  111 per mille  velocity=   218 rpm
  t=0.60s  torque=  111 per mille  velocity=   221 rpm
  t=0.80s  torque=  111 per mille  velocity=   221 rpm
  t=1.00s  torque=  111 per mille  velocity=   221 rpm
  t=1.20s  torque=  111 per mille  velocity=   221 rpm
  t=1.40s  torque=  111 per mille  velocity=   221 rpm
  t=1.60s  torque=  111 per mille  velocity=   221 rpm
  t=1.80s  torque=  111 per mille  velocity=   221 rpm
```
