# Step response

Command a reference and record how the axis follows it, to CSV: the raw material for tuning gains and for the plots of this documentation. Four modes: a Profile Position move, a CSV velocity trapezoid, a CSP sine, and a CST torque step.

## Key points

- Records the lock-free cached feedback every SYNC period - no bus traffic added.
- The target column is in the mode's own unit, so target and actual compare directly.
- Change one gain, record again, overlay the two CSVs: that is tuning.

## Step by step

1. **Choose a mode** on the command line; the recorder writes one row per SYNC period from the cached (PDO) feedback.
2. **profile** - a PPM move: the drive generates the ramp. (`epos4_sim` moves PPM at a fixed slow rate, so this trace is only meaningful on hardware.)
3. **velocity** - a trapezoid staged every period in CSV.
4. **position** - a 20° sine in CSP.
5. **torque** - a 0.05 N·m step in CST, cut off above 1500 rpm.

Plot the CSV with anything - the documentation's own plots come from `scripts/plots.py`.

<figure markdown="span">
  ![Step response](../assets/plots/step_velocity.svg)
  <figcaption>Output of `step_response ... velocity`, plotted.</figcaption>
</figure>

## Running it

```bash
ros2 run eposlib_examples step_response \
  install/eposlib/share/eposlib/config/epos4_network/master.dcf can0 2 velocity out.csv
```

## Source

```cpp title="examples/src/step_response.cpp"
--8<-- "src/step_response.cpp"
```

## Output against epos4_sim

```text
recorded velocity to /out/step_velocity.csv
```
