#!/bin/bash


script_path=$(cd "$(dirname "$0")"; pwd)

. ${script_path}/script/nstack_var.sh
. ${script_path}/script/nstack_fun.sh

config_name=${script_path}/script/nstack_var.sh
if [ ! -e $config_name ]; then
    log $LINENO "$config_name not exit, plz pay attention and add back!,or it has resourcce leak."
fi

cur_user=`whoami`

########################################################
# init_log_file:nstack.log and dpdk.log
init_log_file
log $LINENO "#######################stop nstack#######################"

#############################################
# step1 stop nstack master
retry=3
stop_nStackProcess nStackMaster $retry

#############################################
# step2 stop nstack
stop_nStackProcess nStackMain $retry


#############################################
# step3 stop all apps that usg the nstack hugepage
if [ ${cur_user} = "root" ]; then
    stop_nStackApps
else
    log $LINENO "not root, app not stopped"
fi

#############################################
# step4 delete the huge page files created by nstack
recover_hugepage

#############################################
# step5 recover the nic configuration
recover_network

#############################################
# step6 delete pid file
delete_pid_file

exit 0
