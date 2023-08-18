#!/bin/bash

#/*---------------------------------------------------------------------------*/
#/* Define CH55xduino ttyACM VID/PID (1209:c550) */
#/*---------------------------------------------------------------------------*/
VID_CH55xduino="1209"
PID_CH55xduino="C550"

#/*---------------------------------------------------------------------------*/
#/* Define tmp file */
#/*---------------------------------------------------------------------------*/
UPS_TTY_NODE=""
UPS_TTY_DATA="/tmp/ttyUPS.dat"

#/*---------------------------------------------------------------------------*/
#/* UPS Command List */
#/*---------------------------------------------------------------------------*/
# Send command to read battery volt to UPS.
UPS_CMD_BATTERY_VOLT="@V0#"
# Send command to read battery level to UPS.
# 0 : BATTERY LEVEL 0 (3400 mV)
# 1 : BATTERY LEVEL 1 (3550 mV)
# 2 : BATTERY LEVEL 2 (3650 mV)
# 3 : BATTERY LEVEL 3 (3750 mV)
# 4 : BATTERY LEVEL 4 (3900 mV)
UPS_CMD_BATTERY_LEVEL="@L0#"
# Send command to read charger status to UPS.
UPS_CMD_BATTERY_STATUS="@C0#"
# Send command to ups off to UPS.
UPS_CMD_POWEROFF="@P0#"
# Send command to power on level to UPS.
# 0 ~ 4 : BATTERY LEVEL
# * : Detect charging status.(default)
UPS_CMD_POWERON="@O0#"

#/*---------------------------------------------------------------------------*/
UPS_CMD_STR=""

#/*---------------------------------------------------------------------------*/
UPS_BATTERY_VOLT="0"
UPS_STATUS_CHRG="0"
UPS_STATUS_FULL="0"

#/*---------------------------------------------------------------------------*/
#/* UPS Battery Level define value */
#/*---------------------------------------------------------------------------*/
BAT_LEVEL_FULL="4300"
BAT_LEVEL_LV4="3900"
BAT_LEVEL_LV3="3750"
BAT_LEVEL_LV2="3650"
BAT_LEVEL_LV1="3550"

#/* System force power level */
#POWER_OFF_VOLT=${BAT_LEVEL_LV3}

#/* Power off when battery discharge condition detected. */
POWER_OFF_VOLT=${BAT_LEVEL_FULL}

UPS_SEND_CMD=""
UPS_SEND_STR=""
UPS_ON_LEVEL=""
UPS_WATCHDOG_TIME=""

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
function kill_dead_process {
	PID=""
	PID=`ps -eaf | grep ${UPS_TTY_NODE} | grep -v grep | awk '{print $2}'`
	if [ -n "$PID" ]; then
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
function ups_cmd_send {
	# ttyACM response data wait settings.
	cat ${UPS_TTY_NODE} > ${UPS_TTY_DATA} &
	sleep 1

	# Get PID (cat command) to kill background process
	PID=""
	PID=$!

	#/* Send command string to UPS */
	echo -ne ${UPS_CMD_STR} > ${UPS_TTY_NODE}
	sleep 1

	#/* Update data */
	case ${UPS_CMD_STR} in
		${UPS_CMD_BATTERY_VOLT})
			# Update battery volt data.
			UPS_BATTERY_VOLT=`cut -c 3-6 < ${UPS_TTY_DATA}`
			;;
		${UPS_CMD_BATTERY_LEVEL})
			# Update charger status data.
			UPS_BATTERY_LV4=`cut -c 3 < ${UPS_TTY_DATA}`
			UPS_BATTERY_LV3=`cut -c 4 < ${UPS_TTY_DATA}`
			UPS_BATTERY_LV2=`cut -c 5 < ${UPS_TTY_DATA}`
			UPS_BATTERY_LV1=`cut -c 6 < ${UPS_TTY_DATA}`
			;;
		${UPS_CMD_CHARGER_STATUS})
			# Update charger status data.
			UPS_STATUS_CHRG=`cut -c 6 < ${UPS_TTY_DATA}`
			UPS_STATUS_FULL=`cut -c 4 < ${UPS_TTY_DATA}`
			;;
		* )
			;;
	esac

	# Kill background process(cat cmd)
	if [ -n "$PID" ]; then
		kill $PID
	fi
}

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
function read_ups_volt {
	# ttyACM response data wait settings.
	cat ${UPS_TTY_NODE} > ${UPS_TTY_DATA} &

	# Get PID (cat command) to kill background process
	PID=""
	PID=$!

	# Send command to read battery volt to UPS.
	sleep 1
	echo -ne "@V0#" > ${UPS_TTY_NODE}
	sleep 1

	# Kill background process(cat cmd)
	if [ -n "$PID" ]; then
		kill $PID
	fi

	# Update battery volt data.
	UPS_BATTERY_VOLT=`cut -c 3-6 < ${UPS_TTY_DATA}`
}

