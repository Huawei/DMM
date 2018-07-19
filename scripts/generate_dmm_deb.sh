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
#!/bin/sh

cur_directory=${PWD}

mkdir -p /tmp/mkdeb
mkdir -p /tmp/mkdeb/DEBIAN
mkdir -p /tmp/mkdeb/usr/bin
mkdir -p /tmp/mkdeb/usr/lib

cd ../
git archive --format=tar.gz -o /tmp/dmm.tar.gz --prefix=dmm/ HEAD

cd /tmp/
tar xzvf dmm.tar.gz
cd dmm/build
cmake ..
make -j 8

cd ../
cp -f release/bin/* /tmp/mkdeb/usr/bin
cp -f release/lib64/* /tmp/mkdeb/usr/lib
cp -f pkg/deb/control /tmp/mkdeb/DEBIAN/

cd /tmp/
dpkg-deb -b mkdeb/ ${cur_directory}/../release/deb/dmm.deb

cd ${cur_directory}
