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

if(WITH_HAL_LIB)
else()
endif()
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${NSTACKTOOLS_PATH})
LINK_DIRECTORIES(${LIB_PATH_SHARED} ${LIB_PATH_STATIC})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/platform/SecureC/include/)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fPIC -fPIE -pie -m64 -mssse3 -std=gnu89")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wshadow -Wfloat-equal -Wformat=2")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector -fstack-protector-all")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,relro,-z,now -Wl,--disable-new-dtags,--rpath,.")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,noexecstack -mcmodel=medium")

INCLUDE_DIRECTORIES(
	../src/include/
	../../../thirdparty/glog/glog-0.3.4/src/
	../../SecureC/include/
)

#ADD_EXECUTABLE(narp narp.c)
#ADD_EXECUTABLE(nnetstat nnetstat.c)

ADD_EXECUTABLE(ntcpdump ntcpdump.c)
if(WITH_SECUREC_LIB)
TARGET_LINK_LIBRARIES(
    ntcpdump
    pthread
    rt
    securec
    -Wl,--whole-archive
    libjson-c.a
    libglog.a
    dmm_api
    -Wl,--no-whole-archive,-lstdc++ -ldl
    rte_eal
    rte_mempool
    rte_mbuf
    rte_ring
)
else()
TARGET_LINK_LIBRARIES(
        ntcpdump
        pthread
        rt
        -Wl,--whole-archive
        libjson-c.a
        libglog.a
        dmm_api
        -Wl,--no-whole-archive,-lstdc++ -ldl
        rte_eal
        rte_mempool
        rte_mbuf
        rte_ring
)
endif()

if(WITH_SECUREC_LIB)
ADD_DEPENDENCIES(ntcpdump JSON GLOG SECUREC DPDK)
else()
ADD_DEPENDENCIES(ntcpdump JSON GLOG DPDK)
endif()


ADD_EXECUTABLE(nping nping.c)

if(WITH_SECUREC_LIB)
ADD_DEPENDENCIES(nping SECUREC DPDK)
else()
ADD_DEPENDENCIES(nping DPDK)
endif()

if(WITH_SECUREC_LIB)
TARGET_LINK_LIBRARIES(nping libnStackAPI.so securec -Wl,-rpath=. -lpthread -lrt -ldl)
else()
TARGET_LINK_LIBRARIES(nping libnStackAPI.so -Wl,-rpath=. -lpthread -lrt -ldl)
endif()
