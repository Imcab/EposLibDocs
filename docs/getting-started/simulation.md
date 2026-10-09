# Simulation

`epos4_sim` is a CANopen slave that behaves like an EPOS4, so the whole stack - the master,
the boot, the PDOs, the state machine, the control modes - can be developed and tested on a
virtual CAN bus without a drive.

## Running it

```bash
# Once per boot: the virtual CAN interface.
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# The simulated drive: EDS, node-ID, interface.
CFG=install/eposlib/share/eposlib/config/epos4_network
install/eposlib/lib/eposlib/epos4_sim $CFG/epos4.eds 2 vcan0 &

# Anything that talks to node 2 on vcan0:
install/eposlib/lib/eposlib/api_demo $CFG/master.dcf vcan0 2
```

Several simulated drives can share the bus - one `epos4_sim` per node-ID:

```bash
install/eposlib/lib/eposlib/epos4_sim $CFG/epos4.eds 2 vcan0 &
install/eposlib/lib/eposlib/epos4_sim $CFG/epos4.eds 3 vcan0 &
```

The master DCF must describe every node it should boot; see
[Multiple drives](../network/multi-drive.md).

!!! note "In a container"
    A container started with `--privileged` (and without `--network host`) has its own
    network namespace, and can create its own `vcan0` without touching the host's network.
    That is how the examples of this documentation are tested.

## What it models

`epos4_sim` loads the **real maxon EDS**, so it answers with the maxon identity, accepts the
master's configuration download and enforces each object's data type and access - a field
written with the wrong width is refused exactly as the drive refuses it.

| Feature | Modelled |
|---|---|
| NMT, boot, heartbeat (producer and consumer) | yes, including the `0x8130` fault when the master's heartbeat stops |
| PDO mapping from the concise DCF | yes |
| CiA 402 state machine, fault reset | yes |
| Profile Position (PPM) | yes |
| Cyclic Synchronous Position, Velocity, Torque | yes |
| Homing | yes, every method of section 3.5.3, against a virtual axis with end stops, limit switches, a home switch and an index pulse |
| Digital outputs, touch probe | yes |
| Analog I/O, supply voltage, temperature, current | yes, with plausible values (48 V, 35 °C) |
| Profile Velocity (PVM) | **no** |
| Physics: inertia, load, gravity, friction | **simplified**: motion is kinematic |
| Values the drive computes and the EDS leaves at 0 | partly: `0x6076` (rated torque) and `0x3000:05` (main sensor resolution) read 0 |

The last row matters for torque: in the simulator «Motor rated torque» is whatever the EDS
default says, 0, so anything that converts N m to thousandths reports that the motor data
is missing. To exercise torque in N m, give the simulator an EDS whose `[6076]`
`DefaultValue` is non-zero.

## Recipes

**A fault on demand.** Kill the master process with `SIGKILL` while the drive is enabled:
300 ms later the simulated drive raises `0x8130` (heartbeat lost), as the real one does.
The next program that starts sees the fault, and `ClearFault()` recovers it.

**A drive that disappears.** Stop `epos4_sim`: `IsCyclicHealthy()` turns false within 50 ms,
and SDO reads time out.

**Watching the traffic.** `candump -tz vcan0` in another terminal shows exactly what goes on
the bus, frame by frame.
