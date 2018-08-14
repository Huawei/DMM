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


CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11)
PROJECT(nStack)

option(RMWS    "automatically remove trailing whitespace using git pre-commit hook" OFF)
option(WITH_SECUREC_LIB "Option description" OFF)
option(WITH_HAL_LIB "Option description" OFF)

SET(CMAKE_C_COMPILER "gcc")
SET(EXECUTABLE_PATH ${CMAKE_CURRENT_LIST_DIR}/release/bin)
SET(LIB_PATH_STATIC ${PROJECT_BINARY_DIR})
SET(LIB_PATH_SHARED ${CMAKE_CURRENT_LIST_DIR}/release/lib64)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${LIB_PATH_STATIC})
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_PATH})
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LIB_PATH_SHARED})
SET(DMM_DPDK_INSTALL_DIR "" CACHE STRING "to get the dpdk install path by cmd")
SET(DMM_REL_INC_DIR ${CMAKE_CURRENT_LIST_DIR}/release/include)
SET(DMM_ARCH x86)

SET(GENERATE_RPM_PATH ${CMAKE_CURRENT_LIST_DIR}/release/rpm)
file(MAKE_DIRECTORY ${GENERATE_RPM_PATH})

SET(GENERATE_DEB_PATH ${CMAKE_CURRENT_LIST_DIR}/release/deb)
file(MAKE_DIRECTORY ${GENERATE_DEB_PATH})

