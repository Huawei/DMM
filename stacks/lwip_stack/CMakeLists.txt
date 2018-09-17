#########################################################################
#
# Copyright (c) 2018 Huawei Technologies Co.,Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#########################################################################

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11)
PROJECT(nStack)
SET(CMAKE_C_COMPILER "gcc")
SET(OS_RELEASE "" CACHE STRING "User-specified OS release.")
SET(EXECUTABLE_PATH ${CMAKE_CURRENT_LIST_DIR}/release/bin)
SET(LIB_PATH_STATIC ${PROJECT_BINARY_DIR})
SET(LIB_PATH_SHARED ${CMAKE_CURRENT_LIST_DIR}/release/lib64)
SET(NSTACKTOOLS_PATH ${CMAKE_CURRENT_LIST_DIR}/release/tools)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${LIB_PATH_STATIC})
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH})
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LIB_PATH_SHARED})


MESSAGE(STATUS "Top dir is: " ${CMAKE_CURRENT_LIST_DIR})
MESSAGE(STATUS "Static library dir: " ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
MESSAGE(STATUS "Executable binary dir: " ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
MESSAGE(STATUS "Shared library dir: " ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

option(WITH_SECUREC_LIB "Option description" OFF)
option(WITH_HAL_LIB "Option description" OFF)

if(WITH_SECUREC_LIB)
  add_definitions(-DSECUREC_LIB)
endif()

if(WITH_HAL_LIB)
  add_definitions(-DHAL_LIB)
endif()

SET(post_compile "${PROJECT_BINARY_DIR}/post_compile.sh")
FILE(WRITE ${post_compile} "#!/bin/bash\n")
FILE(APPEND ${post_compile}
"

find ${CMAKE_CURRENT_LIST_DIR}/release/ -name \"*.sh\" -exec chmod +x {} \\;
find ${CMAKE_CURRENT_LIST_DIR}/release/ -name \"*.py\" -exec chmod +x {} \\;
ln -sfn ./run_nstack_main.sh ${CMAKE_CURRENT_LIST_DIR}/release/script/run_nstack.sh
echo post compile process success.
"
)

ADD_CUSTOM_TARGET(DPDK ALL COMMAND sh post_compile.sh)

execute_process(
    COMMAND cp -rf ${CMAKE_CURRENT_LIST_DIR}/../../release/include	${CMAKE_CURRENT_LIST_DIR}/src/
    COMMAND cp ${CMAKE_CURRENT_LIST_DIR}/../../release/lib64/libnStackAPI.so	${CMAKE_CURRENT_LIST_DIR}/release/lib64/
    COMMAND cp ${CMAKE_CURRENT_LIST_DIR}/../../release/lib64/libdmm_api.so	${CMAKE_CURRENT_LIST_DIR}/release/lib64/
    COMMAND echo "stacklwip prepare ok"
)
execute_process(
	COMMAND sh ${CMAKE_CURRENT_LIST_DIR}/release/lwip_helper_files/download_lwip.sh
)

ADD_DEFINITIONS(-D_GNU_SOURCE -DNSTACK_GETVER_VERSION="18.04")

SET(JSON_C_SRC ${CMAKE_CURRENT_LIST_DIR}/../../thirdparty/json/json-c-0.12.1)
INCLUDE(ExternalProject)
ExternalProject_Add(
		JSON
		SOURCE_DIR ${JSON_C_SRC}
		BUILD_IN_SOURCE 1
		CONFIGURE_COMMAND sh autogen.sh COMMAND sh ./configure --enable-static --with-pic
        BUILD_COMMAND make -j 8
		INSTALL_COMMAND cp -f .libs/libjson-c.a ${LIB_PATH_STATIC}/
)

SET(SECUREC_SRC ${CMAKE_CURRENT_LIST_DIR}/../SecureC/src)
SET(SECUREC_SRC_H ${CMAKE_CURRENT_LIST_DIR}/../SecureC/include)

if(WITH_SECUREC_LIB)
INCLUDE(ExternalProject)
ExternalProject_Add(
		SECUREC
		SOURCE_DIR ${SECUREC_SRC}
		BUILD_IN_SOURCE 1
		CONFIGURE_COMMAND ""
        BUILD_COMMAND COMMAND make -j 8
		INSTALL_COMMAND cp -f ${SECUREC_SRC}/../lib/libsecurec.so ${LIB_PATH_SHARED}/
)
endif()

SET(GLOG_SRC ${CMAKE_CURRENT_LIST_DIR}/../../thirdparty/glog/glog-0.3.4)
INCLUDE(ExternalProject)
ExternalProject_Add(
		GLOG
		SOURCE_DIR ${GLOG_SRC}
		BUILD_IN_SOURCE 1
		CONFIGURE_COMMAND autoreconf COMMAND sh configure CFLAGS=-fPIC CXXFLAGS=-fPIC
        BUILD_COMMAND COMMAND make -j 8
		INSTALL_COMMAND cp -f ${GLOG_SRC}/.libs/libglog.a ${LIB_PATH_STATIC}/
)

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(lwip_src)
ADD_SUBDIRECTORY(lwip_src/socket)
ADD_SUBDIRECTORY(tools)
#ADD_SUBDIRECTORY(thirdparty/rsrdma)
