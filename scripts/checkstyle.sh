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

CURR_DIR=`dirname $0`
DMM_DIR=${CURR_DIR}/../
EXIT_CODE=0
FIX="0"
FULL="0"
CHECKSTYLED_FILES=""
UNCHECKSTYLED_FILES=""
UNCHECK_LIST=""
DOS2UNIX="0"

SHOW_HELP="1"
FIX="0"
FULL="0"
CHECK="0"

# user options.... -a, -c, -f

while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -f|--fixstyle)
    FIX="1"
    DOS2UNIX="1"
    SHOW_HELP="0"
    shift # past argument
    ;;
    -c|--checkstyle)
    CHECK="1"
    SHOW_HELP="0"
    shift # past argument
    ;;
    -a|--all)
    FULL="1"
    FIX="1"
    SHOW_HELP="0"
    shift # past argument
    ;;
    -h|--help)
    shift # past argument
    ;;
    *)    # unknown option
    shift # past argument
    ;;
esac
done

if [ "${SHOW_HELP}" == "1" ] ; then
        echo "help option:"
        echo " -f, --fixstyle      - fix coding style"
        echo " -c, --checkstyle    - check coding style"
        echo " -a, --all           - fix style including dos2unix"
  exit
fi
cd ${DMM_DIR}


if [ "${FULL}" == "1" ]; then
	FILELIST=$(git ls-tree -r HEAD --name-only)
else
	FILELIST=$((git diff HEAD~1.. --name-only; git ls-files -m ) | sort -u)
fi

# Check to make sure we have indent.  Exit if we don't with an error message, but
# don't *fail*.
command -v indent > /dev/null
if [ $? != 0 ]; then
    echo "Cound not find required command \"indent\".  Checkstyle aborted"
    exit ${EXIT_CODE}
fi
indent --version

# Check to make sure we have clang-format.  Exit if we don't with an error message, but
# don't *fail*.
HAVE_CLANG_FORMAT=0
#command -v clang-format > /dev/null
#if [ $? != 0 ]; then
#    echo "Could not find command \"clang-format\". Checking C++ files will cause abort"
#else
#    clang-format --version
#    x=$(echo "" | clang-format 2>&1)
#    if [[ "$x" == "" ]]; then
#        HAVE_CLANG_FORMAT=1
#    else
#	echo "Output produced while formatting empty file (expected empty string):"
#	echo "$x"
#        echo "Could not find working \"clang-format\". Checking C++ files will cause abort"
#    fi
#fi

for i in ${FILELIST}; do
     if [ -f ${i} ] &&  ( [ ${i: -2} == ".c" ] || [ ${i: -2} == ".h" ] ) && [[ ${i} != *"glog"* ]] && [[ ${i} != *"lwip_src/lwip"* ]] && [[ ${i} != *"lwip_helper_files"* ]]; then
       #grep -q "fd.io coding-style-patch-verification: ON" ${i}
       if [ $? == 0 ]; then
            if [ ${DOS2UNIX} == 1 ]; then
                dos2unix ${i}
            fi
            EXTENSION=`basename ${i} | sed 's/^\w\+.//'`
            case ${EXTENSION} in
                hpp|cpp|cc|hh)
                    CMD="clang-format"
                    if [ ${HAVE_CLANG_FORMAT} == 0 ]; then
                            echo "C++ file detected. Abort. (missing clang-format)"
                            exit ${EXIT_CODE}
                    fi
                    ;;
                *)
                    CMD="indent"
                    ;;
            esac
            CHECKSTYLED_FILES="${CHECKSTYLED_FILES} ${i}"
            if [ ${FIX} == 0 ]; then
                if [ "${CMD}" == "clang-format" ]
                then
                    clang-format ${i} > ${i}.out2
                else
                    indent -gnu -nut ${i} -o ${i}.out1 > /dev/null 2>&1
                    indent -gnu -nut ${i}.out1 -o ${i}.out2 > /dev/null 2>&1
                fi
                # Remove trailing whitespace
                sed -i -e 's/[[:space:]]*$//' ${i}.out2
                diff -q ${i} ${i}.out2
            else
                if [ "${CMD}" == "clang-format" ]; then
                    clang-format -i ${i} > /dev/null 2>&1
                else
                    indent -nut -sob ${i}
                    indent -nut -sob ${i}
                fi
                # Remove trailing whitespace
                sed -i -e 's/[[:space:]]*$//' ${i}
            fi
            if [ $? != 0 ]; then
                EXIT_CODE=1
                echo
                echo "Checkstyle failed for ${i}."
                if [ "${CMD}" == "clang-format" ]; then
                    echo "Run clang-format as shown to fix the problem:"
                    echo "clang-format -i ${DMM_DIR}${i}"
                else
                    echo "Run indent (twice!) as shown to fix the problem:"
                    echo "indent ${DMM_DIR}${i}"
                    echo "indent ${DMM_DIR}${i}"
                fi
            fi
            if [ -f ${i}.out1 ]; then
                rm ${i}.out1
            fi
            if [ -f ${i}.out2 ]; then
                rm ${i}.out2
            fi
        else
            UNCHECKSTYLED_FILES="${UNCHECKSTYLED_FILES} ${i}"
        fi
    else
        UNCHECKSTYLED_FILES="${UNCHECKSTYLED_FILES} ${i}"
    fi
done

if [ ${FULL} == "1" ] && [ ${FULL} == "1" ] ; then
  for i in ${FILELIST}; do
      egrep -qlr $'\r'\$ ${i}
      if [ $? == 0 ] && [[ ${i} != *"glog"* ]] && [[ ${i} != *"lwip_src/lwip"* ]] && [[ ${i} != *"lwip_helper_files"* ]] && [[ ${i} != *"resources"* ]] && [[ ${i} != *"build"* ]] && ( [ ${i: -2} == ".c" ] || [ ${i: -2} == ".h" ] || [ ${i: -3} == ".sh" ] && [ ${i} != "checkstyle.sh" ]); then
          sed -e 's/\r//g' ${i} > ${i}.tmp
          echo "dos2unix conoversion happened for ${i}"
          mv ${i}.tmp ${i} 
      fi
  done
fi

#delete temp files
echo "deleting temp files generated by ${CMD}"

find . -name "*~" -type f -delete

if [ ${EXIT_CODE} == 0 ]; then
    git status
    echo "*******************************************************************"
    echo "* DMM CHECKSTYLE SUCCESSFULLY COMPLETED"
    echo "*******************************************************************"
else
    echo "*******************************************************************"
    echo "* DMM CHECKSTYLE FAILED"
    echo "* CONSULT FAILURE LOG ABOVE"
    echo "* NOTE: Running 'checkstyle.sh -f' *MAY* fix the issue"
    echo "*******************************************************************"
fi
cd ${CURR_DIR}
exit ${EXIT_CODE}
