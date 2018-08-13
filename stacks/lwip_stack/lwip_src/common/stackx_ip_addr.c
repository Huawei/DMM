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

#include <stdint.h>
#include <stddef.h>
#include "stackx_ip_addr.h"
#include "nstack_log.h"
#include "spl_def.h"

/* Here for now until needed in other places in stackx*/
#ifndef isprint
#define in_range(c, lo, up) ((u8)c >= lo && (u8)c <= up)
#define isprint(c) in_range(c, 0x20, 0x7f)
#define isdigit(c) in_range(c, '0', '9')
#define isxdigit(c) (isdigit(c) || in_range(c, 'a', 'f') || in_range(c, 'A', 'F'))
#define islower(c) in_range(c, 'a', 'z')
#define isspace(c) (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v')
#endif

/**
 * Ascii internet address interpretation routine.
 * The value returned is in network order.
 *
 * @param cp IPaddress in ascii represenation (e.g. "127.0.0.1")
 * @return ipaddress in network order
 */
u32
spl_ipaddr_addr (const char *cp)
{
  spl_ip_addr_t val;

  if (spl_ipaddr_aton (cp, &val))
    {
      return ip4_addr_get_u32 (&val);
    }

  return (SPL_IPADDR_NONE);
}

/**
 * Check whether "cp" is a valid ascii representation
 * of an Internet address and convert to a binary address.
 * Returns 1 if the address is valid, 0 if not.
 * This replaces inet_addr, the return value from which
 * cannot distinguish between failure and a local broad cast address.
 *
 * @param cp IPaddress in ascii represenation (e.g. "127.0.0.1")
 * @param addr pointer to which to save the ipaddress in network order
 * @return 1 if cp could be converted to addr, 0 on failure
 */
int
spl_ipaddr_aton (const char *cp, spl_ip_addr_t * addr)
{
  u32 val;
  u8 base;
  char c;
  u32 parts[4];
  u32 *pp = parts;

  if (cp == NULL)
    {
      return 0;
    }

  c = *cp;
  for (;;)
    {
      /*
       * Get number up to ``.''.
       * Values are specified as for C:
       * 0x=hex, 0=octal, 1-9=decimal.
       */
      if (!isdigit (c))
        {
          return (0);
        }

      val = 0;
      base = 10;
      if (c == '0')
        {
          c = *++cp;
          if ((c == 'x') || (c == 'X'))
            {
              base = 16;
              c = *++cp;
            }
          else
            {
              base = 8;
            }
        }

      for (;;)
        {
          if (isdigit (c))
            {
              val = (val * base) + (int) (c - '0');
              c = *++cp;
            }
          else if ((base == 16) && isxdigit (c))
            {
              val = (val << 4) | (int) (c + 10 - (islower (c) ? 'a' : 'A'));
              c = *++cp;
            }
          else
            {
              break;
            }
        }

      if (c == '.')
        {
          /*
           * Internet format:
           *  a.b.c.d
           *  a.b.c   (with c treated as 16 bits)
           *  a.b (with b treated as 24 bits)
           */
          if (pp >= parts + 3)
            {
              return (0);
            }

          *pp++ = val;
          c = *++cp;
        }
      else
        {
          break;
        }
    }

  /*
   * Check for trailing characters.
   */
  if ((c != '\0') && !isspace (c))
    {
      return (0);
    }

  /*
   * Concoct the address according to
   * the number of parts specified.
   */
  switch (pp - parts + 1)
    {
    case 0:
      return (0);               /* initial nondigit */

    case 1:                    /* a -- 32 bits */
      break;

    case 2:                    /* a.b -- 8.24 bits */
      if (val > 0xffffffUL)
        {
          return (0);
        }

      val |= parts[0] << 24;
      break;

    case 3:                    /* a.b.c -- 8.8.16 bits */
      if (val > 0xffff)
        {
          return (0);
        }

      val |= (parts[0] << 24) | (parts[1] << 16);
      break;

    case 4:                    /* a.b.c.d -- 8.8.8.8 bits */
      if (val > 0xff)
        {
          return (0);
        }

      val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
      break;
    default:
      NSPOL_LOGERR ("unhandled");

      return (0);
    }

  if (addr)
    {
      ip4_addr_set_u32 (addr, spl_htonl (val));
    }

  return (1);
}

/**
 * Convert numeric IPaddress into decimal dotted ASCII representation.
 * returns ptr to static buffer; not reentrant!
 *
 * @param addr ipaddress in network order to convert
 * @return pointer to a global static (!) buffer that holds the ASCII
 *         represenation of addr
 */
char *
spl_ipaddr_ntoa (const spl_ip_addr_t * addr)
{
  static char str[16];

  return spl_ipaddr_ntoa_r (addr, str, 16);
}

/**
 * Same as spl_ipaddr_ntoa, but reentrant since a user-supplied buffer is used.
 *
 * @param addr ipaddress in network order to convert
 * @param buf target buffer where the string is stored
 * @param buflen length of buf
 * @return either pointer to buf which now holds the ASCII
 *         representation of addr or NULL if buf was too small
 */
char *
spl_ipaddr_ntoa_r (const spl_ip_addr_t * addr, char *buf, int buflen)
{
  u32 s_addr;
  char inv[3];
  char *rp;
  u8 *ap;
  u8 rem;
  u8 n;
  u8 i;
  int len = 0;

  s_addr = ip4_addr_get_u32 (addr);

  rp = buf;
  ap = (u8 *) & s_addr;
  for (n = 0; n < 4; n++)
    {
      i = 0;
      do
        {
          rem = *ap % (u8) 10;
          *ap /= (u8) 10;
          inv[i++] = '0' + rem;
        }
      while (*ap);

      while (i--)
        {
          if (len++ >= buflen)
            {
              return NULL;
            }

          *rp++ = inv[i];
        }

      if (len++ >= buflen)
        {
          return NULL;
        }

      *rp++ = '.';
      ap++;
    }

  *--rp = 0;
  return buf;
}
