#!/bin/bash
set -x
set -e

git_folder=$(dirname "$0")
pushd ${git_folder}
CEPH_BRANCH=`git branch --show-current`
DAOS_BRANCH='libds3'

popd
default_branch='add-daos-rgw-sal'

function usage()
{
    # turn off echo
    set +x

    echo ""
    echo "./build-dev.sh"
    echo -e "\t-h --help"
    echo -e "\t-cb --ceph-branch=<development_ceph_branch>"
    echo -e "\t-db --daos-branch=<daos_branch>"
    echo ""
}

while (( $# ))
    do
    PARAM=`echo $1 | awk -F= '{print $1}'`
    VALUE=`echo $1 | awk -F= '{print $2}'`
    case $PARAM in
        -h | --help)
            usage
            exit 0
            ;;
        -cb | --ceph-branch)
            CEPH_BRANCH=$VALUE
            ;;
        -db | --daos-branch)
            DAOS_BRANCH=$VALUE
            ;;
        *)
            echo "Unknown option $1"
            usage
            exit 1
            ;;
    esac
    shift
done

function get_s3_dockerfile()
{
    if [[ $CEPH_BRANCH == $default_branch ]]; then
        echo "Dockerfile"
    else
        sed "s/dgw-single-host/dgw-single-dev/g" < Dockerfile > /tmp/Dockerfile.dev.s3
        echo "/tmp/Dockerfile.dev.s3"
    fi
}

function get_ceph_dockerfile()
{
    if [[ $CEPH_BRANCH == $default_branch ]]; then
        echo "Dockerfile"
    else
        sed "s/add-daos-rgw-sal/${CEPH_BRANCH}/g; s/daos-single-host/daos-single-dev/g" < Dockerfile > /tmp/Dockerfile.dev.ceph
        echo "/tmp/Dockerfile.dev.ceph"
    fi
}

if [[ $CEPH_BRANCH == '' ]]; then
    echo "Failed to identify/use the branch specified"
    exit 1
fi

function build_daos()
{
    set +e
    daosrocky=`docker images | grep -o "daos-rocky"`
    set -e
    if [[ ! $daosrocky == 'daos-rocky' ]]; then
        docker build "https://github.com/daos-stack/daos.git#${DAOS_BRANCH}" -f utils/docker/Dockerfile.el.8 -t daos-rocky
        if [[ ! $? == 0 ]]; then exit 1; fi
    fi
    pushd daos
    docker build . -t "daos-single-$image_type"
    if [[ ! $? == 0 ]]; then exit 1; fi
    popd
}

image_type="host"
if [[ ! $CEPH_BRANCH == $default_branch ]]; then
    image_type="dev"
fi

build_daos

pushd ceph
docker build . -t "dgw-single-$image_type" -f $(get_ceph_dockerfile)
if [[ ! $? == 0 ]]; then exit 1; fi
popd

pushd s3-tests
docker build . -t "dgw-s3-$image_type" -f $(get_s3_dockerfile)
if [[ ! $? == 0 ]]; then exit 1; fi
popd
