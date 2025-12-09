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

#include <curl/curl.h>
#include <gio/gio.h>
#include <glib.h>
#include <jansson.h>
#include <open62541/server.h>

#include "bdi_plugin.h"
#include "error.h"
#include "log.h"
#include "ua_utils.h"
#include "vapix_utils.h"

#define UA_PLUGIN_NAMESPACE     "http://www.axis.com/OpcUA/BasicDeviceInformation/"
#define UA_PLUGIN_NAME          "opc-bdi-plugin"
#define UA_BDI_OBJ_DISPLAY_NAME "BasicDeviceInfo"
#define UA_BDI_OBJ_DESCRIPTION  "BasicDeviceInfo"

#define ERR_NOT_INITIALIZED "The " UA_PLUGIN_NAME " is not initialized"
#define ERR_NO_NAME         "The " UA_PLUGIN_NAME " was not given a name"

#define BASIC_DEVICE_INFO_CGI_ENDPOINT "basicdeviceinfo.cgi"

DEFINE_GQUARK(UA_PLUGIN_NAME)

typedef struct plugin {
  /* user-friendly name of the plugin */
  gchar *name;
  /* OPC-UA namespace index */
  UA_UInt16 ns;
  /* an open62541 logger */
  UA_Logger *logger;
  /* keep track of data that needs to be rolled back in case of failure */
  rollback_data_t *rbd;
} plugin_t;

static plugin_t *plugin;

static gboolean
vapix_get_basic_device_information(GHashTable **bdi_hashtable, GError **err)
{
  gboolean retval = FALSE;
  CURL *curl_h;
  g_autofree gchar *credentials = NULL;
  g_autofree gchar *response = NULL;
  json_error_t parse_error;
  json_t *json_response = NULL;
  const gchar *key;
  json_t *value;
  json_t *data;
  json_t *prop_list;
  const gchar *request = "{"
                         "  \"apiVersion\": \"1.3\","
                         "  \"method\": \"getAllProperties\""
                         "}";

  g_assert(bdi_hashtable != NULL);
  g_assert(err == NULL || *err == NULL);

  curl_h = curl_easy_init();
  if (curl_h == NULL) {
    SET_ERROR(err, -1, "curl_easy_init failed");
    goto out;
  }

  credentials = vapix_get_credentials("vapix-basicdeviceinfo-user", err);
  if (credentials == NULL) {
    g_prefix_error(err, "Failed to get the VAPIX credentials: ");
    goto out;
  }

  response = vapix_request(curl_h,
                           credentials,
                           BASIC_DEVICE_INFO_CGI_ENDPOINT,
                           HTTP_POST,
                           JSON_data,
                           request,
                           err);
  if (response == NULL) {
    g_prefix_error(err, "Failed to get the basic device information: ");
    goto out;
  }

  json_response = json_loads(response, 0, &parse_error);

  if (json_response == NULL) {
    SET_ERROR(err, -1, "Invalid JSON response: %s", parse_error.text);
    goto out;
  }

  data = json_object_get(json_response, "data");

  if (data == NULL) {
    SET_ERROR(err, -1, "No property called 'data' in response");
    goto out;
  }

  prop_list = json_object_get(data, "propertyList");

  if (prop_list == NULL) {
    SET_ERROR(err, -1, "No property called 'propertyList' in response");
    goto out;
  }

  /* allocate a hash table and populate it with the data we got over vapix */
  *bdi_hashtable =
          g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  json_object_foreach(prop_list, key, value)
  {
    if (key && value) {
      LOG_D(plugin->logger,
            "got key: %s, value: %s",
            key,
            json_string_value(value));
      g_hash_table_insert(*bdi_hashtable,
                          g_strdup(key),
                          g_strdup(json_string_value(value)));
    }
  }

  retval = TRUE;

out:
  g_clear_pointer(&response, g_free);
  g_clear_pointer(&json_response, json_decref);

  if (curl_h != NULL) {
    curl_easy_cleanup(curl_h);
  }

  return retval;
}

static gboolean
add_variable_to_object(UA_Server *server,
                       UA_NodeId parent,
                       gchar *name,
                       gchar *value,
                       GError **err)
{
  UA_StatusCode retval;
  UA_VariableAttributes attr = UA_VariableAttributes_default;
  UA_String ua_value;

  g_assert(server != NULL);
  g_assert(name != NULL);
  g_assert(value != NULL);
  g_assert(err == NULL || *err == NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->rbd != NULL);

  attr.accessLevel = UA_ACCESSLEVELMASK_READ;
  ua_value = UA_STRING(value);
  UA_Variant_setScalar(&attr.value, &ua_value, &UA_TYPES[UA_TYPES_STRING]);

  attr.displayName = UA_LOCALIZEDTEXT("en-US", name);
  attr.description = UA_LOCALIZEDTEXT("en-US", name);

  retval =
          UA_Server_addVariableNode_rb(server,
                                       UA_NODEID_NUMERIC(plugin->ns, 0),
                                       parent,
                                       UA_NODEID_NUMERIC(0,
                                                         UA_NS0ID_HASPROPERTY),
                                       UA_QUALIFIEDNAME(plugin->ns, name),
                                       UA_NODEID_NUMERIC(0,
                                                         UA_NS0ID_PROPERTYTYPE),
                                       attr,
                                       NULL,
                                       plugin->rbd,
                                       NULL);
  if (retval != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "UA_Server_addVariableNode_rb() failed: %s",
              UA_StatusCode_name(retval));
    return FALSE;
  }

  return TRUE;
}

