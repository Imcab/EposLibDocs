# PDO mapping

## What the example network maps

| Channel | Direction | Objects | Used by |
|---|---|---|---|
| RPDO1 | master → drive | Controlword `0x6040` (16) + Target position `0x607A` (32) | every command; PPM and CSP targets |
| RPDO2 | master → drive | Target velocity `0x60FF` (32) + Target torque `0x6071` (16) | PVM and CSV targets; CST target |
| TPDO1 | drive → master | Statusword `0x6041` (16) + Position actual `0x6064` (32) | state, position |
| TPDO2 | drive → master | Velocity actual `0x606C` (32) + Torque actual `0x6077` (16) | velocity, torque |

All four are synchronous (transmission type 1). This is enough for every operating mode
the library drives: the cyclic path publishes the Controlword and the active mode's target
on each SYNC, and the cached feedback (`GetCachedPosition/Velocity/Torque/Statusword()`)
comes from the two TPDOs.

## How the library uses it

You never touch PDOs directly. The library looks at the mapping the master holds:

- **Reads prefer PDO.** A status signal whose object arrives in a TPDO is served from the
  last received value - no bus traffic - as long as a PDO arrived in the last **100 ms**.
  After that it asks the drive over SDO, so a drive that dropped to pre-operational is not
  reported from stale data.
- **Writes prefer PDO.** A write to an object mapped into an RPDO - the Controlword, a
  target - goes into the PDO and leaves with the next SYNC. Writing it over SDO would last
  until the next SYNC, when the master's own copy overwrites it. Objects that are not
  mapped are written over SDO.
- **The cyclic path needs it.** `EnterCyclic*Mode()` publishes through the RPDOs; without
  the mapping there is nothing to publish on, and `IsCyclicHealthy()` stays false.

`motor.IsPdoActive()` turns true on the first PDO received, and
`motor.GetTimeSinceLastPdo()` says how long ago the last one was.

## Checking that both ends agree

A PDO carries no description of itself. If the drive's mapping differs from the master's,
nothing fails: the master decodes the frames with its own layout and reads plausible,
wrong values. Check after boot:

```cpp
std::vector<epos4::signals::PdoMismatch> mismatches;
if (auto ec = motor.CheckPdoMapping(mismatches)) {
  // no_such_file_or_directory: the network configures nothing on this node
  // bad_message: the master's concise DCF is malformed
} else {
  for (const auto & m : mismatches) {
    std::printf("%s\n", epos4::signals::Describe(m).c_str());
  }
}
```

It reads the drive's four RPDOs and four TPDOs over SDO and compares them with the concise
DCF the master downloads to the node (`0x1F22`), and lists:

- every PDO object whose value on the drive differs from what the master configured
  (`Kind::kValue`), and
- every channel valid on the drive that the network description does not configure
  (`Kind::kNotConfigured`).

```text
RPDO1 COB-ID is 0x00000201, the master expects 0x00000205
TPDO2 object 2 is 0x606C:00/32, the master expects 0x6077:00/16
TPDO3 is valid on the drive (CAN-ID 0x381) but the network description does not configure it
```

An empty list means every frame's layout matches. A non-empty list nearly always means the
master's configuration download failed - see [Boot and PDOs](../troubleshooting/boot-and-pdo.md).

To see the mapping itself:

```cpp
epos4::signals::PdoMapping mapping;
motor.ReadPdoMapping(mapping);
std::printf("%s", epos4::signals::Describe(mapping).c_str());
// RPDO1  0x202  sync   0x6040:00/16 0x607A:00/32  (48 bits)
// ...
```

## Changing the mapping

Edit `bus.yml` and rebuild; the master writes the new mapping to the drive on the next boot.
Do not remap at run time over SDO: it would change the drive and leave the master's own
dictionary describing the old layout.

Rules for a mapping:

- At most **64 bits** per PDO (8 bytes).
- An object must be PDO-mappable on the drive; the EDS says which (`PDOMapping=1`). Supply
  voltage and power stage temperature, for example, are not, and are always read over SDO.
- Keep the four objects the library's cyclic path uses - Controlword, Statusword and the
  targets of the modes you use - or that mode cannot run cyclically.

Example: adding the following error to TPDO3, at 100 Hz:

```yaml
  tpdo:
    3:
      cob_id: "auto"
      transmission: 1
      mapping:
        - {index: 0x60F4}   # Following error actual value, 32 bit
```

`GetFollowingError().Refresh()` then reads it from the PDO instead of over SDO.

!!! note "Bus load"
    Each synchronous PDO is one frame per SYNC. Four PDOs per drive at 100 Hz is 400
    frames per second per drive. See [Multiple drives](multi-drive.md#bus-load) before adding
    channels on a crowded bus.
