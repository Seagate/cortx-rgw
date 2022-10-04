#!/bin/bash

source $CEPH_PATH/src/daos/require_variables.sh

function initialize_daos_bin()
{
    require_variables DAOS_PATH
    if [[ "$DAOS_BIN" == "" ]]; then
        if [[ -e $DAOS_PATH/install/bin/dmg ]]; then
            DAOS_BIN="$DAOS_PATH/install/bin/"
        fi
        if [[ -e $DAOS_PATH/bin/dmg ]]; then
            DAOS_BIN="$DAOS_PATH/bin/"
        fi
    fi
    if [[ "$DAOS_BIN" == "" ]]; then
        echo "dmg was not found in the usual places, exiting"
        exit 1
    fi
}
