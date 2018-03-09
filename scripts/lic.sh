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

function remove_copyright_a {
    printf "%s\n" 1,14d w q | ed "$1"
}

function remove_copyright_b {
    printf "%s\n" 1,14d w q | ed "$1"
}

function add_copyright {
    if [[ $1 == Makefile ]]; then
        ed "$1" <<END
0i
/*
 * Copyright (c) 2015 Huawei and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
.
w
q
END
    else
        ed "$1" <<END
0i
/*
 * Copyright (c) 2015  HUAWEI and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
.
w
q
END
    fi
}

shopt -s nullglob globstar
#for file in **/*.[ch]; do
 #    ./replace_header.sh "$file"
#done

for i in $(find ./../ -type f)
do
 if [[ ${i} != *"thirdparty"* ]] && [[ ${i} != *"testcode"* ]] && [[ ${i} != *"resources"* ]] && [[ ${i} != *"build"* ]] ; then
   if [ "${i: -2}" == ".c" ] || [ "${i: -2}" == ".h" ] ; then
     ./replace_header.sh "$i" "header.template"
   elif [[ "${i}" == *"Makefile" ]] || [ "${i: -4}" == ".txt" ] || [ "${i: -3}" == ".sh" ] ; then
       grep -q "Apache License" ${i}
       if [ $? != 0 ]; then
          ./replace_header.sh "$i" "header2.template"
       fi
   fi
 fi
done
