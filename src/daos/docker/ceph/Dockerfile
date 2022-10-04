# Copyright 2022 Seagate Corporation
# All rights reserved.
#
# 'recipe' for Docker to build an image of radosgw + DAOS
#

# Pull base image
ARG BASE_DISTRO=daos-single-host:latest
FROM $BASE_DISTRO
LABEL maintainer="seagate-daos@seagate.com"

# get & build the ceph source
COPY ccache.conf /etc/ccache.conf

# redhat.repo is only available from within Seagate's network and is needed since xmlstarlet was removed from @system repo
COPY redhat.repo /etc/yum.repos.d/redhat.repo
RUN sudo dnf clean all
RUN sudo dnf makecache
# redhat.repo end changes

RUN sudo dnf install dnf-plugins-core -y
RUN sudo dnf config-manager --set-enabled powertools
RUN sudo dnf -y module enable javapackages-tools
RUN sudo dnf install epel-release -y
RUN sudo dnf install gcc-toolset-11 -y

# xz is needed to decompress the tar file
RUN sudo dnf install xz -y
RUN sudo dnf install wget -y
RUN sudo mkdir -p /opt/ccache && sudo chmod -R 777 /opt/ccache
RUN mkdir -p /opt/ccache/build/
RUN cd /opt/ccache/build && wget https://github.com/ccache/ccache/releases/download/v4.6.1/ccache-4.6.1.tar.xz
RUN cd /opt/ccache/build && tar -xf ccache-4.6.1.tar.xz
RUN mkdir -p /opt/ccache/build/ccache-4.6.1/build
RUN cd /opt/ccache/build/ccache-4.6.1/build && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc  -DZSTD_FROM_INTERNET=ON -DHIREDIS_FROM_INTERNET=ON .. && make && sudo make install
ENV SOURCE_DATE_EPOCH=946684800
RUN cd /opt && sudo mkdir -p ceph && sudo chmod 777 ceph && git clone --recurse https://github.com/zalsader/ceph --branch add-daos-rgw-sal
RUN cd /opt/ceph && ./install-deps.sh
RUN pip3 install cython prettytable

# scl_source needs to be executed on the same line as the compile, else it doesn't use gcc-11
RUN source scl_source enable gcc-toolset-11 && cd /opt/ceph && cmake3 -GNinja -DPC_DAOS_INCLUDEDIR=${DAOS_PATH}/include -DPC_DAOS_LIBDIR=${DAOS_PATH}/lib64 -DWITH_PYTHON3=3.6 -DWITH_RADOSGW_DAOS=YES -DWITH_CCACHE=ON -DENABLE_GIT_VERSION=OFF -B build && cd build && ninja vstart
RUN cd /opt/ceph/build && sudo MDS=0 RGW=1 ../src/vstart.sh -f -n --without-dashboard
RUN cd /opt/ceph/build && sudo ../src/stop.sh
COPY ceph.sed /opt/ceph/build
RUN cd /opt/ceph/build && sed -i -f ceph.sed ceph.conf
ENV CEPH_PATH=/opt/ceph
