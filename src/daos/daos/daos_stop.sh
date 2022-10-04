#!/bin/bash

source $CEPH_PATH/src/daos/require_variables.sh
source $CEPH_PATH/src/daos/wait_for.sh

function daos_wait_stop()
{
    local check_command='ps -e | grep daos_engine'
    eval "$COMMAND_PREFIX $check_command"
    if [[ $? == 0 ]]; then
        return 0
    fi
    return 1
}

daos_stop()
{
    require_variables DAOS_BIN COMMAND_PREFIX
    # wait until daos_engine is not running
    $COMMAND_PREFIX ${DAOS_BIN}/dmg system stop --force
    wait_for 20 daos_wait_stop "DAOS system failed to stop"
}
