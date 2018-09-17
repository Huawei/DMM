#!/bin/bash

PID_FILE_DIR=/var/ICTS_BASE/run
PID_FILE=${PID_FILE_DIR}/nstack.pid

RUNNING_FILE_DIR=/var/log/nStack
RUNNING_FILE_NAME=running.log

OPERATION_FILE_NAME=operation.log
NSTACK_CTRL_LOG_FILE_NAME=omc_ctrl.log
GLOG_FAILURE_FILE_NAME=fail_dump.log
NSTACK_FAILURE_FILE_NAME=nstack_error.log

MASTER_FILE_DIR=/var/log/nStack
MASTER_FILE_NAME=master.log

LOG_FILE_DIR=/var/log/nStack
LOG_FILE=${LOG_FILE_DIR}/nstack.log
LOG_FILE_NAME=nstack.log

DPDK_FILE_DIR=/var/log/nstack-dpdk
DPDK_FILE=${DPDK_FILE_DIR}/nstack_dpdk.log
DPDK_FILE_NAME=nstack_dpdk.log

#this is env variable, it is for nstack usage, it will be modified from ./start_nstack -i hostinfo.ini

export VM_ID=agent-node-x

export DPDK_INSTALL_PATH="/tmp/dpdk/dpdk-18.02/"
export DPDK_LIB_PATH=${DPDK_INSTALL_PATH}/x86_64-native-linuxapp-gcc/lib
export DPDK_TOOL_DIR=${DPDK_INSTALL_PATH}/usertools
export DPDK_MOD_PATH=${DPDK_INSTALL_PATH}/x86_64-native-linuxapp-gcc/kmod

cur_user=`whoami`
if [ "root"  != "${cur_user}" ]
then
    HOME_DIR=$HOME
    if [ -z "${HOME}" ]
    then
        HOME_DIR=/var/run
    fi
else
    HOME_DIR=/var/run
fi
RUNTIME_DIR=$HOME_DIR/ip_module

DPDK_NIC_LIST_FILE=$RUNTIME_DIR/.nstack_dpdk_nic_list

#single file is 50M=50*1024*1024
MAX_LOG_FILE_SIZE=52428800
HUGE_PAGES=2048
HUGE_DIR=/mnt/nstackhuge
SLEEP_INTERVAL=100 # tcpip thread sleep time, unit: us
BIND_CPU=0
MEM_SIZE=2048
RTP_CORE_MASK=2

MASTER_EXEC_PATH="/product/gpaas/nStackMaster/bin"
RUN_NSTACK_FILE=run_nstack.sh


#default config definitions
DEF_SOCK_NUM=8192
