# Authors

<div class="author-card" markdown>

<img class="author-card__photo" src="https://github.com/Imcab.png?size=240" alt="Imad Jared Cabrera Trejo" width="120" height="120">

<div markdown>

### Imad Jared Cabrera Trejo

**Author and maintainer of EposLib**

Robotics engineer and student at Tecnológico de Monterrey (CEM). FIRST alumni, software
mentor of FRC team 3472, founder of STZ Robotics and member of QuantumRobotics.

[:fontawesome-brands-github: @Imcab](https://github.com/Imcab){ .md-button .md-button--primary }
[EposLib](https://github.com/Imcab/EposLib){ .md-button }
[ros2units](https://github.com/Imcab/ros2units){ .md-button }

</div>

</div>

## About the project

EposLib started as the motor layer of a rover: maxon EPOS4 drives on a CAN bus, controlled
from a ROS 2 computer, with an API that should feel as direct as the ones FRC teams use for
their motor controllers - a device object, configuration groups, control requests and status
signals. Its design follows that idea, and owes a lot to CTRE's Phoenix 6.

## Contributing

Issues and pull requests are welcome on [GitHub](https://github.com/Imcab/EposLib):

- **Bugs** - with the output of the [device information](../examples/device-info.md)
  example, the firmware version, and a `candump` of the problem if it is on the bus.
- **Hardware reports** - a feature verified on a real drive is worth as much as code. The
  library is pre-1.0 until the cyclic path and homing are verified on hardware.
- **Documentation** - this site lives in [Imcab/EposLibDocs](https://github.com/Imcab/EposLibDocs).
  Every example is compiled and run against the simulator before publishing
  (`scripts/check_examples.sh`, `scripts/check_ros2.sh`).

## License

EposLib, its examples and this documentation are released under the
[Apache License 2.0](https://github.com/Imcab/EposLib/blob/main/LICENSE).

EposLib is an independent project and is not affiliated with maxon. maxon's logo, photos
and figures belong to maxon; the ROS logo to Open Robotics. See [Credits](../reference/credits.md).
