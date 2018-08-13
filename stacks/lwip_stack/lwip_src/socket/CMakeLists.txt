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

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g -fPIE -pie -fPIC -m64 -mssse3 -std=gnu89")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wshadow -Wfloat-equal -Wformat=2")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector -fstack-protector-all")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,relro,-z,now -Wl,--disable-new-dtags")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,noexecstack -mcmodel=medium")

SET(ADAPT_DIRECTORIES "${PROJECT_SOURCE_DIR}/src/adapt/")
#SET(DMM_API "${PROJECT_SOURCE_DIR}/src/nSocket/include/")

ADD_DEFINITIONS(-D_GNU_SOURCE -D_FORTIFY_SOURCE=2)
ADD_DEFINITIONS(-DDPDK_MODULE=0)
if(WITH_HAL_LIB)
SET(RTP_CONFIG ${CMAKE_CURRENT_LIST_DIR}/../../../src/include/rtp_config.h)
else()
    SET(PAL_H_DIRECTORIES "/usr/include/dpdk/")
    SET(RTP_CONFIG ${PROJECT_SOURCE_DIR}/../../src/framework/common/base/include/common/common_sys_config.h)
    INCLUDE_DIRECTORIES(
        ${PAL_H_DIRECTORIES}
        ${ADAPT_DIRECTORIES}
#        ${DMM_API}
    )
endif()
SET(COMPLE_CONFIG ${CMAKE_CURRENT_LIST_DIR}/../../src/include/compile_config.h)
#SET(MGR_COM ${PROJECT_SOURCE_DIR}/src/framework/ipc/mgr_com/mgr_com.h)
SET(MGR_COM ${CMAKE_CURRENT_LIST_DIR}/../../src/include/mgr_com.h)
ADD_DEFINITIONS(-include ${RTP_CONFIG})
ADD_DEFINITIONS(-include ${COMPLE_CONFIG})
ADD_DEFINITIONS(-include ${MGR_COM})
if(WITH_SECUREC_LIB)
LINK_LIBRARIES(pthread rt securec)
else()
LINK_LIBRARIES(pthread rt)
endif()
LINK_DIRECTORIES(${LIB_PATH_SHARED} ${LIB_PATH_STATIC})
INCLUDE_DIRECTORIES(
        ${CMAKE_CURRENT_LIST_DIR}/../../../../thirdparty/json/json-c-0.12.1/
        ${CMAKE_CURRENT_LIST_DIR}/../../../../thirdparty/glog/glog-0.3.4/src/
	${CMAKE_CURRENT_LIST_DIR}/../../src/include/
#	${PROJECT_SOURCE_DIR}/src/framework/include/
#	${PROJECT_SOURCE_DIR}/src/framework/common/include/
#        ${ADAPT_DIRECTORIES}
#        ${DMM_API}
)

FILE(GLOB COMMON ../common/*.c)
FILE(GLOB SOCKET ./*.c)
ADD_LIBRARY(socket STATIC ${COMMON} ${SOCKET})
ADD_DEPENDENCIES(socket JSON GLOG DPDK)
TARGET_INCLUDE_DIRECTORIES(
	socket
	PRIVATE
	../common/
	./
	${CMAKE_CURRENT_LIST_DIR}/../../src/sbr/
	${CMAKE_CURRENT_LIST_DIR}/../../src/include/
#	${PROJECT_SOURCE_DIR}/src/framework/include/
#	${PROJECT_SOURCE_DIR}/src/framework/common/include/
#       ${ADAPT_DIRECTORIES}
#       ${DMM_API}   
)
