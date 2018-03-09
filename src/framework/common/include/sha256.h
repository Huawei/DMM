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

#ifndef _SHA256_H_
#define _SHA256_H_
#include "types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* Note that the following function prototypes are the same */
/* for both the bit and byte oriented implementations.  But */
/* the length fields are in bytes or bits as is appropriate */
/* for the version used.  Bit sequences are arrays of bytes */
/* in which bit sequence indexes increase from the most to  */
/* the least significant end of each byte                   */

#define SHA256_DIGEST_SIZE  32  /* in bytes */
#define SHA256_BLOCK_SIZE   64  /* in bytes */

typedef struct
{
  u32 count[2];
  u32 hash[8];
  u32 wbuf[16];
} SHA256_CTX;

/* SHA256 hash data in an array of bytes into hash buffer   */
/* and call the hash_compile function as required.          */

/*===========================================================================*\
    Function    :Sha256_upd
    Description :
    Calls       :
    Called by   :
    Return      :void -
    Parameters  :
        SHA256_CTX ctx[1] -
        const unsigned char data[] -
        size_t len -
    Note        :
\*===========================================================================*/
void Sha256_upd (SHA256_CTX ctx[1], const u8 data[], size_t len);

/* SHA256 Final padding and digest calculation  */

/*===========================================================================*\
    Function    :Sha256_set
    Description :
    Calls       :
    Called by   :
    Return      :void -
    Parameters  :
        SHA256_CTX ctx[1] -
    Note        :
\*===========================================================================*/
void Sha256_set (SHA256_CTX ctx[1]);

/*===========================================================================*\
    Function    :Sha256_fin
    Description :
    Calls       :
    Called by   :
    Return      :void -
    Parameters  :
        SHA256_CTX ctx[1] -
        unsigned char hval[] -
    Note        :
\*===========================================================================*/
void Sha256_fin (SHA256_CTX ctx[1], u8 hval[]);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* _SHA256_H_ */
