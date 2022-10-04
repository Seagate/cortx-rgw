#!/bin/bash

function usage()
{
    # turn off echo
    set +x

    # BOOLEAN_VALUES is copied from src/daos/set_boolean.sh because it may not be available.  The idea is that
    # this script is completely standalone until the repo/branch is set
    local BOOLEAN_VALUES="T[RUE]|Y[ES]|F[ALSE]|N[O]|1|0"
    echo ""
    echo "./install-single-host-dgw.sh"
    echo -e "\t-h --help"
    echo -e "\t-db --daos-branch=<git-branch-to-use> default=libds3"
    echo -e "\t-cb --ceph-branch=<git-branch-to-use> default=add-daos-rgw-sal"
    echo -e "\t-dp --daos-path=<git-clone-path> default=/opt/daos"
    echo -e "\t-cp --ceph-path=<git-clone-path> default=/opt/ceph"
    echo -e "\t-dr --daos-repo=<git-repo> default=https://github.com/daos-stack/daos"
    echo -e "\t-cr --ceph-repo=<git-repo> default=https://github.com/zalsader/ceph"
    echo -e "\t-cbp --ccache-build-path=<ccache-build-path> default=/opt/ccache-build"
    echo -e "\t-cc --ccache-path=<ccache-path> default=/opt/ccache"
    echo -e "\t-ep --enable-passwordless-sudo=$BOOLEAN_VALUES default=true"
    echo ""
}

