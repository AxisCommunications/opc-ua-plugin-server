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

#ifndef __OPCUA_PARAMETER_H__
#define __OPCUA_PARAMETER_H__

#include <glib.h>
#include <open62541/plugin/log.h>
#include <open62541/types.h>

#include "opcua_server.h"

#define LOG_LEVEL_MIN 0
#define LOG_LEVEL_MAX 4

#define MIN_PORT 1024
#define MAX_PORT 65535

/**
 * init_ua_parameters:
 * @ctx: application context
 * @app_name: name of the application (must match `appName` in `manifest.json`)
 * @err: return location for a #GError
 *
 * Reads in the application parameters defined in the `paramConfig` section of
 * the `manifest.json` file and initializes runtime variables accordingly.
 *
 * Returns: TRUE if setup was successful, FALSE otherwise.
 */
gboolean
init_ua_parameters(app_context_t *ctx, const gchar *app_name, GError **err);

#endif /* __OPCUA_PARAMETER_H__ */
