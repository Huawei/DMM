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

#ifndef STACKX_ERR_H
#define STACKX_ERR_H

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define ERR_OK 0                /* No error, everything OK. */
#define ERR_MEM -1              /* Out of memory error.     */
#define ERR_BUF -2              /* Buffer error.            */
#define ERR_TIMEOUT -3          /* Timeout.                 */
#define ERR_RTE -4              /* Routing problem.         */
#define ERR_INPROGRESS -5       /* Operation in progress    */
#define ERR_VAL -6              /* Illegal value.           */
#define ERR_WOULDBLOCK -7       /* Operation would block.   */
#define ERR_USE -8              /* Address in use.          */
#define ERR_ISCONN -9           /* Already connected.       */

#define SPL_ERR_IS_FATAL(e) ((e) < ERR_ISCONN)

#define ERR_ABRT -10            /* Connection aborted.      */
#define ERR_RST -11             /* Connection reset.        */
#define ERR_CLSD -12            /* Connection closed.       */
#define ERR_CONN -13            /* Not connected.           */

#define ERR_ARG -14             /* Illegal argument.        */

#define ERR_IF -15              /* Low-level netif error    */
#define ERR_ALREADY -16         /* previous connect attemt has not yet completed */
#define ERR_PROTOTYPE -17       /* prototype error or some other generic error.
                                   the operation is not allowed on current socket */

#define ERR_CALLBACK -18        /* callback error    */
#define ERR_CANTASSIGNADDR -19  /* Cannot assign requested address */
#define ERR_CONTAINER_ID -20    /*Illegal container id */
#define ERR_NOTSOCK			-21     /*not a socket */

#define ERR_CLOSE_WAIT -22      /*closed in established state */

#define ERR_EPROTONOSUPPORT -23 /* Protocol not supported */

#define ERR_FAULTRECOVERY -24   /*SPL just recovered from a fatal fault */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
