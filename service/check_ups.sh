#!/bin/bash

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
#
#
# Script configuration start
#
#
#/*---------------------------------------------------------------------------*/
#/* Battery level definition value for UPS system */
#/*---------------------------------------------------------------------------*/
BATTERY_LEVEL_FULL="4300"

#/* Battery level define (1 ~ 9) */
BATTERY_LEVEL_3550mV="3550"
BATTERY_LEVEL_3600mV="3600"
BATTERY_LEVEL_3650mV="3650"
BATTERY_LEVEL_3700mV="3700"
BATTERY_LEVEL_3750mV="3750"
BATTERY_LEVEL_3800mV="3800"
BATTERY_LEVEL_3850mV="3850"
BATTERY_LEVEL_3900mV="3900"
BATTERY_LEVEL_3950mV="3950"

BATTERY_LEVEL_0mV="0"

#/*---------------------------------------------------------------------------*/
#/* Set battery level for system power off */
#/* BATERRY_LEVEL_FULL : Power off when battery discharge condition detected. */
#/* BATERRY_LEVEL_0mV  : Maintains power until the battery is fully discharged. */
#/*---------------------------------------------------------------------------*/
CONFIG_POWEROFF_BATTERY_LEVEL=${BATTERY_LEVEL_FULL}
# CONFIG_POWEROFF_BATTERY_LEVEL=${BATTERY_LEVEL_3900mV}

#/*---------------------------------------------------------------------------*/
#/* Set battery level for system power on */
#/* Power on when battery charge condition detected.(default) */
# BATTERY_LEVEL_0mV : Detect charging status.(default)
#/*---------------------------------------------------------------------------*/
CONFIG_POWERON_BATTERY_LEVEL=${BATTERY_LEVEL_0mV}
# CONFIG_POWERON_BATTERY_LEVEL=${BATTERY_LEVEL_3550mV}

#/*---------------------------------------------------------------------------*/
#/* Set watchdog reset time */
# 0     : Disable.(default)
# 1 ~ 9 : Watchdog reset time(sec) : Warnning
#
# WARNING: The watchdog reset value must be greater than the script execution time.
#
# The script takes about 4-5 seconds to run once.
#
#/*---------------------------------------------------------------------------*/
CONFIG_WATCHDOG_RESET_TIME=""

#/*---------------------------------------------------------------------------*/
#
#
# Script configuration end
#
#
#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/

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
#/* battery log filename (default disable) */
#/* eg) UPS_TTY_LOG="/tmp/ttyUPS.log" */
#/*---------------------------------------------------------------------------*/
UPS_TTY_LOG=""

#/*---------------------------------------------------------------------------*/
#/* Script start time and date */
#/*---------------------------------------------------------------------------*/
CURRENT_TIME=$(date)

#/*---------------------------------------------------------------------------*/
#/* UPS Command List */
#/*---------------------------------------------------------------------------*/
# Send command to read battery volt to UPS.
UPS_CMD_BATTERY_VOLT="@V0#"

# Send command to read battery level to UPS.
# LED 1 : BATTERY DISPLAY LEVEL 1 (3550 mV > Battery voltage)
# LED 2 : BATTERY DISPLAY LEVEL 2 (3650 mV > Battery voltage)
# LED 3 : BATTERY DISPLAY LEVEL 3 (3750 mV > Battery voltage)
# LED 4 : BATTERY DISPLAY LEVEL 4 (3900 mV > Battery voltage)
UPS_CMD_BATTERY_LEVEL="@L0#"

# Send command to read charger status to UPS.
UPS_CMD_CHARGER_STATUS="@C0#"

# Send command to ups off to UPS.
UPS_CMD_POWEROFF="@P0#"

# Send command to power on level to UPS.
# *     : Detect charging status.(default)
# 0 ~ 4 : BATTERY LEVEL
UPS_CMD_POWERON="@O0#"

# Send command to watchdog reset time to UPS.
# *     : Disable.(default)
# 1 ~ 9 : Watchdog reset time
UPS_CMD_WATCHDOG="@W0#"

#/*---------------------------------------------------------------------------*/
#/* for communication with ups */
#/*---------------------------------------------------------------------------*/
UPS_CMD_STR=""

