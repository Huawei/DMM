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
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "dmm_config.h"
#include "dmm_share.h"
#include "dmm_fs.h"

#define DMM_FSHM_FMT "%s/dmm-fshm-%d"   /* VAR_DIR pid */

inline static void
set_fshm_path (struct dmm_share *share)
{
  (void) snprintf (share->path, sizeof (share->path), DMM_FSHM_FMT,
                   DMM_VAR_DIR, share->pid);
}

/*
input: share->path, share->size, share->pid
output: share->base
*/
int
dmm_fshm_create (struct dmm_share *share)
{
  int fd, ret;
  void *base;

  set_fshm_path (share);

  fd = open (share->path, O_RDWR | O_CREAT, 0666);
  if (fd < 0)
    {
      return -1;
    }

  ret = ftruncate (fd, share->size);
  if (ret < 0)
    {
      (void) close (fd);
      return -1;
    }

  base = mmap (NULL, share->size,
               PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
  if (base == MAP_FAILED)
    {
      (void) close (fd);
      return -1;
    }

  share->base = base;

  (void) close (fd);
  return 0;
}

int
dmm_fshm_delete (struct dmm_share *share)
{
  (void) munmap (share->base, share->size);
  (void) unlink (share->path);

  return 0;
}

/*
input: share->path, share->size, share->base(OPT)
output: share->base(if-null)
*/
int
dmm_fshm_attach (struct dmm_share *share)
{
  int fd;
  void *base;

  if (share->type != DMM_SHARE_FSHM)
    {
      return -1;
    }

  fd = open (share->path, O_RDWR);
  if (fd < 0)
    {
      return -1;
    }

  if (share->size <= 0)
    {
      share->size = dmm_file_size (fd);
      if (share->size == 0)
        {
          (void) close (fd);
          return -1;
        }
    }

  base = mmap (share->base, share->size,
               PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
  if (base == MAP_FAILED)
    {
      (void) close (fd);
      return -1;
    }

  if (NULL == share->base)
    {
      share->base = base;
    }
  else if (base != share->base)
    {
      (void) munmap (base, share->size);
      (void) close (fd);
      return -1;
    }

  (void) close (fd);
  return 0;
}

int
dmm_fshm_detach (struct dmm_share *share)
{
  (void) munmap (share->base, share->size);

  return 0;
}
