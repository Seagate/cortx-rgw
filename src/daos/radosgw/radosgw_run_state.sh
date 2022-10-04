#!/bin/bash

source $CEPH_PATH/src/daos/require_variables.sh

isRadosgwRunning()
{
    require_variables summary
    if [ $summary == true ]; then
        # if summarizing, we don't care if its running
        return 1
    fi
    local pid_path="$CEPH_PATH/build/out/radosgw.8000.pid"
    if [[ -e $pid_path ]]; then
        local RADOSGW_PID=`cat $pid_path`
        local RADOSGW_RUNNING=`ps $RADOSGW_PID | grep -o $RADOSGW_PID`
        if [[ $RADOSGW_RUNNING == $RADOSGW_PID ]]; then
            return 1
        fi
    fi
    return 0
}
