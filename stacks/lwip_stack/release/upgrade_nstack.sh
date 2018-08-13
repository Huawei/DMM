#!/bin/bash

. ./script/nstack_var.sh
. ./script/nstack_fun.sh

VERSION_CHK=0
UPG_MASTER=0

# OLD_VERSION indicates the nStackServer version, in order to support Version upgrade and rollback feature
# OLD_VERSION=1 B031~newest
# OLD_VERSION=0 B020~B030
OLD_VERSION=1
cur_user=`whoami`

upg_pre_chk()
{
    ##########check input info###########################
    if [ -z "${DST_VER_PATH}" ]; then
        log $LINENO "dst ${DST_VER_PATH} path error!"
        return 1
    fi

    if [ "x${UPG_RBK}" != "x1" -a "x${UPG_RBK}" != "x2" ]; then
        log $LINENO "type error ${UPG_RBK}!"
        return 1
    fi

    ##########check src version info######################
    SRC_STOP_FILE=${SRC_VER_PATH}/stop_nstack.sh
    if [ ! -f "$SRC_STOP_FILE" ]; then
        log $LINENO "${SRC_STOP_FILE} path error!"
        return 1
    fi

    MASTER_PATH=`get_nstack_bin nStackMaster`
    if [ -z "${MASTER_PATH}" ]; then
        log $LINENO "master not start"
        return 1
    fi

    if [ ! -d "${MASTER_PATH}${MASTERBIN}" ]; then
        log $LINENO "${MASTER_PATH} path error!"
        return 1
    fi

    SRC_VERSION=`get_version 1`
    if [ -z "${SRC_VERSION}" ]; then
        log $LINENO "get srcVersion error"
        exit 1
    fi

    ###########check dst version info######################
    DST_RUN_FILE=${DST_VER_PATH}/script/run_nstack_main.sh
    if [ ! -f "$DST_RUN_FILE" ]; then
        log $LINENO "${DST_RUN_FILE} path error. Try the path of older nstack version then."
        DST_RUN_FILE=${DST_VER_PATH}/bin/run_nstack_main.sh
        OLD_VERSION=0
        if [ ! -f "$DST_RUN_FILE" ]; then
            log $LINENO "${DST_RUN_FILE} older path error!"
            return 1
        fi
    fi

    DST_STOP_FILE=${DST_VER_PATH}/stop_nstack.sh
    if [ ! -f "$DST_STOP_FILE" ]; then
        log $LINENO "${DST_STOP_FILE} path error!"
        return 1
    fi

    DST_STAT_FILE=${DST_VER_PATH}/start_nstack.sh
    if [ ! -f "$DST_STAT_FILE" ]; then
        log $LINENO "${DST_STAT_FILE} path error!"
        return 1
    fi

    if [ ! -x "${DST_VER_PATH}/bin/nStackCtrl" ]; then
        log $LINENO "${DST_VER_PATH} path nStackCtrl exec error!"
        return 1
    fi

    DST_VERSION=`get_version 4 ${DST_VER_PATH}/bin/nStackCtrl`
    if [ -z "${DST_VERSION}" ]; then
        log $LINENO "get dstversion error"
        return 1
    fi
}

restart_upgrade()
{
    local monipid=`pidof monit`
    local stop_moni=0
    if [ ! -z "${monipid}" ]; then
        for i in `seq 60`
        do
            if [ ${cur_user} = "paas" ]; then
                stop_fin=`/var/ICTS_BASE/Monit/bin/monit summary | grep nstack | grep "Not monitored"`
            else
                stop_fin=`su paas -s /bin/bash -c "/var/ICTS_BASE/Monit/bin/monit summary" | grep nstack | grep "Not monitored"`
            fi
            if [ -n "${stop_fin}" ]; then
                break;
            fi

            if [ $stop_moni -eq 0 ]; then
                if [ ${cur_user} = "paas" ]; then
                    /var/ICTS_BASE/Monit/bin/monit unmonitor nstack
                else
                    su paas -s /bin/bash -c "/var/ICTS_BASE/Monit/bin/monit unmonitor nstack"
                fi
                stop_moni=1
                log $LINENO "stop monit"
            fi

            sleep 1
        done
        cd ${SRC_VER_PATH}
        ./stop_nstack.sh
        cd -
        cd ${DST_VER_PATH}
        if [ -n "${LOG_PATH}" ]; then
            ./start_nstack.sh -l $LOG_PATH -i $hostinfo_path
        else
            ./start_nstack.sh -i $hostinfo_path
        fi
        cd -

        log $LINENO "with monit restart"
        if [ $stop_moni -eq 1 ]; then
            if [ ${cur_user} = "paas" ]; then
                /var/ICTS_BASE/Monit/bin/monit monitor nstack
            else
                su paas -s /bin/bash -c "/var/ICTS_BASE/Monit/bin/monit monitor nstack"
            fi
            log $LINENO "monit restart"
        fi
    else
        cd ${SRC_VER_PATH}
        ./stop_nstack.sh
        cd -
        cd ${DST_VER_PATH}
        if [ -n "${LOG_PATH}" ]; then
            ./start_nstack.sh -l $LOG_PATH -i $hostinfo_path
        else
            ./start_nstack.sh -i $hostinfo_path
        fi
        cd -
        log $LINENO "none monit restart"
    fi
    return 0
}

