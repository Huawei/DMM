#!/bin/bash

cur_dir=$(cd "$(dirname "$0")"; pwd)
alarm_history=${cur_dir}/.nstack_fault_alarm

. ./script/nstack_var.sh
. ./script/nstack_fun.sh

if [ ! -f ${alarm_history} ]; then
    ./nStackCtrl --module alm -n nsMain -t abnormal
    log $LINENO "nstack is not running ok, send fault alarm"
    touch ${alarm_history}
else
    log $LINENO "nstack is not running ok, send fault alarm already"
fi

exit 0