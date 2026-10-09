# Setting up the CAN bus

## Wiring

A CAN bus is a single twisted pair, **CAN_H** and **CAN_L**, daisy-chained from node to
node, with a common ground.

- **Terminate both ends, and only the ends, with 120 Ω** between CAN_H and CAN_L. With the
  power off, the resistance between CAN_H and CAN_L measured anywhere on the bus should be
  about **60 Ω** (two 120 Ω in parallel). 120 Ω means one terminator is missing; 40 Ω means
  one too many.
- Keep stubs (the branch from the trunk to a node) short. At 1 Mbit/s that means a few
  tens of centimetres at most.
- Share the ground (CAN_GND) between the adapter and the drives.
- The EPOS4 has its CAN connectors and, depending on the variant, a termination switch or
  jumper; see its *Hardware Reference*.

## Node-ID and bit rate on the drive

Every drive needs a **unique node-ID** from 1 to 127, and every node on the bus must use the
**same bit rate**. The master in the example network is node 1, so the drives start at 2.

On the EPOS4 the node-ID comes from the DIP switches, or from the software setting
(`0x2000`) when the switches are all off. The CAN bit rate (`0x2001`) defaults to 1 Mbit/s.
Both can also be set from EposLib, through `configs::CommunicationConfigs` - they take
effect only after `Save()` and a power cycle, see [Configuration](../api/configuration.md).

!!! danger "Changing a node-ID keeps the old PDO COB-IDs"
    The PDO COB-IDs a drive has saved are absolute values. If a drive was configured and
    saved as node 1 and then moved to node 5, its PDOs still use node 1's identifiers, the
    master's configuration download fails, and no PDO ever arrives.
    [Boot and PDOs](../troubleshooting/boot-and-pdo.md#cob-ids) shows how to recognise and
    fix it.

## Bringing up the interface

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 1000000
sudo ip link set can0 up
ip -details -statistics link show can0
```

The last command should show `state UP`, `bitrate 1000000`, and `can state ERROR-ACTIVE`.
`ERROR-PASSIVE` or `BUS-OFF` means frames are not being acknowledged: wiring, termination,
or a bit rate that differs from the drives'.

### Making it permanent

With systemd-networkd, create `/etc/systemd/network/80-can.network`:

```ini
[Match]
Name=can0

[CAN]
BitRate=1M
RestartSec=100ms
```

and enable it with `sudo systemctl enable --now systemd-networkd`. `RestartSec` makes the
interface recover by itself from bus-off.

## Looking at the traffic

`can-utils` (`sudo apt install can-utils`) is the first tool to reach for:

```bash
candump -tz can0          # every frame, with a relative timestamp
candump can0,080:7FF      # only SYNC
cansend can0 000#0000     # NMT: reset all nodes... careful, see below
```

What a healthy EposLib network looks like at 100 Hz, node 2:

| CAN-ID | What | From |
|---|---|---|
| `0x080` | SYNC, every 10 ms | master |
| `0x182`, `0x282` | TPDO1, TPDO2: Statusword + position, velocity + torque | drive |
| `0x202`, `0x302` | RPDO1, RPDO2: Controlword + targets | master |
| `0x701` | master heartbeat, every 100 ms | master |
| `0x702` | drive heartbeat, every 500 ms | drive |
| `0x582` / `0x602` | SDO response / request, only when the program reads or writes an object | both |
| `0x082` | EMCY, only when the drive reports an error | drive |

The COB-ID of every per-node message is a base plus the node-ID: `0x180 + 2 = 0x182`. The
[CANopen primer](../concepts/canopen.md) explains each kind of message.

!!! warning "Sending raw frames"
    `cansend` bypasses everything: an NMT command or a Controlword sent by hand acts on the
    drive immediately. Use it to diagnose, with the motor free to move or unpowered.
