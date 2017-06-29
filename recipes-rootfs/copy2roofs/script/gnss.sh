#! /bin/bash

NAME=gnss
DESC="Initialization of gnss chip"

case $1 in
start)
	# GNSS reset
	# GPIO4 IO28 => (4 - 1) * 32 + 28 = 124
	echo "124" > /sys/class/gpio/export
	echo "out" > /sys/class/gpio/gpio124/direction
	echo "1" > /sys/class/gpio/gpio124/value
	# GNSS force on set to low
	# GPIO4 IO27 => (4 - 1) * 32 + 27 = 123
	echo "123" > /sys/class/gpio/export
	echo "out" > /sys/class/gpio/gpio123/direction
	echo "0" > /sys/class/gpio/gpio123/value
	# GNSS sleep ctrl
	# GPIO4 IO26 => (4 - 1) * 32 + 26 = 122
	echo "122" > /sys/class/gpio/export
	echo "out" > /sys/class/gpio/gpio122/direction
	echo "1" > /sys/class/gpio/gpio122/value
;;

stop)
	# GNSS sleep ctrl (wake up)
	echo "0" > /sys/class/gpio/gpio122/value
;;

*)
	echo "Usage $0 {start|stop}"
	exit
esac
