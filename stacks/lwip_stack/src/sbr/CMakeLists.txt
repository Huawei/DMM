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
endif()


FILE(GLOB SBR *.c)
ADD_LIBRARY(nstack SHARED ${SBR})
TARGET_LINK_LIBRARIES(nstack -Wl,--whole-archive socket -Wl,--no-whole-archive dmm_api nStackMaintain)
ADD_DEPENDENCIES(nstack socket DPDK)
INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${PAL_H_DIRECTORIES}
)
