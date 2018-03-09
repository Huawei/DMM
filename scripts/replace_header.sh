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

#!/bin/bash
SOURCE_DIR=`dirname $0`
if [ $# != 2 ]; then
    echo "Usage: replace_header.sh <file> <header.template>"
    exit 1
fi

cat $SOURCE_DIR/$2 > $SOURCE_DIR/tmp
cat $1 | awk -f $SOURCE_DIR/remove_header.awk >> $SOURCE_DIR/tmp
mv $SOURCE_DIR/tmp $1

