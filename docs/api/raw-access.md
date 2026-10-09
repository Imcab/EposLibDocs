# Raw object access

The object dictionary has more objects than deserve a method. For anything the API does not
wrap:

```cpp
template<typename T> std::error_code ReadObject(od::Entry entry, T & value);
template<typename T> std::error_code WriteObject(od::Entry entry, T value);
```

```cpp
// An index constant needs od::At(index, subindex):
std::int16_t torque{};
motor.ReadObject(epos4::od::At(epos4::od::cia402::kTorqueActualValue), torque);

// A sub-index constant is already an od::Entry:
std::uint32_t nominal{};
motor.ReadObject(epos4::od::maxon::kMotorData_NominalCurrent, nominal);

// A literal address works too:
std::uint8_t pairs{};
motor.ReadObject(epos4::od::At(0x3001, 0x03), pairs);

motor.WriteObject<std::uint32_t>(epos4::od::At(epos4::od::cia402::kProfileAcceleration), 5000);
```

`T` is one of `std::uint8_t`, `std::int8_t`, `std::uint16_t`, `std::int16_t`,
`std::uint32_t`, `std::int32_t` - the six types the library instantiates; any other type
fails to link. It must match the object's data type in width. A wrong width is refused by the drive with an abort code (`0x06070010` *data type does not
match*). The [Object dictionary](../reference/object-dictionary.md) lists every object and
its constant; the EDS gives the data type (`DataType=0x7` is UNSIGNED32, `0x6` UNSIGNED16,
`0x5` UNSIGNED8, `0x4` INTEGER32, `0x3` INTEGER16, `0x2` INTEGER8).

Both are always SDO transfers - one round trip each - and return `not_connected` before
`Start()`.

!!! warning "Writing objects the library manages"
    Writing the Controlword, the mode of operation or a target through `WriteObject()`
    bypasses the library's own copy of them: the next library call, or the next SYNC, writes
    the library's value again. Use the API for anything it covers, and `WriteObject()` for
    what it does not.

## The od namespaces

| Namespace | Range | Portable |
|---|---|---|
| `od::comm` | CiA 301 communication objects, `0x1000`-`0x1FFF` | any CANopen device |
| `od::maxon_comm` | maxon communication objects, `0x2000`-`0x2FFF` | EPOS4 |
| `od::maxon` | maxon manufacturer objects, `0x3000`-`0x5FFF` | EPOS4 |
| `od::cia402` | CiA 402 drive profile, `0x6000`-`0x6FFF` | any CiA 402 drive |

No other file of the library contains a bare hexadecimal object address: every index has a
name here.
