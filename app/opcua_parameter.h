/**
 * Copyright (C), Axis Communications AB, Lund, Sweden
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
