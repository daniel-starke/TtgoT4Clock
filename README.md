TTGO T4 Clock
=============

Simple clock using the TTGO T4 V1.3 development board.

See board [schema](etc/T4_v1.3.pdf).

Building
--------

The following dependencies are given.  
- C++14
- [PlatformIO](https://platformio.org/)
- [Arduino for ESP32](https://github.com/espressif/arduino-esp32)
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
- [ESPNtpClient](https://github.com/gmag11/ESPNtpClient)

Change to source code `src/main.cpp` to configure the system:
- `WIFI_SSID` - SSID of the used WIFI
- `WIFI_PASS` - password for the given SSID
- `NTP_SERVER` - NTP server address (e.g. the network router or pool.ntp.org)
- `TFT_PASS_COLOR` - text color when within the given timespan (RGB565)
- `TFT_FAIL_COLOR` - text color when outside the given timespan (RGB565)
- `TFT_PASS_FROM` -  `HH:MM` string of the timespan start
- `TFT_PASS_TO` -  `HH:MM` string of the timespan end
- `tftBl` - list of possible TFT brightness values (0..255) selectable via buttons
- `tftBlIndex` -  default TFT brightness as defined at this index in `tftBl`

To build and upload the firmware:
```sh
pio run -t upload
```

Housing
-------

A simple case is provided as 3D printable files.
- [etc/case.FCStd](etc/case.FCStd) - FreeCAD construction file
- [etc/case-Bottom.stl](etc/case-Bottom.stl) - bottom part of the case for 3D printing
- [etc/case-Top.stl](etc/case-Top.stl) - top part of the case for 3D printing

Use the given orientation for printing and the following parameters:
- layer height: 0.2mm
- infill: ≥15%
- solid layers: ≥5 top, ≥4 bottom
- perimeters: ≥3
- supports: none

Required for assembly:
- 4x M2x12mm countersink screws

There is no need to create threads for the screws if the housing has been
printed with the correct tolerance.

RGB565 Color Picker
-------------------

See https://rgbcolorpicker.com/565

License
-------

See [LICENSE](LICENSE).  
