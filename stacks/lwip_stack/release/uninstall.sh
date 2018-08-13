#!/bin/bash

. ./script/nstack_var.sh
. ./script/nstack_fun.sh

########################################################
# init_log_file:nstack.log and dpdk.log
init_log_file
log $LINENO "#######################uninstall nstack#######################"


#############################################
# step1 stop the applications that use the shared huge memory of nstack
stop_nStackApps

#############################################
# step2 delete the huge page files created by nstack
recover_hugepage


#############################################
# step3 recover the nic configuration
recover_network


#############################################
# step4 delete nstack log files
delete_log_file


# log file has been deleted, should print on screen now
echo "uninstall nStack successfully!"

exit 0
