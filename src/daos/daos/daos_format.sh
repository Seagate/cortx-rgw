#!/bin/bash

source $CEPH_PATH/src/daos/require_variables.sh
source $CEPH_PATH/src/daos/wait_for.sh

function daos_wait_format()
{
    result=`$COMMAND_PREFIX ${DAOS_BIN}/dmg system query --json | jq .status`
    if [ $result -ge 0 ]; then
        return 0
    fi
    return 1
}

function daos_format()
{
    require_variables DAOS_BIN COMMAND_PREFIX
    local result=-1025
    $COMMAND_PREFIX ${DAOS_BIN}/dmg storage format --force
    wait_for 20 daos_wait_format "DAOS format failed"
}
