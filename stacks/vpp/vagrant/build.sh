#!/bin/bash -x
#########################################################################
# Copyright (c) 2018 Huawei Technologies Co.,Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#########################################################################

set -x

TIMESTAMP=$(date +%Y-%m-%d_%H-%M-%S)
log_file="/tmp/build_log.txt-$TIMESTAMP"
exec 1> >(tee -a "$log_file")  2>&1

# Get Command Line arguements if present
TEMP_DIR=$1
if [ "x$1" != "x" ]; then
    TEMP_DIR=$1
    DMM_BUILD_DIR=${TEMP_DIR}/build
    DPDK_BUILD_SCRIPT_DIR=${DMM_BUILD_DIR}/../scripts
    VPP_BUILD_DIR=${TEMP_DIR}/stacks/vpp/vpp/
else
    TEMP_DIR=`dirname $(readlink -f $0)`/..
    DMM_BUILD_DIR=${TEMP_DIR}/../../build
    DPDK_BUILD_SCRIPT_DIR=${DMM_BUILD_DIR}/../scripts
    VPP_BUILD_DIR=${TEMP_DIR}
fi

echo 0:$0
echo 1:$1
echo 2:$2
echo TEMP_DIR: $TEMP_DIR
echo DMM_BUILD_DIR: $DMM_BUILD_DIR
echo DPDK_BUILD_SCRIPT_DIR: $DPDK_BUILD_SCRIPT_DIR
echo VPP_BUILD_DIR: $VPP_BUILD_DIR

OS_ID=$(grep '^ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')
OS_VERSION_ID=$(grep '^VERSION_ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')
KERNEL_OS=`uname -o`
KERNEL_MACHINE=`uname -m`
KERNEL_RELEASE=`uname -r`
KERNEL_VERSION=`uname -v`

echo KERNEL_OS: $KERNEL_OS
echo KERNEL_MACHINE: $KERNEL_MACHINE
echo KERNEL_RELEASE: $KERNEL_RELEASE
echo KERNEL_VERSION: $KERNEL_VERSION
echo OS_ID: $OS_ID
echo OS_VERSION_ID: $OS_ID

#DPDK download path
DPDK_DOWNLOAD_PATH=/tmp/dpdk

#dpdk installation path
DPDK_INSTALL_PATH=/usr

#set and check the environment for Linux
if [ "$OS_ID" == "ubuntu" ]; then
    export DEBIAN_FRONTEND=noninteractive
    export DEBCONF_NONINTERACTIVE_SEEN=true

    APT_OPTS="--assume-yes --no-install-suggests --no-install-recommends -o Dpkg::Options::=\"--force-confdef\" -o Dpkg::Options::=\"--force-confold\""
    sudo apt-get update ${APT_OPTS}
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -yq git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump libpcre3 libpcre3-dev zlibc zlib1g zlib1g-dev vim ethtool unzip
elif [ "$OS_ID" == "debian" ]; then
    echo "not tested for debian and exit"
    exit 1
    export DEBIAN_FRONTEND=noninteractive
    export DEBCONF_NONINTERACTIVE_SEEN=true

    APT_OPTS="--assume-yes --no-install-suggests --no-install-recommends -o Dpkg::Options::=\"--force-confdef\" -o Dpkg::Options::=\"--force-confold\""
    sudo apt-get update ${APT_OPTS}
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -yq git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump libpcre3 libpcre3-dev zlibc zlib1g zlib1g-dev vim
elif [ "$OS_ID" == "centos" ]; then
    sudo yum install -y git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump vim sudo yum-utils pcre-devel zlib-devel
elif [ "$OS_ID" == "opensuse" ]; then
    echo "not tested for opensuse and exit"
    exit 1
    sudo yum install -y git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump vim sudo yum-utils pcre-devel zlib-devel
fi

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

if [ ! -d  /usr/include/dpdk ] || [ ! -d  /usr/share/dpdk ] || [ ! -d  /usr/lib/modules/4.4.0-31-generic/extra/dpdk ]; then
     mkdir -p $DPDK_DOWNLOAD_PATH

     cd $DPDK_DOWNLOAD_PATH
     wget -N https://fast.dpdk.org/rel/dpdk-18.02.tar.xz --no-check-certificate
     tar xvf dpdk-18.02.tar.xz
     cd dpdk-18.02/


     sed -i 's!CONFIG_RTE_EXEC_ENV=.*!CONFIG_RTE_EXEC_ENV=y!1' config/common_base
     sed -i 's!CONFIG_RTE_BUILD_SHARED_LIB=.*!CONFIG_RTE_BUILD_SHARED_LIB=y!1' config/common_base
     sed -i 's!CONFIG_RTE_LIBRTE_EAL=.*!CONFIG_RTE_LIBRTE_EAL=y!1' config/common_base
     sed -i 's!CONFIG_RTE_EAL_PMD_PATH=.*!CONFIG_RTE_EAL_PMD_PATH="/tmp/dpdk/drivers/"!1' config/common_base

     sudo make install  T=x86_64-native-linuxapp-gcc DESTDIR=${DPDK_INSTALL_PATH} -j 4

     mkdir -p /tmp/dpdk/drivers/
     cp -f /usr/lib/librte_mempool_ring.so /tmp/dpdk/drivers/
