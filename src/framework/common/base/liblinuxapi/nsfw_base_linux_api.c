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

#include "nsfw_base_linux_api.h"
#include "nstack_log.h"
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <dlfcn.h>

#define NSFW_BASE_OK   0
#define NSFW_BASE_FAIL (-1)

#define nsfw_call_ret(symbol, para){ \
   if (NSFW_BASE_OK != nsfw_posix_api_init()) \
   { \
        return NSFW_BASE_FAIL; \
   } \
   if (g_nsfw_posix_api.pf##symbol)   \
   {   \
       return g_nsfw_posix_api.pf##symbol para;\
   }   \
   errno = ENOSYS; \
   return NSFW_BASE_FAIL; \
}

typedef enum
{
  BASE_STATE_INIT,
  BASE_STATE_SUCCESS,
  BASE_STATE_FAIL
} nsfw_base_state;

typedef struct __base_linux_api
{
#define BASE_MK_DECL(ret, fn, args)   ret (*pf##fn) args;
#include "base_linux_api_declare.h"
} base_linux_api;

nsfw_base_state g_nsfw_module_state = BASE_STATE_INIT;
pthread_mutex_t g_nsfw_init_mutex = PTHREAD_MUTEX_INITIALIZER;
base_linux_api g_nsfw_posix_api = { 0 };

void *g_linux_lib_handle = (void *) 0;

int
nsfw_posix_symbol_load ()
{
  g_linux_lib_handle = dlopen ("libc.so.6", RTLD_NOW | RTLD_GLOBAL);
  if ((void *) 0 == g_linux_lib_handle)
    {
      /* optimize dlopen err print  */
      NSSOC_LOGERR ("cannot dlopen libc.so.6] err_string=%s", dlerror ());
      return NSFW_BASE_FAIL;
    }
#define BASE_MK_DECL(ret, fn, args)  \
         g_nsfw_posix_api.pf##fn = (typeof(g_nsfw_posix_api.pf##fn))dlsym(g_linux_lib_handle, #fn);
#include <base_linux_api_declare.h>

  return NSFW_BASE_OK;
}

/*****************************************************************
Parameters    : void
Return        :
Description   :  linux posix api init with threadonce
*****************************************************************/
static inline int
nsfw_posix_api_init ()
{
  int iret = NSFW_BASE_OK;

  /*if init already, just return success, if init fail before, just return err */
  if (BASE_STATE_INIT != g_nsfw_module_state)
    {
      return (BASE_STATE_SUCCESS ==
              g_nsfw_module_state ? NSFW_BASE_OK : NSFW_BASE_FAIL);
    }

  (void) pthread_mutex_lock (&g_nsfw_init_mutex);

  /*if init already, just return success, if init fail before, just return err */
  if (BASE_STATE_INIT != g_nsfw_module_state)
    {
      (void) pthread_mutex_unlock (&g_nsfw_init_mutex);
      return (BASE_STATE_SUCCESS ==
              g_nsfw_module_state ? NSFW_BASE_OK : NSFW_BASE_FAIL);
    }

  iret = nsfw_posix_symbol_load ();
  if (NSFW_BASE_OK == iret)
    {
      g_nsfw_module_state = BASE_STATE_SUCCESS;
    }
  else
    {
      g_nsfw_module_state = BASE_STATE_FAIL;
    }

  (void) pthread_mutex_unlock (&g_nsfw_init_mutex);
  return iret;
}
/* *INDENT-OFF* */
/*****************************************************************************
*   Prototype    : nsfw_base_socket
*   Description  : linux socket api
*   Input        : int a
*                  int b
*                  int c
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :

*
*****************************************************************************/
int nsfw_base_socket(int a, int b, int c)
{
   nsfw_call_ret(socket, (a, b, c))
}

/*****************************************************************************
*   Prototype    : nsfw_base_bind
*   Description  : linux fd bind api
*   Input        : int a
*                  const struct sockaddr* b
*                  socklen_t c
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_bind(int a, const struct sockaddr* b, socklen_t c)
{
   nsfw_call_ret(bind, (a, b, c))
}

/*****************************************************************************
*   Prototype    : nsfw_base_listen
*   Description  : linux fd listen api
*   Input        : int a
*                  int b
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_listen(int a, int b)
{
    nsfw_call_ret(listen, (a, b))
}

/*****************************************************************************
*   Prototype    : nsfw_base_shutdown
*   Description  : linux shutdown api
*   Input        : int a
*                  int b
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*
*****************************************************************************/
int nsfw_base_shutdown(int a, int b)
{
    nsfw_call_ret(shutdown, (a, b))
}

