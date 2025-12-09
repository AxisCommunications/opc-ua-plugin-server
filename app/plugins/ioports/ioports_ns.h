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

#ifndef __IOPORTS_NS_H__
#define __IOPORTS_NS_H__

#include <open62541/server.h>

#include "ioports_types.h"
#include "ua_utils.h"

#define UA_PLUGIN_NAMESPACE "http://www.axis.com/OpcUA/IOPorts/"

#define IOP_OBJ_NR_PROPS 8
/* I/O port: object properties */
#define CONFIGURABLE_BNAME "Configurable"
#define DIRECTION_BNAME    "Direction"
#define DISABLED_BNAME     "Disabled"
#define INDEX_BNAME        "Index"
#define NAME_BNAME         "Name"
#define NORMALSTATE_BNAME  "NormalState"
#define STATE_BNAME        "State"
#define USAGE_BNAME        "Usage"

/* adds the ioports namespace to the information model */
UA_StatusCode
ioports_ns(UA_Server *server, rollback_data_t *rbd);

#endif /* __IOPORTS_NS_H__ */
