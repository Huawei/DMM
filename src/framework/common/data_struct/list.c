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

#include "list.h"

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
inline int
list_empty (const struct list_head *head)
{
  return head->next == head;
}

inline void
list_del (struct list_head *entry)
{
  if (entry->prev == NULL || entry->next == NULL)
    {
      return;
    }
  entry->next->prev = entry->prev;
  entry->prev->next = entry->next;
  entry->next = NULL;
  entry->prev = NULL;
}

/*get the first element of the list, need to check if list empty or not before calling this.*/
inline struct list_head *
list_get_first (struct list_head *head)
{
  return head->next;
}

inline void
list_add (struct list_head *newp, struct list_head *head)
{
  head->next->prev = newp;
  newp->next = head->next;
  newp->prev = head;
  head->next = newp;
}

inline void
list_link (struct list_head *newhead, struct list_head *head)
{
  struct list_head *tmp;

  newhead->prev->next = head;
  head->prev->next = newhead;

  tmp = newhead->prev;
  newhead->prev = head->prev;
  head->prev = tmp;
}

inline void
list_add_tail (struct list_head *newp, struct list_head *head)
{
  list_add (newp, head->prev);
}

inline void
hlist_del_init (struct hlist_node *n)
{
  struct hlist_node *next = n->next;
  struct hlist_node **pprev = n->pprev;

  if (pprev == NULL && next == NULL)
    {
      return;
    }

  if (pprev)
    {
      *pprev = next;
    }

  if (next)
    {
      next->pprev = pprev;
    }

  n->next = NULL;
  n->pprev = NULL;
}

/**
 * next must be != NULL
 * add n node before next node
 *
 * @n: new node
 * @next: node in the hlist
 */
inline void
hlist_add_before (struct hlist_node *n, struct hlist_node *next)
{
  n->pprev = next->pprev;
  n->next = next;
  next->pprev = &n->next;
  *(n->pprev) = n;
}

/**
 * next must be != NULL
 * add n node after next node
 *  actual behavior is add after n
 * @n: node in the hlist
 * @next: new node
 */
inline void
hlist_add_after (struct hlist_node *n, struct hlist_node *next)
{
  next->next = n->next;
  n->next = next;
  next->pprev = &n->next;
  if (next->next)
    {
      next->next->pprev = &next->next;
    }
}

/* add after the head */
inline void
hlist_add_head (struct hlist_node *n, struct hlist_head *h)
{
  struct hlist_node *first = h->first;

  n->next = first;
  if (first)
    {
      first->pprev = &n->next;
    }

  h->first = n;
  n->pprev = &h->first;
}

inline int
hlist_unhashed (const struct hlist_node *h)
{
  return !h->pprev;
}

inline int
hlist_empty (const struct hlist_head *h)
{
  return !h->first;
}
