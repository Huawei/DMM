#!/bin/bash -x

set -x

OS_ID=$(grep '^ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')

DMM_DIR=`dirname $(readlink -f $0)`/../
BUILD_DIR=${DMM_DIR}/build

############### build rsocket
echo "rsocket build start"
cd $DMM_DIR/stacks/rsocket
if [ "$OS_ID" == "ubuntu" ]; then
    wget http://www.mellanox.com/downloads/ofed/MLNX_OFED-4.4-1.0.0.0/MLNX_OFED_LINUX-4.4-1.0.0.0-ubuntu16.04-x86_64.tgz
    tar -zxvf MLNX_OFED_LINUX-4.4-1.0.0.0-ubuntu16.04-x86_64.tgz
    cd MLNX_OFED_LINUX-4.4-1.0.0.0-ubuntu16.04-x86_64
elif [ "$OS_ID" == "centos" ]; then
    CENT_VERSION=`grep -oE '[0-9]+\.[0-9]+' /etc/redhat-release`
    wget http://www.mellanox.com/downloads/ofed/MLNX_OFED-4.4-1.0.0.0/MLNX_OFED_LINUX-4.4-1.0.0.0-rhel${CENT_VERSION}-x86_64.tgz
    tar -zxvf MLNX_OFED_LINUX-4.4-1.0.0.0-rhel${CENT_VERSION}-x86_64.tgz
    cd MLNX_OFED_LINUX-4.4-1.0.0.0-rhel${CENT_VERSION}-x86_64
fi

sudo ./mlnxofedinstall --force || exit 1

cd $BUILD_DIR
make dmm_rsocket
if [ $? -eq 0 ]; then
    echo "rsocket build has SUCCESS"
else
    echo "rsocket build has FAILED"
    exit 1
fi
echo "rsocket build finished"