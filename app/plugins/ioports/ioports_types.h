/**
 * MIT License
 *
 * Copyright (c) 2025 Axis Communications AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __IOPORTS_TYPES_H__
#define __IOPORTS_TYPES_H__

#include <open62541/types.h>

/**
 * Every type is assigned an index in an array containing the type descriptions.
 * These descriptions are used during type handling (copying, deletion,
 * binary encoding, ...). */
#define UA_TYPES_IOP_COUNT 2
extern UA_EXPORT UA_DataType UA_TYPES_IOP[UA_TYPES_IOP_COUNT];

/* IOPortDirectionType */
typedef enum {
  UA_IOPORTDIRECTIONTYPE_INPUT = 0,
  UA_IOPORTDIRECTIONTYPE_OUTPUT = 1,
  __UA_IOPORTDIRECTIONTYPE_FORCE32BIT = 0x7fffffff
} UA_IOPortDirectionType;

UA_STATIC_ASSERT(sizeof(UA_IOPortDirectionType) == sizeof(UA_Int32),
                 enum_must_be_32bit);

#define UA_TYPES_IOP_IOPORTDIRECTIONTYPE 0

/* IOPortStateType */
typedef enum {
  UA_IOPORTSTATETYPE_OPEN = 0,
  UA_IOPORTSTATETYPE_CLOSED = 1,
  __UA_IOPORTSTATETYPE_FORCE32BIT = 0x7fffffff
} UA_IOPortStateType;

UA_STATIC_ASSERT(sizeof(UA_IOPortStateType) == sizeof(UA_Int32),
                 enum_must_be_32bit);

#define UA_TYPES_IOP_IOPORTSTATETYPE 1

#define UA_TYPE_IOP_STATETYPE_NAME "IOPortStateType"
#define UA_TYPE_IOP_DIRTYPE_NAME   "IOPortDirectionType"

#endif /* __IOPORTS_TYPES_H__ */