fi

#===========check running env =================
hugepagesize=$(cat /proc/meminfo | grep Hugepagesize | awk -F " " {'print$2'})
if [ "$hugepagesize" == "2048" ]; then
    pages=2560
elif [ "$hugepagesize" == "1048576" ]; then
    pages=5
fi
sudo sysctl -w vm.nr_hugepages=$pages
HUGEPAGES=`sysctl -n  vm.nr_hugepages`
if [ $HUGEPAGES != $pages ]; then
    echo "ERROR: Unable to get $pages hugepages, only got $HUGEPAGES.  Cannot finish."
    exit
fi


hugepageTotal=$(cat /proc/meminfo | grep -c "HugePages_Total:       0")
if [ $hugepageTotal -ne 0 ]; then
  echo "HugePages_Total is zero"
  exit
fi

hugepageFree=$(cat /proc/meminfo | grep -c "HugePages_Free:        0")
if [ $hugepageFree -ne 0 ]; then
  echo "HugePages_Free is zero"
  exit
fi

hugepageSize=$(cat /proc/meminfo | grep -c "Hugepagesize:          0 kB")
if [ $hugepageSize -ne 0 ]; then
  echo "Hugepagesize is zero"
  exit
fi


sudo mkdir /mnt/nstackhuge -p
if [ "$hugepagesize" == "2048" ]; then
sudo mount -t hugetlbfs -o pagesize=2M none /mnt/nstackhuge/
elif [ "$hugepagesize" == "1048576" ]; then
    sudo mount -t hugetlbfs -o pagesize=1G none /mnt/nstackhuge/
fi

#===========build DMM=================
echo "DMM build started....."

cd $DMM_BUILD_DIR
ldconfig
rm -rf *
cmake ..
make -j 8
if [ $? -eq 0 ]; then
    echo "DMM build is SUCCESS"
else
    echo "DMM build has FAILED"
    exit 1
fi
echo "DMM build finished....."

git config --global http.sslVerify false
#===========build vpp===========
echo "vpp build started....."
make vpp-stack
if [ $? -eq 0 ]; then
    echo "vpp build is SUCCESS"
else
    echo "vpp build has FAILED"
    exit 1
fi
echo "vpp build finished....."

#===========set environment===========
sudo mkdir -p /etc/vpp/
cp /dmm/stacks/vpp/configure/startup.conf /etc/vpp/
cp /dmm/stacks/vpp/configure/vpp_config /etc/vpp/

sudo cp -r /dmm/stacks/vpp/vpp/build-root/install-vpp_debug-native/vpp/lib64/vpp_plugins/ /usr/lib/

sudo modprobe uio
sudo insmod ${DPDK_DOWNLOAD_PATH}/dpdk-18.02/x86_64-native-linuxapp-gcc/kmod/igb_uio.ko

sudo ifconfig enp0s9 down

if [ "$OS_ID" == "centos" ]; then
    ifaddress1=$(ifconfig enp0s9 | grep 'inet' | cut -d: -f2 | awk '{print $2}')
    echo $ifaddress1
    ifaddresscut=$(ifconfig enp0s9 | grep 'inet' | head -n 1 | awk -F " " '{print $2}' | awk -F "." '{print $1"."$2"."$3}')
    echo $ifaddresscut
elif [ "$OS_ID" == "ubuntu" ]; then
    ifaddress1=$(ifconfig enp0s9 | grep 'inet' | head -n 1 | cut -d: -f2 | awk '{print $1}')
    echo $ifaddress1
fi

cd /etc/vpp/

sudo sed -i 's!192.168.1.1!'$ifaddress1'!1' vpp_config

cd $DMM_BUILD_DIR/../release/bin
sudo cp ../../stacks/vpp/configure/*.json ./
sudo cp ../../stacks/vpp/vpp/build-root/install-vpp_debug-native/vpp/lib64/libdmm_vcl.so ../lib64/
sudo sed -i 's!192.168.1.1!'$ifaddresscut'.0!1' rd_config.json
