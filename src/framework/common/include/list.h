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

#ifndef LIST_H_
#define LIST_H_

#include <signal.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "types.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

struct list_head
{
  union
  {
    struct list_head *next;
  };

  union
  {
    struct list_head *prev;
  };
};

struct hlist_node
{
    /**
     * @pprev: point the previous node's next pointer
     */
  union
  {
    struct hlist_node *next;
  };

  union
  {
    struct hlist_node **pprev;
  };
};

struct hlist_head
{
  struct hlist_node *first;
};

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define list_for_each_entry_type(tpos, typeof_tpos,pos, head, member) \
    for (pos = ((head)->next); \
         pos && pos != (head) && ({tpos = list_entry(pos, typeof_tpos, member); 1;}); \
         pos = ((pos)->next))

#define LINT_LIST()

#define list_for_each_entry(tpos, pos, head, member) \
    for (pos = ((head)->next); \
         pos && pos != (head) && ({tpos = list_entry(pos, typeof(*tpos), member); 1;}); \
         pos = ((pos)->next))

#define list_for_each_entry_list_head(tpos, pos, head, member) \
    for (pos = (struct list_head *)((head)->next); \
         pos && pos != (struct list_head *)(head) && ({tpos = list_entry(pos, typeof(*tpos), member); 1;}); \
         pos = (pos)->next)
#define list_for_each_safe_entry(tpos, pos, n, head, member) \
    for (pos = (head)->next,n = pos->next; \
         pos && pos != (head) && ({tpos = list_entry(pos, typeof(*tpos), member); 1;}); \
         pos = n,n = (pos)->next)

#define INIT_LIST_HEAD(list) {(list)->next = (list); (list)->prev = (list);}

/*
 * @head: the list to test.
 */
inline void list_add (struct list_head *newp, struct list_head *head);
inline void list_link (struct list_head *newhead, struct list_head *head);
inline void list_add_tail (struct list_head *newp, struct list_head *head);
inline int list_empty (const struct list_head *head);
inline void list_del (struct list_head *entry);
inline struct list_head *list_get_first (struct list_head *head);
inline void hlist_del_init (struct hlist_node *n);

struct hlist_tail
{
  struct hlist_node *end;
};

struct hlist_ctl
{
  struct hlist_head head;
  struct hlist_tail tail;
};
#define INIT_HLIST_CTRL(ptr) {(ptr)->head.first = NULL; (ptr)->tail.end = NULL;}

inline int hlist_empty (const struct hlist_head *h);
inline void hlist_add_head (struct hlist_node *n, struct hlist_head *h);

#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
#define INIT_HLIST_NODE(ptr) {(ptr)->next = NULL; (ptr)->pprev = NULL;}
#define hlist_entry(ptr, type, member) \
    container_of(ptr, type, member)

/***
 * hlist_for_each_entry - iterate over list of given type
 * @member: the name of the hlist_node within the struct.
 * @head:   the head for your list.
 * @pos:    the &struct hlist_node to use as a loop cursor.
 * @tpos:   the type * to use as a loop cursor.
 */
#define hlist_for_each_entry(tpos, pos, head, member) \
    for (pos = (head)->first; \
         pos && ({tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->next)

/**
 * hlist_for_each_entry_type - iterate over list of given type
 * @member: the name of the hlist_node within the struct.
 * @head:   the head for your list.
 * @pos:    the &struct hlist_node to use as a loop cursor.
 * @tpos:   the type * to use as a loop cursor.
 */
#define hlist_for_each_entry_type(tpos, typeof_tpos,pos, head, member) \
        for (pos = (head)->first; \
             pos && ({tpos = hlist_entry(pos, typeof_tpos, member); 1;}); \
             pos = pos->next)

inline void hlist_del_init (struct hlist_node *n);

/**
 * next must be != NULL
 * add n node before next node
 *
 * @n: new node
 * @next: node in the hlist
 */
inline void hlist_add_before (struct hlist_node *n, struct hlist_node *next);

/**
 * next must be != NULL
 * add n node after next node
 * actual behavior is add after n
 * @n: node in the hlist
 * @next: new node
 */
inline void hlist_add_after (struct hlist_node *n, struct hlist_node *next);

/* add after the head */
inline void hlist_add_head (struct hlist_node *n, struct hlist_head *h);

inline int hlist_unhashed (const struct hlist_node *h);

inline int hlist_empty (const struct hlist_head *h);

#ifdef __cplusplus
/* *INDENT-OFF* */
};
/* *INDENT-ON* */
#endif

#endif /* HASH_H_ */
