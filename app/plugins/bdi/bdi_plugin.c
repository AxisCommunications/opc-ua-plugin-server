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

#define UA_PLUGIN_NAMESPACE     "http://www.axis.com/OpcUA/BasicDeviceInformation/"
#define UA_PLUGIN_NAME          "opc-bdi-plugin"
#define UA_BDI_OBJ_DISPLAY_NAME "BasicDeviceInfo"
#define UA_BDI_OBJ_DESCRIPTION  "BasicDeviceInfo"

#define ERR_NOT_INITIALIZED "The " UA_PLUGIN_NAME " is not initialized"
#define ERR_NO_NAME         "The " UA_PLUGIN_NAME " was not given a name"

#define VAPIX_URL "http://127.0.0.12/axis-cgi/%s"

#define BASIC_DEVICE_INFO_CGI_ENDPOINT "basicdeviceinfo.cgi"

#define CONF1_DBUS_SERVICE     "com.axis.HTTPConf1"
#define CONF1_DBUS_OBJECT_PATH "/com/axis/HTTPConf1/VAPIXServiceAccounts1"
#define CONF1_DBUS_INTERFACE   "com.axis.HTTPConf1.VAPIXServiceAccounts1"

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
static size_t
post_write_cb(gchar *ptr, size_t size, size_t nmemb, void *userdata)
{
  size_t processed_bytes;

  g_assert(ptr != NULL);
  g_assert(userdata != NULL);

  processed_bytes = size * nmemb;

  g_string_append_len((GString *) userdata, ptr, processed_bytes);

  return processed_bytes;
}

static void
set_curl_setopt_error(CURLcode res, GError **err)
{
  g_assert(err == NULL || *err == NULL);

  SET_ERROR(err,
            -1,
            "curl_easy_setopt error %d: '%s'",
            res,
            curl_easy_strerror(res));
}

static gchar *
post_full(CURL *handle,
          const gchar *credentials,
          const gchar *endpoint,
          const gchar *request,
          GError **err)
{
  glong code;
  GString *response;
  gchar *url = NULL;
  CURLcode res;

  g_assert(handle != NULL);
  g_assert(credentials != NULL);
  g_assert(endpoint != NULL);
  g_assert(request != NULL);
  g_assert(err == NULL || *err == NULL);

  url = g_strdup_printf(VAPIX_URL, endpoint);
  response = g_string_new(NULL);

  res = curl_easy_setopt(handle, CURLOPT_URL, url);

  if (res != CURLE_OK) {
    set_curl_setopt_error(res, err);
    goto err_out;
  }

  res = curl_easy_setopt(handle, CURLOPT_USERPWD, credentials);

  if (res != CURLE_OK) {
    set_curl_setopt_error(res, err);
    goto err_out;
  }

  res = curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

  if (res != CURLE_OK) {
    set_curl_setopt_error(res, err);
    goto err_out;
  }

  res = curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request);

  if (res != CURLE_OK) {
    set_curl_setopt_error(res, err);
    goto err_out;
  }

  res = curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, post_write_cb);

  if (res != CURLE_OK) {
    set_curl_setopt_error(res, err);
    goto err_out;
  }

  res = curl_easy_setopt(handle, CURLOPT_WRITEDATA, response);

  if (res != CURLE_OK) {
    set_curl_setopt_error(res, err);
    goto err_out;
  }

  res = curl_easy_perform(handle);

  if (res != CURLE_OK) {
    SET_ERROR(err,
              -1,
              "curl_easy_perform error %d: '%s'",
              res,
              curl_easy_strerror(res));

    goto err_out;
  }

  res = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &code);

  if (res != CURLE_OK) {
    SET_ERROR(err,
              -1,
              "curl_easy_getinfo error %d: '%s'",
              res,
              curl_easy_strerror(res));

    goto err_out;
  }

  if (code != 200) {
    SET_ERROR(err,
              -1,
              "Got response code %ld from request to %s with response '%s'",
              code,
              request,
              response->str);
    g_string_free(response, TRUE);
    g_clear_pointer(&url, g_free);
    return NULL;
  }

err_out:
  g_clear_pointer(&url, g_free);

  return g_string_free(response, FALSE);
}

static gchar *
parse_credentials(GVariant *result, GError **err)
{
  gchar *v_creds = NULL;
  gchar *credentials = NULL;
  gchar **split = NULL;
  guint len;

  g_assert(result != NULL);
  g_assert(err == NULL || *err == NULL);

  g_variant_get(result, "(&s)", &v_creds);

  split = g_strsplit(v_creds, ":", -1);
  if (split == NULL) {
    SET_ERROR(err, -1, "Error parsing credential string: '%s'", v_creds);
    goto out;
  }

  len = g_strv_length(split);
  if (len != 2) {
    SET_ERROR(err,
              -1,
              "Invalid credential string length (%u): '%s'",
              len,
              v_creds);
    goto out;
  }

  credentials = g_strdup_printf("%s:%s", split[0], split[1]);

out:
  if (split != NULL) {
    g_strfreev(split);
  }

  return credentials;
}