static gboolean
add_bdi_object(UA_Server *server, UA_NodeId *outId, GError **err)
{
  UA_StatusCode status;
  UA_ObjectAttributes attr = UA_ObjectAttributes_default;

  g_assert(plugin != NULL);
  g_assert(plugin->rbd != NULL);
  g_assert(server != NULL);
  g_assert(outId != NULL);
  g_assert(err == NULL || *err == NULL);

  attr.displayName = UA_LOCALIZEDTEXT("en-US", UA_BDI_OBJ_DISPLAY_NAME);
  attr.description = UA_LOCALIZEDTEXT("en-US", UA_BDI_OBJ_DESCRIPTION);

  status =
          UA_Server_addObjectNode_rb(server,
                                     UA_NODEID_NUMERIC(plugin->ns, 0),
                                     UA_NODEID_NUMERIC(0,
                                                       UA_NS0ID_OBJECTSFOLDER),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                     UA_QUALIFIEDNAME(plugin->ns,
                                                      UA_BDI_OBJ_DISPLAY_NAME),
                                     UA_NODEID_NUMERIC(0,
                                                       UA_NS0ID_BASEOBJECTTYPE),
                                     attr,
                                     NULL,
                                     plugin->rbd,
                                     outId);

  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "Failed to add object node BasicDeviceInfo: %s",
              UA_StatusCode_name(status));
    return FALSE;
  }

  return TRUE;
}

static gboolean
add_basic_device_info_data(UA_Server *server, UA_NodeId bdiNode, GError **err)
{
  GHashTable *bdi_hashtable = NULL;
  GHashTableIter ht_iter;
  gpointer key;
  gpointer value;
  gboolean retval = FALSE;

  g_assert(server != NULL);
  g_assert(plugin != NULL);
  g_assert(err == NULL || *err == NULL);

  /* Fetch the available basic device information over vapix placing the
   * result in a hash table */
  if (!vapix_get_basic_device_information(&bdi_hashtable, err)) {
    g_prefix_error(err, "vapix_get_basic_device_information() failed: ");
    goto err_out;
  }

  if (bdi_hashtable == NULL) {
    SET_ERROR(err, -1, "vapix_get_basic_device_information(): emtpy result!");
    goto err_out;
  }

  LOG_D(plugin->logger,
        "### BasicDeviceInfo entries: %d",
        g_hash_table_size(bdi_hashtable));

  /* Iterate over the 'bdi_hashtable' and update the OPC-UA information model
   * with the actual values. */
  g_hash_table_iter_init(&ht_iter, bdi_hashtable);
  while (g_hash_table_iter_next(&ht_iter, &key, &value)) {
    if (!add_variable_to_object(server,
                                bdiNode,
                                (gchar *) key,
                                (gchar *) value,
                                err)) {
      g_prefix_error(err, "add_variable_to_object() failed: ");
      goto err_out;
    }
  }

  retval = TRUE;

err_out:
  g_hash_table_unref(bdi_hashtable);

  return retval;
}

static void
plugin_cleanup(void)
{
  g_assert(plugin != NULL);

  plugin->logger = NULL;
  g_clear_pointer(&plugin->name, g_free);

  /* free up allocated rollback data, if any */
  ua_utils_clear_rbd(&plugin->rbd);

  g_clear_pointer(&plugin, g_free);
}

/* Exported functions */
gboolean
opc_ua_create(UA_Server *server,
              UA_Logger *logger,
              G_GNUC_UNUSED gpointer *params,
              GError **err)
{
  GError *lerr = NULL;
  UA_NodeId bdi_node;

  g_return_val_if_fail(server != NULL, FALSE);
  g_return_val_if_fail(logger != NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  if (plugin != NULL) {
    return TRUE;
  }

  plugin = g_new0(plugin_t, 1);

  plugin->name = g_strdup(UA_PLUGIN_NAME);
  plugin->logger = logger;
  plugin->rbd = g_new0(rollback_data_t, 1);

  plugin->ns = UA_Server_addNamespace(server, UA_PLUGIN_NAMESPACE);

  /* add a bdi object to the opc-ua server */
  if (!add_bdi_object(server, &bdi_node, err)) {
    g_prefix_error(err, "add_bdi_object() failed: ");
    goto err_out;
  }

  /* add the actual values to the nodes */
  if (!add_basic_device_info_data(server, bdi_node, err)) {
    g_prefix_error(err, "add_basic_device_info_data() failed: ");
    goto err_out;
  }

  /* the information model was successfully populated so now we can free up our
   * rollback data since we no longer need it */
  ua_utils_clear_rbd(&plugin->rbd);

  return TRUE;

err_out:
  if (!ua_utils_do_rollback(server, plugin->rbd, &lerr)) {
    LOG_E(plugin->logger,
          "ua_utils_do_rollback() failed: %s",
          GERROR_MSG(lerr));
    g_clear_error(&lerr);
  }

  plugin_cleanup();

  return FALSE;
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