function ceph_get()
{
    # wget --output-document=folder_free_space.sh https://github.com/zalsader/ceph/blob/docker-build/src/daos/folder_free_space.sh?raw=true
    while (( $# )); do
        local output_file=$(basename -- $1)
        local repo_path=$1
        wget --output-document=${output_file} $CEPH_REPO/blob/$CEPH_BRANCH/${repo_path}?raw=true
        CLEANUP_FILES+=( ${output_file} )
        shift
    done
}

function free_space_check()
{
    declare -n CHECK_PATH=$1
    if [[ ! -d $CHECK_PATH ]]; then
        if [[ ! -w $(dirname $CHECK_PATH) ]]; then
            sudo chmod 777 $(dirname $CHECK_PATH)
        fi
        mkdir -p $CHECK_PATH
    fi
    local FREE_SPACE=$(folder_free_space $CHECK_PATH)
    if [ $FREE_SPACE -lt $2 ]; then
        echo "Not enough free disk space for $CHECK_PATH, requires a minimum of $2, found $FREE_SPACE"
        exit 1
    fi
}

function set_passwordless_sudo()
{
    if [[ $PASSWORDLESS_SUDO == true ]]; then
        sudo cp /etc/sudoers /etc/sudoers.txt
        sudo chmod 666 /etc/sudoers.txt
        echo "ALL            ALL = (ALL) NOPASSWD: ALL" >> /etc/sudoers.txt
        sudo chmod 110 /etc/sudoers.txt
        sudo mv /etc/sudoers.txt /etc/sudoers
    fi
}

function assign_path()
{
    declare -n ASSIGN_PATH=$1
    set +e
    grep "$1" ~/.bashrc
    if [[ ! $? == 0 ]]; then
        echo "export $1=$ASSIGN_PATH" >> ~/.bashrc
    fi
    set -e
    export $1=$ASSIGN_PATH
}

function append_ccache_source_date()
{
    set +e
    grep "SOURCE_DATE_EPOCH" ~/.bashrc
    if [[ ! $? == 0 ]]; then
        echo "export SOURCE_DATE_EPOCH=946684800" >> ~/.bashrc
    fi
    set -e
    export SOURCE_DATE_EPOCH=946684800
}

function use_gcc11()
{
    set +e
    sudo dnf install gcc-toolset-11 -y
    grep "gcc-toolset-11" ~/.bashrc
    if [[ ! $? == 0 ]]; then
        echo "source scl_source enable gcc-toolset-11" >> ~/.bashrc
    fi
    set -e
    source scl_source enable gcc-toolset-11
}

function search_first_word_and_append_to_ccache()
{
    local first_word=$(echo $1 | cut -d " " -f 1)
    set +e
    grep "^${first_word}" /etc/ccache.conf
    if [[ ! $? == 0 ]]; then
        sudo bash -c "echo \"$1\" >> /etc/ccache.conf"
    fi
    set -e
}

function install_powertools()
{
    sudo dnf install dnf-plugins-core -y
    sudo dnf config-manager --set-enabled powertools
    sudo dnf -y module enable javapackages-tools
    sudo dnf install epel-release -y
}

function create_daos_certificates()
{
    pushd /tmp
    $DAOS_PATH/install/lib64/daos/certgen/gen_certificates.sh
    sudo mkdir -p /etc/daos/certs/clients
    sudo cp /tmp/daosCA/certs/* /etc/daos/certs/.
    sudo cp /tmp/daosCA/certs/agent.crt /etc/daos/certs/clients/agent.crt
    sudo rm -rf /tmp/daosCA
    popd
}

function setup_hugepages()
{
    set +e
    grep -E "vm.nr_hugepages.*=.*[0-9]+" /etc/sysctl.conf
    if [[ ! $? == 0 ]]; then
        sudo bash -c 'echo "vm.nr_hugepages = 512" >> /etc/sysctl.conf'
        REBOOT_REQUIRED=true
    else
        local huge_pages_config=`grep -E "vm.nr_hugepages.*=.*[0-9]+" /etc/sysctl.conf`
        local huge_pages_count=`echo "$huge_pages_config" | grep -oE "[0-9]+"`
        echo "huge_pages_count=$huge_pages_count huge_pages_config=$huge_pages_config"
        if [ "$huge_pages_count" -lt 512 ]; then
            sudo sed -iE "s/vm.nr_hugepages[ \t]*=[ \t]*[0-9]*/vm.nr_hugepages = 512/" /etc/sysctl.conf
            REBOOT_REQUIRED=true
        fi
    fi
    set -e
}

function build_daos()
{
    pushd $(dirname -- $DAOS_PATH)
    if [[ ! -e $DAOS_PATH/README.md ]]; then
        git clone --recurse-submodules $DAOS_REPO --branch $DAOS_BRANCH
    fi
    assign_path DAOS_PATH
    cd $DAOS_PATH
    # allow dnf to assume "y"
    sudo dnf --assumeyes install dnf-plugins-core
    sudo dnf config-manager --save --setopt=assumeyes=True
    sudo ./utils/scripts/install-el8.sh
    sudo yum install python3-scons -y
    sudo pip3 install meson==0.59.2 ninja pyelftools distro
    scons-3 --config=force --build-deps=yes install
    popd

    # get the DAOS yaml files
    ceph_get src/daos/docker/daos/docker_daos_agent.yml src/daos/docker/daos/docker_daos_control.yml src/daos/docker/daos/docker_daos_server.yml

    # don't use yq as it makes unintended changes & removes comments
    FABRIC_INTERFACE=$(ip a l | grep -m 1 BROADCAST | cut -d " " -f 2 | sed "s/:$//")
    sed -i "s/fabric_iface:.*$/fabric_iface: $FABRIC_INTERFACE/" docker_daos_server.yml

    mkdir -p $DAOS_PATH/daos/install/etc/
    cp docker_daos_server.yml $DAOS_PATH/install/etc/daos_server.yml
    cp docker_daos_agent.yml $DAOS_PATH/install/etc/daos_agent.yml
    cp docker_daos_control.yml $DAOS_PATH/install/etc/daos_control.yml
    create_daos_certificates

    setup_hugepages
}

build_ccache()
{
    sudo mkdir -p $CCACHE_BUILD_PATH
    sudo chmod 777 $CCACHE_BUILD_PATH
	pushd $CCACHE_BUILD_PATH
	wget https://github.com/ccache/ccache/releases/download/v4.6.1/ccache-4.6.1.tar.xz
	tar -xf ccache-4.6.1.tar.xz
	cd ccache-4.6.1/
	mkdir -p build
	cd build
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc  -DZSTD_FROM_INTERNET=ON -DHIREDIS_FROM_INTERNET=ON ..
	make
	sudo make install
    popd
    sudo mkdir -p $CCACHE_PATH
    sudo chmod -R 777 $CCACHE_PATH
}

function starlet_workaround()
{
    # as a workaround for xmlstarlet being removed from the el8 repo, we've stored the Seagate repo
    set +e
    sudo dnf provides xmlstarlet
    local dnf_result=$?
    set -e
    if [[ ! $dnf_result == 0 ]]; then
        starlet_manually_installed=true
        sed -i "/xmlstarlet/d" ceph.spec.in
        sudo pip3 install -U pip
        sudo pip3 install -U setuptools
        sudo pip3 install xmlstarlet
    fi
}

function build_ceph()
{
    pushd $(dirname -- $CEPH_PATH)
    if [[ ! -e $CEPH_PATH/README.md ]]; then
        git clone --recurse $CEPH_REPO --branch $CEPH_BRANCH
    fi
    assign_path CEPH_PATH
    cd $CEPH_PATH
    starlet_workaround
    ./install-deps.sh
    if [[ $starlet_manually_installed == true ]]; then
        git restore ceph.spec.in
    fi
    sudo yum install ccache -y
    search_first_word_and_append_to_ccache "max_size = 25G"
    search_first_word_and_append_to_ccache "sloppiness = time_macros"
    search_first_word_and_append_to_ccache "run_second_cpp = true"
    append_ccache_source_date
    cmake3 -GNinja -DPC_DAOS_INCLUDEDIR=${DAOS_PATH}/install/include -DPC_DAOS_LIBDIR=${DAOS_PATH}/install/lib64 -DWITH_PYTHON3=3.6 -DWITH_RADOSGW_DAOS=YES -DWITH_CCACHE=ON -DENABLE_GIT_VERSION=OFF -B build
    cd build
    ninja vstart
    ceph_get src/daos/docker/ceph/ceph.sed
    RGW=1 ../src/vstart.sh -d -n
    ../src/stop.sh
    sed -i -f ceph.sed ceph.conf
    popd
}

# Defaults
CCACHE_BUILD_PATH='/opt/ccache-build'
CCACHE_PATH='/opt/ccache'
CEPH_BRANCH='add-daos-rgw-sal'
DAOS_BRANCH='libds3'
CEPH_PATH='/opt/ceph'
DAOS_PATH='/opt/daos'
CLEANUP_FILES=()
CEPH_REPO='https://github.com/zalsader/ceph'
DAOS_REPO='https://github.com/daos-stack/daos'
REBOOT_REQUIRED=false
PASSWORDLESS_SUDO=true

# Read args
while (( $# ))
    do
    PARAM=`echo $1 | awk -F= '{print $1}'`
    VALUE=`echo $1 | awk -F= '{print $2}'`
    case $PARAM in
        -h | --help)
            usage
            exit 0
            ;;
        -db | --daos-branch)
            DAOS_BRANCH=$VALUE
            ;;
        -cb | --ceph-branch)
            CEPH_BRANCH=$VALUE
            ;;
        -dp | --daos-path)
            # eval is to expand ~
            eval DAOS_PATH=$VALUE
            ;;
        -cp | --ceph-path)
            # eval is to expand ~
            eval CEPH_PATH=$VALUE
            ;;
        -cbp | --ccache-build-path)
            # eval is to expand ~
            eval CCACHE_BUILD_PATH=$VALUE
            ;;
        -cc | --ccache-path)
            # eval is to expand ~
            eval CCACHE_PATH=$VALUE
            ;;
        -dr | --daos-repo)
            DAOS_REPO=$VALUE
            ;;
        -cr | --ceph-repo)
            CEPH_REPO=$VALUE
            ;;
        -ep | --enable-passwordless-sudo)
            # lets hope it actually finds the shell script
            ceph_get src/daos/set_boolean.sh
            source ./set_boolean.sh
            set_boolean PASSWORDLESS_SUDO $VALUE
            ;;
        *)
            echo "Unknown option $1"
            usage
            exit 1
            ;;
    esac
    shift
done

# Fail on errors
set -e
# Debug
set -x
# Main
ceph_get src/daos/folder_free_space.sh
source ./folder_free_space.sh
set_passwordless_sudo
free_space_check DAOS_PATH 10000000
free_space_check CEPH_PATH 70000000
DAOS_STORAGE='/tmp'
free_space_check DAOS_STORAGE 5000000
starlet_manually_installed=false

# install packages & install the latest
sudo dnf install openssl git jq net-tools iproute -y
sudo dnf update -y

use_gcc11
install_powertools
build_daos
build_ccache
build_ceph

# cleanup files downloaded during the script (before the reboot)
rm -f ${CLEANUP_FILES[@]}

if [[ $REBOOT_REQUIRED == true ]]; then
    echo -e "*** WARNING ***"
    read -p "Reboot is required.  Press enter to reboot now or ^C to cancel and reboot later"
    sudo reboot
fi
