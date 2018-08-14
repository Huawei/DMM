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

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fPIE -pie -fPIC -m64 -mssse3 -std=gnu89")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wshadow -Wfloat-equal -Wformat=2")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector -fstack-protector-all")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,relro,-z,now -Wl,--disable-new-dtags")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,noexecstack -mcmodel=medium")

ADD_DEFINITIONS(-D_GNU_SOURCE)
ADD_DEFINITIONS(-DDPDK_MODULE=0)
if(WITH_HAL_LIB)
SET(RTP_CONFIG ${CMAKE_CURRENT_LIST_DIR}/../src/include/rtp_config.h)
else()
    SET(PAL_H_DIRECTORIES "/usr/include/dpdk/")
    SET(RTP_CONFIG ${PROJECT_SOURCE_DIR}/../../src/framework/common/base/include/common/common_sys_config.h)
    INCLUDE_DIRECTORIES(
        ${PAL_H_DIRECTORIES}
    )
endif()
SET(COMPLE_CONFIG ${CMAKE_CURRENT_LIST_DIR}/../src/include/compile_config.h)
SET(MGR_COM ${CMAKE_CURRENT_LIST_DIR}/../src/include/mgr_com.h)
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
	${CMAKE_CURRENT_LIST_DIR}/../../../SecureC/include
	${CMAKE_CURRENT_LIST_DIR}/../../../thirdparty/json/json-c-0.12.1/
	${CMAKE_CURRENT_LIST_DIR}/../../../thirdparty/glog/glog-0.3.4/src/
	${CMAKE_CURRENT_LIST_DIR}/../src/include/
	${CMAKE_CURRENT_LIST_DIR}/lwip/include/
	${CMAKE_CURRENT_LIST_DIR}/lwip/include/lwip/
)

FILE(GLOB API api/*.c)
FILE(GLOB_RECURSE CORE core/*.c)
FILE(GLOB INSTANCE instance/*.c)
FILE(GLOB IPMODULE ip_module/*.c)
FILE(GLOB NETIF netif/*.c)
FILE(GLOB RECYCLE recycle/*.c)
FILE(GLOB_RECURSE MAINTAIN maintain/*.c)
FILE(GLOB COMMON common/*.c)
FILE(GLOB_RECURSE LWIPSRC lwip/arch/*.c lwip/core/*.c lwip/netif/*.c)
FOREACH(item ${LWIPSRC})
  IF(${item} MATCHES "slipif.c")
    LIST(REMOVE_ITEM LWIPSRC ${item})
  ENDIF(${item} MATCHES "slipif.c")
ENDFOREACH(item)

ADD_LIBRARY(stacklwip STATIC ${API} ${ARCH} ${CORE} ${NETIF} ${NETTOOL} ${IPMODULE} ${COMMON} ${INSTANCE} ${RECYCLE} ${MAINTAIN} ${LWIPSRC})
ADD_DEPENDENCIES(stacklwip JSON GLOG DPDK)
TARGET_INCLUDE_DIRECTORIES(
	stacklwip
	BEFORE
	PRIVATE
	common/
	socket/
	include/
	include/util/
	include/log/
	include/ipv4/
	include/ipv4/stackx
	include/stackx/
	include/netif/
	include/nstack/
	include/ip_module/
	lwip/include/lwip/
	lwip/include/lwip/arch/
	lwip/include/lwip/apps/
	lwip/include/lwip/priv/
	lwip/include/lwip/prot/
	lwip/include/netif/
	lwip/include/netif/ppp/
	lwip/include/netif/ppp/polarssl/
	${CMAKE_CURRENT_LIST_DIR}/../src/sbr/
	${CMAKE_CURRENT_LIST_DIR}/../src/include/
	${CMAKE_CURRENT_LIST_DIR}/../src/alarm/
)

