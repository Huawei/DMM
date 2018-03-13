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
    SET(COMM_CONFIG ${PROJECT_SOURCE_DIR}/src/framework/common/base/include/common/common_sys_config.h)
    #SET(PRI_DIRECTORIES "framework/common/base/include/common/")
endif()

INCLUDE_DIRECTORIES(
	framework/log/
	framework/include/
	framework/common/include/
	framework/common/base/include/
    framework/common/base/include/common/
    ${PRI_DIRECTORIES}
	../platform/SecureC/include/
	../thirdparty/glog/glog-0.3.4/src/
)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g -fPIC -m64 -mssse3 -std=gnu89")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wshadow -Wfloat-equal -Wformat=2")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector -fstack-protector-all")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,relro,-z,now -Wl,--disable-new-dtags,--rpath,../lib64")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,noexecstack -mcmodel=medium")

SET(COMPLE_CONFIG ${PROJECT_SOURCE_DIR}/src/framework/common/include/compile_config.h)
ADD_DEFINITIONS(-include ${COMM_CONFIG})
ADD_DEFINITIONS(-include ${COMPLE_CONFIG})
ADD_DEFINITIONS(-D_GNU_SOURCE -D_FORTIFY_SOURCE=2)
LINK_DIRECTORIES(${LIB_PATH_SHARED} ${LIB_PATH_STATIC})

if(WITH_SECUREC_LIB)
LINK_LIBRARIES(pthread dl securec)
else()
LINK_LIBRARIES(pthread dl)
endif()

ADD_SUBDIRECTORY(framework)
ADD_SUBDIRECTORY(nSocket)
