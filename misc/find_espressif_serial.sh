#!/bin/bash

for f in `ls /dev/tty[A-Z]*`; do
    out=`udevadm info -q property --export -p $(udevadm info -q path -n $f) | \
        grep ID_SERIAL=`
    if [[ "$out" == *"Espressif_USB_JTAG_serial_debug_unit"* ]] ; then
	echo $f
    fi
done
