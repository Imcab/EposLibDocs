# API Usage

The API has three verbs, the same ones as Phoenix 6: **configure** a device, **command** it
with control requests, and **read** it through status signals.

```cpp
#include <ros2units/units.h>
#include "epos4/hardware/Epos4.hpp"

using namespace units::literals;

epos4::CanBus bus{{"can0", dcf, 1}};
epos4::Epos4 motor{bus, 2};
bus.Start();
motor.WaitUntilReady();

// Configure: only the fields that are set are written.
epos4::configs::Epos4Configuration cfg;
cfg.motionProfile.profileAcceleration = 5000;
motor.GetConfigurator().Apply(cfg);

// Command: a request per operating mode; the mode switch is done for you.
motor.Enable();
motor.SetControl(epos4::controls::ProfilePosition{}.WithPosition(50000));

// Read: cached signals, refreshed explicitly.
std::int32_t position = motor.GetPosition().Refresh().GetValue();
```

| Page | Covers |
|---|---|
| [Bus and device lifecycle](lifecycle.md) | `CanBus`, `Start()` / `Stop()` / restart, `WaitUntilReady()`, boot status |
| [Enabling and stopping](enabling.md) | `Enable()`, `Disable()`, `ClearFault()`, `Halt()`, `QuickStop()` |
| [Control requests](control-requests.md) | `SetControl()` with Profile Position, Profile Velocity, Halt |
| [Cyclic control](cyclic-control.md) | CSP, CSV, CST: the lock-free path for control loops |
| [Homing](homing.md) | `Home()`, homing methods, `SetPosition()` |
| [Status signals](status-signals.md) | `StatusSignal`, every signal the device exposes, `RefreshAll()` |
| [Configuration](configuration.md) | `Configurator`, `Apply()` / `Refresh()` / `Save()`, every group |
| [Encoders](encoders.md) | `Encoder`: sensor slots, encoder types, angles |
| [Digital and analog I/O](io.md) | inputs, outputs, analog channels, touch probe |
| [Brake, stops and protection](safety.md) | holding brake, stop options, STO, supply and thermal limits |
| [Diagnostics and faults](diagnostics.md) | error descriptions, EMCY, history, identity, PDO checks |
| [Raw object access](raw-access.md) | `ReadObject()` / `WriteObject()` for everything not wrapped |
| [The core library](core.md) | `eposlib::core`: the logic, without a bus |

## Conventions

**Errors are values.** Calls that can fail return `std::error_code` (falsy on success) or
`bool`. Nothing in normal operation throws; `CanBus::Start()` is the exception - it throws
`std::system_error` when the interface or the DCF cannot be opened.

```cpp
if (auto ec = motor.SetControl(request)) {
  std::printf("rejected: %s\n", ec.message().c_str());
}
```

Common codes:

| `std::errc` | Meaning |
|---|---|
| `not_connected` | the bus has not been started, or was stopped |
| `timed_out` | the drive did not get there in time, or the bus loop died mid-call |
| `invalid_argument` | a quantity without a mechanism, an invalid request, a forbidden output |
| `operation_not_permitted` | not allowed in the current state, e.g. encoder configuration while powered |
| `not_supported` | the drive does not implement that mode or homing method |
| `argument_out_of_domain` | outside the range the manual allows, e.g. an analog output beyond ±4 V |
| SDO abort codes | the drive refused the request; see [SDO abort codes](../reference/sdo-abort-codes.md) |

**Optional means "do not touch".** Every configuration field and every override in a
request is `std::optional`. Unset fields are not written; the drive keeps what it has.

**Raw units or quantities.** A plain number is the drive's own unit (quadcounts, rpm,
thousandths of rated torque); a `units::` quantity is converted. See [Units](../concepts/units.md).

**Builders.** Requests are aggregates with `With...()` setters that return `*this`:

```cpp
auto move = epos4::controls::ProfilePosition{}
  .WithPosition(90_deg)
  .WithVelocity(1500)
  .WithAcceleration(5000)
  .WithRelative(true);
```

## Headers

| Header | Brings |
|---|---|
| `epos4/hardware/Epos4.hpp` | `CanBus`, `Epos4`, `Configurator`, every request, config group, signal type and enum |
| `epos4/hardware/Encoder.hpp` | `Encoder` and the encoder configuration groups |
| `epos4/core/Cia402StateMachine.hpp` | `core::Decode`, `core::Controlword`, `core::PlanStep` |
| `epos4/Version.hpp` | `EPOS4_VERSION`, `GetVersion()`, `GetVersionString()` |
| `ros2units/units.h` | the units and their literals (`using namespace units::literals`) |

Everything is in the namespace `epos4`, with `epos4::controls`, `epos4::configs`,
`epos4::signals`, `epos4::core` and `epos4::od` for the object dictionary.
