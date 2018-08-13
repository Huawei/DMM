#!/bin/bash

check_file_size()
{
    if [ -n "$1" ]&&[ -f "$1" ]
    then
        local log_size=`stat -c %s $1`
        if [ ${log_size} -gt $MAX_LOG_FILE_SIZE ]
        then
            mv $1 $1.bk
        fi
    fi
}

log()
{
    check_file_size $LOG_FILE

    #1 line number
    local printStr="$2"
    cur_date=`date +'%m%d %H:%M:%S.%6N' -u`
    pid=$$
    file=$0
    local log_head="I${cur_date} ${file##*/}:$1 ${pid}]"

    printf "${log_head} $printStr\n" >> $LOG_FILE 2>/dev/null
}

get_call_stack()
{
    local proc_name="$1"
    log $LINENO "gdb info in call stack:"
    gdb -batch -ex "t a a bt" -p `pidof $proc_name`>> $LOG_FILE 2>/dev/null
    log $LINENO "top in call stack:"
    top -H -p `pidof $proc_name` -n 1 -b >> $LOG_FILE 2>/dev/null
    log $LINENO "mpstat in call stack:"
    mpstat -A >> $LOG_FILE 2>/dev/null
}

gdb_set()
{
    local mod="$1"
    if [ "$mod" = "stop" ];then
        gdb -batch -ex "set g_hbt_switch=0" -p `pidof nStackMain`
        gdb -batch -ex "set g_hbt_switch=0" -p `pidof nStackMaster`
        log $LINENO "gdb stop"
    else
        gdb -batch -ex "set g_hbt_switch=1" -p `pidof nStackMain`
        gdb -batch -ex "set g_hbt_switch=1" -p `pidof nStackMaster`
        log $LINENO "gdb start"
    fi
}

get_core_mask()
{
    local sys_core=`cat /proc/cpuinfo | grep processor | wc -l`
    log $LINENO "system has ${sys_core} cores"
    #if sys_core too large 2^$sys_core will overflow; so reset sys_core; z00203440 20170323
    if [ $sys_core -gt 8 ]
    then
        sys_core=8
        log $LINENO "sys_core too large reset to ${sys_core}"
    fi

    base=`echo | awk -v ex=$sys_core 'BEGIN{a=2^ex; print a}'`
    local temp=`expr $base - 1`
    printf %X $temp
    return $sys_core
}

init_log_file()
{
    if [ -n $LOG_FILE_DIR ]&&[ ! -f $LOG_FILE_DIR ]
    then
          sudo mkdir -p $LOG_FILE_DIR
    fi

    check_file_size $LOG_FILE

    if [ -n $DPDK_FILE_DIR ]&&[ ! -f $DPDK_FILE_DIR ]
    then
        sudo mkdir -p $DPDK_FILE_DIR
    fi

    check_file_size $DPDK_FILE

    #must create the directory under the current user account
    if [ -n $RUNTIME_DIR ]&&[ ! -f $RUNTIME_DIR ]
    then
        mkdir -p $RUNTIME_DIR
    fi
}

