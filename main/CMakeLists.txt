# CMakeLists.txt
#
# Copyright (C) 2025, Charles Chiou

set(LIBMESHTASTIC_PATH ../libmeshtastic)
set(MESHARDUINO_PATH ${LIBMESHTASTIC_PATH}/Meshtastic-arduino/src)
file(GLOB MESHTASTIC_PROTOS_SRCS "${MESHARDUINO_PATH}/meshtastic/*.c")
set(MESHARDUINO_SRCS
  ${MESHARDUINO_PATH}/pb_common.c
  ${MESHARDUINO_PATH}/pb_encode.c
  ${MESHARDUINO_PATH}/pb_decode.c
  )
set(LIBMESHTASTIC_SRCS
  ${LIBMESHTASTIC_PATH}/serial-esp32.c
  ${LIBMESHTASTIC_PATH}/protocol.c
  ${LIBMESHTASTIC_PATH}/SimpleClient.cxx
  ${LIBMESHTASTIC_PATH}/HomeChat.cxx
  ${LIBMESHTASTIC_PATH}/BaseNVM.cxx
  ${LIBMESHTASTIC_PATH}/SimpleShell.cxx
  )
idf_component_register(
  SRCS
  "${MESHTASTIC_PROTOS_SRCS}" "${MESHARDUINO_SRCS}" "${LIBMESHTASTIC_SRCS}"
  "serial.c"
  "MeshRoof.cxx"
  "MeshRoofShell.cxx"
  "EspWifi.cxx"
  "meshroof.cxx"
  INCLUDE_DIRS "." "${LIBMESHTASTIC_PATH}" "${MESHARDUINO_PATH}"
  "${CMAKE_CURRENT_BINARY_DIR}/../../"
  REQUIRES esp_driver_gpio esp_driver_uart esp_driver_usb_serial_jtag console
           esp_timer esp_netif esp_wifi nvs_flash
  )

