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
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g -fPIC -m64 -mssse3 -std=gnu89")

LINK_DIRECTORIES(${LIB_PATH_SHARED} ${LIB_PATH_STATIC})

ADD_EXECUTABLE(vc_common multi_tcp_common_app_Cli.c)
ADD_DEPENDENCIES(vc_common nStackAPI)
TARGET_LINK_LIBRARIES(vc_common libnStackAPI.so -lpthread -lrt)

ADD_EXECUTABLE(vs_epoll multi_tcp_epoll_app_Ser.c)
ADD_DEPENDENCIES(vs_epoll nStackAPI)
TARGET_LINK_LIBRARIES(vs_epoll libnStackAPI.so -lpthread -lrt)

ADD_EXECUTABLE(vs_select multi_tcp_select_app_Ser.c)
ADD_DEPENDENCIES(vs_select nStackAPI)
TARGET_LINK_LIBRARIES(vs_select libnStackAPI.so -lpthread -lrt)

ADD_EXECUTABLE(kc_common multi_tcp_common_app_Cli.c)
TARGET_LINK_LIBRARIES(kc_common pthread)

ADD_EXECUTABLE(ks_epoll multi_tcp_epoll_app_Ser.c)
TARGET_LINK_LIBRARIES(ks_epoll pthread)

ADD_EXECUTABLE(ks_select multi_tcp_select_app_Ser.c)
TARGET_LINK_LIBRARIES(ks_select pthread)

ADD_EXECUTABLE(vs_common multi_tcp_common_Ser.c)
ADD_DEPENDENCIES(vs_common nStackAPI)
TARGET_LINK_LIBRARIES(vs_common libnStackAPI.so -lpthread -lrt)

ADD_EXECUTABLE(ks_common multi_tcp_common_Ser.c)
TARGET_LINK_LIBRARIES(ks_common pthread)

