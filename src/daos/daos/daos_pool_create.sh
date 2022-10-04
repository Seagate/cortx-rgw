#!/bin/bash

source $CEPH_PATH/src/daos/require_variables.sh
source $CEPH_PATH/src/daos/wait_for.sh

function get_daos_pools_status()
{
    require_variables DAOS_BIN COMMAND_PREFIX
    $COMMAND_PREFIX ${DAOS_BIN}/dmg system query --json > /tmp/daos_pools_status.json
    jq .status /tmp/daos_pools_status.json
}

function get_daos_pools_state()
{
    require_variables DAOS_BIN COMMAND_PREFIX
    $COMMAND_PREFIX ${DAOS_BIN}/dmg system query --json > /tmp/daos_pools_state.json
    jq .response.members[].state /tmp/daos_pools_state.json | sed 's/"//g'
}

function daos_wait_create_pool()
{
    local result=$(get_daos_pools_status)
    if [ $result -eq 0 ]; then
        result=$(get_daos_pools_state)
        if [[ $result == 'joined' ]] || [[ $result == 'Ready' ]]; then
            return 0
        fi
    fi
    return 1
}

function daos_pool_create()
{
    require_variables DAOS_BIN COMMAND_PREFIX
    $COMMAND_PREFIX ${DAOS_BIN}/dmg pool create --size=4GB tank
    wait_for 20 daos_wait_create_pool "DAOS pool creation failed"
}
