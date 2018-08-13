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

SET(RTP_DIRECTORIES "${PROJECT_SOURCE_DIR}/src/include/")

INCLUDE_DIRECTORIES(
	framework/log/
	framework/include/
	framework/common/include/
	framework/common/base/include/
	${RTP_DIRECTORIES}
	${RTP_DIRECTORIES}/generic
	../../SecureC/include/
	../../../thirdparty/glog/glog-0.3.4/src/
)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fPIC -m64 -mssse3 -std=gnu89")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wshadow -Wfloat-equal -Wformat=2")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector -fstack-protector-all")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,relro,-z,now -Wl,--disable-new-dtags,--rpath,../lib64")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,noexecstack -mcmodel=medium")

if(WITH_HAL_LIB)
SET(RTP_CONFIG ${PROJECT_SOURCE_DIR}/src/include/rtp_config.h)
else()
    SET(RTP_CONFIG ${CMAKE_CURRENT_LIST_DIR}/../../../src/framework/common/base/include/common/common_sys_config.h)
endif()
SET(COMPLE_CONFIG ${PROJECT_SOURCE_DIR}/src/include/compile_config.h)
ADD_DEFINITIONS(-include ${RTP_CONFIG})
ADD_DEFINITIONS(-include ${COMPLE_CONFIG})
ADD_DEFINITIONS(-D_GNU_SOURCE)
ADD_DEFINITIONS(-DDPDK_MODULE=0)
#ADD_DEFINITIONS(-DNSTACK_RSRDMA)
LINK_DIRECTORIES(${LIB_PATH_SHARED} ${LIB_PATH_STATIC})

if(WITH_SECUREC_LIB)
LINK_LIBRARIES(pthread dl securec)
else()
LINK_LIBRARIES(pthread dl)
endif()

ADD_SUBDIRECTORY(maintain)
ADD_SUBDIRECTORY(io_adpt)
ADD_SUBDIRECTORY(sbr)
#ADD_SUBDIRECTORY(nStackCtrl)
ADD_SUBDIRECTORY(nStackMain)
ADD_SUBDIRECTORY(tools)
ADD_SUBDIRECTORY(alarm)
#ADD_SUBDIRECTORY(nstackcmd)
