# Installation

EposLib is a plain CMake project with a `package.xml`, so the same sources build three
ways: in a ROS 2 workspace with `colcon`, in Docker, or with CMake alone.

## In a ROS 2 workspace

```bash
sudo apt install ros-humble-lely-core-libraries python3-vcstool

mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
git clone https://github.com/Imcab/EposLib.git
vcs import . < EposLib/epos.repos        # clones ros2units next to it

cd ~/ros2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-up-to eposlib
source install/setup.bash
```

`epos.repos` pins the one dependency that is not a Debian package, `ros2units`.
`ros-humble-lely-core-libraries` provides the Lely libraries **and** `dcfgen`, the tool that
turns a network description into the master's DCF at build time.

What gets installed:

| Path under `install/eposlib/` | Contents |
|---|---|
| `lib/libeposlib.so`, `lib/libeposlib_core.so` | the two libraries |
| `include/epos4/` | the public headers |
| `lib/eposlib/` | `epos4_sim`, `api_demo` and the three Lely walkthrough examples |
| `share/eposlib/config/epos4_network/` | the example network: `bus.yml`, `epos4.eds`, `master.dcf`, `node_2.bin` |
| `lib/cmake/eposlib/` | the package config for `find_package(eposlib)` |

### Running the tests

```bash
sudo apt install libgtest-dev
colcon build --packages-select eposlib
colcon test --packages-select eposlib && colcon test-result --verbose
```

None of the tests needs a bus or hardware. When `ament_uncrustify` is on the path, a style
check runs as one more test.

## Docker { #docker }

A container is the simplest way to get a Humble toolchain on a machine that runs another
distribution, and the way to give a whole team the same environment. A Dockerfile that
includes Lely:

```dockerfile
FROM osrf/ros:humble-desktop

ARG USERNAME=ros
ARG USER_UID=1000
ARG USER_GID=1000

RUN apt-get update && apt-get install -y \
    ros-humble-lely-core-libraries \
    python3-colcon-common-extensions python3-vcstool \
    can-utils libgtest-dev sudo git \
    && rm -rf /var/lib/apt/lists/*

# Same UID as the host user, so build/ install/ log/ in a mounted workspace
# are not owned by root.
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd -m -s /bin/bash --uid $USER_UID --gid $USER_GID $USERNAME \
    && usermod -aG dialout,plugdev $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME

USER $USERNAME
RUN echo 'source /opt/ros/humble/setup.bash' >> ~/.bashrc \
    && echo '[ -f ~/ros2_ws/install/setup.bash ] && source ~/ros2_ws/install/setup.bash' >> ~/.bashrc
WORKDIR /home/$USERNAME/ros2_ws
```

```bash
docker build -t ros2_humble_epos .
docker run -it --rm \
  --network host \
  --volume ~/ros2_ws:/home/ros/ros2_ws \
  ros2_humble_epos bash
```

`--network host` is what lets the container see the host's `can0`: SocketCAN interfaces
live in the network namespace. Bring the interface up **on the host**
([Setting up the CAN bus](can-setup.md)); the container uses it as it is.

## On an NVIDIA Jetson

Jetsons run Ubuntu on ARM64, and the Humble packages exist for it. Two things differ from
a laptop:

- **The CAN controller is on the module.** Load the drivers and bring it up:
  ```bash
  sudo modprobe can && sudo modprobe can_raw && sudo modprobe mttcan
  sudo ip link set can0 type can bitrate 1000000
  sudo ip link set can0 up
  ```
  The pins need a CAN transceiver between them and the bus; the Jetson does not have one.
- **Build on the Jetson itself** (or in an ARM64 container). Copying a workspace built on
  an x86 laptop does not work - the libraries are architecture-specific.

!!! warning "Keep the copy on the robot up to date"
    If you copy EposLib onto the robot by hand instead of cloning it, it does not follow the
    repository. After pulling a new version on your laptop, copy it again and rebuild on the
    robot - otherwise you are debugging a library the rest of the team no longer runs.

## Plain CMake, without ROS

EposLib does not use ROS. On any Linux with Lely installed:

```bash
git clone https://github.com/Imcab/ros2units.git
git clone https://github.com/Imcab/EposLib.git
cmake -S EposLib -B build -DCMAKE_PREFIX_PATH=$PWD/ros2units
cmake --build build -j
sudo cmake --install build          # or --prefix somewhere of your own
```

Lely is found through `pkg-config` (`liblely-coapp`) and ros2units through its header, so
`CMAKE_PREFIX_PATH` (and `PKG_CONFIG_PATH`, if Lely is somewhere unusual) is all it needs.
Lely itself can come from its PPA, from source, or from the ROS package.
