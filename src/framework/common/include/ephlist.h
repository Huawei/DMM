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

#ifndef _EPHLIST_H_
#define _EPHLIST_H_

#include <stdio.h>
#include "types.h"
#include "common_mem_pal.h"
#include "common_mem_buf.h"
#include "common_pal_bitwide_adjust.h"
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

struct ep_hlist_node
{
  struct ep_hlist_node *next, **pprev;
};

struct ep_node_list
{
  struct ep_hlist_node *head;
  struct ep_hlist_node *tail;
};

struct ep_hlist
{
  struct ep_hlist_node node;
  struct ep_hlist_node *head;
  struct ep_hlist_node *tail;
};

#define ep_hlist_entry(ptr, type, member) container_of(ptr, type, member)

#define EP_HLIST_INIT_NODE(node) {\
            (node)->next = NULL;\
            (node)->pprev = NULL; \
        }

#define EP_HLIST_INIT(ptr) {\
            EP_HLIST_INIT_NODE(&((ptr)->node)); \
            (ptr)->head = (struct ep_hlist_node*)ADDR_LTOSH_EXT(&((ptr)->node)); \
            (ptr)->tail = (struct ep_hlist_node*)ADDR_LTOSH_EXT(&((ptr)->node)); \
        }

#define EP_HLIST_PREV(ptr) ((struct ep_hlist_node*)(ADDR_SHTOL((ptr)->pprev)))
/* list check may below zero check header, because if app crash before
   do list->size++, it will lead problem */
#define EP_HLIST_EMPTY(list) (NULL == ((struct ep_hlist_node*)ADDR_SHTOL((list)->head))->next)
#define EP_HLIST_NODE_LINKED(node) (!(!(node)->pprev))

static __inline void ep_hlist_del (struct ep_hlist *list,
                                   struct ep_hlist_node *n);
static __inline void ep_hlist_add_tail (struct ep_hlist *list,
                                        struct ep_hlist_node *node);

/*
 *    list , n are local pointer, don't need to cast
 */
static __inline void
ep_hlist_del (struct ep_hlist *list, struct ep_hlist_node *n)
{
  if (!EP_HLIST_NODE_LINKED (n))
    return;
  EP_HLIST_PREV (n)->next = n->next;
  if (n->next)
    {
      ((struct ep_hlist_node *) ADDR_SHTOL (n->next))->pprev = n->pprev;
    }
  else
    {
      list->tail = (struct ep_hlist_node *) (n->pprev);
    }
  EP_HLIST_INIT_NODE (n);
}

/**
 * list, node are local pointer , don't need to case
 */
static __inline void
ep_hlist_add_tail (struct ep_hlist *list, struct ep_hlist_node *node)
{
  struct ep_hlist_node *tail =
    (struct ep_hlist_node *) ADDR_SHTOL (list->tail);
  EP_HLIST_INIT_NODE (node);
  node->pprev = (struct ep_hlist_node **) ADDR_LTOSH_EXT (&tail->next);
  tail->next = (struct ep_hlist_node *) ADDR_LTOSH_EXT (node);
  list->tail = (struct ep_hlist_node *) ADDR_LTOSH_EXT (node);
}

/*#########################################################*/
struct list_node
{
  struct list_node *next;
};

struct ep_list
{
  struct list_node node;
  struct list_node *head;
};

#define ep_list_entry(ptr, type, member) container_of(ptr, type, member)

#define EP_LIST_INIT_NODE(node) {\
            (node)->next = NULL;\
        }

#define EP_LIST_INIT(ptr) {\
            EP_LIST_INIT_NODE(&((ptr)->node)); \
            (ptr)->head = (struct list_node*)ADDR_LTOSH_EXT(&((ptr)->node)); \
        }

#define EP_LIST_EMPTY(list) (NULL == ((struct list_node*)ADDR_SHTOL((list)->head))->next)

static __inline void ep_list_del (struct ep_list *list, struct list_node *n);
static __inline void ep_list_add_tail (struct ep_list *list,
                                       struct list_node *node);

/*
 *    list , n are local pointer, don't need to cast
 */
static __inline void
ep_list_del (struct ep_list *list, struct list_node *n)
{
  if (NULL == n)
    {
      return;
    }

  struct list_node *p_node;
  struct list_node *p_prev = NULL;
  p_node = ((struct list_node *) ADDR_SHTOL (list->head));
  while (NULL != p_node && p_node != n)
    {
      p_prev = p_node;
      p_node = ((struct list_node *) ADDR_SHTOL (p_node->next));
    }

  if (p_node != n || p_prev == NULL)
    {
      return;
    }

  p_prev->next = n->next;

  EP_LIST_INIT_NODE (n);
  return;
}

/**
 * list, node are local pointer , don't need to case
 */
static __inline void
ep_list_add_tail (struct ep_list *list, struct list_node *node)
{

  struct list_node *p_node;
  struct list_node *p_prev = NULL;
  p_node = ((struct list_node *) ADDR_SHTOL (list->head));
  while (NULL != p_node)
    {
      p_prev = p_node;
      p_node = ((struct list_node *) ADDR_SHTOL (p_node->next));
    }

  if (NULL == p_prev)
    {
      return;
    }

  EP_LIST_INIT_NODE (node);
  p_prev->next = (struct list_node *) ADDR_LTOSH_EXT (node);
  return;
}

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* _HLIST_H_ */
