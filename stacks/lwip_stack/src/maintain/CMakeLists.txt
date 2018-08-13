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
    SET(PAL_BITWIDE_ADJUST ${PROJECT_SOURCE_DIR}/../../src/framework/common/base/include/common_pal_bitwide_adjust.h)
    ADD_DEFINITIONS(-include ${PAL_BITWIDE_ADJUST})
    INCLUDE_DIRECTORIES(
        ${PAL_H_DIRECTORIES}
    )
endif()

FILE(GLOB_RECURSE NSTACKMAINTAIN *.c)

LINK_LIBRARIES(m dl rt dmm_api)
if(WITH_SECUREC_LIB)
INCLUDE_DIRECTORIES(
	./
	${JSON_C_SRC}
	${SECUREC_SRC}
	${CMAKE_CURRENT_LIST_DIR}/../include/
)
else()
INCLUDE_DIRECTORIES(
        ./
        ${JSON_C_SRC}
        ${CMAKE_CURRENT_LIST_DIR}/../include/
)
endif()
ADD_LIBRARY(nStackMaintain STATIC ${NSTACKMAINTAIN})
