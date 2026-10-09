# FAQ

## Why does SetControl() return invalid_argument? { #invalid-argument }

The request uses a quantity - `90_deg`, `15_rpm` - and the device has no mechanism. Call
`SetMechanism(quadCountsPerRevolution, gearRatio)` first, or use raw units. The library
refuses rather than guess a resolution. The same for `StageTarget*()`, which returns false.

## Why does a torque in N m do nothing? { #rated-torque-zero }

Every torque is in thousandths of «Motor rated torque» (`0x6076`), which the drive computes
from the motor data: nominal current × torque constant. If the motor data was never
configured it is **0**, and there is no defined conversion - `TorqueToPerThousand()` returns
`nullopt` and `StageTargetTorque(0.5_Nm)` returns false. Check
`GetMotorRatedTorque().Refresh().GetValue()`; configure `MotorConfigs` (or run EPOS Studio's
startup wizard) and `Save()`.

## What does "per mille" mean? Where does the rated torque come from?

Thousandths (‰), not percent. 1000 ‰ is the motor's rated torque - the torque it can deliver
continuously without overheating - computed by the drive from the motor data and reported in
`0x6076`. With 9.28 A × 104.79 mN·m/A, 1000 ‰ = 0.9724 N·m, so 100 ‰ = 0.097 N·m. It is the
torque at the **motor shaft**: multiply by the gear ratio for the output. See [Units](../concepts/units.md).

## Why does the motor keep turning after the program ends? { #keeps-turning }

In torque mode, **zero torque is not a brake**: the motor stops pushing and coasts. Then
`Disable()` removes power, and an unpowered motor turns freely. To stop it: switch to CSV
with a zero target and wait for standstill before disabling - the drive's velocity loop
brakes actively - or `QuickStop()`. On a joint that carries a load, configure a holding
brake. See [Cyclic control](../api/cyclic-control.md#torque).

## Why doesn't the velocity follow my torque ramp? { #torque-ramp }

Because in torque mode nobody controls velocity: acceleration = (motor torque - load torque)
/ inertia. A constant torque on a free shaft keeps accelerating until the motor reaches its
no-load speed (speed constant × supply voltage), where the torque collapses. The velocity
keeps rising after the torque ramp ends and keeps going after it returns to zero. For a
velocity ramp, use a velocity mode: Profile Velocity (the drive ramps) or CSV (your program
ramps). See the [cyclic velocity example](../examples/cyclic-velocity.md).

## What should --vel-max / the velocity limit be on a free motor?

Below the motor's no-load speed, which on maxon data sheets is the speed constant × supply
voltage: with 104.79 mN·m/A, the speed constant is 30000 / (π × 104.79) ≈ 91 rpm/V, so about
1060 rpm at 11.7 V. Anything above that can never trip. On a free shaft a constant torque
cannot be held anyway - block the output to measure torque.

## Can I use EposLib without ROS?

Yes. It is plain CMake; ROS is only one way to build it. See
[Installation](../getting-started/installation.md#plain-cmake-without-ros).

## Can I run my laptop on Jazzy and the robot on Humble?

Build and run EposLib in a Humble container (or on the robot). Distributions do not
interoperate reliably on one network. EposLib itself does not use ROS, but your nodes do.

## Do I need PDOs?

For the cyclic modes, yes. For configuration and profiled moves, no: everything falls back
to SDO when the drive sends no PDOs. With PDOs, status reads cost no bus traffic.

## How many drives can one bus carry?

Up to 127 node-IDs; in practice the limit is bus load. At 100 Hz with four PDOs per drive,
about eight drives reach ~40 % load. See [Multiple drives](../network/multi-drive.md#bus-load).

## How fast can the control loop run?

As fast as SYNC: setpoints leave on SYNC. 100 Hz is the example; 200-500 Hz is possible on a
small bus. Faster SYNC means more load for every drive.

## Is it real-time safe?

The cyclic path is: staging setpoints and reading cached feedback are lock-free atomic
operations, with no allocation and no system call. Everything else blocks on the bus and
belongs outside the control loop.

## Can I change the PDO mapping from code?

No, on purpose: the mapping lives in `bus.yml`, so the master and the drive are configured
from the same description. Remapping the drive over SDO would leave the master decoding the
old layout. Edit `bus.yml` and rebuild.

## Does it work with other CiA 402 drives?

The state machine, the operating modes and `od::cia402` are standard. The configuration
groups, the error tables and the maxon objects are EPOS4-specific, and the library is only
tested against the EPOS4.
