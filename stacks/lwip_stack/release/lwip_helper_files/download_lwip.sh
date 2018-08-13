#########################################################################
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
#!/bin/bash -x

set -x

SCRIPT_DIR=$(dirname "$0")
echo $SCRIPT_DIR
LWIP_DOWNLOAD_DIR="${SCRIPT_DIR}/../../lwip_src/"
echo $LWIP_DOWNLOAD_DIR
if [ ! -d "${LWIP_DOWNLOAD_DIR}/lwip/" ]; then
	mkdir -p ${LWIP_DOWNLOAD_DIR}/lwip/
        cd ${LWIP_DOWNLOAD_DIR}/
	wget -N --no-check-certificate http://download.savannah.nongnu.org/releases/lwip/lwip-2.0.3.zip
	unzip ${LWIP_DOWNLOAD_DIR}/lwip-2.0.3.zip "lwip-2.0.3/src/*" -d ${LWIP_DOWNLOAD_DIR}/lwip
	mv ${LWIP_DOWNLOAD_DIR}/lwip/lwip-2.0.3/src/* ${LWIP_DOWNLOAD_DIR}/lwip/
	rm -rf ${LWIP_DOWNLOAD_DIR}/lwip/lwip-2.0.3/
	cp -r ${SCRIPT_DIR}/arch/ ${LWIP_DOWNLOAD_DIR}/lwip/
	cp -r ${SCRIPT_DIR}/lwip/arch/ ${LWIP_DOWNLOAD_DIR}/lwip/include/lwip/
	cp -r ${SCRIPT_DIR}/core/* ${LWIP_DOWNLOAD_DIR}/lwip/core/
	cp -r ${SCRIPT_DIR}/include/* ${LWIP_DOWNLOAD_DIR}/lwip/include/lwip/
	mv ${LWIP_DOWNLOAD_DIR}/lwip/include/lwip/errno.h ${LWIP_DOWNLOAD_DIR}/lwip/include/lwip/lwip_errno.h
fi