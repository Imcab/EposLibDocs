# Multiple drives

## One block per drive

Copy a node block and change the node-ID - everything else, including the COB-IDs
(`cob_id: "auto"`), follows from it. A four-wheel drivetrain:

```yaml
master:
  node_id: 1
  baudrate: 1000
  heartbeat_producer: 100
  sync_period: 10000

node_2: &wheel                     # front left
  node_id: 2
  dcf: "epos4.eds"
  heartbeat_producer: 500
  heartbeat_consumer: true
  rpdo:
    1: {cob_id: "auto", transmission: 1, mapping: [{index: 0x6040}, {index: 0x607A}]}
    2: {cob_id: "auto", transmission: 1, mapping: [{index: 0x60FF}, {index: 0x6071}]}
    3: {enabled: false}
    4: {enabled: false}
  tpdo:
    1: {cob_id: "auto", transmission: 1, mapping: [{index: 0x6041}, {index: 0x6064}]}
    2: {cob_id: "auto", transmission: 1, mapping: [{index: 0x606C}, {index: 0x6077}]}
    3: {enabled: false}
    4: {enabled: false}

node_3:                             # front right
  <<: *wheel
  node_id: 3

node_4:                             # rear left
  <<: *wheel
  node_id: 4

node_5:                             # rear right
  <<: *wheel
  node_id: 5
```

YAML anchors (`&wheel`, `<<: *wheel`) keep the blocks identical; only the node-ID differs.

```cpp
epos4::CanBus bus{{"can0", dcf, 1}};
epos4::Epos4 frontLeft{bus, 2};
epos4::Epos4 frontRight{bus, 3};
epos4::Epos4 rearLeft{bus, 4};
epos4::Epos4 rearRight{bus, 5};
bus.Start();
```

The complete program is the [multiple drives example](../examples/multi-drive.md).

## Planning node-IDs

- **The master is node 1**; drives from 2 up.
- **Lower node-IDs win bus arbitration.** COB-ID = base + node-ID, and on CAN the lower
  identifier wins a collision. Give the low numbers to what must not be starved.
- **Label the hardware** with its node-ID. On a robot that is taken apart and put back
  together, a swapped connector is otherwise a drive answering as the wrong joint - and the
  master will happily drive it, because both drives have the same identity.
  `revision_number` and `serial_number` in `bus.yml` pin a block to one physical drive if
  that matters.

## Bus load { #bus-load }

A CAN frame with 8 data bytes is about **130 bits** on the wire with stuffing; at
**1 Mbit/s** that is ~130 µs. Per SYNC period, the network sends:

- 1 SYNC frame, plus
- each drive's synchronous PDOs: 4 in the example mapping (2 RPDO + 2 TPDO).

| Drives | Frames per 10 ms | Bus time per 10 ms | Load at 100 Hz | Load at 200 Hz |
|---|---|---|---|---|
| 1 | 5 | ~0.6 ms | ~6 % | ~12 % |
| 4 | 17 | ~2.2 ms | ~22 % | ~44 % |
| 6 | 25 | ~3.3 ms | ~33 % | ~65 % |
| 8 | 33 | ~4.3 ms | ~43 % | ~86 % |

Heartbeats, EMCY and SDO traffic come on top. Keep the cyclic load well under ~50 % so
that SDO requests - configuration, diagnostics - still get through promptly, and remember
that all synchronous TPDOs are sent right after SYNC, in a burst. With many drives, prefer
a lower SYNC rate, fewer PDOs, or a second CAN bus (a second `CanBus` on `can1`).

## Reading many drives at once

Each drive has its own driver thread, so SDO reads to **different** drives overlap:

```cpp
epos4::signals::RefreshAll(
  frontLeft.GetSupplyVoltage(), frontRight.GetSupplyVoltage(),
  rearLeft.GetSupplyVoltage(), rearRight.GetSupplyVoltage());
```

costs about one round trip instead of four. Reads to the **same** drive still queue, as
CANopen requires - one SDO transfer per node at a time.

## Simulating it

One `epos4_sim` per node:

```bash
for id in 2 3 4 5; do
  install/eposlib/lib/eposlib/epos4_sim epos4.eds $id vcan0 &
done
```
