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

#ifndef __IOPORTS_NODEIDS_H__
#define __IOPORTS_NODEIDS_H__

/* clang-format off */
#define UA_IOPID_IOPORTOBJTYPE 1004 /* ObjectType */
#define UA_IOPID_IOPEVENTTYPE 1005 /* ObjectType */
#define UA_IOPID_IOPSTATEEVENTTYPE 1008 /* ObjectType */
#define UA_IOPID_IOPDIRECTIONEVENTTYPE 1011 /* ObjectType */
#define UA_IOPID_IOPNORMALSTATEEVENTTYPE 1014 /* ObjectType */
#define UA_IOPID_IOPORTDIRECTIONTYPE 3004 /* DataType */
#define UA_IOPID_IOPORTSTATETYPE 3005 /* DataType */
#define UA_IOPID_IOPORTS 5006 /* Object */
#define UA_IOPID_IOPORTOBJTYPE_CONFIGURABLE 6007 /* Variable */
#define UA_IOPID_IOPORTOBJTYPE_DIRECTION 6008 /* Variable */
#define UA_IOPID_IOPORTOBJTYPE_DISABLED 6009 /* Variable */
#define UA_IOPID_IOPORTOBJTYPE_INDEX 6010 /* Variable */
#define UA_IOPID_IOPORTOBJTYPE_NAME 6011 /* Variable */
#define UA_IOPID_IOPORTOBJTYPE_NORMALSTATE 6012 /* Variable */
#define UA_IOPID_IOPORTOBJTYPE_STATE 6013 /* Variable */
#define UA_IOPID_IOPORTOBJTYPE_USAGE 6014 /* Variable */
#define UA_IOPID_IOPORTDIRECTIONTYPE_ENUMSTRINGS 6026 /* Variable */
#define UA_IOPID_IOPORTSTATETYPE_ENUMSTRINGS 6042 /* Variable */
/* clang-format on */

#endif /* __IOPORTS_NODEIDS_H__ */
