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
# This script removes ANSI C file header comments
BEGIN { start = 0; incomment = 0;}

{
    if (start == 0)
    {
        if (incomment == 0)
        {
            # C++ single line comment
            if (/^\/\//)
            {
                # print "single line"
            }
            # single line comment
            else if (/^\/\*[^\/]*\*\/$/)
            {
                # print "single line"
            }
            # multi line comment
            else if (/^\/\*/)
            {
                #print "start comment"
                # start multi line comment
                incomment = 1
            }
            else
            {
                # first line that is not a comment, start normal output
                print $0;
                start = 1;
            }
        }
        else
        {
            # search for comment end
            if (/\*\//)
            {
                #print "end comment"
                incomment = 0;
            }
        }
    }
    else
    {
        # print the complete line for the rest of the file
        print $0
    }
}