delete_log_file() {
    if [ -n $RUNNING_FILE_DIR ]&&[ -d $RUNNING_FILE_DIR ]&&[ -n $RUNNING_FILE_NAME ]; then
        rm -rf $RUNNING_FILE_DIR/*${RUNNING_FILE_NAME}*
    fi
    if [ -n $RUNNING_FILE_DIR ]&&[ -d $RUNNING_FILE_DIR ]&&[ -n $OPERATION_FILE_NAME ]; then
        rm -rf $RUNNING_FILE_DIR/*${OPERATION_FILE_NAME}*
    fi
    if [ -n $RUNNING_FILE_DIR ]&&[ -d $RUNNING_FILE_DIR ]&&[ -n $GLOG_FAILURE_FILE_NAME ]; then
        rm -rf $RUNNING_FILE_DIR/*${GLOG_FAILURE_FILE_NAME}*
    fi
    if [ -n $RUNNING_FILE_DIR ]&&[ -d $RUNNING_FILE_DIR ]&&[ -n $NSTACK_FAILURE_FILE_NAME ]; then
        rm -rf $RUNNING_FILE_DIR/*${NSTACK_FAILURE_FILE_NAME}*
    fi
    if [ -n $MASTER_FILE_DIR ]&&[ -d $MASTER_FILE_DIR ]&&[ -n $MASTER_FILE_NAME ]; then
        rm -rf $MASTER_FILE_DIR/*${MASTER_FILE_NAME}*
    fi
    if [ -n $MASTER_FILE_DIR ]&&[ -d $MASTER_FILE_DIR ]&&[ -n $GLOG_FAILURE_FILE_NAME ]; then
        rm -rf $MASTER_FILE_DIR/*${GLOG_FAILURE_FILE_NAME}*
    fi
    if [ -n $MASTER_FILE_DIR ]&&[ -d $MASTER_FILE_DIR ]&&[ -n $NSTACK_FAILURE_FILE_NAME ]; then
        rm -rf $MASTER_FILE_DIR/*${NSTACK_FAILURE_FILE_NAME}*
    fi
    if [ -n $LOG_FILE_DIR ]&&[ -d $LOG_FILE_DIR ]&&[ -n $LOG_FILE_NAME ]; then
        rm -rf $LOG_FILE_DIR/*${LOG_FILE_NAME}*
    fi
    if [ -n $DPDK_FILE_DIR ]&&[ -d $DPDK_FILE_DIR ]&&[ -n $DPDK_FILE_NAME ]; then
        rm -rf $DPDK_FILE_DIR/*${DPDK_FILE_NAME}*
    fi
    if [ -n $RUNTIME_DIR ]&&[ -d $RUNTIME_DIR ]; then
        rm -rf $RUNTIME_DIR
    fi
    if [ -n ${MASTER_EXEC_PATH} ]&&[ -d $MASTER_EXEC_PATH ]; then
        rm -rf $MASTER_EXEC_PATH
    fi
}

init_hugepage()
{
    log $LINENO "init hugepage..."
##############################################################
#
# TO DO:
# verify the free pages carefully, but not with 1G pagesize
#
#############################################################

    #for 1G hugepage

    # check total hugepage number
#    local nr_pagedir=/sys/devices/system/node/node0/hugepages/hugepages-1048576kB/nr_hugepages
#    if [ ! -f ${nr_pagedir} ]
#    then
#        log $LINENO "system not support 1G huge pages, exit!"
#        exit 1
#    fi

 #   local nr_huge_pages=`cat ${nr_pagedir}`
 #   if [ ${nr_huge_pages} -lt 3 ]
 #   then
 #       log $LINENO "total huge pages is not set correctly, current ${nr_huge_pages}, exit!"
 #       exit 1
 #   fi

    # check free hugepage number
  #  local fr_pagedir=/sys/devices/system/node/node0/hugepages/hugepages-1048576kB/free_hugepages
  #  if [ ! -f ${fr_pagedir} ]
  #  then
  #      log $LINENO "system not support 1G huge pages, exit!"
  #      exit 1
  #  fi

   # local fr_huge_pages=`cat ${fr_pagedir}`
   # if [ ${fr_huge_pages} -lt 3 ]
   # then
   #     log $LINENO "free huge pages is not set correctly, current ${fr_huge_pages}, exit!"
   #     lsof $HUGE_DIR >> $LOG_FILE 2>/dev/null
   #     exit 1
   # fi

   # mount | grep $HUGE_DIR >> $LOG_FILE 2>/dev/null
   # if [ $? -eq 0 ] ; then
   #     log $LINENO "$HUGE_DIR dir exist"
   #     if test $(pgrep -f $1 | wc -l) -eq 0 ; then
   #         log $LINENO "proccess $1 not exist and clean huge files"
   #         if [ -n $HUGE_DIR ]&&[ -d $HUGE_DIR ]; then
   #             rm -rf $HUGE_DIR/*
   #         fi
   #     else
   #         log $LINENO "proccess $1 exist"
   #     fi
   # else
   #     log $LINENO "$HUGE_DIR not exist and create"
   #     sudo mkdir -p $HUGE_DIR
        #directory right can't larger than 750

   #     cur_user=`whoami`
   #     if [ "root"  != "$cur_user" ]
   #     then
   #         non_root_uid=`id -u $cur_user`
   #         non_root_gid=`id -g $cur_user`
   #         sudo mount -t hugetlbfs -o pagesize=1G,uid=$non_root_uid,gid=$non_root_gid none $HUGE_DIR >> $LOG_FILE 2>/dev/null
   #     else
   #         mount -t hugetlbfs -o pagesize=1G none $HUGE_DIR >> $LOG_FILE 2>/dev/null
   #     fi
   # fi
}

recover_hugepage()
{
    log $LINENO "recover hugepage..."

    if [ -n "$HUGE_DIR" ]&&[ -d "$HUGE_DIR" ]; then
        rm -rf $HUGE_DIR/*
    fi
}

init_network()
{
    log $LINENO "init network..."

    recover_network

    sudo modprobe uio

    local script_path=$(cd "$(dirname "$0")"; pwd)
    local is_igb_uio_loaded=`lsmod | grep igb_uio | wc -l`
    if [ ${is_igb_uio_loaded} -eq 0 ]
    then
        log $LINENO "igb_uio is not installed. install it now."
        sudo insmod $DPDK_MOD_PATH/igb_uio.ko
    fi

    # check if there is un-recognized nic
    sudo $DPDK_TOOL_DIR/dpdk-devbind.py -s | grep -A32 "Other network" | \
        grep unused | awk -F"'" '{print $1, $2}' | while read NIC_BFS BIND_DRIVER_NAME
    do
        local TYPE_CHECK_ixgbe=`echo $BIND_DRIVER_NAME |grep "82599ES 10-Gigabit SFI/SFP+ Network Connection"|wc -l`
        local TYPE_CHECK_ixgbevf=`echo $BIND_DRIVER_NAME |grep "82599 Ethernet Controller Virtual Function"|wc -l`
        local TYPE_CHECK_vmxnet3=`echo $BIND_DRIVER_NAME |grep "VMXNET3 Ethernet Controller"|wc -l`
        local TYPE_CHECK_virtio=`echo $BIND_DRIVER_NAME |grep "Virtio network device"|wc -l`

        if [ $TYPE_CHECK_ixgbe -gt 0 ]; then
            sudo $DPDK_TOOL_DIR/dpdk-devbind.py --bind=ixgbe $NIC_BFS
        elif [ $TYPE_CHECK_ixgbevf -gt 0 ]; then
            sudo $DPDK_TOOL_DIR/dpdk-devbind.py --bind=ixgbevf $NIC_BFS
        elif [ $TYPE_CHECK_vmxnet3 -gt 0 ]; then
            sudo $DPDK_TOOL_DIR/dpdk-devbind.py --bind=vmxnet3 $NIC_BFS
        elif [ $TYPE_CHECK_virtio -gt 0 ]; then
            sudo $DPDK_TOOL_DIR/dpdk-devbind.py --bind=virtio-pci $NIC_BFS
        fi

    done
}

recover_network()
{
    log $LINENO "recover network..."

    if [ -z "$DPDK_NIC_LIST_FILE" ]||[ ! -f "$DPDK_NIC_LIST_FILE" ]
    then
        log $LINENO "$DPDK_NIC_LIST_FILE is not exist. There is no nic used by nstack to unbind."
    else
        log $LINENO "The NIC(s) to unbind: "
        cat $DPDK_NIC_LIST_FILE | awk -F" '|' | drv=| unused=" '{print $1,$4}' >> $LOG_FILE 2>/dev/null
        cat $DPDK_NIC_LIST_FILE | awk -F" '|' | drv=| unused=" '{print $1,$4}' | while read NIC_BFS BIND_DRIVER_TYPE
        do
            sudo $DPDK_TOOL_DIR/dpdk-devbind.py --bind=$BIND_DRIVER_TYPE $NIC_BFS >> $LOG_FILE 2>/dev/null
        done
        rm $DPDK_NIC_LIST_FILE
    fi

    touch $DPDK_NIC_LIST_FILE
    chmod 600 $DPDK_NIC_LIST_FILE
}


check_args_main()
{
if [ -z $HUGE_DIR ]||[ -z $MEM_SIZE ]||[ -z $RTP_CORE_MASK ]||[ -z $SLEEP_INTERVAL ]||\
    [ -z $BIND_CPU ]||[ -z $START_TYPE ]
then
    log $LINENO "nStackMain Args is null"
    return 1
fi

# check CORE_MASK -- see in get_core_mask()
# check HUGE_DIR
if [ -z $HUGE_DIR ]||[ ! -d $HUGE_DIR ]; then
    log $LINENO "HUGE_DIR="$HUGE_DIR" is invalid"
    return 1
fi

# check MEM_SIZE
if [ $MEM_SIZE -lt 2048 ]||[ $MEM_SIZE -gt 32768 ]; then
    log $LINENO "MEM_SIZE="$MEM_SIZE" is invalid"
    return 1
fi

# check RTP_CORE_MASK
if [ $RTP_CORE_MASK -lt 1 ]; then
    log $LINENO "RTP_CORE_MASK="$RTP_CORE_MASK" is invalid"
    return 1
fi

# check SLEEP_INTERVAL
if [ $SLEEP_INTERVAL -lt 0 ]; then
    log $LINENO "SLEEP_INTERVAL="$SLEEP_INTERVAL" is invalid"
    return 1
fi

# check BIND_CPU
if [ $BIND_CPU -lt 0 ]; then
    log $LINENO "BIND_CPU="$BIND_CPU" is invalid"
    return 1
fi

# check START_TYPE
if [ $START_TYPE -lt 0 ]||[ $START_TYPE -gt 5 ]; then
    log $LINENO "START_TYPE="$START_TYPE" is invalid"
    return 1
fi

return 0
}

run_nStackMain()
{

    log $LINENO "run nstack main..."

    sudo setcap CAP_IPC_OWNER,CAP_FOWNER,CAP_NET_ADMIN,CAP_IPC_LOCK,CAP_NET_RAW,CAP_SYS_RAWIO,CAP_SYS_ADMIN,CAP_CHOWN,CAP_SYS_NICE=eip ./bin/nStackMain 2>/dev/null

    local script_path=$(cd "$(dirname "$0")"; pwd)
    export NSTACK_CONFIG_PATH=${script_path}/../configure
    export LD_LIBRARY_PATH=${script_path}/lib64/:$LD_LIBRARY_PATH
    export NSTACK_LOG_ON=INF

    log $LINENO "$env NSTACK_CONFIG_PATH=$NSTACK_CONFIG_PATH"
    log $LINENO "$env DPDK_TOOL_DIR=$DPDK_TOOL_DIR"
    log $LINENO "$env LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
    log $LINENO "$env DPDK_LIB_PATH=$DPDK_LIB_PATH"
    log $LINENO "./bin/nStackMain -c $1 -n 4 --huge-dir=$2 -m $3 stack -c $4 -sleep $5 -bind_cpu $6"
    check_file_size $DPDK_FILE
    cd ..; cd bin/
    ./nStackMain -c $1 -n 4 --huge-dir=$2 -m $3 stack -c $4 -sleep $5 -bind_cpu $6 >> $DPDK_FILE &
}


stop_nStackProcess()
{
    log $LINENO "$1 exiting..."

    local i=0

    while [ ${i} -lt $2 ]
    do
        #pid=`ps aux | grep $1 |grep -v grep| awk '{print $2}'`
        pid=`pidof $1`
        if [ "x${pid}" != "x" ]
        then
            kill -9 $pid 2>/dev/null
        else
            break
        fi
        i=`expr ${i} + 1`
        sleep 1
    done
}

stop_nStackApps()
{
    log $LINENO "stop the apps that use the shared nstackhuge memory..."

    app_pid=`lsof $HUGE_DIR | awk '{print $2}'`
    if [ "x${app_pid}" != "x" ]
    then
        kill -9 ${app_pid} 2>/dev/null
    fi
}

get_nstack_bin()
{
    local app=$1
    master_pid=`pidof $app | head -n 1`
    if [ "x${master_pid}" != "x" ];then
       sudo readlink -f "/proc/${master_pid}/exe" | sed "s/$app$//g" 2>/dev/null
    fi
}



get_abs_path()
{
    local input_path=$1
    if [ -d "$input_path" -o -f "$input_path" ]; then
        echo "$(dirname $(readlink -e $input_path))/$(basename $input_path)"
        return 0
    fi
    echo $input_path
    return 1
}



CONFIG_FILE="nStackConfig.json"
VAR_FILE="nstack_var.sh"

modify_nstack_log_path()
{
    if echo $1 |grep -Eq '^((/[a-zA-Z_]+[a-zA-Z0-9_-]+(/[a-zA-Z0-9_-]+)*/?)|/)$' > /dev/null
    then
        local parameter=$1
    else
        log $LINENO "the path is invalid, use the default config path!"
        return
    fi

    if [ ! -d $1 ]; then
        log $LINENO "the folder is not existed, use the default config!"
        return
    fi

    if [ ! -w $1 ]; then
        log $LINENO "the folder has no permission, use the default config!"
        return
    fi

    local script_path=$(cd "$(dirname "$0")"; pwd)
    local config_name=${script_path}/configure/$CONFIG_FILE

    if [ ! -f $config_name ]; then
        log $LINENO "$CONFIG_FILE no exit! use default config in code."
        return
    fi

    sed -i 's#\("stackpool_log_path": "\).*#\1'"$1"'",#g' ${config_name}
    sed -i 's#\("master_log_path": "\).*#\1'"$1"'",#g' ${config_name}
    sed -i 's#\("nstack_log_path": "\).*#\1'"$1"'",#g' ${config_name}
    sed -i 's#\("dpdk_log_path": "\).*#\1'"$1"'"#g' ${config_name}
}

