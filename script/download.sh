#!/bin/bash

FW_FILE=$PWD/$1
USB_NODE=$2

if [ $# -ne 2 ]; then
	echo ""
	echo "***************************************** "
	echo ""
	echo "Usage)"
	echo "	./download.sh {f/w .hex filename} {ttyNode}"
	echo ""
	echo "***************************************** "
	echo ""
	exit 1;
fi

# ttynode check, file check
if ! [ -e $FW_FILE ]; then
	echo "F/W file not found, $FW_FILE"
	exit 1
fi

if ! [ -e $USB_NODE ]; then
	# if bootloader mode or ignore usb state.
	echo "USB reset (4348:55e0)"
	usbreset 4348:55e0
	sleep 1
	if ! [ -e $USB_NODE ]; then	
		echo "USB node not found. Node name = $USB_NODE"
		vnproch55x -r 2 -t ch552 $FW_FILE 
		exit 1
	fi
else
	usbreset 1209:c550
fi

# ch55x usb : change normal mode to  bootloader mode.
stty -F $USB_NODE 1200
sleep 1

# info display
echo "***************************************** "
echo ""
echo "Firmware upgrade info"
echo ""
echo "F/W Filename = $FW_FILE"
echo ""
echo "***************************************** "
vnproch55x -r 2 -t ch552 $FW_FILE 
sleep 1
stty -F $USB_NODE 9600
echo "Done..."
sleep 1

