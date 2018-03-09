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

#ifndef COMPILING_CHECK_H
#define COMPILING_CHECK_H

/* protect the value of macro whose value impacts the version
 * compatibility, can't be changed!!!  */
#define    COMPAT_PROTECT(name, value) \
static inline char value_of_##name##_equal_to() \
{ \
    char __dummy1[(name) - (value)]; \
    char __dummy2[(value) - (name)]; \
    return __dummy1[-1] + __dummy2[-1]; \
}

/* check whether struct size is equal to a special value */
#define    SIZE_OF_TYPE_EQUAL_TO(type, size) \
static inline char size_of_##type##_equal_to_##size() \
{ \
    char __dummy1[sizeof(type) - size]; \
    char __dummy2[size - sizeof(type)]; \
    return __dummy1[-1] + __dummy2[-1]; \
}

/* check whether struct size is not equal to a special value */
#define    SIZE_OF_TYPE_UNEQUAL_TO(type, size) \
static inline char size_of_##type##_unequal_to_##size() \
{ \
    char __dummy1[0==(10/(sizeof(type)-size))]; \
    return __dummy1[-1]; \
}

/* check whether struct size is not larger than a special value */
#define    SIZE_OF_TYPE_NOT_LARGER_THAN(type, size)  \
static inline char size_of_##type##_not_larger_than_##size() \
{ \
    char __dummy1[size - sizeof(type)]; \
    return __dummy1[-1]; \
}

/* check whether struct size + sizeof(void*) is not larger than a special value */
/* reserve 8 bytes for 64 bits pointers */
#define    SIZE_OF_TYPE_PLUS8_NOT_LARGER_THAN(type, size)  \
static inline char size_of_##type##_not_larger_than_##size() \
{ \
    char __dummy1[size - sizeof(type) - sizeof(void*)]; \
    return __dummy1[-1]; \
}

/* check whether struct size is not smaller than a special value  */
#define    SIZE_OF_TYPE_NOT_SMALLER_THAN(type, size) \
static inline char size_of_##type##_not_smaller_than_##size() \
{ \
    char __dummy1[sizeof(type) - size]; \
    return __dummy1[-1]; \
}

/* check whether struct size is smaller than a special value */
#define    SIZE_OF_TYPE_SMALLER_THAN(type, size) \
    SIZE_OF_TYPE_NOT_LARGER_THAN(type, size) \
    SIZE_OF_TYPE_UNEQUAL_TO(type, size)

/* check whether struct size is larger than a special value */
#define    SIZE_OF_TYPE_LARGER_THAN(type, size) \
    SIZE_OF_TYPE_NOT_SMALLER_THAN(type, size) \
    SIZE_OF_TYPE_UNEQUAL_TO(type, size)

/* check whether struct size is smaller than a special value, version 2 */
#define    SIZE_OF_TYPE_SMALLER_THAN2(type, size) \
static inline char size_of_##type##_smaller_than2_##size() \
{ \
    char __dummy1[size - sizeof(type) - 1]; \
    return __dummy1[-1]; \
}

/* check whether struct size is larger than a special value, version 2 */
#define    SIZE_OF_TYPE_LARGER_THAN2(type, size) \
static inline char size_of_##type##_larger_than2_##size() \
{ \
    char __dummy1[sizeof(type) - size - 1]; \
    return __dummy1[-1]; \
}

/* check whether struct size is equal to an integer multiple of a special value  */
#define    SIZE_OF_TYPE_IS_MULTIPLE_OF(type, size) \
static inline char size_of_##type##_is_multiple_of_##size() \
{ \
    char __dummy1[0 - (sizeof(type) % size)]; \
    return __dummy1[-1]; \
}

#endif /*compiling_check.h */
