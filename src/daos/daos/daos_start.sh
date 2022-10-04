#!/bin/bash

source $CEPH_PATH/src/daos/require_variables.sh
source $CEPH_PATH/src/daos/daos/initialize_daos_bin.sh
source $CEPH_PATH/src/daos/wait_for.sh

function get_daos_system_error()
{
    require_variables DAOS_BIN COMMAND_PREFIX
    $COMMAND_PREFIX ${DAOS_BIN}/dmg system query --json > /tmp/daos_system_error.json
    jq .error /tmp/daos_system_error.json
}

function daos_wait_start()
{
    # the response will contain "unable to contact the DAOS Management Service" while its initializing
    # once the response contains "storage format required", the system is ready for formatting
    local result=$(get_daos_system_error)
    if [[ ! $result =~ "unable to contact the DAOS Management Service" ]]; then
        return 0
    fi
    return 1
}

function daos_start()
{
    initialize_daos_bin
    require_variables DAOS_BIN COMMAND_PREFIX
    local wait_needed=false
    $COMMAND_PREFIX ps -e | grep -o daos_server
    if [[ ! $? == 0 ]]; then
        $COMMAND_PREFIX mkdir -p /var/run/daos_server
        $COMMAND_PREFIX ${DAOS_BIN}/daos_server start &
        wait_needed=true
    fi
    $COMMAND_PREFIX ps -e | grep -o daos_agent
    if [[ ! $? == 0 ]]; then
        $COMMAND_PREFIX mkdir -p /var/run/daos_agent
        $COMMAND_PREFIX ${DAOS_BIN}/daos_agent start &
        wait_needed=true
    fi
    if [[ $wait_needed == true ]]; then
        wait_for 20 daos_wait_start "DAOS system failed to start"
    fi
}