run_upgrade()
{

    if [ -n "${DST_RUN_FILE}" ]; then
        if [ $OLD_VERSION -ge 1 ]; then
            ln -s -f "$DST_RUN_FILE" "${DST_VER_PATH}/script/${RUN_NSTACK_FILE}"
        else
            ln -s -f "$DST_RUN_FILE" "${DST_VER_PATH}/bin/${RUN_NSTACK_FILE}"
        fi
        if [ $? -ne 0 ]; then
            log $LINENO "dst version link failed!"
            return 1
        fi
    fi

    # copy old config to avoid config change issue
    copy_config $SRC_VER_PATH $DST_VER_PATH

    ./bin/nStackCtrl --module vermgr -t $UPG_RBK -s $SRC_VERSION -d $DST_VERSION
    ret=$?
    if [ $ret -eq 121 ]; then
        restart_upgrade
        WAIT_TIME=50
        RESTART_MASTER=1
        return 0
    fi

    if [ $ret -ne 0 ]; then
        log $LINENO "nStackCtrl send vermsg err $ret code"
        return 1
    fi

    nStackMain_pid=`pidof nStackMain`
    if [ -n "${DST_RUN_FILE}" ]; then
        ln -s -f "$DST_RUN_FILE" "${MASTER_PATH}${RUN_NSTACK_FILE}"
        if [ $? -ne 0 ]; then
            log $LINENO "link failed!"
            kill -9 $nStackMain_pid
            return 1
        fi
    fi
    kill -9 $nStackMain_pid
    return 0
}

after_upg_chk()
{
    for i in `seq $WAIT_TIME`
    do
        NEW_DST_VERSION=`get_version 1`
        if [ -n "${NEW_DST_VERSION}" ]; then
            if [ "$NEW_DST_VERSION" != "$DST_VERSION" ]; then
                log $LINENO "new version:$NEW_DST_VERSION maybe error! expect version is:$DST_VERSION"
            else
                break;
            fi
        fi
        sleep 1
    done

    if [ -z "${NEW_DST_VERSION}" ]; then
        log $LINENO "nStackCtrl get dstVersion error"
        return 1
    fi

    if [ "$NEW_DST_VERSION" != "$DST_VERSION" -a $VERSION_CHK -eq 1 ]; then
        log $LINENO "new version:$NEW_DST_VERSION maybe error! expect version is:$DST_VERSION"
        return 1
    fi

    DST_MAIN_PATH=`get_nstack_bin nStackMain`
    if [ -z "${DST_MAIN_PATH}" ]; then
        log $LINENO "main not start"
        return 1
    fi

    if [ "$DST_MAIN_PATH" != "${DST_VER_PATH}/bin/" ]; then
        log $LINENO "main start failed run;$DST_MAIN_PATH expect:${DST_VER_PATH}/script !"
        return 1
    fi
}


run_upgrade_master()
{
    core_mask=`get_core_mask`
    START_TYPE="secondary"
    ./bin/nStackCtrl --module vermgr -t $UPG_RBK -s $SRC_VERSION -d $DST_VERSION -p 2
    ret=$?
    if [ $ret -ne 0 ]; then
        log $LINENO "nStackMaster nStackCtrl send vermsg err $ret code"
        return 1
    fi

    nStackMaster_pid=`pidof nStackMaster`
    kill -9 $nStackMaster_pid
    cd ${DST_VER_PATH}/script/
    ./run_nstack_master.sh ${core_mask} $HUGE_DIR $MEM_SIZE $START_TYPE
    log $LINENO "./script/run_nstack_master.sh ${core_mask} $HUGE_DIR $MEM_SIZE $START_TYPE"
    cd -
    return 0
}


