#!/bin/bash
# RADOSGW_STARTOPTIONS are the start options for vstart.  typically its "-d" if you want extended logging from ceph
# By setting this variable, the desired state can be maintained if/when radosgw needs to be restarted
# export RADOSGW_STARTOPTIONS="" to launch without extended logging

source $CEPH_PATH/src/daos/require_variables.sh

radosgw_stop()
{
    require_variables COMMAND_PREFIX
    $COMMAND_PREFIX bash -c "pushd ${CEPH_PATH}/build && RGW=1 ../src/stop.sh && popd"
}
