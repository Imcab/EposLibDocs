# Hardware

The EPOS4 variants `signals::HardwareName()` recognises, from Table 6-66 of the firmware
specification. The code is the high word of the product code (`0x1018:02`), what
`DeviceIdentity::HardwareVersion()` returns. A code missing here means hardware newer than
the library's tables: `HardwareName()` returns `nullptr` rather than guess.

--8<-- "hardware-codes.md"

The Module and the Compact of the same rating share every code: they are the same
electronics in a different housing. The power stage is what matters, because it sets the
current limits - and the master already refuses to boot a drive whose product code differs
from the DCF's, so a wrong model fails at boot rather than running with another model's
limits.

## Variant-dependent features

| Feature | Not available on |
|---|---|
| High-speed digital inputs | Disk 60/8, Disk 60/12, Micro 24/1.5, Micro 24/5; disabled whenever sensor 2 is configured |
| Digital incremental encoder 2 (sensor slot 2) | Disk 60/8, Disk 60/12, Micro 24/1.5, Micro 24/5 |
| High-speed digital output 2 | everything except Disk 60/8 and Disk 60/12 |
| Holding brake opening / retaining voltage (`0x3158:04/05`) | everything except Disk 60/8, Disk 60/12, Module/Compact 60/20 |
| Analog output 2 | Disk 60/8, Disk 60/12, Micro variants |
| STO inputs (`0x3202:01`) | Disk and Micro variants |
| STO card status (`0x3202:02`) | everything except Module/Compact 60/20 |
| RS232 | EtherCAT variants |

Configuration fields for these objects are marked hardware-dependent: `Refresh()` leaves
them unset when absent instead of failing.
