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
    SET(PAL_H_DIRECTORIES "${DMM_DPDK_INSTALL_DIR}/include/dpdk/")
    SET(DPDK_LIB_DIRECTORIES "${DMM_DPDK_INSTALL_DIR}/lib64")
    SET(DPDK_LIBS "${DPDK_LIB_DIRECTORIES}/librte_eal.so")
    list(APPEND DPDK_LIBS "${DPDK_LIB_DIRECTORIES}/librte_mempool.so")
    list(APPEND DPDK_LIBS "${DPDK_LIB_DIRECTORIES}/librte_mbuf.so")
    list(APPEND DPDK_LIBS "${DPDK_LIB_DIRECTORIES}/librte_ring.so")
endif()

INCLUDE_DIRECTORIES(
	include/
	kernel/
	nstack/
	nstack/event/
	nstack/event/epoll
	nstack/event/select
	${SECUREC_SRC_H}
        ${PAL_H_DIRECTORIES}
	${CMAKE_CURRENT_LIST_DIR}/../adapt
	${CMAKE_CURRENT_LIST_DIR}/../../thirdparty/json/json-c-0.12.1
)

FILE(GLOB_RECURSE nSocket *.c)
ADD_LIBRARY(nStackAPI SHARED ${nSocket})
TARGET_LINK_LIBRARIES(
	nStackAPI
	-Wl,--whole-archive
	${LIB_PATH_STATIC}/libjson-c.a
	${LIB_PATH_STATIC}/libglog.a
	dmm_api
	-Wl,--no-whole-archive,-lstdc++
        ${DPDK_LIBS})
ADD_DEPENDENCIES(nStackAPI dmm_api)