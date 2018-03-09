This repository contains a C++ implementation of the Google logging
module.  Documentation for the implementation is in doc/.

See INSTALL for (generic) installation instructions for C++: basically
    cd ./glog-0.3.3
   ./configure && make && make install

For RTOS 32 bit:
    cd ./glog-0.3.3
   ./configure CXXFLAGS=-m32 && make && make install

Or, you can use automatic script:
    sh build_for_ci.sh

and you can collect the latest lib & includes:
    sh collect.sh

you can find them in glog/lib & glog/include, moreover path then to glog.tar.gz

Add feature:
    1. Support custom log, header file reference: nstack_glog_in.h;
    2. Provide a C interface to invoke, header file reference: nstack_glog.ph;
    3. Add the Debug level Release version, actually same to the INFO format log;
    4. Provide dynamically print ,adjust the log level, use  NSTACK_LOG_LEVEL_ENABLE=[DEBUG/INFO/WARNING/ERROR/FATAL] , and first need use glogInit(char *param) and glogLevelSet(int logLevel)default is INFO level.
    5. Fix bug: twice Init & shutdown.
