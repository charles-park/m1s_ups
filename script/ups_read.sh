#!/bin/bash

TTY_UPS_DATA="/tmp/ttyUPS.dat"
TTY_UPS_NODE="/dev/ttyACM0"

UPS_VOLT_DATA="0"
CHARGER_CHRG="0"
CHARGER_FULL="0"

function read_volt {
	# ttyACM receive data background setup.
	cat ${TTY_UPS_NODE} > ${TTY_UPS_DATA} &

	# catch cat command PID for kill background process
	PID=$!

	sleep 0.5
	echo -ne "@V0#" > ${TTY_UPS_NODE}
	sleep 0.5 

	# cat background process kill
	kill $PID

	# update data
	UPS_VOLT_DATA=`cut -c 3-6 < ${TTY_UPS_DATA}`
}


function read_charger_status {
	# ttyACM receive data background setup.
	cat ${TTY_UPS_NODE} > ${TTY_UPS_DATA} &

	# catch cat command PID for kill background process
	PID=$!

	sleep 0.5
	echo -ne "@C0#" > ${TTY_UPS_NODE}
	sleep 0.5 

	# cat background process kill
	kill $PID

	# update data
	CHARGER_CHRG=`cut -c 6 < ${TTY_UPS_DATA}`
	CHARGER_FULL=`cut -c 4 < ${TTY_UPS_DATA}` 
}

# hexdump -ve '1/1 "%.2x"' /root/ttyDump.dat
# aaa=`cut -c 3-6 < /tmp/ttyUPS.dat` 
# aaa=`cut -c 3-6 < ${TTY_UPS_DATA}` 

# ttyACM Baudrate setup
stty -F ${TTY_UPS_NODE} 9600 raw -echo

read_volt
echo "Battery Voltage(mV) = ${UPS_VOLT_DATA}"

read_charger_status
echo "Charger Status CHRG = ${CHARGER_CHRG}"
echo "Charger Status FULL = ${CHARGER_FULL}"

