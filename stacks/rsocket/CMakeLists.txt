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


SET(rdmacm_dir librdmacm-1.1.0)
SET(dmm_inc_dir ${DMM_REL_INC_DIR})
SET(RSOCKET_DEBUG 0)

########################

SET(rdmacm_url https://github.com/ofiwg/librdmacm/archive/v1.1.0.tar.gz)

INCLUDE(ExternalProject)
ExternalProject_Add(
    rdmacm
    URL                 ${rdmacm_url}
    SOURCE_DIR          ${CMAKE_CURRENT_LIST_DIR}/${rdmacm_dir}
    DOWNLOAD_DIR        ${CMAKE_CURRENT_LIST_DIR}
    PATCH_COMMAND       patch -p1 -i ../rsocket.patch
    CONFIGURE_COMMAND   ./autogen.sh && ./configure dmm_inc_dir=${DMM_REL_INC_DIR} RSOCKET_DEBUG=${RSOCKET_DEBUG}
    BUILD_IN_SOURCE     1
    BUILD_COMMAND       make
    INSTALL_COMMAND     cp -f libdmm_rdmacm.a ${LIB_PATH_STATIC}/
    DEPENDS             DPDK
)
set_target_properties(rdmacm PROPERTIES EXCLUDE_FROM_ALL TRUE)

########################

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g -fPIC -m64 -pthread")

ADD_LIBRARY(dmm_rsocket  SHARED src/rsocket_adpt.c)

ADD_DEFINITIONS(-D_GNU_SOURCE -DRSOCKET_DEBUG=${RSOCKET_DEBUG})

INCLUDE_DIRECTORIES(${DMM_REL_INC_DIR})
INCLUDE_DIRECTORIES(./src ${rdmacm_dir} ${rdmacm_dir}/include ${rdmacm_dir}/src)

TARGET_LINK_LIBRARIES(dmm_rsocket
    -Wl,--whole-archive
    ${LIB_PATH_STATIC}/libdmm_rdmacm.a
    -Wl,--no-whole-archive
    ibverbs pthread dl rt
)

ADD_DEPENDENCIES(dmm_rsocket rdmacm DPDK)

set_target_properties(dmm_rsocket PROPERTIES EXCLUDE_FROM_ALL TRUE)
