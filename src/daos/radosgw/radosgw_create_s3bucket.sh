#!/bin/bash

source $CEPH_PATH/src/daos/require_variables.sh

radosgw_create_s3bucket()
{
    require_variables COMMAND_PREFIX
    $COMMAND_PREFIX sh $CEPH_PATH/src/daos/docker/s3-tests/setup.sh
}
