# Bus and device lifecycle

## CanBus

```cpp
epos4::CanBus bus{{"can0", "/path/to/master.dcf", 1}};
```

`CanBus::Options` has three fields, in this order:

| Field | Default | |
|---|---|---|
| `interface` | `"can0"` | SocketCAN interface name: `can0`, `can1`, `vcan0` |
| `masterDcf` | - (required) | the master DCF generated from `bus.yml` |
| `masterNodeId` | 1 | the master's node-ID; must match `master.node_id` in `bus.yml` |

Designated initializers (`.interface = ...`) are C++20; in C++17 pass the fields in order,
or fill a named `Options`:

```cpp
epos4::CanBus::Options options;
options.interface = "can0";
options.masterDcf = dcf;
epos4::CanBus bus{options};
```

One `CanBus` per CAN network. Two networks - `can0` and `can1` - are two `CanBus` objects,
each with its own master and thread.

## The order of things

```cpp
epos4::CanBus bus{{"can0", dcf, 1}};   // 1. the bus
epos4::Epos4 shoulder{bus, 2};         // 2. every device, BEFORE Start()
epos4::Epos4 elbow{bus, 3};
shoulder.SetMechanism(4096, 1.0 / 160.0);   //    devices can be set up now
shoulder.SetEmergencyCallback(onEmcy);

bus.Start();                           // 3. open the socket, boot the network
shoulder.WaitUntilReady();             // 4. wait for each node
elbow.WaitUntilReady();
```

!!! danger "Declare devices before `Start()`"
    A CANopen master boots each node once, at start-up, and routes a node's PDOs only to a
    driver registered at that moment. A device constructed after `Start()` answers SDO but
    never receives a PDO - which looks exactly like a PDO mapping that failed to take. The
    library still attaches it, for SDO use; the PDOs arrive only after the next `Start()`.

Before `Start()`, a device can already be configured locally - `SetMechanism()`,
`SetEmergencyCallback()`, taking references to its signals, its configurator and its
encoder - but anything that talks to the drive returns `std::errc::not_connected` (or
`false`), and signals refreshed then report `not_connected`.

### Destruction order

An `Epos4` must not outlive its `CanBus`. Declare devices after the bus - in the same scope,
or as later class members - and C++ destroys them first. Destroying a device while the bus
runs is fine: it unregisters itself and its driver is removed on the bus thread.

## Start

`bus.Start()`:

1. opens the CAN interface and loads the DCF - throws `std::system_error` if either fails;
2. rewrites the DCF's relative file paths into a `master.resolved.dcf` next to it, so the
   program can run from any directory;
3. attaches every registered device;
4. resets the network and starts the event loop on its own thread. The master then boots
   every node in the DCF.

Calling `Start()` on a running bus does nothing.

## WaitUntilReady

```cpp
if (!motor.WaitUntilReady(std::chrono::seconds{5})) {
  // the node never answered
}
```

Blocks until the node answers an SDO request - the first request after `Start()` fails until
the master has finished booting it, so call this rather than sleeping. Returns false on
timeout, or immediately if the bus is not running.

It checks that the node **answers**, not that it was configured correctly. A node whose
configuration download failed still answers SDO; check the boot status as well.

## Boot status

```cpp
const epos4::signals::BootStatus boot = motor.GetBootStatus();
if (!boot.Succeeded()) {
  std::printf("boot failed: '%c' %s\n", boot.errorStatus, boot.what.c_str());
}
```

| Field | |
|---|---|
| `count` | boots the master has completed for this node since the bus started; 0 while the first one is pending |
| `errorStatus` | 0, or the CiA 302-2 letter of the step that failed |
| `what` | Lely's description of it |
| `Succeeded()` | `count > 0` and the status is 0 or `'L'` |

The boot completes shortly after the node first answers SDO, so poll `count` for a moment
after `WaitUntilReady()` if you need it. No bus access.

| Status | Meaning (CiA 302-2) |
|---|---|
| `'A'` | the node is not listed in the master's network list (`0x1F81`) |
| `'B'` | no response to reading the device type (`0x1000`) |
| `'C'` | the device type differs from the DCF (`0x1F84`) |
| `'D'` | **the vendor-ID differs** (`0x1F85`) - not a maxon drive, or the wrong DCF |
| `'E'` | heartbeat event: no heartbeat received during the boot |
| `'F'` | node guarding event |
| `'G'` | the objects for a program download are not configured or inconsistent |
| `'H'` | a software update is required but not allowed |
| `'I'` | a software update is required but no image is available |
| `'J'` | **the configuration download failed** - one of the master's writes was refused |
| `'K'` | heartbeat event during the start of error control |
| `'L'` | the node was already operational - advisory, the boot went on |
| `'M'` | **the product code differs** (`0x1F86`) |
| `'N'` | the revision number differs (`0x1F87`) |
| `'O'` | the serial number differs (`0x1F88`) |

[Boot and PDOs](../troubleshooting/boot-and-pdo.md) covers what to do about each.

## Stop

```cpp
bus.Stop();
```

Deconfigures the network and stops the event loop, letting it finish by itself (with a 2 s
grace period before forcing it). The destructor calls it. After `Stop()`, device calls fail
with `std::errc::not_connected` at once - including a `Disable()` in a shutdown path - instead
of waiting for a loop that is gone.

Stopping the master also stops its heartbeat: drives with `heartbeat_consumer: true` fault
with `0x8130` 300 ms later. See [SYNC and heartbeat](../network/sync-heartbeat.md#after-every-program-exits).

## Restarting

`Start()` may be called again after `Stop()` - to recover from an unplugged adapter, for
example. The previous master is torn down, every device gets a new driver, and the network
boots again:

```cpp
bus.Stop();
// ... reconnect the adapter, bring can0 back up ...
bus.Start();
motor.WaitUntilReady();
motor.ClearFault();    // the drives will have lost the heartbeat meanwhile
motor.Enable();
```

Devices, and the references to their signals, stay valid across a restart. The cyclic path
is deactivated - re-enter the cyclic mode after enabling. Do not call `Start()` / `Stop()`
while other threads are calling into the devices of that bus.