after_upg_master_chk()
{
    for i in `seq 10`
    do
        NEW_MAS_VERSION=`get_version 2`
        if [ -n "${NEW_MAS_VERSION}" ]; then
            if [ "$NEW_MAS_VERSION" != "$DST_VERSION" ]; then
                log $LINENO "new version:$NEW_MAS_VERSION maybe error! expect version is:$DST_VERSION"
            else
                break;
            fi
        fi
        sleep 1
    done

    if [ -z "${NEW_MAS_VERSION}" ]; then
        log $LINENO "nStackCtrl get dstVersion error"
        return 1
    fi

    if [ "$NEW_MAS_VERSION" != "$DST_VERSION" -a $VERSION_CHK -eq 1 ]; then
        log $LINENO "new version:$NEW_MAS_VERSION maybe error! expect version is:$DST_VERSION"
        return 1
    fi

    DST_MAS_PATH=`get_nstack_bin nStackMaster`
    if [ -z "${DST_MAS_PATH}" ]; then
        log $LINENO "master not start"
        return 1
    fi

    if [ "$DST_MAS_PATH" != "${DST_VER_PATH}/script/" ]; then
        log $LINENO "main start failed run;$DST_MAS_PATH expect:${DST_VER_PATH}/script !"
        return 1
    fi
    return 0
}

opr_suc()
{
    log $LINENO "operation successful $SRC_VERSION to $NEW_DST_VERSION"
}

mod_log_path()
{
    local dst_proc=$1
    local dst_log_path=$2
    ./bin/nStackCtrl --module setlog -n maspath -v ${dst_log_path} -p $dst_proc
    ret=$?
    if [ $ret -ne 0 ]; then
        log $LINENO "mod_log_path error err $ret code"
        return 1
    fi
    return 0
}

ini_file_path="invalid_path_upgrade"

while getopts 's:d:t:l:i:' opt
do
    case $opt in
        s)
            SRC_VER_PATH=`get_abs_path $OPTARG`
            ;;
        :)
            echo "-$OPTARG needs an argument"
            exit 1
            ;;
        d)
            DST_VER_PATH=`get_abs_path $OPTARG`
            ;;
        :)
            echo "-$OPTARG needs an argument"
            exit 1
            ;;
        t)
            UPG_RBK=$OPTARG
            ;;
        :)
            echo "-$OPTARG needs an argument"
            exit 1
            ;;
        l)
            LOG_PATH=`get_abs_path $OPTARG`
            ;;
        :)
            echo "-$OPTARG needs an argument"
            exit 1
            ;;
        i)
            ini_file_path=`get_abs_path $OPTARG`
            ;;
        :)
            echo "-$OPTARG needs an argument"
            exit 1
            ;;
        *)
            echo "command not recognized"
            usage
            exit 1
            ;;
    esac
done

if  [ -z $ini_file_path ]; then
    hostinfo_path="invalid_path_upgrade"
 else
    hostinfo_path=$ini_file_path
fi

(
flock -e -n 201
if [ $? -eq 1 ]
then
    log $LINENO "another upgrade process is running now, exit"
    exit 1
fi

if [ -n "${LOG_PATH}" ]; then
    modify_nstack_log_path ${LOG_PATH}
fi
modify_log_var
) 201>>./uplockfile

if [ -f "uplockfile" ]; then
    rm uplockfile
fi

. ./script/nstack_var.sh

RESTART_MASTER=0
WAIT_TIME=60

log $LINENO "######################upgrade nstack######################"
log $LINENO "src ver path ${SRC_VER_PATH},dst ver path ${DST_VER_PATH}, type ${UPG_RBK}, log path ${LOG_PATH}"

upg_pre_chk
ret=$?
if [ $ret -ne 0 ]; then
    log $LINENO "pre_chk error err $ret code"
    exit 1
fi

if [ "${DST_VERSION}" = "${SRC_VERSION}" -a $VERSION_CHK -eq 1 ]; then
    log $LINENO "version equal ${DST_VERSION} ${SRC_VERSION}"
    exit 0
fi
log $LINENO "prepare ok"

log $LINENO "src ver $SRC_VERSION with master path $MASTER_PATH,dst ver $DST_VERSION,dst run file $DST_RUN_FILE, type ${UPG_RBK}"
run_upgrade
ret=$?
if [ $ret -ne 0 ]; then
    log $LINENO "run_proc error err $ret code"
    exit 1
fi

log $LINENO "wait new version start"
sleep 2
after_upg_chk
ret=$?
if [ $ret -ne 0 ]; then
    log $LINENO "after_chk error err $ret code"
    exit 1
fi

if [ $RESTART_MASTER -ne 0 ]; then
    log $LINENO "master has restart!"
    opr_suc
    exit 0
fi

if [ $UPG_MASTER -eq 0 ]; then
    if [ -n "${LOG_PATH}" ]; then
        mod_log_path 2 ${LOG_PATH}
    fi
    opr_suc
    exit 0
fi

run_upgrade_master
ret=$?
if [ $ret -ne 0 ]; then
    log $LINENO "run_proc master error err $ret code"
    exit 1
fi

log $LINENO "wait new version master start"
sleep 2
after_upg_master_chk
ret=$?
if [ $ret -ne 0 ]; then
    log $LINENO "after_chk master error err $ret code"
    exit 1
fi

opr_suc
