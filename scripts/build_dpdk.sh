#!/bin/bash -x

set -x

SCRIPT_DIR=`dirname $(readlink -f $0)`

#DPDK download path
#if any change kindly update DPDK_DOWNLOAD_PATH in DMM/stacks/lwip_stack/vagrant/start_nStackMain.sh
DPDK_DOWNLOAD_PATH=/tmp/dpdk

#dpdk installation path
DPDK_INSTALL_PATH=/usr

OS_ID=$(grep '^ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')
echo OS_ID: $OS_ID

#DPDK will be having dependancy on linux headers
if [ "$OS_ID" == "ubuntu" ]; then
    sudo apt-get -y install git build-essential linux-headers-`uname -r`
    sudo apt-get -y install libnuma-dev
elif [ "$OS_ID" == "debian" ]; then
    sudo apt-get -y install git build-essential linux-headers-`uname -r`
elif [ "$OS_ID" == "centos" ]; then
    sudo yum groupinstall -y "Development Tools"
    sudo yum install -y kernel-headers
    sudo yum install -y numactl-devel
elif [ "$OS_ID" == "opensuse" ]; then
    sudo yum groupinstall -y "Development Tools"
    sudo yum install -y kernel-headers
fi

#===========build DPDK================

if [ "$OS_ID" == "centos" ]; then
    bash -x $SCRIPT_DIR/build_dpdk1802.sh || exit 1
else

    if [ ! -d  /usr/include/dpdk ] || [ ! -d  /usr/share/dpdk ] || [ ! -d  /usr/lib/modules/4.4.0-31-generic/extra/dpdk ]; then
        mkdir -p $DPDK_DOWNLOAD_PATH

        cd $DPDK_DOWNLOAD_PATH
        wget -N https://fast.dpdk.org/rel/dpdk-18.02.tar.xz --no-check-certificate
        tar xvf dpdk-18.02.tar.xz
        cd dpdk-18.02

        sed -i 's!CONFIG_RTE_EXEC_ENV=.*!CONFIG_RTE_EXEC_ENV=y!1' config/common_base
        sed -i 's!CONFIG_RTE_BUILD_SHARED_LIB=.*!CONFIG_RTE_BUILD_SHARED_LIB=y!1' config/common_base
        sed -i 's!CONFIG_RTE_LIBRTE_EAL=.*!CONFIG_RTE_LIBRTE_EAL=y!1' config/common_base
        sed -i 's!CONFIG_RTE_EAL_PMD_PATH=.*!CONFIG_RTE_EAL_PMD_PATH="/tmp/dpdk/drivers/"!1' config/common_base

        sudo make install  T=x86_64-native-linuxapp-gcc DESTDIR=${DPDK_INSTALL_PATH} -j 4

        if [ $? -eq 0 ]; then
            echo "DPDK build is SUCCESS"
        else
            echo "DPDK build has FAILED"
            exit 1
        fi

        mkdir -p /tmp/dpdk/drivers/
        cp -f /usr/lib/librte_mempool_ring.so /tmp/dpdk/drivers/
    fi
fi
