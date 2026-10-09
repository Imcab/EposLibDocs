# The network description (bus.yml)

`bus.yml` is read by Lely's **`dcfgen`**, which `eposlib_generate_dcf()` runs at build time.
It has one block for the master, one block per node, and an optional `options` block.

## The example network

EposLib's `config/epos4_network/bus.yml`, without its comments:

```yaml
master:
  node_id: 1
  baudrate: 1000
  heartbeat_producer: 100
  sync_period: 10000

node_2:
  node_id: 2
  dcf: "epos4.eds"
  heartbeat_producer: 500
  heartbeat_consumer: true
  rpdo:
    1:
      cob_id: "auto"
      transmission: 1
      mapping:
        - {index: 0x6040}   # Controlword          16 bit
        - {index: 0x607A}   # Target position      32 bit
    2:
      cob_id: "auto"
      transmission: 1
      mapping:
        - {index: 0x60FF}   # Target velocity      32 bit
        - {index: 0x6071}   # Target torque        16 bit
    3: {enabled: false}
    4: {enabled: false}
  tpdo:
    1:
      cob_id: "auto"
      transmission: 1
      mapping:
        - {index: 0x6041}   # Statusword           16 bit
        - {index: 0x6064}   # Position actual      32 bit
    2:
      cob_id: "auto"
      transmission: 1
      mapping:
        - {index: 0x606C}   # Velocity actual      32 bit
        - {index: 0x6077}   # Torque actual        16 bit
    3: {enabled: false}
    4: {enabled: false}
```

Node blocks may have any name (`node_2`, `left_wheel`); the convention in EposLib is to name
them after the node-ID, because the file describes a network, not a machine.

## master

| Key | Default | Meaning |
|---|---|---|
| `node_id` | 255 | The master's node-ID. Must match `CanBus::Options::masterNodeId` (1 in the examples). |
| `baudrate` | 1000 | kbit/s. Informative: the bit rate is set on the interface (`ip link`). |
| `heartbeat_producer` | 0 | ms between the master's heartbeats. Drives with `heartbeat_consumer: true` watch for it. |
| `heartbeat_consumer` | true | Whether the master watches the nodes' heartbeats. |
| `heartbeat_multiplier` | 1 | |
| `sync_period` | 0 | **µs** between SYNC messages. 0 disables SYNC; the cyclic modes need it. |
| `sync_window` | 0 | µs after SYNC in which synchronous PDOs must be sent. |
| `sync_overflow` | 0 | SYNC counter overflow value; 0 = no counter. |
| `emcy_inhibit_time` | 0 | Minimum time between the master's own EMCY messages (×100 µs). |
| `nmt_inhibit_time` | 0 | Minimum time between NMT commands (×100 µs). |
| `error_behavior` | `{1: 0}` | The master's own reaction to communication errors. |
| `start` | true | Whether the master starts itself (enters Operational) after booting. |
| `start_nodes` | true | Whether the master starts the nodes it boots. |
| `start_all_nodes` | false | Start all nodes with one broadcast NMT command. |
| `reset_all_nodes` | false | Reset all nodes with one broadcast at start-up. |
| `stop_all_nodes` | false | Stop all nodes with one broadcast on shutdown. |
| `boot_time` | 0 | ms the master waits for mandatory nodes to boot; 0 = forever. |
| `vendor_id`, `product_code`, `revision_number`, `serial_number` | 0 | The master's own identity. |

## Each node

| Key | Default | Meaning |
|---|---|---|
| `node_id` | - | The drive's node-ID, 1-127. |
| `dcf` | - | The EDS (or DCF) describing the drive, relative to `bus.yml`. |
| `heartbeat_producer` | the EDS value | ms between the **drive's** heartbeats (writes `0x1017`). |
| `heartbeat_consumer` | false | Make the **drive** watch the master's heartbeat (writes `0x1016`). See [SYNC and heartbeat](sync-heartbeat.md). |
| `heartbeat_multiplier` | `options` value (3) | The drive's consumer time = master's producer time × this. |
| `rpdo`, `tpdo` | - | PDO configuration, below. |
| `boot` | true | Whether the master boots this node at all. |
| `mandatory` | false | Whether a failed boot of this node stops the network from starting. |
| `reset_communication` | true | Whether the master sends NMT reset communication before booting the node. |
| `revision_number`, `serial_number` | - | If set, the master checks them at boot (`0x1F87`, `0x1F88`) and refuses a different drive. |
| `sdo` | - | Extra objects to write at boot, below. |
| `error_behavior` | - | Values for the drive's `0x1029` sub-indices. |
| `guard_time`, `life_time_factor`, `retry_factor` | 0, 0, 3 | Node guarding. Heartbeat is the modern alternative; EposLib uses heartbeat. |
| `software_file`, `software_version`, `restore_configuration`, `configuration_file`, `dcf_path`, `time_cob_id` | - | Firmware update and file paths; not needed for EPOS4 networks. |

The identity in the EDS - vendor `0xFB` (maxon) and the product code - is always checked:
a drive that answers with another identity is not booted (error status `'D'` or `'M'`).

### rpdo and tpdo

Up to four of each, keyed 1-4:

| Key | Meaning |
|---|---|
| `enabled` | `false` to disable the channel. |
| `cob_id` | `"auto"` = the standard base + node-ID (`0x200 + id` for RPDO1, `0x180 + id` for TPDO1, ...), or a number. |
| `transmission` | `1` = synchronous, every SYNC. `255` = event-driven (TPDO: on change). `253` = on RTR only (TPDO). |
| `inhibit_time` | TPDO: minimum time between event-driven transmissions, ×100 µs. |
| `event_timer` | TPDO: send at least every this many ms, even without change. |
| `event_deadline` | RPDO: deadline for reception. |
| `sync_start` | TPDO: SYNC counter value to start on. |
| `mapping` | List of `{index: 0x...}` or `{index: 0x..., sub_index: n}`. Up to 64 bits per PDO. |

!!! danger "Disable the channels you do not use"
    `dcfgen` only touches the PDOs listed in `bus.yml`. The EPOS4's own defaults leave RPDO3
    and RPDO4 **valid**, carrying the Controlword and targets on `0x400` / `0x500` + node-ID -
    any other device sending on those identifiers would command the axis. List unused
    channels as `{enabled: false}`. `Epos4::CheckPdoMapping()` reports any channel that is
    valid on the drive and not configured by the network.

### sdo: objects written at boot

Any object can be written by the master each time it boots the node, before the node is
started:

```yaml
node_2:
  # ...
  sdo:
    - {index: 0x60C2, sub_index: 1, value: 10}   # Interpolation time period = 10 ms
    - {index: 0x6080, value: 5000}               # Max motor speed [rpm]
```

This is the place for settings that belong to the **network** rather than to the drive -
the interpolation period that must match `sync_period`, for example - because they are
re-applied after every power cycle without `Save()`. Settings that belong to the drive
(motor data, gains) are better saved on the drive once.

## options

```yaml
options:
  heartbeat_multiplier: 3.0   # default for every node's consumer time
  dcf_path: ""                # where the generated node_N.bin are referenced from
  retry_factor: 3
```

## After editing

The DCF is regenerated on the next build whenever `bus.yml` or an EDS changes. Rebuild,
then run [device information](../examples/device-info.md) against the drives: `boot ok` and
`mapping: drive and master agree` confirm that the new description took effect.
