#!/bin/bash

#/*---------------------------------------------------------------------------*/
#/* find CH55xduino ttyACM node (1209:c550) */
#/*---------------------------------------------------------------------------*/
VID_CH55xduino="1209"
PID_CH55xduino="C550"

#/*---------------------------------------------------------------------------*/
UPS_TTY_NODE=""
UPS_TTY_DATA="/tmp/ttyUPS.dat"

#/*---------------------------------------------------------------------------*/
UPS_STATUS_VOLT="0"
UPS_STATUS_CHRG="0"
UPS_STATUS_FULL="0"

#/*---------------------------------------------------------------------------*/
#/* UPS Battery Level define value */
#/*---------------------------------------------------------------------------*/
BAT_LEVEL_LV4="3900"
BAT_LEVEL_LV3="3750"
BAT_LEVEL_LV2="3650"
BAT_LEVEL_LV1="3550"

#/* System force power level */
POWER_OFF_VOLT=${BAT_LEVEL_LV3}

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
function kill_dead_process {
	PID=`ps -eaf | grep ${UPS_TTY_NODE} | grep -v grep | awk '{print $2}'`
	if [[ "" !=  "$PID" ]]; then
		echo "------------------------------------------------------------"
		echo "Killing $PID"
		kill -9 $PID
		echo "------------------------------------------------------------"
	fi
}

#/*---------------------------------------------------------------------------*/
function find_tty_node {
	UPS_TTY_NODE=`find $(grep -l "PRODUCT=$(printf "%x/%x" "0x${VID_CH55xduino}" "0x${PID_CH55xduino}")" \
					/sys/bus/usb/devices/[0-9]*:*/uevent | sed 's,uevent$,,') \
					/dev/null -name dev -o -name dev_id  | sed 's,[^/]*$,uevent,' |
					xargs sed -n -e s,DEVNAME=,/dev/,p -e s,INTERFACE=,,p`

}

#/*---------------------------------------------------------------------------*/
function read_ups_volt {
	# ttyACM response data background setup.
	cat ${UPS_TTY_NODE} > ${UPS_TTY_DATA} &

	# Get PID(cat command) for kill background process
	PID=$!

	# UPS read volt command send
	sleep 0.5
	echo -ne "@V0#" > ${UPS_TTY_NODE}
	sleep 0.5

	# Kill Background process(cat cmd)
	kill $PID

	# UPS Volt data update
	UPS_STATUS_VOLT=`cut -c 3-6 < ${UPS_TTY_DATA}`
}

#/*---------------------------------------------------------------------------*/
function read_ups_status {
	# ttyACM response data background setup.
	cat ${UPS_TTY_NODE} > ${UPS_TTY_DATA} &

	# Get PID(cat command) for kill background process
	PID=$!

	# UPS read status command send
	sleep 0.5
	echo -ne "@C0#" > ${UPS_TTY_NODE}
	sleep 0.5

	# Kill Background process(cat cmd)
	kill $PID

	# Update UPS Status data
	UPS_STATUS_CHRG=`cut -c 6 < ${UPS_TTY_DATA}`
	UPS_STATUS_FULL=`cut -c 4 < ${UPS_TTY_DATA}`
}

#/*---------------------------------------------------------------------------*/
function check_ups_status {
	#/* UPS Status : Error...(Battery Removed) */
	if [ ${UPS_STATUS_CHRG} -eq "0" -a ${UPS_STATUS_FULL} -eq "0" ]; then
		echo "------------------------------------------------------------"
		echo "ERROR: Battery Removed. force power off..."
		echo "------------------------------------------------------------"
		system_poweroff
	fi

	#/* UPS Status : Discharging... */
	if [ ${UPS_STATUS_CHRG} -eq "1" -a ${UPS_STATUS_FULL} -eq "1" ]; then
		echo "UPS Battery Status : Discharging..."
		echo "UPS Battery Volt = ${UPS_STATUS_VOLT} mV"
		#/* UPS Battery Status : Low Battery */
		if [ ${UPS_STATUS_VOLT} -lt ${POWER_OFF_VOLT} ]; then
			echo "------------------------------------------------------------"
			echo "UPS Battery Volt ${UPS_STATUS_VOLT} mV is less then Power Off Volt ${POWER_OFF_VOLT} mV"
			echo "------------------------------------------------------------"
			system_poweroff
		fi
	else
		if [ ${UPS_STATUS_CHRG} -eq "0" ]; then
			echo "UPS Battery Status : Charging..."
		else
			echo "UPS Battery Status : Full Charged."
		fi
		echo "UPS Battery Volt = ${UPS_STATUS_VOLT} mV"
	fi
}

#/*---------------------------------------------------------------------------*/
function system_poweroff {
	# ttyACM response data background setup.
	cat ${UPS_TTY_NODE} > ${UPS_TTY_DATA} &

	# Get PID(cat command) for kill background process
	PID=$!

	# UPS Poweroff command send
	sleep 0.5
	echo -ne "@P0#" > ${UPS_TTY_NODE}
	sleep 0.5

	# Kill Background process(cat cmd)
	kill -9 $PID 2>&1

	echo "------------------------------------------------------------"
	echo "run poweroff command..."
	echo "------------------------------------------------------------"
	# poweroff
	# exit 0
}

#/*---------------------------------------------------------------------------*/
#/* find CH55xduino ttyACM node (1209:c550) */
#/*---------------------------------------------------------------------------*/
find_tty_node

#/*---------------------------------------------------------------------------*/
#/* tty noode가 없는 경우 처리 (script exit) */
#/*---------------------------------------------------------------------------*/
if [ -z "${UPS_TTY_NODE}" ]; then
	echo "------------------------------------------------------------"
	echo "Can't found ttyACM(CH55xduino) device. (1209:c550)"
	echo "------------------------------------------------------------"
	exit 1
else
	echo "------------------------------------------------------------"
	echo "Found ttyACM(CH55xduino) device. Node name = ${UPS_TTY_NODE}"
	echo "------------------------------------------------------------"
fi

#/*---------------------------------------------------------------------------*/
#/* 강제종료시 발생되는 cat command의 dead process kill. */
#/*---------------------------------------------------------------------------*/
kill_dead_process

#/*---------------------------------------------------------------------------*/
#/* ttyACM Baudrate setup */
#/*---------------------------------------------------------------------------*/
stty -F ${UPS_TTY_NODE} 9600 raw -echo

#/*---------------------------------------------------------------------------*/
#/* Main Loop */
#/*---------------------------------------------------------------------------*/
while true
do
	echo "------------------------------------------------------------"
	read_ups_volt
	read_ups_status
	date
	check_ups_status
	echo "------------------------------------------------------------"
done

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
