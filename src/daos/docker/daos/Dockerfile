# Copyright 2022 Seagate Corporation
# All rights reserved.
#
# 'recipe' for Docker to build an image of radosgw + DAOS
#

# Pull base image
ARG BASE_DISTRO=daos-rocky:latest
FROM $BASE_DISTRO
LABEL maintainer="seagate-daos@seagate.com"

# needed to create certs, git, ccache
RUN sudo dnf install openssl git ccache jq net-tools iproute -y
RUN sudo dnf update -y

# copy the daos yml files for a local run
COPY docker_daos_server.yml /opt/daos/etc/daos_server.yml
COPY docker_daos_agent.yml /opt/daos/etc/daos_agent.yml
COPY docker_daos_control.yml /opt/daos/etc/daos_control.yml
ENV DAOS_PATH=/opt/daos
RUN sudo bash -c 'echo "vm.nr_hugepages = 513" >> /etc/sysctl.conf'

# create the certificates that will be used by daos
RUN cd /tmp && /opt/daos/lib64/daos/certgen/gen_certificates.sh
RUN sudo mkdir -p /etc/daos/certs/clients
RUN sudo cp /tmp/daosCA/certs/* /etc/daos/certs/.
RUN sudo cp /tmp/daosCA/certs/agent.crt /etc/daos/certs/clients/agent.crt
RUN sudo rm -rf /tmp/daosCA
