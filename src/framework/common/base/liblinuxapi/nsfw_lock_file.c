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
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "types.h"
#include "nstack_securec.h"
#include "nsfw_init.h"
#include "common_mem_api.h"

#include "nstack_log.h"
#include "nsfw_maintain_api.h"
#include "nsfw_mgr_com_api.h"

#include "nsfw_base_linux_api.h"
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C"{
/* *INDENT-ON* */
#endif /* __cplusplus */

#define NSFW_FILE_PATH_LEN 128
#define LOCK_FOLDER "/ip_module/"
#define LOCK_SUFFIX ".pid"

#define read_lock(fd, offset, whence, len) nsfw_lock_reg((fd), F_SETLK, F_RDLCK, (offset), (whence), (len))
#define readw_lock(fd, offset, whence, len) nsfw_lock_reg((fd), F_SETLKW, F_RDLCK, (offset), (whence), (len))
#define write_lock(fd, offset, whence, len) nsfw_lock_reg((fd), F_SETLK, F_WRLCK, (offset), (whence), (len))
#define writew_lock(fd, offset, whence, len) nsfw_lock_reg((fd), F_SETLKW, F_WRLCK, (offset), (whence), (len))
#define un_lock(fd, offset, whence, len) nsfw_lock_reg((fd), F_SETLK, F_UNLCK, (offset), (whence), (len))

i32
nsfw_lock_reg (i32 fd, i32 cmd, i32 type, off_t offset, i32 whence, off_t len)
{
  struct flock lock_file;
  lock_file.l_type = type;
  lock_file.l_start = offset;
  lock_file.l_whence = whence;
  lock_file.l_len = len;
  return (fcntl (fd, cmd, &lock_file));
}

/*****************************************************************************
*   Prototype    : nsfw_proc_start_with_lock
*   Description  : lock file start
*   Input        : u8 proc_type
*   Output       : None
*   Return Value : i32
*   Calls        :
*   Called By    :
*****************************************************************************/
i32
nsfw_proc_start_with_lock (u8 proc_type)
{
  NSFW_LOGINF ("lock_file init]type=%u", proc_type);
  char *module_name = nsfw_get_proc_name (proc_type);
  if (NULL == module_name)
    {
      NSFW_LOGERR ("proc type error]proc_type=%u", proc_type);
      return 0;
    }

  const char *directory = NSFW_DOMAIN_DIR;
  const char *home_dir = getenv ("HOME");

  if (getuid () != 0 && home_dir != NULL)
    {
      directory = home_dir;
    }

  int ret;
  char lock_fpath[NSFW_FILE_PATH_LEN] = { 0 };
  ret = STRCPY_S (lock_fpath, NSFW_FILE_PATH_LEN, directory);
  if (EOK != ret)
    {
      NSFW_LOGERR ("lock init STRCPY_S failed]ret=%d", ret);
      return -1;
    }

  ret = STRCAT_S (lock_fpath, NSFW_FILE_PATH_LEN, LOCK_FOLDER);
  if (EOK != ret)
    {
      NSFW_LOGERR ("lock init STRCAT_S failed]ret=%d", ret);
      return -1;
    }

  ret = STRCAT_S (lock_fpath, NSFW_FILE_PATH_LEN, module_name);
  if (EOK != ret)
    {
      NSFW_LOGERR ("lock init STRCAT_S failed]ret=%d", ret);
      return -1;
    }

  ret = STRCAT_S (lock_fpath, NSFW_FILE_PATH_LEN, LOCK_SUFFIX);
  if (EOK != ret)
    {
      NSFW_LOGERR ("lock init STRCAT_S failed]ret=%d", ret);
      return -1;
    }

  i32 fd;
  if ((fd = open (lock_fpath, O_RDWR | O_CREAT, 0640)) == -1)
    {                           /* file permission no large than 0640 */
      NSFW_LOGERR ("open lock file error!]path=%s,error = %d", lock_fpath,
                   errno);
      return -1;
    }

  int rc = nsfw_set_close_on_exec (fd);
  if (rc == -1)
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("set exec err]fd=%d, errno=%d", fd, errno);
      return -1;
    }

  if (write_lock (fd, 0, SEEK_SET, 0) < 0)
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("get lock file error!]path=%s,error = %d", lock_fpath,
                   errno);
      return -1;
    }

  char buf[32] = { 0 };
  if (ftruncate (fd, 0) < 0)
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("ftruncate file error!]path=%s,error = %d", lock_fpath,
                   errno);
      return -1;
    }

  ret =
    SNPRINTF_S (buf, sizeof (buf), sizeof (buf) - 1, "%ld", (long) getpid ());
  if (-1 == ret)
    {
      NSTCP_LOGERR ("SNPRINTF_S failed]ret=%d", ret);
      (void) nsfw_base_close (fd);
      return -1;
    }

  if (write (fd, buf, strlen (buf) + 1) < 0)
    {
      (void) nsfw_base_close (fd);
      NSFW_LOGERR ("write file error!]path=%s,error = %d", lock_fpath, errno);
      return -1;
    }

  return 0;
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif /* __cplusplus */
