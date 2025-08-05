This project is used for a device which is made from two components:
  - An ESP32-S3 device (E.g. Seeed XIAO ESP32-S3)
  - A meshtastic device (E.g. Heltec V3)

The system communicates on a Meshtastic network and accepts commands as
direct messages from a requesting user to control various outputs. Since the
ESP32-S3 has WIFI networking and runs FreeRTOS, there is a lot more advanced
things which can be performed. The first application which this project
was created for is to switch antennas and turn on/off bi-directional LNA.
Later we'll try to mount a yagi antenna on a rotator and use this platform
to control the rotator.