modify_log_var()
{
    local script_path=$(cd "$(dirname "$0")"; pwd)
    local config_name=${script_path}/configure/$CONFIG_FILE

    if [ ! -f $config_name ]; then
        log $LINENO "$CONFIG_FILE no exit! use default config in code."
        return
    fi

    local running_log_path=`grep -Po '"stackpool_log_path": ".*?"' $config_name | sed -n -e 's/"//gp' | awk -F' ' '{print $2}'`
    local master_log_path=`grep -Po '"master_log_path": ".*?"' $config_name | sed -n -e 's/"//gp' | awk -F' ' '{print $2}'`
    local nstack_log_path=`grep -Po '"nstack_log_path": ".*?"' $config_name | sed -n -e 's/"//gp' | awk -F' ' '{print $2}'`
    local dpdk_log_path=`grep -Po '"dpdk_log_path": ".*?"' $config_name | sed -n -e 's/"//gp' | awk -F' ' '{print $2}'`

#modify the nstack_var.sh
    local var_config_name=${script_path}/script/$VAR_FILE

    if [ ! -f $var_config_name ]; then
        log $LINENO "$VAR_FILE not exist , exit!"
        exit 1
    fi

#check the path from json if it is OK.
    if [ -n "$running_log_path" ]&&[ -w "$running_log_path" ]; then
        sed -i "s:RUNNING_FILE_DIR=.*:RUNNING_FILE_DIR=${running_log_path}:g" $var_config_name
    else
        log $LINENO "stackpool_log_path:$running_log_path in $CONFIG_FILE is invalid, use the default config!"
    fi

    if [ -n "$master_log_path" ]&&[ -w "$master_log_path" ]; then
        sed -i "s:MASTER_FILE_DIR=.*:MASTER_FILE_DIR=${master_log_path}:g" $var_config_name
    else
        log $LINENO "master_log_path:$master_log_path in $CONFIG_FILE is invalid, use the default config!"
    fi

    if [ -n "$nstack_log_path" ]&&[ -w "$nstack_log_path" ]; then
        sed -i "s:LOG_FILE_DIR=.*:LOG_FILE_DIR=${nstack_log_path}:g" $var_config_name
    else
        log $LINENO "nstack_log_path:$nstack_log_path in $CONFIG_FILE is invalid, use the default config!"
    fi

    if [ -n "$dpdk_log_path" ]&&[ -w "$dpdk_log_path"  ]; then
        sed -i "s:DPDK_FILE_DIR=.*:DPDK_FILE_DIR=${dpdk_log_path}:g" $var_config_name
    else
        log $LINENO "dpdk_log_path:$dpdk_log_path in $CONFIG_FILE is invalid, use the default config!"
    fi
}


