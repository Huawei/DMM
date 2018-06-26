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

/*Possible access of out-of-bounds pointer:
 The algorithms has been tested on purify. So no
 out of bounds access possible.*/

/*Possible creation of out-of-bounds pointer
 No Out of bounds pointers are created.- false positive.*/

#include <string.h>             /* for mem copy function  etc.        */
#include "sha256.h"
#include "nstack_securec.h"
#include "types.h"
#include "nstack_log.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define rotleft32(x,n)   (((x) << n) | ((x) >> (32 - n)))
#define rotright32(x,n)   (((x) >> n) | ((x) << (32 - n)))

#if !defined(bswap_32)
#define bswap_32(x) ((rotright32((x), 24) & 0x00ff00ff) | (rotright32((x), 8) & 0xff00ff00))
#endif

#ifdef LITTLE_ENDIAN
#define SWAP_THE_BYTES
#else
#undef  SWAP_THE_BYTES
#endif

#define ch(a,b,c)       ((c) ^ ((a) & ((b) ^ (c))))
#define maj(a,b,c)      (((a) & (b)) | ((c) & ((a) ^ (b))))

  /* round transforms for the SHA256 & SHA512 compression functions */

#define vf(m,n) v[(m - n) & 7]

#define hf(n) (p[n & 15] += \
    g_1(p[(n + 14) & 15]) + p[(n + 9) & 15] + g_0(p[(n + 1) & 15]))

#define v_cycle(m,n)                                \
{ \
    vf(7,m) += (n ? hf(m) : p[m]) + k_0[m+n]        \
    + s_1(vf(4,m)) + ch(vf(4,m),vf(5,m),vf(6,m));   \
    vf(3,m) += vf(7,m);                             \
    vf(7,m) += s_0(vf(0,m))+ maj(vf(0,m),vf(1,m),vf(2,m)); \
}

#define SHA256_MASK (SHA256_BLOCK_SIZE - 1)     /* SHA256_MASK */

#if defined(SWAP_THE_BYTES)
#define bsw_32(p,n) \
{ \
    u32 _i = (n); \
    while (_i--) \
    { \
        ((u32*)p)[_i] = bswap_32(((u32*)p)[_i]); \
    } \
}

#else
#define bsw_32(p,n)
#endif

#define s_0(x)  (rotright32((x),  2) ^ rotright32((x), 13) ^ rotright32((x), 22))
#define s_1(x)  (rotright32((x),  6) ^ rotright32((x), 11) ^ rotright32((x), 25))
#define g_0(x)  (rotright32((x),  7) ^ rotright32((x), 18) ^ ((x) >>  3))
#define g_1(x)  (rotright32((x), 17) ^ rotright32((x), 19) ^ ((x) >> 10))
#define k_0     k256

/* rotated SHA256 round definition. Unlike swapping the  variables as in    */
/* FIPS-180, different variables are being 'rotated' on each round,         */
/* returning to their starting positions every 8 rounds                     */

#define q(i)  v##i

#define one_cycle(a,b,c,d,e,f,g,h,k,w)  \
    q(h) += s_1(q(e)) + ch(q(e), q(f), q(g)) + k + w; \
    q(d) += q(h); q(h) += s_0(q(a)) + maj(q(a), q(b), q(c))

/*
Description: SHA256 mixing data
Value Range: None
Access: Used to mix with data to create SHA256 key.
Remarks:
*/
static const u32 k256[64] = {
  010242427630, 016115642221, 026560175717, 035155355645,
  07125541133, 013174210761, 022217701244, 025307057325,
  033001725230, 02240655401, 04414302676, 012503076703,
  016257456564, 020067530776, 023367003247, 030146770564,
  034446664701, 035757443606, 01760316706, 04403120714,
  05572226157, 011235102252, 013454124734, 016676304332,
  023017450522, 025014343155, 026000623710, 027726277707,
  030670005763, 032551710507, 0662461521, 02412224547,
  04755605205, 05606620470, 011513066774, 012316006423,
  014502471524, 016632405273, 020160544456, 022234426205,
  024257764241, 025006463113, 030222705560, 030733050643,
  032144564031, 032646203044, 036403432605, 02032520160,
  03151140426, 03615666010, 04722073514, 06454136265,
  07107006263, 011666125112, 013347145117, 015013467763,
  016443701356, 017051261557, 020462074024, 021461601010,
  022057577772, 024424066353, 027676321767, 030634274362,
};

#define v_ v
#define ptr p

