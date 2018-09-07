#!/bin/bash

set -x
DMM_DIR=`dirname $(readlink -f $0)`/..
LIB_PATH=$DMM_DIR/release/lib64/

mkdir -p $DMM_DIR/config/app_test
cd $DMM_DIR/config/app_test

#===========check hugepages=================
source $DMM_DIR/scripts/check_hugepage.sh

echo -e "\e[41m Preapring APP test directory.....\e[0m"

OS_ID=$(grep '^ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')
if [ "$OS_ID" == "ubuntu" ]; then
	ifaddress1=$(ifconfig enp0s8 | grep 'inet addr' | cut -d: -f2 | awk '{print $1}')
	echo $ifaddress1
	ifaddress2=$(ifconfig enp0s9 | grep 'inet addr' | cut -d: -f2 | awk '{print $1}')
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

cp -r ${DMM_DIR}/release/lib64/* .
cp -r ${DMM_DIR}/release/configure/* .
cp -r ${DMM_DIR}/release/bin/* .
chmod 775 *

#disable ASLR, othewise it may have some problems when mapping memory for secondary process
echo 0 > /proc/sys/kernel/randomize_va_space

sudo mkdir -p /var/run/ip_module/
sudo mkdir -p /var/log/nStack/ip_module/

export LD_LIBRARY_PATH=$LIB_PATH
export NSTACK_LOG_ON=DBG