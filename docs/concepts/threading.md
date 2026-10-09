# Threading and timing

## Which calls block

| Kind | Calls | Cost |
|---|---|---|
| **Lock-free** | `StageTargetPosition/Velocity/Torque()`, `GetCachedPosition/Velocity/Torque/Statusword/ErrorCode()`, `IsCyclicHealthy()`, `IsCyclicModeActive()`, `IsPdoActive()`, `GetTimeSinceLastPdo()`, `GetTimeSinceLastEmergency()`, `GetBootStatus()` | an atomic load or store |
| **Cached** | `StatusSignal::GetValue()`, `GetStatus()`, `GetAge()` | no bus access |
| **Blocking, usually free** | `StatusSignal::Refresh()` of a value that is in a PDO | a hop to the driver thread, no frames |
| **Blocking, one round trip** | `Refresh()` of anything else, `ReadObject()`, `WriteObject()` | two frames, ~1-3 ms |
| **Blocking, several round trips** | `Enable()`, `Disable()`, `SetControl()`, `Home()`, `ClearFault()`, `Configurator::Apply()` / `Refresh()` | milliseconds to seconds |

Every blocking call has an upper bound. `Enable()`, `Disable()`, `ClearFault()`, `Home()` and
`SetPosition()` take a timeout. A single bus request gives up after the master's SDO timeout,
and a request the bus can no longer complete - because it was stopped - fails at once with
`std::errc::not_connected`, or with `std::errc::timed_out` after 2 s if its loop dies
mid-call.

## The rules

1. **Control loops use the lock-free calls only.** Stage the setpoint, read the cached
   feedback, check `IsCyclicHealthy()`. A `Refresh()` in a 100 Hz loop is a thread hop and a
   wait every cycle, and with several axes it is the whole budget.

2. **Never call into a device from its own callbacks.** The emergency callback runs on the
   CANopen thread. A blocking call from there waits for a thread that is busy running the
   callback: a deadlock. Set a flag and act on it from your own thread
   ([fault monitor example](../examples/fault-monitor.md)).

3. **One thread per signal.** A `StatusSignal` is not thread-safe on its own: refresh a
   given signal from one thread at a time. Different signals, and different devices, may be
   refreshed concurrently - that is what `signals::RefreshAll()` does.

4. **Lifecycle calls are not concurrent with device calls.** `CanBus::Start()`, `Stop()` and
   destroying an `Epos4` must not overlap with calls on the devices of that bus from other
   threads.

5. **Devices are declared before `Start()`, and destroyed before the bus.** Declare the
   devices after the bus, in the same scope or as later members, and C++ destruction order
   does the rest.

## Timing of the cyclic path

```text
 SYNC n          SYNC n+1         SYNC n+2
   │  RPDO sent    │  drive applies │  TPDO reflects
   │  with target  │  the target    │  the result
```

A setpoint staged now leaves with the next SYNC, is applied by the drive on the SYNC after
that, and its effect shows up in the feedback of the following one: **two to three SYNC
periods** of latency - 20 to 30 ms at 100 Hz. Feedback read with `GetCached*()` is at most
one SYNC period old.

The library accounts for this where it matters. The PPM setpoint handshake and homing wait
for a PDO after the third SYNC before trusting the Statusword, so a second move is never
mistaken for the end of the first.

## A loop that keeps time

```cpp
constexpr auto kPeriod = std::chrono::milliseconds{10};   // = the SYNC period
auto next = std::chrono::steady_clock::now();
while (running) {
  motor.StageTargetVelocity(ComputeTarget());
  if (!motor.IsCyclicHealthy()) {
    break;
  }
  next += kPeriod;
  std::this_thread::sleep_until(next);   // absolute: no drift
}
```

`sleep_until` on an absolute time keeps the period exact on average; `sleep_for` accumulates
the time the loop body took. The loop does not have to be synchronised with SYNC: the drive
takes whatever value is staged when the SYNC goes out. Running it faster than SYNC only
overwrites setpoints that never leave.
