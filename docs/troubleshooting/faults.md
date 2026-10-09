# Common faults

`motor.DescribeLastError()` prints the cause and recovery of any fault from the manual; all
73 are in [Device error codes](../reference/error-codes.md). These are the ones you will
meet first.

## 0x8130 CAN heartbeat error { #0x8130 }

The drive stopped receiving the master's heartbeat for longer than its consumer time
(300 ms in the example network).

- **At every program start**: expected. The previous program's master stopped its heartbeat
  when it exited. Clear it when your program starts - `ClearFault()` sends the NMT reset
  communication this fault requires. See
  [SYNC and heartbeat](../network/sync-heartbeat.md#after-every-program-exits).
- **While running**: the master stalled or died - a crashed program, a frozen thread, a bus
  problem. The drive did what it is configured to do: stopped the motor.

## 0x8120 CAN passive mode

The drive's CAN controller saw too many errors: wiring, termination, bit rate - see
[CAN bus](can-bus.md#error-passive-or-bus-off). Like `0x8130`, it needs an NMT reset
communication before the fault reset; `ClearFault()` does it.

## 0x3220 Undervoltage { #0x3220 }

The supply dropped below the undervoltage limit. On a battery, under load: the pack sags, or
the wiring is too thin. Watch `GetSupplyVoltage()` while the axis accelerates. A 12 V supply
is close to the EPOS4's minimum; a motor drawing several amps can pull it under.

## 0x3210 Overvoltage

Usually regenerative braking: a decelerating motor feeds energy back into the supply, and a
supply that cannot absorb it - a lab supply, a full battery behind a diode - rises. Gentler
deceleration, a shunt regulator, or a supply that can sink current.

## 0x8611 Following error { #0x8611 }

The actual position fell further behind the demanded one than the following error window
(`LimitConfigs::followingErrorWindow`). Causes: a trajectory steeper than the axis can
follow (too much acceleration or speed for the load), an obstruction, gains too low, or the
current limit reached. In CSP, steps in the setpoint are the usual cause - send a smooth
trajectory, and set the interpolation period.

## 0x4210 Thermal overload

The power stage exceeded its temperature limit. Compare `GetPowerStageTemperature()` with
`GetPowerStageTemperatureLimit()` during operation; improve cooling or reduce the load.

## 0x2310 Overcurrent, 0x2320 Power stage protection

Current above what the power stage allows: a short in the motor winding, gains too high, or
very aggressive acceleration and deceleration. Check the motor wiring, then the current
controller gains.

## 0x7388 Hall sensor error

An impossible Hall pattern: wiring, or a Hall sensor configured on a motor that has none.
`GetEncoder().GetHallPattern()` shows the live pattern.

## Limit switch errors

A limit switch reached outside homing raises a limit error: that is its job. If it fires
when the axis is nowhere near the end stop, check the polarity (`DigitalInputConfigs::polarity`)
and the wiring with `GetDigitalInputPins()`.

## Enable() returns false { #enable-fails }

| Situation | What to do |
|---|---|
| `IsFaulted()` | read `DescribeLastError()`, fix the cause, `ClearFault()`, `Enable()` again |
| the state stays in Switch on disabled | something holds it there: STO inputs not energised, a `kDriveEnable` input mapped and inactive, the bus not running |
| `GetBootStatus()` not ok | the node was not configured; see [Boot and PDOs](boot-and-pdo.md) |

`Enable()` never resets a fault by itself - see [The CiA 402 state machine](../concepts/state-machine.md#faults).
