#!/bin/sh

DIR=`S=\`readlink "$0"\`; [ -z "$S" ] && S=$0; dirname $S`
cd $DIR
. ./nstack_var.sh


if [ -n "${MASTER_EXEC_PATH}" ]; then
    mkdir -p $MASTER_EXEC_PATH
    chown -R paas: $MASTER_EXEC_PATH
    chmod 750 $MASTER_EXEC_PATH
    cp ../bin/nStackMaster $MASTER_EXEC_PATH -rf
    cp ../configure/nStackConfig.json $MASTER_EXEC_PATH -rf
    ln -s -f $(cd "$(dirname "$0")"; pwd)/run_nstack_main.sh $MASTER_EXEC_PATH/${RUN_NSTACK_FILE}
    cd $MASTER_EXEC_PATH
fi

runnStackMaster(){
    sudo setcap CAP_IPC_OWNER,CAP_NET_ADMIN,CAP_DAC_OVERRIDE=eip ./nStackMaster

    script_path=$(cd "$(dirname "$0")"; pwd)
    export NSTACK_CONFIG_PATH=${script_path}

    ./nStackMaster -c $1 -n 4 --huge-dir=$2 -m $3 --proc-type=$4 >> ${DPDK_FILE}
}

runnStackMaster $1 $2 $3 $4 &

exit 0
