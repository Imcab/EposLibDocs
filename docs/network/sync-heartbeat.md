# SYNC and heartbeat

## SYNC

```yaml
master:
  sync_period: 10000   # microseconds: 100 Hz
```

The master broadcasts SYNC every `sync_period` µs. On each one, every drive samples its
feedback into its synchronous TPDOs and applies the setpoints it received in its synchronous
RPDOs, so all axes act on the same instant instead of drifting apart by however long their
frames took.

- The **cyclic modes** (CSP, CSV, CST) need SYNC. The profiled modes (PPM, PVM) do not, but
  with the example mapping their Controlword also rides an RPDO, so SYNC must run.
- `sync_period: 0` disables SYNC entirely.

### Interpolation time period

In CSP and CSV the drive interpolates between the setpoints it receives over a window
called the **interpolation time period** (`0x60C2:01`, in **ms**). It must equal the SYNC
period:

```cpp
epos4::configs::CyclicConfigs cyclic;
cyclic.interpolationTimePeriodMs = 10;      // sync_period: 10000 µs
motor.GetConfigurator().Apply(cyclic);
```

or, so that it is re-applied on every boot, in `bus.yml`:

```yaml
node_2:
  sdo:
    - {index: 0x60C2, sub_index: 1, value: 10}
```

| If it is | The motion |
|---|---|
| 0 | steps: the drive jumps to each new setpoint within 0.4 ms and holds it until the next |
| equal to the SYNC period | smooth |
| larger than the SYNC period | lags the commanded trajectory |

Neither mistake raises an error, which is why it is worth setting explicitly.

### Choosing the period

| Period | Rate | Use |
|---|---|---|
| 10 ms | 100 Hz | the example; wheels, most arm joints |
| 5 ms | 200 Hz | stiffer trajectory following; more bus load |
| 2 ms | 500 Hz | possible on a small bus; check the [bus load](multi-drive.md#bus-load) |

Shorter periods mean more frames per second for every synchronous PDO of every drive.

## Heartbeat

```yaml
master:
  heartbeat_producer: 100     # the master announces itself every 100 ms

node_2:
  heartbeat_producer: 500     # the drive announces itself every 500 ms
  heartbeat_consumer: true    # the drive watches the master's heartbeat
```

With `heartbeat_consumer: true`, `dcfgen` writes the drive's «Consumer heartbeat time»
(`0x1016`) as the master's period × `heartbeat_multiplier` (3 by default): **300 ms** here.
If the master's heartbeat stops for that long, the drive:

1. raises **`0x8130` CAN heartbeat error**;
2. applies its **abort connection option code** (`0x6007`; default: decelerate on the quick
   stop ramp, then disable);
3. drops to NMT pre-operational, as its error behavior (`0x1029`) says, which stops its PDOs.

That is the protection against a crashed control program: without it, a drive in CSP keeps
obeying the last setpoint of a master that no longer exists. The example network chose
100 ms × 3 = 300 ms over Lely's usual 1 s × 3 = 3 s for that reason.

### Recovering

A lost heartbeat needs an **NMT reset communication** before the fault reset, or the drive
ignores the reset. `ClearFault()` does both:

```cpp
if (motor.IsFaulted()) {
  motor.ClearFault();   // 0x8130: NMT reset communication, wait for the boot, fault reset
}
```

### After every program exits

The master stops its heartbeat when the program ends, so a drive with
`heartbeat_consumer: true` faults with `0x8130` **300 ms after every program exits**, clean
or not. The next program finds the drive in Fault and has to clear it. Programs that enable
a drive should handle it at start:

```cpp
if (motor.IsFaulted()) {
  std::printf("%s\n", motor.DescribeLastError().c_str());
  motor.ClearFault();
}
```

A read-only program - one that never enables - simply reports the fault.

### Watching the drives

The master watches the drives' heartbeats too (`heartbeat_consumer` in the master block,
true by default). When a drive stops answering, Lely logs a heartbeat event; for the
application the signal is `IsCyclicHealthy()` turning false and SDO requests timing out.
