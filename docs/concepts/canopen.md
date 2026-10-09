# CANopen primer

CANopen is a protocol on top of CAN. You do not have to implement any of it - Lely does -
but every problem you will debug is phrased in its terms. This page covers what EposLib
uses.

## Nodes and the object dictionary

Every device on the network is a **node** with a **node-ID** from 1 to 127. Everything a
node knows - its configuration, its state, its measurements - is an entry in its **object
dictionary**, addressed by a 16-bit **index** and an 8-bit **sub-index**:

| Object | Meaning |
|---|---|
| `0x6041:00` | Statusword |
| `0x6064:00` | Position actual value |
| `0x3001:01` | Nominal current of the motor |
| `0x1018:02` | Product code |

The ranges are standard: `0x1000-0x1FFF` communication (CiA 301), `0x2000-0x5FFF`
manufacturer-specific (maxon), `0x6000-0x9FFF` device profile (CiA 402, drives). The
[Object dictionary](../reference/object-dictionary.md) lists every object EposLib names.

The **EDS** (Electronic Data Sheet) is the file that describes a device's object dictionary:
each object's type, access and default value. The **DCF** (Device Configuration File) is an
EDS with the values filled in for one particular network.

## The six kinds of message

Every CAN frame has an 11-bit identifier, the **COB-ID**. In CANopen most COB-IDs are a
base plus the node-ID, which is how the receiver knows what a frame is and who sent it.

| Message | COB-ID | Direction | Purpose |
|---|---|---|---|
| **NMT** | `0x000` | master → nodes | change a node's network state |
| **SYNC** | `0x080` | master → all | the clock tick of the cyclic modes |
| **EMCY** | `0x080` + node | node → all | "an error just happened" |
| **TPDO 1-4** | `0x180/0x280/0x380/0x480` + node | node → master | feedback |
| **RPDO 1-4** | `0x200/0x300/0x400/0x500` + node | master → node | setpoints |
| **SDO** | `0x600` + node / `0x580` + node | request / answer | read or write one object |
| **Heartbeat** | `0x700` + node | node → all | "I am alive, and in this state" |

PDOs are named from the **node's** point of view: the drive *transmits* TPDOs (its feedback)
and *receives* RPDOs (your setpoints).

### One SYNC period on the bus

<div class="epos-anim" data-anim="bus"></div>

### NMT: network management

Each node has a network state:

| State | SDO | PDO | |
|---|---|---|---|
| Initialisation | | | booting; sends a boot-up message when done |
| Pre-operational | yes | no | configuration happens here |
| Operational | yes | yes | normal running |
| Stopped | no | no | |

The master moves nodes between them with NMT commands: *start*, *stop*, *enter
pre-operational*, *reset node*, *reset communication*. At start-up Lely's master **boots**
every node described in its DCF (CiA 302-2): checks its identity against the DCF, downloads
its configuration - the *concise DCF* - and starts it. A step that fails leaves the node
unconfigured; [Boot and PDOs](../troubleshooting/boot-and-pdo.md) lists the error letters.

### SDO: one object at a time

A **Service Data Object** transfer is a request and a confirmed answer: "read `0x6064:00`",
"write 2000 to `0x6083:00`". Reliable, addressed, and slow - two frames and a round trip per
object. A refused request is answered with an **SDO abort code** that says why
(`0x06010002` *write command to a read only object*); EposLib returns it as the
`std::error_code` of the call. See [SDO abort codes](../reference/sdo-abort-codes.md).

### PDO: process data, no questions asked

A **Process Data Object** is a frame of up to 8 bytes with no protocol overhead: its layout
- which objects, in which order, how many bits each - is agreed in advance by the **PDO
mapping**. The example network maps:

| PDO | Carries | Bits |
|---|---|---|
| RPDO1 | Controlword `0x6040` + Target position `0x607A` | 16 + 32 |
| RPDO2 | Target velocity `0x60FF` + Target torque `0x6071` | 32 + 16 |
| TPDO1 | Statusword `0x6041` + Position actual `0x6064` | 16 + 32 |
| TPDO2 | Velocity actual `0x606C` + Torque actual `0x6077` | 32 + 16 |

Each PDO has a **transmission type**: `1` means *synchronous* - a TPDO is sent, and a
received RPDO applied, on every SYNC. That is what makes all axes act on the same instant.

Because nothing in a PDO says what it contains, **both ends must agree on the mapping**. If
they do not, nothing fails: the master decodes the frame with its own layout and reads
values that look plausible and are wrong. EposLib's `CheckPdoMapping()` exists for that -
see [PDO mapping](../network/pdo-mapping.md).

### SYNC

The master broadcasts SYNC at a fixed period, 10 ms (100 Hz) in the example network. Drives
sample their feedback and apply their received setpoints on it.

### Heartbeat

Every node announces its NMT state periodically. A node configured as a **heartbeat
consumer** watches another node's heartbeat and reacts when it stops - the EPOS4 raises
`0x8130` *CAN heartbeat error* and stops the motor. In the example network each drive
watches the master's heartbeat, so a crashed control program stops the drives within 300 ms.
See [SYNC and heartbeat](../network/sync-heartbeat.md).

### EMCY

When an error occurs, the drive sends one EMCY frame with the error code - unsolicited, the
moment it happens. EposLib delivers it to `SetEmergencyCallback()` and keeps the last code
for `GetCachedErrorCode()`. An EMCY with code 0 announces that the errors were reset.
