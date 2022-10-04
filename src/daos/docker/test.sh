#!/bin/bash
# Prerequisites:
#   create folders /opt/daos, /opt/ceph, /opt/s3-tests making sure there is about 50gb free
#   install git, docker
#   move docker storage to a location with about 250gb free
#   git clone daos, ceph & s3-tests (used for determining if docker images need to be rebuilt)
#   export CEPH_PATH=/opt/ceph
#   export DAOS_PATH=/opt/daos
#   export S3TESTS_PATH=/opt/s3-tests
#   (CEPH_PATH, DAOS_PATH & S3TESTS_PATH can be located in different folders on docker host)

set -x
source $CEPH_PATH/src/daos/error_handler.sh
source $CEPH_PATH/src/daos/set_boolean.sh
source $CEPH_PATH/src/daos/require_variables.sh
source $CEPH_PATH/src/daos/daos/daos_start.sh
source $CEPH_PATH/src/daos/daos/daos_stop.sh
source $CEPH_PATH/src/daos/daos/daos_format.sh
source $CEPH_PATH/src/daos/daos/daos_pool_create.sh
source $CEPH_PATH/src/daos/radosgw/radosgw_start.sh
source $CEPH_PATH/src/daos/radosgw/radosgw_stop.sh
source $CEPH_PATH/src/daos/radosgw/radosgw_create_s3bucket.sh

require_variables DAOS_PATH S3TESTS_PATH

function usage()
{
    # turn off echo
    set +x

    require_variables BOOLEAN_VALUES
    echo ""
    echo "./test.sh"
    echo -e "\t-h --help"
    echo -e "\t-y --summary=$BOOLEAN_VALUES default=FALSE"
    echo -e "\t-u --update-confluence=$BOOLEAN_VALUES default=TRUE"
    echo -e "\t-c --cleanup-container=$BOOLEAN_VALUES default=TRUE"
    echo -e "\t-b --build-docker-images=$BOOLEAN_VALUES default=TRUE"
    echo -e "\t-a --artifacts-folder=<folder_of_docker_container> default=/opt"
    echo -e "\t--local-artifacts=<folder_of_host> default=/opt"
    echo -e "\t--ceph-image-name=<image-name> default=dgw-single-host"
    echo -e "\t--daos-image-name=<image-name> default=daos-single-host"
    echo -e "\t--s3tests-image-name=<image-name> default=dgw-s3-tests"
    echo -e "\t--ceph-branch=<branch-to-use> default=add-daos-rgw-sal"
    echo -e "\t--daos-branch=<branch-to-use> default=libds3"
    echo ""
}

BUILDDOCKERIMAGES=true
SUMMARY=false
CLEANUP_CONTAINER=true
UPDATE_CONFLUENCE=true
ARTIFACTS_FOLDER=/opt
LOCAL_ARTIFACTS=/opt
START_DAOS=true
START_RADOSGW=true
CEPH_IMAGE_NAME='dgw-single-host'
DAOS_IMAGE_NAME='daos-single-host'
S3TESTS_IMAGE_NAME='dgw-s3-tests'
CEPH_BRANCH='add-daos-rgw-sal'
DAOS_BRANCH='libds3'

while [ "$1" != "" ]; do
    PARAM=`echo $1 | awk -F= '{print $1}'`
    VALUE=`echo $1 | awk -F= '{print $2}'`
    case $PARAM in
        -h | --help)
            usage
            exit 0
            ;;
        --start-daos)
            set_boolean START_DAOS $VALUE
            ;;
        --start-radosgw)
            set_boolean START_RADOSGW $VALUE
            ;;
        --artifacts-folder)
            ARTIFACTS_FOLDER=$VALUE
            ;;
        --local-artifacts)
            LOCAL_ARTIFACTS=$VALUE
            ;;
        -b | --build-docker-images)
            set_boolean BUILDDOCKERIMAGES $VALUE
            ;;
        -y | --summary)
            set_boolean SUMMARY $VALUE
            ;;
        -c | --cleanup-container)
            set_boolean CLEANUP_CONTAINER $VALUE
            ;;
        -u | --update-confluence)
            set_boolean UPDATE_CONFLUENCE $VALUE
            ;;
        --ceph-image-name)
            CEPH_IMAGE_NAME=$VALUE
            ;;
        --daos-image-name)
            DAOS_IMAGE_NAME=$VALUE
            ;;
        --s3tests-image-name)
            S3TESTS_IMAGE_NAME=$VALUE
            ;;
        --ceph-branch)
            CEPH_BRANCH=$VALUE
            ;;
        --daos-branch)
            DAOS_BRANCH=$VALUE
            ;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            usage
            exit 1
            ;;
    esac
    shift
