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

ADD_EXECUTABLE(tcp_fork_server tcpserver.c)
ADD_DEPENDENCIES(tcp_fork_server nStackAPI)
TARGET_LINK_LIBRARIES(tcp_fork_server libnStackAPI.so -lpthread -lrt)

ADD_EXECUTABLE(tcp_client tcpclient.c)
ADD_DEPENDENCIES(tcp_client nStackAPI)
TARGET_LINK_LIBRARIES(tcp_client libnStackAPI.so -lpthread -lrt)

