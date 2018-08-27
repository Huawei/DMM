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
#include <string.h>
#include "dmm_rwlock.h"
#include "dmm_segment.h"
#include "nstack_log.h"

#define SECTION_SIZE 64         /* cache line size */

#define FIRST_NAME "FIRST SECTION FOR SEGMENT"
#define LAST_NAME "LAST SECTION FOR FREE HEAD"

#define MEM_ERR(fmt, ...) \
    NS_LOGPID(LOGFW, "DMM-MEM", NSLOG_ERR, fmt, ##__VA_ARGS__)

/*
init create:
                     \--total number
  /--head no align    \   can be used      \--tail no align
 / ___________________/\___________________ \
/\/                                        \/\
__<F>{S}[ ][ ][ ][ ][ ][ ][ ][ ][ ][ ][ ]<L>__
^ \ /\ /\____________   ________________/\ /
|  |  |              \ /                  |
|  |  +--the segment  |                   +--last section(free:0 used:0)
|  +--first section(prev_rel:0 used:2 free:11 req_size:sizeof(dmm_segment))
+--base address

init:    <F>{S}[                               ]<L>
alloc A: <F>{S}[                   ]<A>#########<L>
alloc B: <F>{S}[       ]<B>#########<A>#########<L>
free A:  <F>{S}[       ]<B>#########[          ]<L>
alloc C: <F>{S}[ ]<C>###<B>#########[          ]<L>
*/

typedef struct dmm_section
{
  int prev_rel;
  int used_num;
  int free_num;
  int __flags;                  /* reserved */
  size_t req_size;              /* in bytes */
  int less_rel;                 /* for free list */
  int more_rel;                 /* for free list */
  char name[DMM_MEM_NAME_SIZE];
} __attribute__ ((__aligned__ (SECTION_SIZE))) section_t;
SIZE_OF_TYPE_EQUAL_TO (section_t, SECTION_SIZE);

struct dmm_segment
{
  void *base;                   /* base address(maybe not align) */
  size_t size;                  /* full size */
  section_t *first;             /* aligned 64 */
  section_t *last;              /* last section used to handle free list */
  dmm_rwlock_t lock;
  int total_num;                /* MAX:2147483647 ==> MAX size: 128M-64 */
  int used_num;
  int sec_num;
  size_t used_size;             /* real alloc size bytes */
};

/* calculate segment number, auto align 64, include 1 section_t */
inline static int
CALC_NUM (size_t size)
{
  if (size)
    {
      const size_t MASK = SECTION_SIZE - 1;
      return (size + MASK) / SECTION_SIZE + 1;
    }

  return 2;                     /* if size is 0, then alloc 1 block */
}

inline static int
SEC_REL (const section_t * base, const section_t * sec)
{
  return sec - base;
}

section_t *
REL_SEC (section_t * base, int rel)
{
  return base + rel;
}

inline static int
SEC_INDEX (struct dmm_segment *seg, section_t * sec)
{
  return SEC_REL (seg->first, sec);
}

inline static section_t *
LESS_SEC (section_t * sec)
{
  return sec + sec->less_rel;
}

inline static section_t *
MORE_SEC (section_t * sec)
{
  return sec + sec->more_rel;
}

inline static section_t *
PREV_SEC (section_t * sec)
{
  return sec + sec->prev_rel;
}

inline static section_t *
NEXT_SEC (section_t * sec)
{
  return sec + (sec->free_num + sec->used_num);
}

inline static int
CHECK_ADDR (struct dmm_segment *seg, void *mem)
{
  if (mem < (void *) seg->first)
    return -1;
  if (mem > (void *) seg->last)
    return -1;
  if ((long) mem & (SECTION_SIZE - 1))
    return -1;

  return 0;
}

inline static section_t *
mem_lookup (struct dmm_segment *seg, const char name[DMM_MEM_NAME_SIZE])
{
  section_t *sec;

  /* caller ensures the validity of the name */

  for (sec = seg->last; sec != seg->first; sec = PREV_SEC (sec))
    {
      if (sec->name[0] == 0)
        continue;
      if (0 == strcmp (sec->name, name))
        return sec;
    }

  return NULL;
}

static section_t *
mem_alloc (struct dmm_segment *seg, size_t size)
{
  const int num = CALC_NUM (size);
  section_t *sec, *pos, *next, *less, *more;

  if (num > seg->total_num - seg->used_num)
    {
      /* no enough memory */
      return NULL;
    }

  /* find enough free space */
  pos = seg->last;
  do
    {
      pos = MORE_SEC (pos);
      if (pos == seg->last)
        {
          /* no enough memory */
          return NULL;
        }
    }
  while (num > pos->free_num);

  /* allocate pos pos section tail */

  /* change next section's prev possion */
  next = NEXT_SEC (pos);
  next->prev_rel = -num;

  /* create new section */
  sec = PREV_SEC (next);
  sec->prev_rel = SEC_REL (sec, pos);
  sec->used_num = num;
  sec->req_size = size;
  sec->free_num = 0;            /* no free space */
  sec->less_rel = 0;
  sec->more_rel = 0;
  sec->name[0] = 0;

  /* adjust pos */
  pos->free_num -= num;

  less = LESS_SEC (pos);
  more = MORE_SEC (pos);

  /* remove pos free list */
  less->more_rel = SEC_REL (less, more);
  more->less_rel = SEC_REL (more, less);
  pos->more_rel = 0;
  pos->less_rel = 0;

  /* find position */
  while (less != seg->last)
    {
      if (pos->free_num >= less->free_num)
        break;
      less = LESS_SEC (less);
    }

  /* insert into free list */
  more = MORE_SEC (less);
  less->more_rel = SEC_REL (less, pos);
  more->less_rel = SEC_REL (more, pos);
  pos->more_rel = SEC_REL (pos, more);
  pos->less_rel = SEC_REL (pos, less);

  /* adjust segment */
  seg->used_size += size;
  seg->used_num += num;
  seg->sec_num++;

  /* victory */
  return sec;
}

static void
mem_free (struct dmm_segment *seg, section_t * sec)
{
  const int num = sec->used_num + sec->free_num;
  section_t *next = NEXT_SEC (sec);
  section_t *prev = PREV_SEC (sec);
  section_t *more, *less;

  /* adjust next section's prev */
  next->prev_rel = SEC_REL (next, prev);

  if (sec->free_num)
    {
      /* remove from free list */
      more = MORE_SEC (sec);
      less = LESS_SEC (sec);
      more->less_rel = SEC_REL (more, less);
      less->more_rel = SEC_REL (less, more);
    }

  if (prev->free_num)
    {
      /* remove from free list */
      more = MORE_SEC (prev);
      less = LESS_SEC (prev);
      more->less_rel = SEC_REL (more, less);
      less->more_rel = SEC_REL (less, more);
    }
  else
    {
      more = MORE_SEC (seg->last);
    }

  /* put the space to prev's free space */
  prev->free_num += num;

  while (more != seg->last)
    {
      if (prev->free_num <= more->free_num)
        break;
      more = MORE_SEC (more);
    }
  less = LESS_SEC (more);

  /* insert */
  less->more_rel = SEC_REL (less, prev);
  more->less_rel = SEC_REL (more, prev);
  prev->more_rel = SEC_REL (prev, more);
  prev->less_rel = SEC_REL (prev, less);

  /* adjust segment */
  seg->used_size -= sec->req_size;
  seg->used_num -= sec->used_num;
  seg->sec_num--;
}

void
dmm_seg_dump (struct dmm_segment *seg)
{
  section_t *sec;

  dmm_read_lock (&seg->lock);

  (void) printf ("---- segment:%p  base:%p  size:%lu --------------\n"
                 " first[%d]:%p  last[%d]:%p  total_num:%d  used_num:%d\n"
                 " sec_num:%d  used_size:%lu  use%%:%lu%%  free%%:%lu%%\n",
                 seg, seg->base, seg->size,
                 SEC_INDEX (seg, seg->first), seg->first,
                 SEC_INDEX (seg, seg->last), seg->last,
                 seg->total_num, seg->used_num,
                 seg->sec_num, seg->used_size,
                 seg->used_size * 100 / seg->size,
                 (seg->total_num -
                  seg->used_num) * SECTION_SIZE * 100 / seg->size);

  (void) printf ("----------------------------------------\n"
                 "%18s %9s %9s %9s %9s %10s %9s %9s %s\n",
                 "PHYSICAL-ORDER", "section", "prev_rel", "used_num",
                 "free_num", "req_size", "less_rel", "more_rel", "name");

  sec = seg->first;
  while (1)
    {
      (void) printf ("%18p %9d %9d %9d %9d %10lu %9d %9d '%s'\n",
                     sec, SEC_INDEX (seg, sec),
                     sec->prev_rel, sec->used_num, sec->free_num,
                     sec->req_size, sec->less_rel, sec->more_rel, sec->name);
      if (sec == seg->last)
        break;
      sec = NEXT_SEC (sec);
    }

  (void) printf ("----------------------------------------\n"
                 "%18s %9s %9s\n", "FREE-ORDER", "section", "free_num");
  for (sec = MORE_SEC (seg->last); sec != seg->last; sec = MORE_SEC (sec))
    {
      (void) printf ("%18p %9d %9d\n",
                     sec, SEC_INDEX (seg, sec), sec->free_num);
    }

  (void) printf ("----------------------------------------\n");

  dmm_read_unlock (&seg->lock);
}

inline static int
align_section (void *base, size_t size, section_t ** first)
{
  const long MASK = ((long) SECTION_SIZE - 1);
  const int SEG_NUM = CALC_NUM (sizeof (struct dmm_segment));

  const long align = (long) base;
  const long addr = (align + MASK) & (~MASK);
  const size_t total = (size - (addr - align)) / SECTION_SIZE;

  if (total > 0x7fffFFFF)
    return -1;
  if (total < SEG_NUM + 1)      /* first+segment + last */
    return -1;

  *first = (section_t *) addr;
  return (int) total;
}

struct dmm_segment *
dmm_seg_create (void *base, size_t size)
{
  const int SEG_NUM = CALC_NUM (sizeof (struct dmm_segment));
  section_t *first, *last;
  struct dmm_segment *seg;
  int total = align_section (base, size, &first);

  if (total <= 0)
    return NULL;

  last = first + (total - 1);

  /* first section */
  first->prev_rel = 0;
  first->used_num = SEG_NUM;
  first->req_size = sizeof (struct dmm_segment);
  first->free_num = total - (SEG_NUM + 1);
  first->less_rel = SEC_REL (first, last);
  first->more_rel = SEC_REL (first, last);
  first->name[0] = 0;
  (void) strncpy (&first->name[1], FIRST_NAME, sizeof (first->name) - 1);

  /* last section */
  last->prev_rel = SEC_REL (last, first);
  last->used_num = 0;
  last->req_size = 0;
  last->free_num = 0;
  last->less_rel = SEC_REL (last, first);
  last->more_rel = SEC_REL (last, first);
  last->name[0] = 0;
  (void) strncpy (&last->name[1], LAST_NAME, sizeof (first->name) - 1);

  /* segment */
  seg = (struct dmm_segment *) (first + 1);
  dmm_rwlock_init (&seg->lock);
  seg->base = base;
  seg->size = size;
  seg->first = first;
  seg->last = last;
  seg->total_num = total;
  seg->sec_num = 2;
  seg->used_size = sizeof (struct dmm_segment);
  seg->used_num = first->used_num;

  return seg;
}

struct dmm_segment *
dmm_seg_attach (void *base, size_t size)
{
  section_t *first, *last;
  struct dmm_segment *seg;
  int total = align_section (base, size, &first);

  if (total <= 0)
    return NULL;

  last = first + (total - 1);
  seg = (struct dmm_segment *) (first + 1);

  if (seg->base != base)
    return NULL;
  if (seg->size != size)
    return NULL;
  if (seg->total_num != total)
    return NULL;

  if (seg->first != first)
    return NULL;
  if (first->name[0] != 0)
    return NULL;
  if (strncmp (&first->name[1], FIRST_NAME, sizeof (first->name) - 1))
    return NULL;

  if (seg->last != last)
    return NULL;
  if (last->name[0] != 0)
    return NULL;
  if (strncmp (&last->name[1], LAST_NAME, sizeof (last->name) - 1))
    return NULL;

  return seg;
}

void *
dmm_mem_alloc (struct dmm_segment *seg, size_t size)
{
  section_t *sec;

  dmm_write_lock (&seg->lock);
  sec = mem_alloc (seg, size);
  dmm_write_unlock (&seg->lock);

  return sec ? sec + 1 : NULL;
}

int
dmm_mem_free (struct dmm_segment *seg, void *mem)
{
  if (CHECK_ADDR (seg, mem))
    {
      MEM_ERR ("Invalid address:%p", mem);
      return -1;
    }

  dmm_write_lock (&seg->lock);
  mem_free (seg, ((section_t *) mem) - 1);
  dmm_write_unlock (&seg->lock);

  return 0;
}

void *
dmm_mem_lookup (struct dmm_segment *seg, const char name[DMM_MEM_NAME_SIZE])
{
  section_t *sec;

  if (!name || !name[0])
    return NULL;

  dmm_read_lock (&seg->lock);
  sec = mem_lookup (seg, name);
  dmm_read_unlock (&seg->lock);

  return sec ? sec + 1 : NULL;
}

void *
dmm_mem_map (struct dmm_segment *seg, size_t size,
             const char name[DMM_MEM_NAME_SIZE])
{
  void *mem;
  section_t *sec;

  if (!name || !name[0] || strlen (name) >= DMM_MEM_NAME_SIZE)
    return NULL;

  dmm_write_lock (&seg->lock);

  sec = mem_lookup (seg, name);
  if (sec)
    {
      MEM_ERR ("Map '%s' exist", name);
      mem = NULL;
    }
  else if (!(sec = mem_alloc (seg, size)))
    {
      MEM_ERR ("alloc '%s' failed for size %lu", name, size);
      mem = NULL;
    }
  else
    {
      (void) strncpy (sec->name, name, sizeof (sec->name));
      mem = sec + 1;
    }

  dmm_write_unlock (&seg->lock);

  return mem;
}

int
dmm_mem_unmap (struct dmm_segment *seg, const char name[DMM_MEM_NAME_SIZE])
{
  section_t *sec;

  if (!name || !name[0])
    return -1;

  dmm_write_lock (&seg->lock);

  sec = mem_lookup (seg, name);
  if (sec)
    mem_free (seg, sec);

  dmm_write_unlock (&seg->lock);

  return sec != NULL ? 0 : -1;
}
