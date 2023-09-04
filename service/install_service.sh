#!/bin/bash

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
INSTALL_PATH="/root/m1s_ups"
SYSTEM_PATH="/etc/systemd/system"
SREVICE_NAME="m1s_ups"
SERVICE_SCRIPT="check_ups.sh"

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
# service disable for update
if [ $(systemctl is-active ${SERVICE_NAME}) == "active"]; then
    echo "service ${SERVICE_NAME} active -> inactive"
    service {$SERVICE_NAME} disable
fi

mkdir -p ${INSTALL_PATH}

cp "${SERVICE_SCRIPT}" "${INSTALL_PATH}/"
cp "${SERVICE_NAME}.service" "${SYSTEM_PATH}/"

# service enable
systemctl enable "${SERVICE_NAME}"

# service status display
systemctl status "${SERVICE_NAME}"

# service disable
# systemctl disable "${SERVICE_NAME}"

#/*---------------------------------------------------------------------------*/
#/*---------------------------------------------------------------------------*/
