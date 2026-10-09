# Troubleshooting

Find the symptom, follow the link.

| Symptom | Likely cause | Page |
|---|---|---|
| `CanBus::Start()` throws | interface missing or down, DCF path wrong | [CAN bus](can-bus.md#start-throws) |
| `WaitUntilReady()` returns false | no answer: wiring, termination, bit rate, node-ID, power | [CAN bus](can-bus.md#no-answer) |
| `ip link` shows ERROR-PASSIVE or BUS-OFF | frames not acknowledged | [CAN bus](can-bus.md#error-passive-or-bus-off) |
| `error: SDO abort code 08000000 ... object 1000` once at start | the first boot attempt raced the node's reset | [Boot and PDOs](boot-and-pdo.md#1000-general-error) |
| `SDO abort code 06010000 received while updating the configuration` | the master's configuration download was refused | [Boot and PDOs](boot-and-pdo.md#cob-ids) |
| `IsPdoActive()` stays false, "no PDOs" | configuration download failed, or COB-IDs differ | [Boot and PDOs](boot-and-pdo.md) |
| `CheckPdoMapping()` lists differences | the drive kept its own mapping | [Boot and PDOs](boot-and-pdo.md#cob-ids) |
| Values read back plausible but wrong | the two ends decode PDOs differently | [Boot and PDOs](boot-and-pdo.md#plausible-but-wrong-values) |
| Fault `0x8130` every time a program starts | the previous program stopped the master's heartbeat | [Common faults](faults.md#0x8130) |
| `Enable()` returns false | the drive is in Fault, or never leaves a state | [Common faults](faults.md#enable-fails) |
| Fault `0x3220` under load | supply voltage sags | [Common faults](faults.md#0x3220) |
| Fault `0x8611` | following error | [Common faults](faults.md#0x8611) |
| `SetControl()` returns `invalid_argument` | a quantity without `SetMechanism()` | [FAQ](faq.md#invalid-argument) |
| Torque in N m does nothing / `TorqueToPerThousand` is empty | rated torque is 0: motor data missing | [FAQ](faq.md#rated-torque-zero) |
| The motor keeps turning after the program ends | torque mode coasts; `Disable()` removes power | [FAQ](faq.md#keeps-turning) |
| Velocity does not follow the torque ramp | in torque mode velocity is physics, not a command | [FAQ](faq.md#torque-ramp) |
| A move or homing run after `Halt()` never starts | known issue in 0.1 | [Enabling and stopping](../api/enabling.md#halt) |

## Tools

- **`candump -tz can0`** - every frame on the bus, timestamped. The first thing to look at.
- **[device information](../examples/device-info.md)** - boot status, identity, PDO check,
  state, faults, supply, motor data, in one run.
- **`motor.DescribeLastError()`** - the active fault with its cause and recovery.
- **`motor.GetErrorHistory()`** - the faults before the current one.
- **`ip -details -statistics link show can0`** - controller state and error counters.
