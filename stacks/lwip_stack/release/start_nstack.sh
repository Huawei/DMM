#!/bin/bash


script_path=$(cd "$(dirname "$0")"; pwd)

. ${script_path}/script/nstack_var.sh
. ${script_path}/script/nstack_fun.sh

config_name=${script_path}/script/nstack_var.sh
if [ ! -e $config_name ]; then
    log $LINENO "nstack_var.sh not exit, plz check!"
    exit 1
fi

########################################################
##get the log info from the parameter of ./start -l XXX -a XXX ###
nstack_log_path=""
hostinfo_path=""
while getopts "l:i:a:" arg
do
   case $arg in
   l)
      nstack_log_path="$OPTARG"
      ;;
   i)
      hostinfo_path="$OPTARG"
      ;;
   esac
done

hostinfo_stat=0

(
flock -e -n 200
if [ $? -eq 1 ]
then
    log $LINENO "another process is running now, exit"
    exit 1
fi

########################################################
# modify the nstack & dpdk log path config: nStackConfig.json
if [ -n "$nstack_log_path" ]; then
    modify_nstack_log_path $nstack_log_path
fi

if [ -n "$hostinfo_path" -a -e  "$hostinfo_path" -a -r "$hostinfo_path" ]; then
    nstack_alarm_local_ip=($(awk -F '=' '/\['AGENT'\]/{a=1}a==1&&$1"="~/^(\s*)(VM_ID)(\s*)(=)/{print $2 ;exit}' $hostinfo_path))
	modify_local_ip_env
else
    hostinfo_stat=1
fi


########################################################
#set the log path in nstack_var.sh#####
modify_log_var
) 200>>./lockfile

if [ -f "lockfile" ]; then
    rm lockfile
fi

. ${script_path}/script/nstack_var.sh


########################################################
# init_log_file:nstack.log and dpdk.log
# if need print log, the messgae need add after init_log_file
init_log_file

if [ "$hostinfo_stat"  -ne 0 ]; then
    log $LINENO "please use correct -i parameter for start_nstack.sh"
    log $LINENO "host info path:$hostinfo_path"
    hostinfo_stat=0
fi

log $LINENO "######################start nstack######################"

########################################################
# check application running
process_nstack_main=nStackMain

pid_nstack=`pidof $process_nstack_main`

nstack_ctrl_path=${script_path}/bin

pgrep nStackMain
main_run_status=$?
if [ ${main_run_status} -eq 0 ]; then
        log $LINENO "nStackMain is running ok, please stop it first!"
        save_pid_file ${pid_master}
        exit 0
fi

huge_files=`ls /mnt/nstackhuge`
if [ "x${huge_files}" != "x" ]
then
	if [ "x${pid_nstack}" = "x" ]
	then
		log $LINENO "huge page file exist and nStackMain not exist"
		exit 1
	fi
fi


########################################################
# set hugepage
init_hugepage $process_nstack_main


########################################################
# install config
install_config

########################################################
core_mask=1
START_TYPE="primary"
log $LINENO "./script/run_nstack_main.sh ${core_mask} $HUGE_DIR $MEM_SIZE $START_TYPE"
${script_path}/script/run_nstack_main.sh $HUGE_DIR $MEM_SIZE

print_pid=$(ps -ux | grep nStackMain | awk '{print $2}' | awk 'NR == 2')
echo "nStackMain PID:$print_pid"
log $LINENO "nstack start success"
exit 0
