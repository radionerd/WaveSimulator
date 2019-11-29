# Ship Motion Simulator

## Welcome

This is the repository for the Ship Motion Simulator design files

The design is contained in these folders:

* firmware   - Arduino .ino source file and status screenshot

* mechanical - LibreCAD drawings and templates - print the templates, glue on to mdf for cutting and drilling

* schematic  - EaglePCB schematics - also provided as pdf

* parts      - A list of all the purchased parts and where to get them

## Arrangement

The motion platform is controlled by an Arduino Nano mounted on the underside.

The Arduino Nano USB serial connection provides optional command line control and a status screen.

An optional plug in control panel provides convenient user controls using switches, potentiometers and a status LED.

## Background

The simulator was designed to approximate the pitch, roll and heave motions of a ship at sea using low cost model control servos.

For a more accurate simulation consider a Stewart Plaform with six degrees of freedom using linear actuators but higher cost and complexity.

The design was built and tested using the following software:

| Application | Version |
| ----------- | ------- |
|  Arduino  | 1.8.9 |
|  EaglePCB | 8.3.1 |
|  LibreCAD | 2.1.2 |
|  Ubuntu   | 18.04 |

## Photos and Videos

[Video](https://www.youtube.com/watch?v=MUajh_KlW8U) - Motion Research Ltd

[Construction Photos and Videos](https://photos.app.goo.gl/xF3chL2MP4UncZHGA)

## License

Copyright (C)2019 richard.jones.1952@gmail.com

[License GPL V3.0](https://www.gnu.org/licenses/gpl-3.0.en.html)
