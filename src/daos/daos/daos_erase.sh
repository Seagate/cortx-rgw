#!/bin/bash

source $CEPH_PATH/src/daos/require_variables.sh
source $CEPH_PATH/src/daos/wait_for.sh

function daos_wait_erase()
{
    result=`$COMMAND_PREFIX ${DAOS_BIN}/dmg system query --json | jq .status`
    if [ $result -lt 0 ]; then
        return 0
    fi
    return 1
}

function daos_erase()
{
    require_variables DAOS_BIN COMMAND_PREFIX
    local result=0
    $COMMAND_PREFIX ${DAOS_BIN}/dmg system erase
    wait_for 20 daos_wait_erase "DAOS erase failed"
}
