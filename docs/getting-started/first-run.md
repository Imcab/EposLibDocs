# First run

The first program to run against a new drive should **read** everything and change
nothing. The [device information](../examples/device-info.md) example does exactly that;
build it from the examples package, or write the same steps yourself.

## The minimal program

```cpp
#include <cstdio>

#include "epos4/hardware/Epos4.hpp"

int main()
{
  epos4::CanBus bus{{"can0", "install/eposlib/share/eposlib/config/epos4_network/master.dcf", 1}};
  epos4::Epos4 motor{bus, 2};   // before Start()
  bus.Start();

  if (!motor.WaitUntilReady()) {
    return 1;                   // nothing answered within 5 s
  }
  std::printf("%s\n", epos4::signals::ToString(motor.GetState().Refresh().GetValue()));
  bus.Stop();
}
```

Three things happen in that order, and each can go wrong on its own:

1. **`CanBus` opens the interface and loads the master DCF.** `Start()` throws
   `std::system_error` if the interface does not exist or the DCF cannot be read.
2. **The master boots every node the DCF describes**: it checks the node's identity, and
   downloads its configuration - PDO mapping, heartbeat - then starts it.
3. **`WaitUntilReady()` waits until the node answers an SDO request.** It returns false
   after the timeout (5 s by default).

## What to check, in order

Run the [device information](../examples/device-info.md) example:

```bash
ros2 run eposlib_examples device_info \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2
```

```text
boot     : ok ('L' NMT slave was initially operational.)
identity : EPOS4 Module/Compact 50/15, firmware EPOS4_0170h_6552h_0000h_0000h, serial 687996930
pdo      : active
mapping  : drive and master agree
state    : Switch on disabled
mode     : Profile Position (PPM)
supply   : 11.8 V
stage    : 26.9 degC
current  : 0.000 A
nominal  : 9.280 A
Kt       : 104.79 mNm/A
rated    : 0.9724 Nm
position : -819200 qc
velocity : 0 rpm
```

| Line | Expect | If not |
|---|---|---|
| `boot` | `ok` ('L' is fine) | the configuration download failed: [Boot and PDOs](../troubleshooting/boot-and-pdo.md) |
| `identity` | your hardware and firmware | wrong node-ID, or another device answers on it |
| `pdo` | `active` | no PDOs: [Boot and PDOs](../troubleshooting/boot-and-pdo.md) |
| `mapping` | `drive and master agree` | the list of differences says what to fix |
| `state` | `Switch on disabled` | `Fault`: read the fault, [Common faults](../troubleshooting/faults.md) |
| `supply` | your supply voltage | low: the drive will fault with `0x3220` under load |
| `rated` | not zero | the motor data was never configured: torque in N m will not work |

When all of these are right, the drive is ready for a [control request](../api/control-requests.md).

!!! tip "Log the identity"
    On a robot in the field, "which firmware was on that joint" is a question nobody can
    answer afterwards. Log `signals::Describe(identity)` once per drive at start-up.
