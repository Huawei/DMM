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
    SET(PAL_H_DIRECTORIES "/usr/include/dpdk/")
    FILE(GLOB_RECURSE LIBCOMM lib_common_mem/*.c)
endif()

FILE(GLOB_RECURSE COMMON common/*.c)
FILE(GLOB INIT init/*.c)
FILE(GLOB_RECURSE IPC ipc/*.c)
FILE(GLOB LOG log/*.c)
FILE(GLOB SNAPSHOT snapshot/*.c)
FILE(GLOB STARTUP startup/*.c)
FILE(GLOB MAINTAIN maintain/*.c)
FILE(GLOB HAL hal/*.c)
FILE(GLOB DMM_ADPT ../adapt/*.c)
FILE(GLOB MEM mem/*.c)

ADD_LIBRARY(dmm_api SHARED
  ${COMMON} ${INIT} ${IPC} ${LOG} ${SNAPSHOT}
  ${STARTUP} ${MAINTAIN} ${TRACEING} ${HAL}
  ${DMM_ADPT} ${LIBCOMM} ${MEM})

if(WITH_SECUREC_LIB)
ADD_DEPENDENCIES(dmm_api SECUREC JSON GLOG)
else()
ADD_DEPENDENCIES(dmm_api JSON GLOG)
endif()
INCLUDE_DIRECTORIES(
	dmm_api
	PRIVATE
	${JSON_C_SRC}
	${GLOG_SRC}
	${SECUREC_SRC_H}
	ipc/mgr_com/
	hal/
	include/
	common/base/include/generic
	common/base/include/
	${PAL_H_DIRECTORIES}
	common/base/liblinuxapi/
	common/mem_mgr/include
	common/mem_mgr/nsfw_shmem/
	common/mem_mgr/nsfw_nshmem/
	${CMAKE_CURRENT_LIST_DIR}/../nSocket/nstack/event/
	${CMAKE_CURRENT_LIST_DIR}/../nSocket/nstack/event/epoll
	${CMAKE_CURRENT_LIST_DIR}/../nSocket/include
)