done

function set_boolean()
{
    declare -n foo=$1
    case ${2^^} in
        TRUE | T | YES | Y | 1)
            foo=true
            ;;
        FALSE | F | NO | N | 0)
            foo=false
            ;;
        *)
            if [[ "$2" == "" ]]; then
                # just flip the meaning
                foo=$(($foo ^ true))
            else
                echo "ERROR: unknown value \"$VALUE\""
                usage
                exit 1
            fi
            ;;
    esac
}

while [ "$1" != "" ]; do
    PARAM=`echo $1 | awk -F= '{print $1}'`
    VALUE=`echo $1 | awk -F= '{print $2}'`
    case $PARAM in
        -h | --help)
            usage
            exit 0
            ;;
        -b | --build-docker-images)
            set_boolean BUILDDOCKERIMAGES $VALUE
            ;;
        -s | --summary)
            set_boolean SUMMARY $VALUE
            ;;
        -c | --cleanup-container)
            set_boolean CLEANUP_CONTAINER $VALUE
            ;;
        -u | --update-confluence)
            set_boolean UPDATE_CONFLUENCE $VALUE
            ;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            usage
            exit 1
            ;;
    esac
    shift
done

if [[ $RUN_DATE == "" ]]; then
    export RUN_DATE="$(date +"%Y-%m-%d")"
fi
if [[ $CONTAINER_NAME == "" ]]; then
    export CONTAINER_NAME="dgws3-$RUN_DATE"
fi
COMMAND_PREFIX="docker exec -u 0 $CONTAINER_NAME"
DAOS_BIN="/opt/daos/bin"

function start_docker_container()
{
    if [ ! "$(docker ps -q -f name=${CONTAINER_NAME})" ]; then
        if [ "$(docker ps -aq -f status=exited -f name=${CONTAINER_NAME})" ]; then
            # cleanup
            docker rm ${CONTAINER_NAME}
            if [[ ! $? == 0 ]]; then echo "failed"; exit 1; fi
        fi
        # run your container
        docker run -it -d --privileged --cap-add=ALL -h docker-$CONTAINER_NAME --name $CONTAINER_NAME -v /dev:/dev $S3TESTS_IMAGE_NAME
        if [[ ! $? == 0 ]]; then echo "failed"; exit 1; fi
    fi

    docker start $CONTAINER_NAME
    if [[ ! $? == 0 ]]; then echo "failed"; exit 1; fi
}

function start_daos_cortx_s3tests()
{
    if [[ $SUMMARY == false ]]; then
        if [[ $START_DAOS == true ]]; then
            daos_start
            daos_format
            daos_pool_create
        fi

        if [[ $START_RADOSGW == true ]]; then
            radosgw_start
        fi

        radosgw_create_s3bucket

        docker exec -u 0 $CONTAINER_NAME bash -c "sh /opt/ceph/src/daos/docker/s3-tests/run_tests.sh --color-output=false --artifacts-folder=$ARTIFACTS_FOLDER --cleandaos=true --restart=50 --stop-on-test-result=MISSING --run-on-test-result=MISSING,NOT_RUNNING"
        if [[ ! $? == 0 ]]; then echo "failed"; exit 1; fi

        radosgw_stop
        daos_stop
    else
        docker exec -u 0 $CONTAINER_NAME bash -c "sh /opt/ceph/src/daos/docker/s3-tests/run_tests.sh --color-output=false --artifacts-folder=$ARTIFACTS_FOLDER --summary"
        if [[ ! $? == 0 ]]; then echo "failed"; exit 1; fi
    fi
}

function copy_artifact()
{
    if [[ -e $LOCAL_ARTIFACTS/$1 ]]; then
        sudo chmod 666 $LOCAL_ARTIFACTS/$1
    fi
    docker cp $CONTAINER_NAME:$ARTIFACTS_FOLDER/$1 $LOCAL_ARTIFACTS/
    if [[ ! $? == 0 ]]; then echo "failed"; exit 1; fi
}

function copy_s3tests_artifacts()
{
    mkdir -p $LOCAL_ARTIFACTS
    copy_artifact "test_summary.csv"
    copy_artifact "test_output.csv"
    copy_artifact "test_diff.csv"
}

function cleanup_container()
{
    docker stop $CONTAINER_NAME
    docker container rm $CONTAINER_NAME --force
}

function update_confluence_s3tests_page()
{
    if [[ $CONFLUENCE_UPDATE_EMAIL == "" ]] || [[ $CONFLUENCE_UPDATE_KEY == "" ]]; then
        echo "Confluence credentials not set, skipping update of page"
        return
    fi
    if [[ ! -e update_confluence.py ]]; then
        echo "python file not found in: $PWD"
        return
    fi
    python3 update_confluence.py $LOCAL_ARTIFACTS/test_summary.csv
}

