# Boot and PDOs

When the master starts, it **boots** every node in its DCF: checks the node's identity, then
downloads its configuration - the concise DCF, `node_N.bin` - which is where the PDO mapping
and the heartbeat consumer come from. If a step fails, the node usually still answers SDO,
so `WaitUntilReady()` succeeds, but no PDO ever arrives in the layout the master expects.

Check both after `WaitUntilReady()`:

```cpp
auto boot = motor.GetBootStatus();                 // ok, or the letter of the failed step
std::vector<epos4::signals::PdoMismatch> diff;
motor.CheckPdoMapping(diff);                       // empty when both ends agree
```

or run the [device information](../examples/device-info.md) example, which prints both.

## A configuration write was refused { #cob-ids }

```text
error: SDO abort code 06010000 received while updating the configuration of node 05:
       Unsupported access to an object
PDO           : NO (SDO only)
mapping PDO   : 14 DIFFERENCES
  RPDO1 COB-ID is 0x00000201, the master expects 0x00000205
  ...
```

The master's configuration download stopped at a write the drive refused, so the drive
kept the PDO configuration it had saved. The most common cause - and the one in this output -
is **COB-IDs saved for another node-ID**: the drive at node 5 has PDO COB-IDs of node 1
(`0x201`, `0x181`).

The concise DCF first disables each PDO while giving it its new COB-ID
(`0x1400:01 = 0x80000205`). CiA 301 forbids changing the CAN-ID of a PDO that is valid, so a
drive whose RPDO1 is valid at `0x201` refuses that write - and the download stops there.
This happens when a drive was configured and saved as one node-ID and later moved to
another: saved COB-IDs are absolute values.

**Fix it once, without touching the motor data:** for each PDO, first invalidate it keeping
its old ID, then give it the new one, then save. With `can-utils`, with no other program
running, for a drive at node 5 whose PDOs carry node 1's IDs:

```bash
cansend can0 000#8005                    # node 5 to pre-operational
# per PDO: 0x8000_0000 | old ID, then 0x8000_0000 | new ID (expedited SDO download, 4 bytes)
cansend can0 605#2300140101020080   # RPDO1 0x1400:01 = 0x80000201
cansend can0 605#2300140105020080   #                 = 0x80000205
cansend can0 605#2301140101030080   # RPDO2 0x1401:01 = 0x80000301
cansend can0 605#2301140105030080   #                 = 0x80000305
cansend can0 605#2302140101040080   # RPDO3 0x1402:01 = 0x80000401
cansend can0 605#2302140105040080   #                 = 0x80000405
cansend can0 605#2303140101050080   # RPDO4 0x1403:01 = 0x80000501
cansend can0 605#2303140105050080   #                 = 0x80000505
cansend can0 605#2300180181010080   # TPDO1 0x1800:01 = 0x80000181
cansend can0 605#2300180185010080   #                 = 0x80000185
cansend can0 605#2301180181020080   # TPDO2 0x1801:01 = 0x80000281
cansend can0 605#2301180185020080   #                 = 0x80000285
cansend can0 605#2302180181030080   # TPDO3 0x1802:01 = 0x80000381
cansend can0 605#2302180185030080   #                 = 0x80000385
cansend can0 605#2303180181040080   # TPDO4 0x1803:01 = 0x80000481
cansend can0 605#2303180185040080   #                 = 0x80000485
cansend can0 605#2310100173617665   # 0x1010:01 = "save"
```

Watch `candump can0` while sending: each request should be answered with `585#60...`
(accepted); `585#80...` is a refusal, with the abort code in the last four bytes. Then
power-cycle the drive. The next boot downloads the configuration completely, and
`CheckPdoMapping()` comes back empty. The same writes can be made in EPOS Studio's object
dictionary, in the same order.

The frame layout, for another node-ID: `6NN#23 LL HH SS d0 d1 d2 d3` - command `0x23`
(write 4 bytes), index low/high byte, sub-index, value little-endian.

!!! danger "Not RestoreDefaults()"
    Restoring factory defaults also fixes the COB-IDs - and erases the motor data and
    every tuned gain with them.

Other writes that can be refused during the download: an object the network description
writes that does not exist on this hardware or firmware (abort `0x06020000`), or a value
out of range (`0x06090030`). The abort message names the object.

## Identity mismatch ('D', 'M')

The DCF records the vendor-ID and product code of the EDS it was generated from. A node that
answers with another identity is not booted: `'D'` for the vendor (not a maxon drive, or
another device at that node-ID), `'M'` for the product code (another EPOS4 variant than the
EDS describes). Use the EDS of your variant, or fix the node-ID.

## "General error" reading 1000 at start { #1000-general-error }

```text
NMT: entering operational state
error: SDO abort code 08000000 received on upload request of object 1000 (Device type)
       to node 05: General error
listo
```

Once, at start-up, followed by a working node: harmless. The master resets the nodes'
communication as it starts and immediately begins the boot by reading `0x1000`; a drive
still resetting answers that first request with a general error. When the drive announces
itself a moment later the master boots it again, successfully - the proof is that the PDO
mapping, downloaded in that same boot, is in place. If it repeats, or PDOs stay inactive,
it is not this.

## No PDOs, no error

`IsPdoActive()` stays false and nothing was refused:

- **SYNC disabled.** `sync_period: 0` in `bus.yml`: synchronous TPDOs are never sent.
- **The device was constructed after `Start()`.** The master routes a node's PDOs only to a
  driver registered when it booted the node. Declare every device before `Start()`.
- **The DCF does not describe the node**, or describes another node-ID.
- **The node is in pre-operational.** After a heartbeat loss the drive drops to
  pre-operational and stops its PDOs; `ClearFault()` resets its communication and the master
  boots it again.

## Plausible but wrong values { #plausible-but-wrong-values }

A position that is really a velocity, a Statusword that is half of something else: the
drive and the master decode a PDO with different layouts. Nothing fails - CAN has no idea
what the bytes mean. `CheckPdoMapping()` turns it into a list of differences; fix the cause
(usually a failed configuration download, above), never the symptom.