#/*---------------------------------------------------------------------------*/
#/* UPS system data */
#/*---------------------------------------------------------------------------*/
UPS_BATTERY_VOLT="0"
UPS_STATUS_CHRG="0"
UPS_STATUS_FULL="0"

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
#/* Kill previously running processes(dead process). */
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
#/* find ttyNode name : VID_CH55xduino(1209):PID_CH55xduino(c550)
#/*---------------------------------------------------------------------------*/
function find_tty_node {
	UPS_TTY_NODE=`find $(grep -l "PRODUCT=$(printf "%x/%x" "0x${VID_CH55xduino}" "0x${PID_CH55xduino}")" \
					/sys/bus/usb/devices/[0-9]*:*/uevent | sed 's,uevent$,,') \
					/dev/null -name dev -o -name dev_id  | sed 's,[^/]*$,uevent,' |
					xargs sed -n -e s,DEVNAME=,/dev/,p -e s,INTERFACE=,,p`

}

#/*---------------------------------------------------------------------------*/
#/* Send control commands to the UPS via the UPS_CMD_STR variable.
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
# UPS system staus check.
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
		echo "UPS Battery Status (Discharging...)"

		#/* UPS Battery Status : Low Battery */
		if [ ${UPS_BATTERY_VOLT} -lt ${CONFIG_POWEROFF_BATTERY_LEVEL} ]; then
			if [ ${CONFIG_POWEROFF_BATTERY_LEVEL} -eq ${BATTERY_LEVEL_FULL} ]; then
				#/* Power off after Detecting UPS battery discharge. */
				echo "------------------------------------------------------------"
				echo "Detected UPS battery discharge."
				echo "------------------------------------------------------------"
			else
				echo "------------------------------------------------------------"
				echo "UPS Battery Volt : ${UPS_BATTERY_VOLT} mV"
				echo "UPS Battery Volt is lower then ${CONFIG_POWEROFF_BATTERY_LEVEL} mV"
				echo "------------------------------------------------------------"
			fi
			system_poweroff
		fi
	else
		if [ ${UPS_STATUS_CHRG} -eq "0" ]; then
			echo "UPS Battery Status (Charging...)"
		else
			echo "UPS Battery Status (Full Charged..)"
		fi

	fi

	echo "UPS Battery Volt : ${UPS_BATTERY_VOLT} mV"

	if [ ${CONFIG_POWEROFF_BATTERY_LEVEL} -eq ${BATTERY_LEVEL_FULL} ]; then
		echo "SYSTEM Power OFF : Detecting UPS battery discharge."
	else
		echo "SYSTEM Power OFF : UPS Battery Volt is lower then ${CONFIG_POWEROFF_BATTERY_LEVEL} mV"
	fi
}

#/*---------------------------------------------------------------------------*/
# Send command to ups off to UPS.
#/*---------------------------------------------------------------------------*/
function system_poweroff {
	UPS_CMD_STR=${UPS_CMD_POWEROFF}
	ups_cmd_send

	echo "------------------------------------------------------------"
	echo "run poweroff command..."
	echo "------------------------------------------------------------"
	if [ -n "${UPS_TTY_LOG}" ]; then
		echo "${CURRENT_TIME}, POWEROFF" >> ${UPS_TTY_LOG}
		echo "-------------------------" >> ${UPS_TTY_LOG}
	fi
	poweroff
	exit 0
}

#/*---------------------------------------------------------------------------*/
#/* Set watchdog reset time */
# 0     : Disable.(default)
# 1 ~ 9 : Watchdog reset time(sec) : Warnning
#
# WARNING: The watchdog reset value must be greater than the script execution time.
#
# The script takes about 4-5 seconds to run once.
#
#/*---------------------------------------------------------------------------*/
function watchdog_reset {
	if [ ${CONFIG_WATCHDOG_RESET_TIME} -gt "9" -o ${CONFIG_WATCHDOG_RESET_TIME} -lt "0"]
	then
		echo "CONFIG_WATCHDOG_RESET_TIME=${CONFIG_WATCHDOG_RESET_TIME} value error."
		echo "WATCHDOG Disable"
		CONFIG_WATCHDOG_RESET_TIME="0"
	fi

	UPS_CMD_WATCHDOG="@W${CONFIG_WATCHDOG_RESET_TIME}#"
	UPS_CMD_STR=${UPS_CMD_WATCHDOG}
	ups_cmd_send
}