MESSAGE(STATUS "Top dir is: " ${CMAKE_CURRENT_LIST_DIR})
MESSAGE(STATUS "Static library dir: " ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
MESSAGE(STATUS "Executable binary dir: " ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
MESSAGE(STATUS "Shared library dir: " ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})


add_custom_target(clean-cmake-files
  COMMAND ${CMAKE_COMMAND} -P clean-all.cmake
)

if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  message(STATUS "Setting git commit template...")
  execute_process(COMMAND git config --local commit.template ${CMAKE_SOURCE_DIR}/scripts/git/commit-msg-template
                  RESULT_VARIABLE  git_config_ret)
  if(git_config_ret EQUAL 0)
    message(STATUS "Setting git commit template...done")
  else()
    message(WARNING "Setting git commit template...failed")
  endif()

  find_program(GIT_REVIEW git-review)
  if(NOT GIT_REVIEW)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.git/hooks/commit-msg")
      message(STATUS "git-review not found")
      message(STATUS "Setting git commit hook...")
      execute_process(COMMAND ln -s ${CMAKE_SOURCE_DIR}/scripts/git/commit-msg-hook.py ${CMAKE_SOURCE_DIR}/.git/hooks/commit-msg
                      RESULT_VARIABLE  ln_ret)
      if(ln_ret EQUAL 0)
        message(STATUS "Setting git commit hook...done")
      else()
        message(WARNING "Setting git commit hook...failed")
      endif()
    endif()
  else()
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.git/hooks/commit-msg")
      message(AUTHOR_WARNING  "Found git-review but this repo does not seem to have been initialized by git-review. Please manually execute 'git review -s' before cmake.")
    else()
      file(READ ${CMAKE_SOURCE_DIR}/.git/hooks/commit-msg contents)
      string(FIND "${contents}" "commit-msg-hook.py" match_ret)
      if(${match_ret} EQUAL -1)
        file(APPEND ${CMAKE_SOURCE_DIR}/.git/hooks/commit-msg "${CMAKE_SOURCE_DIR}/scripts/git/commit-msg-hook.py $1")
      endif()
    endif()
  endif()

  if(RMWS)
    message(STATUS  "Setting git pre-commit hook...")
    execute_process(COMMAND ln -sf ${CMAKE_SOURCE_DIR}/scripts/git/pre-commit ${CMAKE_SOURCE_DIR}/.git/hooks/pre-commit
                    RESULT_VARIABLE  ln_sf_ret)
    if(ln_sf_ret EQUAL 0)
      message(STATUS  "Setting git pre-commit hook...done")
    else()
      message(WARNING "Setting git pre-commit hook...failed")
    endif()
  endif()
endif()

if(WITH_SECUREC_LIB)
  add_definitions(-DSECUREC_LIB)
endif()

if(WITH_HAL_LIB)
  add_definitions(-DHAL_LIB)
endif()

SET(post_compile "${PROJECT_BINARY_DIR}/post_compile.sh")
FILE(WRITE ${post_compile} "#!/bin/bash\n")
if(WITH_HAL_LIB)
else()
FILE(APPEND ${post_compile}
"

cp -f ${CMAKE_CURRENT_LIST_DIR}/src/nSocket/include/nstack_dmm_api.h ${CMAKE_CURRENT_LIST_DIR}/release/include/
cp -f ${CMAKE_CURRENT_LIST_DIR}/src/adapt/nstack_dmm_adpt.h ${CMAKE_CURRENT_LIST_DIR}/release/include/
cp -f ${CMAKE_CURRENT_LIST_DIR}/src/adapt/nstack_share_res.h ${CMAKE_CURRENT_LIST_DIR}/release/include/
cp -f ${CMAKE_CURRENT_LIST_DIR}/src/nSocket/include/declare_syscalls.h ${CMAKE_CURRENT_LIST_DIR}/release/include/
cp -f ${CMAKE_CURRENT_LIST_DIR}/src/framework/include/* ${CMAKE_CURRENT_LIST_DIR}/release/include/
cp -rf ${CMAKE_CURRENT_LIST_DIR}/src/framework/common/base/include/*.h ${CMAKE_CURRENT_LIST_DIR}/release/include/
cp -rf ${CMAKE_CURRENT_LIST_DIR}/src/framework/common/base/include/common/* ${CMAKE_CURRENT_LIST_DIR}/release/include/

cp -f ${CMAKE_CURRENT_LIST_DIR}/src/framework/common/include/*.h ${CMAKE_CURRENT_LIST_DIR}/release/include/
cp -rf ${CMAKE_CURRENT_LIST_DIR}/src/framework/common/include/generic ${CMAKE_CURRENT_LIST_DIR}/release/include/
cp -f ${CMAKE_CURRENT_LIST_DIR}/src/framework/common/include/arch/${DMM_ARCH}/* ${CMAKE_CURRENT_LIST_DIR}/release/include/
cp -f ${CMAKE_CURRENT_LIST_DIR}/src/framework/ipc/mgr_com/mgr_com.h ${CMAKE_CURRENT_LIST_DIR}/release/include/
cp -f ${CMAKE_CURRENT_LIST_DIR}/src/framework/hal/hal.h ${CMAKE_CURRENT_LIST_DIR}/release/include/
cp -f ${CMAKE_CURRENT_LIST_DIR}/src/nSocket/include/nstack_dmm_api.h ${CMAKE_CURRENT_LIST_DIR}/release/include/
echo post compile process success.
"
)
endif()

SET(clean_compile "${PROJECT_BINARY_DIR}/clean_compile.sh")
FILE(WRITE ${clean_compile} "#!/bin/bash\n")
FILE(APPEND ${clean_compile}
"

rm -rf ${CMAKE_CURRENT_LIST_DIR}/release/include/*.h
rm -rf ${CMAKE_CURRENT_LIST_DIR}/release/include/generic
rm -rf ${CMAKE_CURRENT_LIST_DIR}/release/lib64/libdmm_api.so

echo post clean process success.
"
)

ADD_CUSTOM_TARGET(DPDK ALL COMMAND sh post_compile.sh)
ADD_DEPENDENCIES(DPDK dmm_api)

ADD_CUSTOM_TARGET(pkg-rpm COMMAND sh ../scripts/generate_dmm_rpm.sh)

ADD_CUSTOM_TARGET(pkg-deb COMMAND sh ../scripts/generate_dmm_deb.sh)

ADD_CUSTOM_TARGET(vpp-stack COMMAND sh ../scripts/build_vpp.sh)
ADD_DEPENDENCIES(vpp-stack DPDK)

ADD_CUSTOM_TARGET(checkstyle
 COMMAND bash  ${CMAKE_CURRENT_LIST_DIR}/scripts/checkstyle.sh -c)
ADD_CUSTOM_TARGET(fixstyle
 COMMAND bash  ${CMAKE_CURRENT_LIST_DIR}/scripts/checkstyle.sh -f)

ADD_CUSTOM_TARGET(clean-all
 COMMAND ${CMAKE_BUILD_TOOL} clean
 COMMAND sh clean_compile.sh)

SET(JSON_C_URL https://github.com/json-c/json-c/archive/json-c-0.12.1-20160607.tar.gz)
SET(JSON_C_DOWNLOAD_DIR ${CMAKE_CURRENT_LIST_DIR}/thirdparty/json/)
SET(JSON_C_SRC ${CMAKE_CURRENT_LIST_DIR}/thirdparty/json/json-c-0.12.1)
if(EXISTS "${JSON_C_DOWNLOAD_DIR}/json-c-0.12.1.tar.gz")
    SET(JSON_DOWNLOAD_CMD tar -xvf ${JSON_C_DOWNLOAD_DIR}/json-c-0.12.1.tar.gz  -C ${JSON_C_DOWNLOAD_DIR}/json-c-0.12.1 --strip-components 1)
else()
    SET(JSON_DOWNLOAD_CMD wget --no-check-certificate -O json-c-0.12.1.tar.gz ${JSON_C_URL} || rm -f json-c-0.12.1.tar.gz && tar -xvf ${JSON_C_DOWNLOAD_DIR}/json-c-0.12.1.tar.gz  -C ${JSON_C_DOWNLOAD_DIR}/json-c-0.12.1 --strip-components 1)
endif()
INCLUDE(ExternalProject)
ExternalProject_Add(
		JSON
		DOWNLOAD_DIR ${JSON_C_DOWNLOAD_DIR}
		DOWNLOAD_COMMAND ${JSON_DOWNLOAD_CMD}
		BUILD_IN_SOURCE 1
		SOURCE_DIR ${JSON_C_SRC}
		CONFIGURE_COMMAND sh autogen.sh COMMAND sh ./configure --enable-static --with-pic
        BUILD_COMMAND make -j 8
		INSTALL_COMMAND cp -f .libs/libjson-c.a ${LIB_PATH_STATIC}/
)

SET(SECUREC_SRC ${CMAKE_CURRENT_LIST_DIR}/thirdparty/SecureC/src)
SET(SECUREC_SRC_H ${CMAKE_CURRENT_LIST_DIR}/thirdparty/SecureC/include)

if(WITH_SECUREC_LIB)
INCLUDE(ExternalProject)
ExternalProject_Add(
		SECUREC
		SOURCE_DIR ${SECUREC_SRC}
		BUILD_IN_SOURCE 1
		CONFIGURE_COMMAND ""
        BUILD_COMMAND COMMAND make -j 8
		INSTALL_COMMAND cp -f ${SECUREC_SRC}/../lib/libsecurec.so ${LIB_PATH_SHARED}/
)
endif()

SET(GLOG_SRC ${CMAKE_CURRENT_LIST_DIR}/thirdparty/glog/glog-0.3.4)
INCLUDE(ExternalProject)
ExternalProject_Add(
		GLOG
		SOURCE_DIR ${GLOG_SRC}
		BUILD_IN_SOURCE 1
		CONFIGURE_COMMAND autoreconf -ivf COMMAND sh configure CFLAGS=-fPIC CXXFLAGS=-fPIC
        BUILD_COMMAND COMMAND make -j 8
		INSTALL_COMMAND cp -f ${GLOG_SRC}/.libs/libglog.a ${LIB_PATH_STATIC}/
)

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(app_example)

ADD_SUBDIRECTORY(stacks/rsocket)
ADD_SUBDIRECTORY(thirdparty/apps)
