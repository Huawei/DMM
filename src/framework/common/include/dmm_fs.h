/*
*
* Copyright (c) 2018 Huawei Technologies Co.,Ltd.
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
#ifndef _DMM_FS_H_
#define _DMM_FS_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static size_t
dmm_file_size (int fd)
{
  struct stat st;
  if (fstat (fd, &st) < 0)
    return 0;
  return st.st_size;
}

#endif /* _DMM_FS_H_ */