#/*---------------------------------------------------------------------------*/
#/* Set battery level for system power on */
#/* Power on when battery charge condition detected.(default) */
# 0     : Detect charging status.(default)
# 1 ~ 9 : BATTERY LEVEL
#/*---------------------------------------------------------------------------*/
function ups_poweron_setup {
	#/* Update data */
	case ${CONFIG_POWERON_BATTERY_LEVEL} in
		${BATTERY_LEVEL_3550mV})
			POWERON_BATTERY_LEVEL="1"
			;;
		${BATTERY_LEVEL_3600mV})
			POWERON_BATTERY_LEVEL="2"
			;;
		${BATTERY_LEVEL_3650mV})
			POWERON_BATTERY_LEVEL="3"
			;;
		${BATTERY_LEVEL_3700mV})
			POWERON_BATTERY_LEVEL="4"
			;;
		${BATTERY_LEVEL_3750mV})
			POWERON_BATTERY_LEVEL="5"
			;;
		${BATTERY_LEVEL_3800mV})
			POWERON_BATTERY_LEVEL="6"
			;;
		${BATTERY_LEVEL_3850mV})
			POWERON_BATTERY_LEVEL="7"
			;;
		${BATTERY_LEVEL_3900mV})
			POWERON_BATTERY_LEVEL="8"
			;;
		${BATTERY_LEVEL_3950mV})
			POWERON_BATTERY_LEVEL="9"
			;;
		* )
			POWERON_BATTERY_LEVEL="0"
			;;
	esac

	if [ ${POWERON_BATTERY_LEVEL} -eq "0" ]
	then
		echo "Power on when battery charge condition detected.(default)"
	fi

	UPS_CMD_POWERON="@O${POWERON_BATTERY_LEVEL}#"
	UPS_CMD_STR=${UPS_CMD_POWERON}
	ups_cmd_send
}

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
#/* START Script */
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
#/* Log status display */
#/*---------------------------------------------------------------------------*/
if [ -n "${UPS_TTY_LOG}" ]; then
	echo "------------------------------------------------------------"
	echo "Log Enable (${UPS_TTY_LOG})"
	echo "------------------------------------------------------------"
	echo "------------------------" >> ${UPS_TTY_LOG}
	echo "${CURRENT_TIME}, POWERON" >> ${UPS_TTY_LOG}
fi

#/*---------------------------------------------------------------------------*/
#/* Kill previously running processes(dead process). */
#/*---------------------------------------------------------------------------*/
kill_dead_process

#/*---------------------------------------------------------------------------*/
#/* ttyACM Baudrate setup */
#/*---------------------------------------------------------------------------*/
stty -F ${UPS_TTY_NODE} 9600 raw -echo

#/*---------------------------------------------------------------------------*/
#/* Set battery level for system power on */
#/*---------------------------------------------------------------------------*/
if [ -n "${CONFIG_UPS_ON_BATTERY_LEVEL}" ]; then
	ups_poweron_setup
fi

#/*---------------------------------------------------------------------------*/
#/* Main Loop (The script takes about 4-5 seconds to run once.) */
#/*---------------------------------------------------------------------------*/
while true
do
	echo "------------------------------------------------------------"
	# Send command to read battery volt to UPS.
	UPS_CMD_STR=${UPS_CMD_BATTERY_VOLT}
	ups_cmd_send

	UPS_CMD_STR=${UPS_CMD_CHARGER_STATUS}
	ups_cmd_send

	#/* current date, time */
	CURRENT_TIME=$(date)

	#/* Display UPS Status */
	echo ${CURRENT_TIME}
	check_ups_status

	#/* Battery Log save */
	if [ -n "${UPS_TTY_LOG}" ]; then
		echo "${CURRENT_TIME}, ${UPS_BATTERY_VOLT}" >> ${UPS_TTY_LOG}
	fi

	#/* Watchdog control */
	if [ -n "${CONFIG_WATCHDOG_RESET_TIME}" ]; then
		watchdog_reset
	fi

	echo "------------------------------------------------------------"
done

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
