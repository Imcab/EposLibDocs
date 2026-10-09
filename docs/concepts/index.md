# Concepts

The ideas the rest of the documentation takes for granted. None of them is long, and each
one explains a class of problems before it happens.

- **[Architecture](architecture.md)** - what `CanBus` and `Epos4` own, which thread runs
  what, and why a call is fast or slow.
- **[CANopen primer](canopen.md)** - NMT, SDO, PDO, SYNC, heartbeat and EMCY: the six kinds
  of message on the bus, and what each one is for.
- **[The CiA 402 state machine](state-machine.md)** - the eight states of a drive, how
  `Enable()` walks through them, and why faults are never cleared for you.
- **[Units](units.md)** - quadcounts, rpm, thousandths of rated torque, and how to work in
  degrees and newton metres instead.
- **[Threading and timing](threading.md)** - which calls block, which are lock-free, and
  the rules for calling them from a control loop.
