# Architecture

## Two objects

```cpp
epos4::CanBus bus{{"can0", dcf, 1}};   // one per CAN network
epos4::Epos4 left{bus, 2};             // one per drive on it
epos4::Epos4 right{bus, 3};
bus.Start();
```

**`CanBus`** owns everything that exists once per network: the SocketCAN socket, Lely's
I/O context and event loop, the timer, and the **CANopen master**. Its event loop runs on a
thread of its own, started by `Start()`.

**`Epos4`** is one drive, addressed by node-ID. It owns no CAN state: it registers with its
bus, and when the bus starts it gets a **driver** - a Lely `LoopDriver` - through which every
request to that node goes. Several devices on one bus share the master, the loop and the
socket, the way several Talon FX share a CAN bus.

This mirrors the CANopen network itself: one master, many slaves.

## Two libraries

| Target | Contents | Needs |
|---|---|---|
| `eposlib::core` | state machine, object dictionary, units, configuration groups, error tables, PDO mapping decoder, the cyclic state | nothing - no Lely, no CAN |
| `eposlib::eposlib` | `CanBus`, `Epos4`, `Encoder` | `core` and Lely |

Everything in `core` is pure logic, and the tests exercise it with no bus at all. Link
`eposlib::eposlib` in applications; [The core library](../api/core.md) covers using `core`
alone.

## Threads

```text
 your threads                     bus thread (CanBus)         driver thread (one per Epos4)
 ────────────                     ───────────────────         ─────────────────────────────
 motor.Enable() ── posts ──────────────────────────────────▶  SDO read/write, waits for it
   blocks on a future  ◀────────── SDO answer ─ socket ─────  completes the future

 motor.StageTargetVelocity() ─ atomic store ─▶ OnSync: atomics → RPDO → socket
 motor.GetCachedVelocity()   ◀ atomic load ── OnRpdoWrite: TPDO → atomics
```

- **The bus thread** runs Lely's event loop: it reads and writes the socket, runs the
  timers, sends SYNC and heartbeats, decodes incoming PDOs, and calls the drivers'
  callbacks.
- **One driver thread per device** runs that device's SDO transfers, so a drive that is
  slow to answer only holds up itself.
- **Your threads** call the API.

Two kinds of call follow from this:

- **Blocking calls** - almost every method: `Enable()`, `SetControl()`, `Refresh()` on a
  signal, configuration. They post the work to the driver thread and wait for the answer. A
  round trip is a couple of milliseconds on an idle 1 Mbit/s bus.
- **Lock-free calls** - the cyclic path: `StageTarget*()`, `GetCached*()`,
  `IsCyclicHealthy()`, `GetCachedErrorCode()`. A relaxed atomic load or store, no syscall,
  no waiting: safe in a control loop.

[Threading and timing](threading.md) has the rules that follow.

## SDO and PDO

Data reaches the drive two ways (the [CANopen primer](canopen.md) has the details):

- **SDO** - a request and an answer, addressed to one object. Used for configuration and
  for any read the PDOs do not carry. Costs two frames and a round trip.
- **PDO** - unconfirmed frames whose layout is fixed in advance by the *PDO mapping*. The
  drive sends its feedback on every SYNC without being asked; the master sends setpoints the
  same way.

EposLib picks the transport for you. A status read uses the value from the last PDO when one
arrived in the last 100 ms, and asks over SDO otherwise. A write to an object that is mapped
into an outgoing PDO goes into the PDO - an SDO write would be overwritten on the next SYNC
by the master's own copy - and over SDO only when it is not mapped.

## The network description

Which drives exist, and which objects each PDO carries, is not decided in code: it is in
`bus.yml`, turned into the master's DCF at build time. Both ends of every PDO must agree on
its layout, and the DCF configures both from one description - see
[The network description](../network/bus-yml.md).

## What lives where

```text
include/epos4/
├── CanBus.hpp                 the bus
├── hardware/
│   ├── Epos4.hpp              the device: control, signals, diagnostics; and Configurator
│   └── Encoder.hpp            the feedback subsystem, motor.GetEncoder()
├── controls/ControlRequests.hpp   ProfilePosition, CyclicVelocity, Homing, TouchProbe...
├── configs/                   every configuration group
├── signals/                   StatusSignal, enums, errors, identity, PDO mapping
└── core/                      state machine, units, object dictionary, setpoints
```
