# Reference

Tables generated from EposLib's own sources and data tables, so they always describe the
library they came from. Regenerate them with `scripts/generate.sh` after updating EposLib.

| Page | Contents | Source |
|---|---|---|
| [API reference](api/index.md) | every public member of every class, with its documentation | the headers |
| [Device error codes](error-codes.md) | all 73 errors of Table 7-186, with cause, effect, recovery and reaction | `signals::FindDeviceError()` |
| [SDO abort codes](sdo-abort-codes.md) | the 26 codes of Table 7-187 | `src/signals/Errors.cpp` |
| [Configuration fields](configuration-fields.md) | every field of every configuration group, with its object | `include/epos4/configs/` |
| [Enumerations](enumerations.md) | every enum of the public API, with its values | the headers |
| [Object dictionary](object-dictionary.md) | every object EposLib names, with its constant | `include/epos4/core/ObjectDictionary.hpp` |
| [Hardware](hardware.md) | the EPOS4 variants the library recognises | `signals::HardwareName()` |
| [Changelog](changelog.md) | what changed in each version | `CHANGELOG.md` |
| [Credits and sources](credits.md) | where every maxon image and fact comes from | - |

The object indices, bit layouts and error tables are transcribed from the **EPOS4 Firmware
Specification, edition 2026-07** and the **EPOS4 Communication Guide, edition 2026-04**,
both free to download from maxon. When this reference and the manual disagree, the manual
for your firmware wins - and the difference is worth reporting.