/*****************************************************************************
*   Prototype    : nsfw_base_getsockname
*   Description  : linux getsockname api
*   Input        : int a
*                  struct sockaddr* b
*                  socklen_t* c
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_getsockname(int a, struct sockaddr* b, socklen_t* c)
{
    nsfw_call_ret(getsockname, (a, b, c))
}

/*****************************************************************************
*   Prototype    : nsfw_base_getpeername
*   Description  : linux getpername api
*   Input        : int a
*                  struct sockaddr* b
*                  socklen_t* c
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_getpeername(int a, struct sockaddr* b, socklen_t* c)
{
    nsfw_call_ret(getpeername, (a, b, c))
}

/*****************************************************************************
*   Prototype    : nsfw_base_getsockopt
*   Description  : linux getsockopt api
*   Input        : int a
*                  int b
*                  int c
*                  void* d
*                  socklen_t* e
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_getsockopt(int a, int b, int c, void* d, socklen_t* e)
{
    nsfw_call_ret(getsockopt, (a, b, c, d, e))
}

/*****************************************************************************
*   Prototype    : nsfw_base_setsockopt
*   Description  : linux setsockopt api
*   Input        : int a
*                  int b
*                  int c
*                  const void* d
*                  socklen_t e
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int nsfw_base_setsockopt(int a, int b, int c, const void* d, socklen_t e)
{
    nsfw_call_ret(setsockopt, (a, b, c, d, e))
}

/*****************************************************************************
*   Prototype    : nsfw_base_accept
*   Description  : linux accept api
*   Input        : int a
*                  struct sockaddr* b
*                  socklen_t* c
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int nsfw_base_accept(int a, struct sockaddr* b, socklen_t* c)
{
    nsfw_call_ret(accept, (a, b, c))
}

/*****************************************************************************
*   Prototype    : nsfw_base_accept4
*   Description  : linux accept4 api
*   Input        : int a
*                  struct sockaddr* b
*                  socklen_t* c
*                  int flags
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int nsfw_base_accept4(int a, struct sockaddr* b, socklen_t* c, int flags)
{
    nsfw_call_ret(accept4, (a, b, c, flags))
}

/*****************************************************************************
*   Prototype    : nsfw_base_connect
*   Description  : linux connect api
*   Input        : int a
*                  const struct sockaddr* b
*                  socklen_t c
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_connect(int a, const struct sockaddr* b, socklen_t c)
{
    nsfw_call_ret(connect, (a, b, c))
}

/*****************************************************************************
*   Prototype    : nsfw_base_recv
*   Description  : linux recv api
*   Input        : int a
*                  void* b
*                  size_t c
*                  int d
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*
*****************************************************************************/
ssize_t nsfw_base_recv(int a, void* b, size_t c, int d)
{
    nsfw_call_ret(recv, (a, b, c, d))
}

/*****************************************************************************
*   Prototype    : nsfw_base_send
*   Description  : linux send api
*   Input        : int a
*                  const void* b
*                  size_t c
*                  int d
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*
*****************************************************************************/
ssize_t nsfw_base_send(int a, const void* b, size_t c, int d)
{
    nsfw_call_ret(send, (a, b, c, d))
}

/*****************************************************************************
*   Prototype    : nsfw_base_read
*   Description  : linux read api
*   Input        : int a
*                  void* b
*                  size_t c
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*
*****************************************************************************/
ssize_t nsfw_base_read(int a, void* b, size_t c)
{
    nsfw_call_ret(read, (a, b, c))
}

/*****************************************************************************
*   Prototype    : nsfw_base_write
*   Description  : linux write api
*   Input        : int a
*                  const void* b
*                  size_t c
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*
*****************************************************************************/
ssize_t nsfw_base_write(int a, const void* b, size_t c)
{
    nsfw_call_ret(write, (a, b, c))
}

/*****************************************************************************
*   Prototype    : nsfw_base_writev
*   Description  : linux writev api
*   Input        : int a
*                  const struct iovec * b
*                  int c
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*****************************************************************************/
ssize_t nsfw_base_writev(int a, const struct iovec * b, int c)
{
    nsfw_call_ret(writev, (a, b, c))
}

/*****************************************************************************
*   Prototype    : nsfw_base_readv
*   Description  : linux readv api
*   Input        : int a
*                  const struct iovec * b
*                  int c
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*
*****************************************************************************/
ssize_t nsfw_base_readv(int a, const struct iovec * b, int c)
{
    nsfw_call_ret(readv, (a, b, c))
}

