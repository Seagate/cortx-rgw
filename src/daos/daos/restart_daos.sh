#!/bin/bash

source $CEPH_PATH/src/daos/daos/daos_stop.sh
source $CEPH_PATH/src/daos/daos/daos_erase.sh
source $CEPH_PATH/src/daos/daos/daos_format.sh
source $CEPH_PATH/src/daos/daos/daos_pool_create.sh

restart_daos()
{
    daos_stop
    daos_erase
    daos_format
    daos_pool_create
}
