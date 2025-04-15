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

#include <glib.h>
#include <open62541/server.h>

#include "hello_world_plugin.h"
#include "error.h"
#include "log.h"

#define UA_PLUGIN_NAMESPACE "http://www.axis.com/OpcUA/HelloWorld/"
#define UA_PLUGIN_NAME      "opc-hello-world-plugin"
#define UA_DISPLAY_NAME     "HelloWorldNode"
#define UA_DESCRIPTION      "Hello World Node"
#define UA_VALUE            "Hello World!"

#define ERR_NOT_INITIALIZED "The " UA_PLUGIN_NAME " is not initialized"
#define ERR_NO_NAME         "The " UA_PLUGIN_NAME " was not given a name"

DEFINE_GQUARK(UA_PLUGIN_NAME)

typedef struct plugin {
  /* user-friendly name of the plugin */
  gchar *name;
  /* OPC-UA namespace index */
  UA_UInt16 ns;
  /* an open62541 logger */
  UA_Logger *logger;
} plugin_t;

static plugin_t *plugin;

/* Local functions */
static gboolean
add_hello_world_node(UA_Server *server, GError **err)
{
  UA_StatusCode status;
  UA_String value = UA_STRING(UA_VALUE);
  UA_VariableAttributes attr = UA_VariableAttributes_default;

  g_assert(server != NULL);
  g_assert(err == NULL || *err == NULL);

  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_STRING]);

  attr.displayName = UA_LOCALIZEDTEXT("en-US", UA_DISPLAY_NAME);
  attr.description = UA_LOCALIZEDTEXT("en-US", UA_DESCRIPTION);

  status = UA_Server_addVariableNode(
          server,
          UA_NODEID_STRING(plugin->ns, UA_DISPLAY_NAME),
          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
          UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
          UA_QUALIFIEDNAME(plugin->ns, UA_DISPLAY_NAME),
          UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
          attr,
          NULL,
          NULL);
  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "Failed to add variable node HelloWorldNode: %s",
              UA_StatusCode_name(status));
    return FALSE;
  }

  return TRUE;
}

static void
plugin_cleanup(void)
{
  g_assert(plugin != NULL);

  plugin->logger = NULL;
  g_clear_pointer(&plugin->name, g_free);
  g_slice_free(plugin_t, plugin);
}

/* Exported functions */
gboolean
opc_ua_create(UA_Server *server,
              UA_Logger *logger,
              G_GNUC_UNUSED gpointer *params,
              GError **err)
{
  g_return_val_if_fail(server != NULL, FALSE);
  g_return_val_if_fail(logger != NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  if (plugin != NULL) {
    return TRUE;
  }

  plugin = g_slice_new0(plugin_t);

  plugin->name = g_strdup(UA_PLUGIN_NAME);
  plugin->logger = logger;

  plugin->ns = UA_Server_addNamespace(server, UA_PLUGIN_NAMESPACE);

  if (!add_hello_world_node(server, err)) {
    g_prefix_error(err, "add_hello_world_node() failed: ");
    return FALSE;
  }

  return TRUE;
}

void
opc_ua_destroy(void)
{
  if (plugin == NULL) {
    return;
  }

  plugin_cleanup();
}

const gchar *
opc_ua_get_plugin_name(void)
{
  if (plugin == NULL) {
    return ERR_NOT_INITIALIZED;
  } else if (plugin->name == NULL) {
    return ERR_NO_NAME;
  }

  return plugin->name;
}
