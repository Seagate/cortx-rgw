#!/bin/bash
# usage: Gets the number of free bytes available to a specific folder, defaults to current folder
# FREE_CEPH_BYTES=$(folder_free_space /opt/ceph)

function folder_free_space()
{
    local FOLDER='.'
    if [[ ! $1 == '' ]]; then
        FOLDER=$1
    fi
    df -P $FOLDER | tail -1 | awk '{print $4}'
}
