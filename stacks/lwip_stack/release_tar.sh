if [ ! -d "./nStackServer" ]; then
  mkdir ./nStackServer
else
 rm -rf ./nStackServer/*
fi

if [ ! -d "./nStackClient" ]; then
  mkdir ./nStackClient
else
  rm -rf ./nStackClient/*
fi

if [ ! -d "./nStackTools" ]; then
  mkdir ./nStackTools
else
  rm -rf ./nStackTools/*
fi

if [ -f "./nStackServer.tar.gz" ]; then
  rm -rf ./nStackServer.tar.gz
fi

if [ -f "./nStackClient.tar.gz" ]; then
  rm -rf ./nStackClient.tar.gz
fi

if [ -f "./nStackTools.tar.gz" ]; then
  rm -rf ./nStackTools.tar.gz
fi

mkdir ./nStackServer/lib64
cp ./release/lib64/libnstack.so ./release/lib64/libnStackAPI.so ./release/lib64/libnstackcmd.so ./release/lib64/libsecurec.so ./nStackServer/lib64
mkdir ./nStackServer/bin
cp ./release/bin/nStackCtrl ./release/bin/nStackMain ./release/bin/nStackMaster ./release/bin/set_permission.sh ./nStackServer/bin
mkdir ./nStackServer/conf
cp ./release/conf/nstack.monitrc ./release/conf/nstack_ctl ./release/conf/srvnstack ./nStackServer/conf
mkdir ./nStackServer/configure
cp ./release/configure/ip_data.json ./release/configure/network_data_tonStack.json ./release/configure/nStackConfig.json ./nStackServer/configure
mkdir ./nStackServer/script
cp -d ./release/script/nstack_fun.sh ./release/script/nstack_var.sh ./release/script/run_nstack_main.sh ./release/script/run_nstack_master.sh ./nStackServer/script
mkdir ./nStackServer/tools
cp ./release/tools/nping ./nStackServer/tools/

cp ./release/check_status.sh ./release/send_alarm.sh ./release/start_nstack.sh ./release/stop_nstack.sh ./release/graceful_stop_nstack.sh ./release/uninstall.sh  ./release/upgrade_nstack.sh ./nStackServer/

dos2unix ./nStackServer/*.sh
find ./nStackServer -type f | grep -E "*.sh|*.py" | xargs chmod +x

mkdir ./nStackClient/lib64
cp ./release/lib64/libnstack.so ./release/lib64/libnStackAPI.so ./release/lib64/libsecurec.so ./nStackClient/lib64
mkdir ./nStackClient/include
cp ./release/include/nstack_custom_api.h ./nStackClient/include

cp ./release/tools/nping ./release/tools/ntcpdump ./nStackTools/

# set permission
chown -R paas: ./nStackServer/
chown -R paas: ./nStackClient/
chown -R paas: ./nStackTools/

chmod 750 ./nStackServer
chmod 550 ./nStackServer/bin/
chmod 750 ./nStackServer/configure
chmod 750 ./nStackServer/script
chmod 750 ./nStackServer/conf
chmod 750 ./nStackServer/lib64
chmod 750 ./nStackServer/tools
chmod 750 ./nStackClient
chmod 750 ./nStackClient/include
chmod 750 ./nStackClient/lib64
chmod 750 ./nStackTools

chmod 750 ./nStackServer/bin/nStack*
chmod 750 ./nStackServer/*.sh
chmod 750 ./nStackServer/bin/*.sh
chmod 750 ./nStackServer/conf/*
chmod 750 ./nStackServer/script/*.sh
chmod 640 ./nStackServer/lib64/*
chmod 640 ./nStackServer/configure/*
chmod 500 ./nStackServer/bin/set_permission.sh
chmod 750 ./nStackTools/*


tar -zcvf nStackServer.tar.gz nStackServer
tar -zcvf nStackClient.tar.gz nStackClient
tar -zcvf nStackTools.tar.gz nStackTools
