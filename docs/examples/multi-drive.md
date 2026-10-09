# Multiple drives

Two drives on one bus - a left and a right wheel at nodes 2 and 3 - driven forward and then turned in place. Both receive their setpoints on the same SYNC. It uses the two-drive network in `examples/config/two_drives/bus.yml`, installed with the examples.

## Key points

- One `CanBus`, every device declared before `Start()`.
- `RefreshAll()` over signals of different drives reads them in parallel.
- Each wheel is checked with `IsCyclicHealthy()` every cycle.

## Step by step

1. **One `CanBus`**, two devices at nodes 2 and 3, declared before `Start()`. The DCF (from `config/two_drives/bus.yml`) describes both.
2. **`RefreshAll()`** across the two drives reads both supplies in parallel - each drive has its own SDO channel.
3. **Enable and enter CSV on both.** If either fails, both are disabled.
4. **The loop** stages a velocity per wheel each period: forward, then turn in place. Both setpoints leave in the RPDOs of the same SYNC, so the wheels act together.
5. **Health per wheel** every cycle; stop both if one fails.

## Running it

```bash
ros2 run eposlib_examples multi_drive \
  install/eposlib_examples/share/eposlib_examples/config/two_drives/master.dcf can0
```

## Source

```cpp title="examples/src/multi_drive.cpp"
--8<-- "src/multi_drive.cpp"
```

## Output against epos4_sim

```text
supply: left 48.0 V, right 48.0 V
  t=0.00s  left     0 rpm  right     0 rpm
  t=0.50s  left   799 rpm  right   799 rpm
  t=1.00s  left   799 rpm  right   799 rpm
  t=1.50s  left   799 rpm  right   799 rpm
  t=2.00s  left   799 rpm  right   799 rpm
  t=2.50s  left   600 rpm  right  -599 rpm
  t=3.00s  left   600 rpm  right  -599 rpm
  t=3.50s  left   600 rpm  right  -599 rpm
```