static gchar *
get_vapix_credentials(const gchar *username, GError **err)
{
  GDBusConnection *con = NULL;
  GVariant *result = NULL;
  gchar *credentials = NULL;

  g_assert(username != NULL);
  g_assert(err == NULL || *err == NULL);

  con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, err);
  if (con == NULL) {
    g_prefix_error(err, "Error connecting to D-Bus: ");
    return NULL;
  }

  result = g_dbus_connection_call_sync(con,
                                       CONF1_DBUS_SERVICE,
                                       CONF1_DBUS_OBJECT_PATH,
                                       CONF1_DBUS_INTERFACE,
                                       "GetCredentials",
                                       g_variant_new("(s)", username),
                                       NULL,
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       err);
  if (result == NULL) {
    g_prefix_error(err, "Failed to get credentials: ");
    goto out;
  }

  credentials = parse_credentials(result, err);

  if (credentials == NULL) {
    g_prefix_error(err, "parse_credentials() failed: ");
  }

  g_variant_unref(result);

out:
  g_object_unref(con);

  return credentials;
}

static gboolean
vapix_get_basic_device_information(GHashTable **bdi_hashtable, GError **err)
{
  gboolean retval = TRUE;
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

  credentials = get_vapix_credentials("vapix-basicdeviceinfo-user", err);
  if (credentials == NULL) {
    g_prefix_error(err, "Failed to get the VAPIX credentials: ");
    retval = FALSE;
    goto out;
  }

  response = post_full(curl_h,
                       credentials,
                       BASIC_DEVICE_INFO_CGI_ENDPOINT,
                       request,
                       err);
  if (response == NULL) {
    g_prefix_error(err, "Failed to get the basic device information: ");
    retval = FALSE;
    goto out;
  }

  json_response = json_loads(response, 0, &parse_error);

  if (json_response == NULL) {
    SET_ERROR(err, -1, "Invalid JSON response: %s", parse_error.text);
    retval = FALSE;
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

  attr.accessLevel = UA_ACCESSLEVELMASK_READ;
  ua_value = UA_STRING(value);
  UA_Variant_setScalar(&attr.value, &ua_value, &UA_TYPES[UA_TYPES_STRING]);

  attr.displayName = UA_LOCALIZEDTEXT("en-US", name);
  attr.description = UA_LOCALIZEDTEXT("en-US", name);

  retval =
          UA_Server_addVariableNode(server,
                                    UA_NODEID_NUMERIC(plugin->ns, 0),
                                    parent,
                                    UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                    UA_QUALIFIEDNAME(plugin->ns, name),
                                    UA_NODEID_NUMERIC(0, UA_NS0ID_PROPERTYTYPE),
                                    attr,
                                    NULL,
                                    NULL);
  if (retval != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "UA_Server_addVariableNode() failed: %s",
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
  g_assert(server != NULL);
  g_assert(outId != NULL);
  g_assert(err == NULL || *err == NULL);

  attr.displayName = UA_LOCALIZEDTEXT("en-US", UA_BDI_OBJ_DISPLAY_NAME);
  attr.description = UA_LOCALIZEDTEXT("en-US", UA_BDI_OBJ_DESCRIPTION);

  status =
          UA_Server_addObjectNode(server,
                                  UA_NODEID_NUMERIC(plugin->ns, 0),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                  UA_QUALIFIEDNAME(plugin->ns,
                                                   UA_BDI_OBJ_DISPLAY_NAME),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                  attr,
                                  NULL,
                                  outId);
  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "Failed to add variable node BasicDeviceInfo: %s",
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
  GError *lerr = NULL;
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
                                &lerr)) {
      LOG_W(plugin->logger,
            "add_variable_to_object() failed: %s",
            GERROR_MSG(lerr));
      g_clear_error(&lerr);
    }
  }
  g_hash_table_unref(bdi_hashtable);

  retval = TRUE;

err_out:
  g_clear_error(&lerr);

  return retval;
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
  UA_NodeId bdi_node;

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

  /* add a bdi object to the opc-ua server */
  if (!add_bdi_object(server, &bdi_node, err)) {
    g_prefix_error(err, "add_bdi_object() failed: ");
    plugin_cleanup();
    return FALSE;
  }

  /* add the actual values to the nodes */
  if (!add_basic_device_info_data(server, bdi_node, err)) {
    g_prefix_error(err, "add_basic_device_info_data() failed: ");
    plugin_cleanup();
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
