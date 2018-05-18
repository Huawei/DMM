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
#!/bin/bash -x

log_file="/dmm/scripts/build_log.txt-`date +'%Y-%m-%d_%H-%M-%S'`"
exec 1> >(tee -a "$log_file")  2>&1

# Get Command Line arguements if present
DMM_DIR=$1
if [ "x$1" != "x" ]; then
    DMM_DIR=$1
else
    DMM_DIR=`dirname $(readlink -f $0)`/../
fi

echo 0:$0
echo 1:$1
echo 2:$2
echo DMM_DIR: $DMM_DIR

BUILD_DIR=${DMM_DIR}/build
LIB_PATH=${DMM_DIR}/release/lib64

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
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -yq git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump libpcre3 libpcre3-dev zlibc zlib1g zlib1g-dev vim
elif [ "$OS_ID" == "debian" ]; then
    export DEBIAN_FRONTEND=noninteractive
    export DEBCONF_NONINTERACTIVE_SEEN=true

    APT_OPTS="--assume-yes --no-install-suggests --no-install-recommends -o Dpkg::Options::=\"--force-confdef\" -o Dpkg::Options::=\"--force-confold\""
    sudo apt-get update ${APT_OPTS}
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -yq git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump libpcre3 libpcre3-dev zlibc zlib1g zlib1g-dev vim
elif [ "$OS_ID" == "centos" ]; then
    sudo yum install -y git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump vim sudo yum-utils pcre-devel zlib-devel
elif [ "$OS_ID" == "opensuse" ]; then
    sudo yum install -y git cmake gcc g++ automake libtool wget lsof lshw pciutils net-tools tcpdump vim sudo yum-utils pcre-devel zlib-devel
fi

#set and check env
sudo sysctl -w vm.nr_hugepages=1024
HUGEPAGES=`sysctl -n  vm.nr_hugepages`
if [ $HUGEPAGES != 1024 ]; then
    echo "ERROR: Unable to get 1024 hugepages, only got $HUGEPAGES.  Cannot finish."
    exit
fi

#DPDK will be having dependancy on linux headers
if [ "$OS_ID" == "ubuntu" ]; then
    sudo apt-get -y install git build-essential linux-headers-`uname -r`
elif [ "$OS_ID" == "debian" ]; then
    sudo apt-get -y install git build-essential linux-headers-`uname -r`
elif [ "$OS_ID" == "centos" ]; then
    sudo yum groupinstall -y "Development Tools"
    sudo yum install -y kernel-headers
elif [ "$OS_ID" == "opensuse" ]; then
    sudo yum groupinstall -y "Development Tools"
    sudo yum install -y kernel-headers
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
sudo mount -t hugetlbfs -o pagesize=2M none /mnt/nstackhuge/
sudo mkdir -p /var/run/ip_module/

#===========build DPDK================

if [ "$OS_ID" == "centos" ]; then
    bash -x $DMM_DIR/scripts/build_dpdk.sh
else

    if [ ! -d  $DPDK_DOWNLOAD_PATH ]; then
	    mkdir -p $DPDK_DOWNLOAD_PATH

	    cd $DPDK_DOWNLOAD_PATH
	    rm -rf dpdk-16.04/
	    wget https://fast.dpdk.org/rel/dpdk-16.04.tar.xz --no-check-certificate
	    tar xvf dpdk-16.04.tar.xz
	    cd dpdk-16.04/

	    sed -i 's!CONFIG_RTE_EXEC_ENV=.*!CONFIG_RTE_EXEC_ENV=y!1' config/common_base
	    sed -i 's!CONFIG_RTE_BUILD_SHARED_LIB=.*!CONFIG_RTE_BUILD_SHARED_LIB=y!1' config/common_base
	    sed -i 's!CONFIG_RTE_LIBRTE_EAL=.*!CONFIG_RTE_LIBRTE_EAL=y!1' config/common_base

	    make install  T=x86_64-native-linuxapp-gcc DESTDIR=${DPDK_INSTALL_PATH} -j 4
	    cd x86_64-native-linuxapp-gcc
	    make

    fi
fi

export LD_LIBRARY_PATH=$LIB_PATH
export NSTACK_LOG_ON=DBG
#===========build DMM=================
echo "DMM build started....."

cd $DMM_DIR/thirdparty/glog/glog-0.3.4/ && sudo autoreconf -ivf
cd $BUILD_DIR
rm -rf *
cmake ..
make -j 8

############### Preapre APP test directory
echo -e "\e[41m Preapring APP test directory.....\e[0m"

mkdir -p $DMM_DIR/config/app_test
cd $DMM_DIR/config/app_test

if [ "$OS_ID" == "ubuntu" ]; then
	ifaddress1=$(ifconfig eth1 | grep 'inet addr' | cut -d: -f2 | awk '{print $1}')
	echo $ifaddress1
	ifaddress2=$(ifconfig eth2 | grep 'inet addr' | cut -d: -f2 | awk '{print $1}')
	echo $ifaddress2
elif [ "$OS_ID" == "centos" ]; then
	ifaddress1=$(ifconfig enp0s8 | grep 'inet' | cut -d: -f2 | awk '{print $2}')
	echo $ifaddress1
	ifaddress2=$(ifconfig enp0s9 | grep 'inet' | cut -d: -f2 | awk '{print $2}')
	echo $ifaddress2
fi

echo '{
        "default_stack_name": "kernel",
        "module_list": [
        {
                "stack_name": "kernel",
                "function_name": "kernel_stack_register",
                "libname": "./",
                "loadtype": "static",
                "deploytype": "1",
                "maxfd": "1024",
                "minfd": "0",
                "priorty": "1",
                "stackid": "0",
    },
  ]
}' | tee module_config.json

echo '{
        "ip_route": [
        {
                "subnet": "'$ifaddress1'/24",
                "type": "nstack-kernel",
        },
        {
                "subnet": "'$ifaddress2'/24",
                "type": "nstack-kernel",
        },
        ],
        "prot_route": [
        {
                "proto_type": "1",
                "type": "nstack-kernel",
        },
        {
                "proto_type": "2",
                "type": "nstack-kernel",
        }
        ],
}' | tee rd_config.json

echo "DMM build finished....."