#/*---------------------------------------------------------------------------*/
function read_ups_status {
	# ttyACM response data wait settings.
	cat ${UPS_TTY_NODE} > ${UPS_TTY_DATA} &

	# Get PID (cat command) to kill background process
	PID=""
	PID=$!

	# Send command to read charger status to UPS.
	sleep 1
	echo -ne "@C0#" > ${UPS_TTY_NODE}
	sleep 1

	# Kill background process(cat cmd)
	if [ -n "$PID" ]; then
		kill $PID
	fi

	# Update charger status data.
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
		return
	fi

	#/* UPS Status : Discharging... */
	if [ ${UPS_STATUS_CHRG} -eq "1" -a ${UPS_STATUS_FULL} -eq "1" ]; then
		echo "UPS Battery Status : Discharging..."
		echo "UPS Battery Volt = ${UPS_BATTERY_VOLT} mV"

		#/* UPS Battery Status : Low Battery */
		if [ ${UPS_BATTERY_VOLT} -lt ${POWER_OFF_VOLT} ]; then
			if [ ${POWER_OFF_VOLT} -eq ${BAT_LEVEL_FULL} ]; then
				#/* Power off after Detecting UPS battery discharge. */
				echo "------------------------------------------------------------"
				echo "Detected UPS battery discharge."
				echo "------------------------------------------------------------"
			else
				echo "------------------------------------------------------------"
				echo "Power off when UPS_BATTERY_VOLT is lower than POWER_OFF_VOLT."
				echo "------------------------------------------------------------"
			fi
			system_poweroff
		fi
	else
		if [ ${UPS_STATUS_CHRG} -eq "0" ]; then
			echo "UPS Battery Status : Charging..."
		else
			echo "UPS Battery Status : Full Charged."
		fi

		if [ ${POWER_OFF_VOLT} -eq ${BAT_LEVEL_FULL} ]; then
			echo "UPS Power OFF : Detecting UPS battery discharge."
		else
			echo "UPS Power OFF : Battery Volt = ${POWER_OFF_VOLT} mV"
		fi
	fi
}

#/*---------------------------------------------------------------------------*/
function system_poweroff {
	# ttyACM response data wait settings.
	cat ${UPS_TTY_NODE} > ${UPS_TTY_DATA} &

	# Get PID (cat command) to kill background process
	PID=""
	PID=$!

	# Send command to ups off to UPS.
	sleep 1
	echo -ne "@P0#" > ${UPS_TTY_NODE}
	sleep 1

	# Kill background process(cat cmd)
	if [ -n "$PID" ]; then
		kill -9 $PID
	fi

	echo "------------------------------------------------------------"
	echo "run poweroff command..."
	echo "------------------------------------------------------------"
	# poweroff
	exit 0
}

#/*---------------------------------------------------------------------------*/
#/* find CH55xduino ttyACM node (1209:c550) */
#/*---------------------------------------------------------------------------*/
find_tty_node

#/*---------------------------------------------------------------------------*/
#/* Script exit handling when node not found. */
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
#/* Kill previously running processes. */
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
	# Send command to read battery volt to UPS.
	UPS_CMD_STR=${UPS_CMD_BATTERY_VOLT}
	ups_cmd_send

#	read_ups_volt
	read_ups_status
	date
	check_ups_status
	echo "UPS Battery Volt = ${UPS_BATTERY_VOLT} mV"
	echo "------------------------------------------------------------"
done

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
