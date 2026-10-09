# CAN bus

## Start() throws { #start-throws }

`CanBus::Start()` throws `std::system_error` when it cannot open the interface or read the
DCF.

| Message mentions | Check |
|---|---|
| the interface | `ip link show can0` - does it exist? Is it `UP`? In a container, was it started with `--network host`? |
| `cannot read ...master.dcf` | the path; the DCF is generated at build time - did `dcfgen` run? (`eposlib_generate_dcf` warns when it is missing) |

## No answer from the drive { #no-answer }

`WaitUntilReady()` returns false: nothing answered an SDO request for 5 s. Work outwards:

1. **Is the drive powered and booted?** Its LED; its heartbeat should appear in `candump`
   as `0x700 + node-ID` every 500 ms in the example network.
2. **Does anything appear on the bus at all?** `candump -tz can0` while the program runs
   should show the master's SYNC (`0x080`) and heartbeat (`0x701`). Nothing at all means
   the interface is not transmitting - see below.
3. **The node-ID.** The program's node-ID, the `bus.yml` block and the drive's DIP switches
   (or `0x2000`) must agree. The drive's heartbeat ID tells you its node-ID: `0x705` is node 5.
4. **The bit rate.** Every node, and the interface, at the same rate - 1 Mbit/s in the
   examples.
5. **Termination.** About 60 Ω between CAN_H and CAN_L, power off.

## ERROR-PASSIVE or BUS-OFF { #error-passive-or-bus-off }

`ip -details -statistics link show can0` reports the controller state:

| State | Meaning |
|---|---|
| `ERROR-ACTIVE` | normal |
| `ERROR-WARNING` / `ERROR-PASSIVE` | many errors: frames are not being acknowledged |
| `BUS-OFF` | the controller gave up transmitting |

A controller transmitting alone on a bus - no other node to acknowledge its frames - goes
error-passive within milliseconds. So does one on a bus with a wrong bit rate, missing
termination, or swapped CAN_H / CAN_L. Fix the cause, then bring the interface down and up
again (or configure `restart-ms` / `RestartSec` to recover automatically).

The drive has the same problem from its side: fault `0x8120` *CAN passive mode* or `0x81FD`
*CAN bus turned off*.

## Interface does not exist

- **USB adapter:** `dmesg | tail` after plugging it in; `lsmod | grep -E "gs_usb|peak|kvaser"`.
- **Jetson:** `sudo modprobe mttcan`; the CAN pins need an external transceiver.
- **vcan:** `sudo modprobe vcan && sudo ip link add dev vcan0 type vcan`.

## Permissions

Bringing an interface up needs root (`sudo ip link ...`). Using it does not: once it is up,
an ordinary user can open a SocketCAN socket.

## Several programs on one bus

Only **one CANopen master** may run on a network. Two programs that each create a
`CanBus` on the same `can0` are two masters: both send SYNC and NMT commands, both boot the
nodes, and the result is confusing in every possible way - a drive found enabled that
nobody enabled, configuration downloads that collide. Check with `ps` that no earlier
program is still running, and that EPOS Studio is not connected to the same drive.
