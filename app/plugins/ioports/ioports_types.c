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

#include "ioports_nodeids.h"
#include "ioports_types.h"

/* IOPortDirectionType */
#define IOPortDirectionType_members NULL

/* IOPortStateType */
#define IOPortStateType_members NULL

/* clang-format off */
UA_DataType UA_TYPES_IOP[UA_TYPES_IOP_COUNT] = {
  /* IOPortDirectionType */
  {
   UA_TYPENAME("IOPortDirectionType")               /* .typeName */
          { 0, UA_NODEIDTYPE_NUMERIC, { UA_IOPID_IOPORTDIRECTIONTYPE } },
                                                    /* .typeId */
          { 0, UA_NODEIDTYPE_NUMERIC, { 0 } },      /* .binaryEncodingId */
          sizeof(UA_IOPortDirectionType),           /* .memSize */
          UA_DATATYPEKIND_ENUM,                     /* .typeKind */
          true,                                     /* .pointerFree */
          UA_BINARY_OVERLAYABLE_INTEGER,            /* .overlayable */
          0,                                        /* .membersSize */
          IOPortDirectionType_members               /* .members */
  },
 /* IOPortStateType */
  {
   UA_TYPENAME("IOPortStateType")                   /* .typeName */
          { 0, UA_NODEIDTYPE_NUMERIC, { UA_IOPID_IOPORTSTATETYPE } },
                                                    /* .typeId */
          { 0, UA_NODEIDTYPE_NUMERIC, { 0 } },      /* .binaryEncodingId */
          sizeof(UA_IOPortStateType),               /* .memSize */
          UA_DATATYPEKIND_ENUM,                     /* .typeKind */
          true,                                     /* .pointerFree */
          UA_BINARY_OVERLAYABLE_INTEGER,            /* .overlayable */
          0,                                        /* .membersSize */
          IOPortStateType_members                   /* .members */
  },
};
/* clang-format on */
