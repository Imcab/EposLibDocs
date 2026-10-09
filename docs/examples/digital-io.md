# Digital I/O and touch probe

Read the inputs by function and by pin, assign and switch a general purpose output, read an analog input and set an analog output, then latch the exact position of the encoder's index pulse with the touch probe while the motor turns past it.

## Key points

- Function view (`0x60FD`) versus pin view (`0x3141`): the second is for checking wiring.
- An output function is assigned to a pin once, then switched by function.
- The touch probe latches inside the drive, to encoder resolution.

## Step by step

1. **Inputs**, by name, by function word (`0x60FD`) and by pin (`0x3141:01`).
2. **Output A** assigned to output 1 (`0x3151:01`), switched on, read back by function and by pin, switched off.
3. **Analog:** input 1 in volts; general purpose output A set to 1.5 V.
4. **Touch probe** armed on the index pulse, then a relative move of 4000 qc so the index passes; the latched position and edge count are read back and the probe disarmed.

## Running it

```bash
ros2 run eposlib_examples digital_io \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2
```

## Source

```cpp title="examples/src/digital_io.cpp"
--8<-- "src/digital_io.cpp"
```

## Output against epos4_sim

```text
negative limit 0, positive limit 0, home 0
functions 0x00000000, pins 0x0000
general purpose A: 0
output A: 1, pins 0x0001
analog input 1: 1.234 V
index latched at 2000 qc (1 edges)
```