modify_local_ip_env()
{

#modify the nstack_var.sh
    local var_config_name=${script_path}/script/$VAR_FILE

    if [ ! -f $var_config_name ]; then
        log $LINENO "$VAR_FILE not exist , exit!"
        exit 1
    fi

    sed -i "s:VM_ID=.*:VM_ID=${nstack_alarm_local_ip}:g" $var_config_name 2>/dev/null
}


delete_pid_file()
{
        if [ -n "$PID_FILE" ]&&[ -f "$PID_FILE" ]
        then
            rm -rf  ${PID_FILE}
        fi
}

save_pid_file()
{
	if [ ! -d ${PID_FILE_DIR} ];
	then
	    sudo mkdir -p ${PID_FILE_DIR}
	    chmod 750 ${PID_FILE_DIR}
	fi

	if [ -f ${PID_FILE} ];
	then
	    cur_pid=`cat ${PID_FILE}`
	    if [ ${cur_pid} != $1 ];
	    then
	        rm -rf  ${PID_FILE}
	    else
	        return
	    fi
	fi

	touch ${PID_FILE}
	echo $1 > $PID_FILE

	chown paas:paas $PID_FILE
	chmod 640 $PID_FILE
}

write_pid_to_file()
{
	retry=3
	i=0
	fail=1

	while [ ${i} -lt ${retry} ]
	do
		PID=`pidof nStackMaster`
		if [ ${PID} ]
		then
          fail=0
          break
		fi

		i=`expr ${i} + 1`

		sleep 1
	done

	if [ ${fail} -ne 0 ]
	then
		return 1
	fi

	if [ ! -d ${PID_FILE_DIR} ];
	then
	sudo mkdir -p ${PID_FILE_DIR}
	chmod 750 ${PID_FILE_DIR}
	fi

	touch ${PID_FILE}
	echo $PID > $PID_FILE
	#check log file right
	chown paas:paas $PID_FILE
	chmod 640 $PID_FILE
	return 0
}



install_config()
{
    # set nStackConfig values
    if [ -n "$DEF_SOCK_NUM" ] ;then
        sed -i "s/\"socket_num\":[ \t0-9]*,/\"socket_num\":$DEF_SOCK_NUM,/g" ./configure/nStackConfig.json
    fi
}

copy_config()
{
    # $1 for src nStackServer path
    # $2 for dst nStackServer path

    # NOTE: set src socket_num value into dst path config file, this is for copying old configuration from old version to new version path when upgrading.
    SRC_SOCK_NUM=`grep 'socket_num' $1/configure/nStackConfig.json | awk -F"," '{for (i=1;i<=NF;++i) print $i}' | awk 'gsub("\"socket_num\":","") {sub("^[ \t]*","");sub("[ \t]*$",""); print $0}'`
    sed -i "s/\"socket_num\":[ \t0-9]*,/\"socket_num\":$SRC_SOCK_NUM,/g" $2/configure/nStackConfig.json
}