function cleanup_hugepages()
{
    # check the number of free hugepages.  These may need to be cleared
    # grep HugePages_Free /proc/meminfo
    sudo rm -f /dev/hugepages/spdk*
}

function reboot_jenkins_node()
{
    # reboot moved to the cleanup stage in jenkins - will always reboot even if the build fails)
    # reboots the host in 30 seconds but doesn't wait (jenkins build will not fail)
    (sudo bash -c "(sleep 30 && sudo shutdown -r now) &") &
}

GIT_ENUM_STATES=(UP_TO_DATE PULL_NEEDED PUSH_NEEDED DIVERGED)
tam=${#GIT_ENUM_STATES[@]}
for ((i=0; i < $tam; i++)); do
    name=${GIT_ENUM_STATES[i]}
    declare -r ${name}=$i
done

function get_git_status()
{
    pushd $1
    git remote update

    local LOCAL=$(git rev-parse @)
    local REMOTE=$(git rev-parse "@{u}")
    local BASE=$(git merge-base @ "@{u}")
    local RESULT

    if [ $LOCAL = $REMOTE ]; then
        RESULT=$UP_TO_DATE    # echo "Up-to-date"
    elif [ $LOCAL = $BASE ]; then
        RESULT=$PULL_NEEDED    # echo "Need to pull"
        git pull
    elif [ $REMOTE = $BASE ]; then
        RESULT=$PUSH_NEEDED    # echo "Need to push"
    else
        RESULT=$DIVERGED    # echo "Diverged"
    fi
    popd
    return $RESULT
}

function build_docker_images()
{
    DAOS_BUILT=0
    CEPH_BUILT=0
    get_git_status $DAOS_PATH
    DAOS_GIT=$?
    docker inspect -f '{{ .Created }}' daos-rocky
    DAOS_ROCKY=$?
    docker inspect -f '{{ .Created }}' $DAOS_IMAGE_NAME
    DAOS_SINGLE_HOST=$?
    if [[ $DAOS_GIT == $PULL_NEEDED ]] || [[ $DAOS_ROCKY != 0 ]] || [[ $DAOS_SINGLE_HOST != 0 ]]; then
        pushd daos
        docker build https://github.com/daos-stack/daos.git#$DAOS_BRANCH -f utils/docker/Dockerfile.el.8 -t daos-rocky
        error_handler $? $(basename $0) FUNCNAME LINENO
        docker build . -t "$DAOS_IMAGE_NAME"
        error_handler $? $(basename $0) FUNCNAME LINENO
        DAOS_BUILT=1
        popd
    fi
    get_git_status $CEPH_PATH
    CEPH_GIT=$?
    docker inspect -f '{{ .Created }}' $CEPH_IMAGE_NAME
    DGW_SINGLE=$?
    if [[ $DAOS_BUILT != 0 ]] || [[ $CEPH_GIT == $PULL_NEEDED ]] || [[ $DGW_SINGLE != 0 ]]; then
        pushd ceph
        docker build . -t "$CEPH_IMAGE_NAME"
        error_handler $? $(basename $0) FUNCNAME LINENO
        CEPH_BUILT=1
        popd
    fi
    get_git_status $S3TESTS_PATH
    S3TESTS_GIT=$?
    docker inspect -f '{{ .Created }}' $S3TESTS_IMAGE_NAME
    DGW_S3TESTS=$?
    if [[ $CEPH_BUILT != 0 ]] || [[ $S3TESTS_GIT == $PULL_NEEDED ]] || [[ $DGW_S3TESTS != 0 ]]; then
        pushd s3-tests
        docker build . -t "$S3TESTS_IMAGE_NAME"
        error_handler $? $(basename $0) FUNCNAME LINENO
        popd
    fi

    # remove all of the extra images lying around
    docker rmi $(docker images -f "dangling=true" -q)
}

cleanup_hugepages
hugepages=`grep HugePages_Free /proc/meminfo | grep -oE "[0-9]+"`
if [ $hugepages -lt 512 ]; then
    echo "Not enough free hugepages: $hugepages < 512, skipping run, reboot node required"
else
    if [[ $BUILDDOCKERIMAGES == true ]]; then
        build_docker_images
    fi
    start_docker_container
    start_daos_cortx_s3tests
    copy_s3tests_artifacts
    if [[ $UPDATE_CONFLUENCE == true ]]; then
        update_confluence_s3tests_page
    fi
    if [[ $CLEANUP_CONTAINER == true ]]; then
        cleanup_container
    fi
fi
