# Kromek Sensor

Provides a ROS driver for interfacing with Kromek sensors.

## Overview

The ROS driver requires `spectrometer_driver` which needs `kromek_driver` which relies on the `kromekusb` kernel module.

`catkin build` will build the user components; create the `kromekusb` kernel module manually using the install script in that folder.

## Topics

- /kromek/raw: outputs a std_msgs/UInt32MultiArray message with hit counts in 100 bins for each detector attached.
- /kromek/info: outputs a std_msgs/UInt32MultiArray message with an array of device serial numbers.
- /kromel/sum: outputs a std_msgs/Int64 with the sum of hit counts of all detectors.

By default detectors sense for 1 seconds, so these messages are output every 1 seconds.
The `integration_seconds` param can be set in the launch file.
