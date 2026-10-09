# Diagnostics and faults

A fault reported as `0x8611` is a dead axis and a long walk. Reported as *Following error -
the difference between demand and actual position exceeded the following error window -
reset fault with Controlword*, it is a diagnosis someone can act on. EposLib carries the
whole of chapter 7 of the firmware manual for that.

## Describing the current fault

```cpp
std::string DescribeLastError();
```

Reads the error code (`0x603F`) and the error register (`0x1001`) and renders them with the
manual's tables:

```text
0x8130 CAN heartbeat error
  register : communication
  reaction : Abort connection option code (0x6007)
  cause :
      - CANopen Heartbeat Consumer Procedure or Life Guarding have detected a timeout.
      - Probably, the procedure has failed due to wrong configuration.
  effect :
      - Fault reaction defined in Abort connection option code
  recovery :
      - Send NMT command reset communication, then reset fault with Controlword
```

The raw values: `GetErrorCode()` and `GetErrorRegister()`.

## Error tables

`#include "epos4/signals/Errors.hpp"` (already included by `Epos4.hpp`):

| Function | |
|---|---|
| `DescribeDeviceError(code)` | the multi-line text above |
| `DeviceErrorName(code)` | one line, for logs |
| `FindDeviceError(code)` | the `DeviceError` row: name, register, reaction, cause / effect / recovery lists; `nullptr` for a code the manual does not list |
| `IsWarning(code)` | reaction "w": the drive keeps running |
| `ClearsPosition(code)` | resetting it clears the position - home again |
| `RequiresCommunicationReset(code)` | the recovery starts with an NMT reset communication (`0x8120`, `0x8130`) |
| `DescribeErrorRegister(byte)` | the `0x1001` flags as names |
| `DescribeAbortCode(code)`, `FindAbortCode(code)` | SDO abort codes |

All 73 codes, with cause, effect and recovery: [Device error codes](../reference/error-codes.md).
A `nullptr` from `FindDeviceError()` for a code the drive reports means the firmware is newer
than the tables - worth reporting.

## Emergency messages

The drive sends an EMCY frame the moment an error occurs. Register a callback:

```cpp
motor.SetEmergencyCallback([](const epos4::signals::EmergencyMessage & m) {
  // m.errorCode, m.errorRegister, m.manufacturerSpecific[5]
  std::printf("EMCY %s\n", epos4::signals::Describe(m).c_str());
});
```

- It runs on the **CANopen thread**: keep it short, do not block, and **do not call back into
  the device** - that would deadlock. Set a flag and handle it on your own thread.
- It may be registered before `Start()`, and survives a restart of the bus.
- An EMCY with code **0** announces that the errors were reset.

Without a callback, the last one is still kept, lock-free:

```cpp
std::uint16_t code = motor.GetCachedErrorCode();            // 0 if none, or reset since
auto age = motor.GetTimeSinceLastEmergency();               // duration::max() if never
```

They answer even when the node has gone silent - exactly when an SDO read of `0x603F` cannot.
The age matters: an EMCY from the boot, minutes earlier, is not the cause of what just
happened.

## Error history

```cpp
std::vector<std::uint16_t> history;
motor.GetErrorHistory(history);    // 0x1003, newest first; survives a fault reset
motor.ClearErrorHistory();
```

On an intermittent fault, "what happened before the one I am looking at" is the only useful
question - and the history answers it after the fault was cleared.

## Recovering

```cpp
if (motor.IsFaulted()) {
  std::printf("%s\n", motor.DescribeLastError().c_str());
  const auto code = motor.GetErrorCode().Refresh().GetValue();
  if (motor.ClearFault()) {
    if (epos4::signals::ClearsPosition(code)) {
      // home before trusting the position
    }
    motor.Enable();
  } else {
    // the cause is still present
  }
}
```

[Enabling and stopping](enabling.md#clear-a-fault) explains what `ClearFault()` does, and
[Common faults](../troubleshooting/faults.md) the usual causes.

## Identity

```cpp
epos4::signals::DeviceIdentity id;
motor.GetIdentity(id);
std::printf("%s\n", epos4::signals::Describe(id).c_str());
// EPOS4 Module/Compact 50/15, firmware EPOS4_0170h_6552h_0000h_0000h, serial 687996930
```

| Field / method | Object | |
|---|---|---|
| `vendorId`, `IsMaxon()` | `0x1018:01` | `0xFB` for maxon |
| `productCode`, `HardwareVersion()`, `ApplicationNumber()` | `0x1018:02` | |
| `revisionNumber`, `SoftwareVersion()`, `ApplicationVersion()` | `0x1018:03` | the firmware |
| `serialNumber`, `serialNumberComplete` | `0x1018:04`, `0x2100:01` | |
| `deviceType`, `DeviceProfile()`, `DriveType()` | `0x1000` | 402, servo |
| `deviceName` | `0x1008` | `"EPOS4"` |
| `programSoftware`, `flashStatus`, `IsProgramValid()` | `0x1F56`, `0x1F57` | |

`signals::HardwareName(code)` names the hardware from Table 6-66, and
`signals::FirmwareName(id)` produces the file name maxon uses for that firmware, the same
string EPOS Studio shows. Log the identity once per drive at start-up.

## Boot status and PDO mapping

Two checks for the network configuration:

- `GetBootStatus()` - whether the master's boot of the node succeeded; see
  [Bus and device lifecycle](lifecycle.md#boot-status).
- `CheckPdoMapping(mismatches)` - whether the drive's PDO mapping matches the master's; see
  [PDO mapping](../network/pdo-mapping.md#checking-that-both-ends-agree).

A program that refuses to run when either fails turns a silent misconfiguration into an
error at start-up.

## Version

```cpp
#include "epos4/Version.hpp"
std::printf("EposLib %s\n", epos4::GetVersionString());   // the library linked against
#if EPOS4_VERSION >= EPOS4_VERSION_ENCODE(0, 2, 0)
// ...
#endif
```

`epos4::kFirmwareSpecificationEdition` names the manual edition the tables were transcribed
from.