/*===========================================================================*\
    Function    :Sha256_compile__
    Description : This function generates the digest value for SHA256.
    Compile 64 bytes of hash data into SHA256 digest value
    Calls       :  mem copy - Secure mem copy function.
    Called by   :
    Return      : This is a static internal function which doesn't return any value.
    Parameters  :
        SHA256_CTX ctx[1] -
    Note        : this routine assumes that the byte order in the
    ctx->wbuf[] at this point is such that low address bytes in
    the ORIGINAL byte stream will go into the high end of
    words on BOTH big and little endian systems.
\*===========================================================================*/
NSTACK_STATIC void
Sha256_compile__ (SHA256_CTX ctx[1])
{

  /* macros defined above to this function i.e. v_ and ptr should not be removed */
  /* v_cycle - for 0 to 15 */
  u32 i;
  u32 *ptr = ctx->wbuf;
  u32 v_[8];

  int ret = MEMCPY_S (v_, 8 * sizeof (u32), ctx->hash, 8 * sizeof (u32));
  if (EOK != ret)
    {
      NSPOL_LOGERR ("MEMCPY_S failed");
      return;
    }

  for (i = 0; i < 64; i += 16)
    {
      /*v_cycle operations from 0 to 15 */
      v_cycle (0, i);
      v_cycle (1, i);
      v_cycle (2, i);
      v_cycle (3, i);
      v_cycle (4, i);
      v_cycle (5, i);
      v_cycle (6, i);
      v_cycle (7, i);
      v_cycle (8, i);
      v_cycle (9, i);
      v_cycle (10, i);
      v_cycle (11, i);
      v_cycle (12, i);
      v_cycle (13, i);
      v_cycle (14, i);
      v_cycle (15, i);
    }

  /* update the context */
  ctx->hash[0] += v_[0];
  ctx->hash[1] += v_[1];
  ctx->hash[2] += v_[2];
  ctx->hash[3] += v_[3];
  ctx->hash[4] += v_[4];
  ctx->hash[5] += v_[5];
  ctx->hash[6] += v_[6];
  ctx->hash[7] += v_[7];

  return;
}

#undef v_
#undef ptr

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
void
Sha256_upd (SHA256_CTX sha256_ctx[1], const u8 data[], size_t len)
{
  u32 pos = (u32) (sha256_ctx->count[0] & SHA256_MASK);
  u32 capacity = SHA256_BLOCK_SIZE - pos;
  const u8 *sp = data;
  int ret;

  if ((sha256_ctx->count[0] += (u32) len) < len)
    {
      ++(sha256_ctx->count[1]);
    }

  while (len >= capacity)
    {

      /* tranfer whole blocks while possible  */
      ret =
        MEMCPY_S (((u8 *) sha256_ctx->wbuf) + pos, capacity, sp, capacity);
      if (EOK != ret)
        {
          NSPOL_LOGERR ("MEMCPY_S failed");
          return;
        }

      sp += capacity;
      len -= capacity;
      capacity = SHA256_BLOCK_SIZE;
      pos = 0;
      bsw_32 (sha256_ctx->wbuf, SHA256_BLOCK_SIZE >> 2);
      Sha256_compile__ (sha256_ctx);
    }

  if (len != 0)
    {
      ret =
        MEMCPY_S (((u8 *) sha256_ctx->wbuf) + pos, (u32) len, sp, (u32) len);
      if (EOK != ret)
        {
          NSPOL_LOGERR ("MEMCPY_S failed");
          return;
        }
    }

  return;
}

/* the Final padding and digest calculation of SHA256 */

/*===========================================================================*\
    Function    :SHA_fin1
    Description :
    Calls       :
    Called by   :
    Return      :void -
    Parameters  :
        unsigned char hval[] -
        SHA256_CTX ctx[1] -
         const unsigned int hlen -
    Note        :
\*===========================================================================*/
NSTACK_STATIC void
SHA_fin1 (u8 hval[], SHA256_CTX ctx[1], const unsigned int hlen)
{
  u32 i = (u32) (ctx->count[0] & SHA256_MASK);

  /* Not unusual shift operation. Checked with purify. */

  /*put bytes in the buffer in an order in which references to */
  /*32-bit words will put bytes with lower addresses into the */
  /*top of 32 bit words on BOTH big and little endian machines */
  bsw_32 (ctx->wbuf, (i + 3) >> 2);

  /*now it is needed to mask valid bytes and add padding which is */
  /*a single 1 bit and as many zero bits as necessary. Note that */
  /*we can always add the first padding byte here because the */
  /*buffer always has at least one empty slot */
  ctx->wbuf[i >> 2] &= (u32) 0xffffff80 << 8 * (~i & 3);
  ctx->wbuf[i >> 2] |= (u32) 0x00000080 << 8 * (~i & 3);

  if (i > SHA256_BLOCK_SIZE - 9)
    {
      if (i < 60)
        {
          ctx->wbuf[15] = 0;
        }

      Sha256_compile__ (ctx);
      i = 0;
    }
  else
    {
      /* compute a word index for the empty buffer positions  */
      i = (i >> 2) + 1;
    }

  while (i < 14)
    {
      /* and zero pad all but last two positions        */
      ctx->wbuf[i++] = 0;
    }

  ctx->wbuf[14] = (ctx->count[1] << 3) | (ctx->count[0] >> 29);
  ctx->wbuf[15] = ctx->count[0] << 3;
  Sha256_compile__ (ctx);

  for (i = 0; i < hlen; ++i)
    {
      hval[i] = (u8) (ctx->hash[i >> 2] >> (8 * (~i & 3)));
    }

  return;
}

/*
Description: Internal data for SHA256 digest calculation
Value Range: None
Access: Used to store internal data for SHA256 digest calculation
Remarks:
*/
static const u32 g_i256[] = {
  0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
  0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

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
void
Sha256_set (SHA256_CTX sha256_ctx[1])
{
  int ret;
  sha256_ctx->count[0] = sha256_ctx->count[1] = 0;

  ret =
    MEMCPY_S (sha256_ctx->hash, sizeof (sha256_ctx->hash), g_i256,
              sizeof (g_i256));
  if (EOK != ret)
    {
      NSPOL_LOGERR ("MEMCPY_S failed");
      return;
    }

  return;
}

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
void
Sha256_fin (SHA256_CTX ctx[1], u8 hval[])
{
  SHA_fin1 (hval, ctx, SHA256_DIGEST_SIZE);

  return;
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
