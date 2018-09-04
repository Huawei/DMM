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

#include "stackx_spl_share.h"
#include "common_pal_bitwide_adjust.h"
#include "stackx_event.h"

#define FREE_FD_SET(readfd, writefd, exceptfd) {\
if(readfd)\
free(readfd);\
if(writefd)\
free(writefd);\
if(exceptfd)\
free(exceptfd);\
}

int
lwip_try_select (int fdsize, fd_set * fdread, fd_set * fdwrite,
                 fd_set * fderr, struct timeval *timeout)
{
  int i;
  int nready = 0;
  nstack_fd_set *read_out;
  nstack_fd_set *write_out;
  nstack_fd_set *err_out;
  nstack_fd_set *readfd = (nstack_fd_set *) fdread;
  nstack_fd_set *writefd = (nstack_fd_set *) fdwrite;
  nstack_fd_set *exceptfd = (nstack_fd_set *) fderr;
  sbr_socket_t *sock;
  spl_netconn_t *conn;

  if ((fdsize >= NSTACK_SETSIZE) || (fdsize < 0))
    {
      return 0;
    }
  read_out = malloc (sizeof (nstack_fd_set));
  write_out = malloc (sizeof (nstack_fd_set));
  err_out = malloc (sizeof (nstack_fd_set));
  if ((!read_out) || (!write_out) || (!err_out))
    {
      NSPOL_LOGERR ("malloc nstack_fd_set fail");
      FREE_FD_SET (read_out, write_out, err_out);
      return -1;
    }
  int ret = NSTACK_FD_ZERO (read_out);
  int ret1 = NSTACK_FD_ZERO (write_out);
  int ret2 = NSTACK_FD_ZERO (err_out);

  if ((EOK != ret) || (EOK != ret1) || (EOK != ret2))
    {
      NSPOL_LOGERR ("NSTACK_FD_ZERO fail");
      FREE_FD_SET (read_out, write_out, err_out);
      return -1;
    }

  for (i = 0; i < fdsize; i++)
    {
      if (!((readfd && NSTACK_FD_ISSET (i, readfd))
            || (writefd && NSTACK_FD_ISSET (i, writefd))
            || (exceptfd && NSTACK_FD_ISSET (i, exceptfd))))
        {
          continue;
        }
      sock = sbr_lookup_sk (i);
      if (sock == NULL)
        {
          errno = EBADF;
          FREE_FD_SET (read_out, write_out, err_out);
          return -1;
        }
      conn = sbr_get_conn (sock);
      if (!conn)
        {
          errno = EBADF;
          FREE_FD_SET (read_out, write_out, err_out);
          return -1;
        }
      if (readfd && NSTACK_FD_ISSET (i, readfd) &&
          ((sbr_get_fd_share (sock)->lastdata != NULL)
           || (conn->rcvevent > 0)))
        {
          NSTACK_FD_SET (i, read_out);
          nready++;
        }
      if (writefd && NSTACK_FD_ISSET (i, writefd) && (conn->sendevent != 0))
        {
          NSTACK_FD_SET (i, write_out);
          nready++;
        }
      if (exceptfd && NSTACK_FD_ISSET (i, exceptfd) && (conn->errevent != 0))
        {
          NSTACK_FD_SET (i, write_out);
          nready++;
        }
    }

  //TODO: need to handle fd_set and nstack_fd_set memory issue
  if (readfd)
    {
      *readfd = *read_out;
    }

  if (writefd)
    {
      *writefd = *write_out;
    }

  if (exceptfd)
    {
      *exceptfd = *err_out;
    }

  FREE_FD_SET (read_out, write_out, err_out);
  return nready;
}
