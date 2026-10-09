# Getting Started

From an empty machine to a motor that moves, in order:

1. **[Requirements](requirements.md)** - the drives, adapters and software EposLib works with.
2. **[Installation](installation.md)** - build the library with `colcon`, in Docker, on a
   Jetson, or with plain CMake.
3. **[Setting up the CAN bus](can-setup.md)** - wiring, termination, bit rate and bringing
   up `can0`.
4. **[First run](first-run.md)** - talk to a drive for the first time and check that it is
   configured the way the master expects.
5. **[Simulation](simulation.md)** - run everything against `epos4_sim` on a virtual CAN
   bus, with no hardware at all.
6. **[Using EposLib in your package](your-package.md)** - `find_package(eposlib)`, your own
   network description, and the CMake helpers.

If you only want to see the API, jump to the [minimal example](../index.md#minimal-example)
or the [Examples](../examples/index.md).

!!! tip "No hardware yet?"
    Everything in these pages except the CAN wiring works against the simulator. Do
    [Installation](installation.md), then [Simulation](simulation.md), and come back to
    the bus setup when the drive arrives.
