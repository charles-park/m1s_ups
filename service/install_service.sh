#!/bin/bash

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
INSTALL_PATH="/root/m1s_ups"
SYSTEM_PATH="/etc/systemd/system"
SERVICE_NAME="m1s_ups"
SERVICE_SCRIPT="check_ups.sh"

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
# service disable for update
if [ $(systemctl is-active ${SERVICE_NAME}) == "active" ]; then
    echo "service ${SERVICE_NAME} active -> inactive"
    service ${SERVICE_NAME} stop
    # service disable
    systemctl disable ${SERVICE_NAME}
fi

mkdir -p ${INSTALL_PATH}

cp "${PWD}/${SERVICE_SCRIPT}" "${INSTALL_PATH}/"
cp "${PWD}/${SERVICE_NAME}.service" "${SYSTEM_PATH}/"

# service enable
systemctl enable ${SERVICE_NAME}

# service restart
service ${SERVICE_NAME} start

# service status display
systemctl status ${SERVICE_NAME}


#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