/*****************************************************************************
*   Prototype    : nsfw_base_sendto
*   Description  : linux sendto api
*   Input        : int a
*                  const void * b
*                  size_t c
*                  int d
*                  const struct sockaddr *e
*                  socklen_t f
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*****************************************************************************/
ssize_t nsfw_base_sendto(int a, const void * b, size_t c, int d, const struct sockaddr *e, socklen_t f)
{
    nsfw_call_ret(sendto, (a, b, c, d, e, f))
}

/*****************************************************************************
*   Prototype    : nsfw_base_recvfrom
*   Description  : linux recvfrom api
*   Input        : int a
*                  void *b
*                  size_t c
*                  int d
*                  struct sockaddr *e
*                  socklen_t *f
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*****************************************************************************/
ssize_t nsfw_base_recvfrom(int a, void *b, size_t c, int d,struct sockaddr *e, socklen_t *f)
{
    nsfw_call_ret(recvfrom, (a, b, c, d, e, f))
}

/*****************************************************************************
*   Prototype    : nsfw_base_sendmsg
*   Description  : linux sendmsg api
*   Input        : int a
*                  const struct msghdr *b
*                  int flags
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*
*****************************************************************************/
ssize_t nsfw_base_sendmsg(int a, const struct msghdr *b, int flags)
{
    nsfw_call_ret(sendmsg, (a, b, flags))
}

/*****************************************************************************
*   Prototype    : nsfw_base_recvmsg
*   Description  : linux recvmsg api
*   Input        : int a
*                  struct msghdr *b
*                  int flags
*   Output       : None
*   Return Value : ssize_t
*   Calls        :
*   Called By    :
*
*****************************************************************************/
ssize_t nsfw_base_recvmsg(int a, struct msghdr *b, int flags)
{
    nsfw_call_ret(recvmsg, (a, b, flags))
}

/*****************************************************************************
*   Prototype    : nsfw_base_close
*   Description  : linux close api
*   Input        : int a
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_close(int a)
{
    nsfw_call_ret(close, (a))
}

/*****************************************************************************
*   Prototype    : nsfw_base_select
*   Description  : linux select api
*   Input        : int a
*                  fd_set *b
*                  fd_set *c
*                  fd_set *d
*                  struct timeval *e
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*****************************************************************************/
int nsfw_base_select(int a, fd_set *b, fd_set *c, fd_set *d, struct timeval *e)
{
    nsfw_call_ret(select, (a, b, c, d, e))
}

/*****************************************************************************
*   Prototype    : nsfw_base_ioctl
*   Description  : linux ioctl api
*   Input        : int a
*                  unsigned long b
*                  unsigned long c
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_ioctl(int a, unsigned long b, unsigned long c)
{
    nsfw_call_ret(ioctl, (a, b, c))
}

/*****************************************************************************
*   Prototype    : nsfw_base_fcntl
*   Description  : linux fcntl api
*   Input        : int a
*                  int b
*                  unsigned long c
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_fcntl(int a, int b, unsigned long c)
{
    nsfw_call_ret(fcntl, (a, b, c))
}

/*****************************************************************************
*   Prototype    : nsfw_base_epoll_create
*   Description  : linux epoll_create api
*   Input        : int a
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_epoll_create(int a)
{
    nsfw_call_ret(epoll_create, (a))
}

/*****************************************************************************
*   Prototype    : nsfw_base_epoll_create1
*   Description  : linux epoll_create1 api
*   Input        : int a
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_epoll_create1(int a)
{
    nsfw_call_ret(epoll_create1, (a))
}

/*****************************************************************************
*   Prototype    : nsfw_base_epoll_ctl
*   Description  : linux epoll_ctl api
*   Input        : int a
*                  int b
*                  int c
*                  struct epoll_event *d
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_epoll_ctl(int a, int b, int c, struct epoll_event *d)
{
    nsfw_call_ret(epoll_ctl, (a, b, c, d))
}

/*****************************************************************************
*   Prototype    : nsfw_base_epoll_wait
*   Description  : linux epoll_wait api
*   Input        : int a
*                  struct epoll_event *b
*                  int c
*                  int d
*   Output       : None
*   Return Value : int
*   Calls        :
*   Called By    :
*
*****************************************************************************/
int nsfw_base_epoll_wait(int a, struct epoll_event *b, int c, int d)
{
    nsfw_call_ret(epoll_wait, (a, b, c, d))
}

/*****************************************************************************
*   Prototype    : nsfw_base_fork
*   Description  : linux fork api
*   Input        : void
*   Output       : None
*   Return Value : pid_t
*   Calls        :
*   Called By    :
*
*****************************************************************************/
pid_t nsfw_base_fork(void)
{
    nsfw_call_ret(fork, ())
}
/* *INDENT-ON* */
