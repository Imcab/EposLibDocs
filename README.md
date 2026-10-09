# EposLib documentation

Source of the documentation site for [EposLib](https://github.com/Imcab/EposLib),
published at https://imcab.github.io/EposLibDocs/.

Kept apart from the library so that cloning EposLib onto a robot does not pull the
documentation with it.

## Layout

| Path | |
|---|---|
| `docs/` | the pages (Markdown), and `stylesheets/extra.css` for the look |
| `docs/reference/` | **generated** - do not edit by hand, run `scripts/generate.sh` |
| `examples/` | the `eposlib_examples` colcon package: every complete program the pages show |
| `includes/` | generated fragments the pages include (hardware table, changelog) |
| `scripts/check_examples.sh` | builds every example and runs it against `epos4_sim` |
| `scripts/generate.sh` | regenerates `docs/reference/` and `includes/` from EposLib |
| `scripts/gen_example_pages.py` | writes `docs/examples/*.md` from a `check_examples.sh` log |
| `scripts/snippets_check.cpp` | compile-only check of the API calls the pages show |
| `scripts/last_check.log` | the output of the last example run, shown on the example pages |

## Preview locally

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/mkdocs serve          # http://127.0.0.1:8000/EposLibDocs/
```

## After changing EposLib or an example

All three need Docker and an image with ROS 2 Humble and Lely
(`ros-humble-lely-core-libraries`); `IMAGE=...` picks it, default `ros2_humble_gazebo`.
EposLib and ros2units are taken from `~/ros2_ws/src` unless given as arguments.

```bash
scripts/generate.sh                                   # reference pages
SHOW=60 scripts/check_examples.sh > scripts/last_check.log   # build + run every example
python3 scripts/gen_example_pages.py scripts/last_check.log  # example pages with that output
```

`check_examples.sh` exits non-zero if an example fails to build (warnings count) or fails
against the simulator.

## Publish

```bash
.venv/bin/mkdocs build --strict   # broken links and anchors are errors
.venv/bin/mkdocs gh-deploy        # builds and pushes to the gh-pages branch
```
