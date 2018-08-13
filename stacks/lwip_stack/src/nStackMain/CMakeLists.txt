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

FILE(GLOB_RECURSE MAIN *.c)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE -pie")

ADD_DEFINITIONS(-rdynamic -D__NSTACK_MAIN__)
ADD_EXECUTABLE(nStackMain ${MAIN})
TARGET_LINK_LIBRARIES(
    nStackMain
    -Wl,--whole-archive
    ${LIB_PATH_STATIC}/libjson-c.a
    ${LIB_PATH_STATIC}/libglog.a
    dmm_api
    nStackMaintain
    stacklwip
    nStackHal
    nStackAlarm
    nTcpdump
    -Wl,--no-whole-archive,-lstdc++ -ldl
    -Wl,--no-as-needed
	rte_eal
    rte_ethdev
    rte_mempool
    rte_ring
    rte_mbuf
    rte_pmd_ixgbe
    rte_pmd_virtio
    rte_pmd_e1000
    rte_pmd_vmxnet3_uio
    rte_pmd_bond
    rte_kvargs
    rte_cmdline
)

if(WITH_SECUREC_LIB)
ADD_DEPENDENCIES(nStackMain nStackHal nStackMaintain nStackAlarm stacklwip SECUREC)
else()
ADD_DEPENDENCIES(nStackMain nStackHal nStackMaintain nStackAlarm stacklwip)
endif()

INCLUDE_DIRECTORIES(
	${CMAKE_CURRENT_LIST_DIR}/../alarm/
	${CMAKE_CURRENT_LIST_DIR}/../maintain/
	${CMAKE_CURRENT_LIST_DIR}/../include/
)
