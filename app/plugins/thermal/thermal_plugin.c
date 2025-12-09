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

#include "error.h"
#include "log.h"
#include "plugin.h"
#include "thermal_plugin.h"
#include "thermal_vapix.h"
#include "ua_utils.h"
#include "vapix_utils.h"

#define THERMAL_DESCRIPTION        "Thermal Area"
#define THERMAL_OBJECT_DESCRIPTION "Thermal Areas"
#define THERMAL_NAMESPACE_URI      "http://www.axis.com/OpcUA/Thermal/"
#define UA_PLUGIN_NAME             "opc-thermal-plugin"

#define ERR_NOT_INITIALIZED "The " UA_PLUGIN_NAME " is not initialized"
#define ERR_NO_NAME         "The " UA_PLUGIN_NAME " was not given a name"

#define NBR_OF_RETRIES 10

#define DETECTION_TYPE_BNAME        "DetectionType"
#define ENABLED_BNAME               "Enabled"
#define ID_BNAME                    "Id"
#define NAME_BNAME                  "Name"
#define PRESET_NBR_BNAME            "PresetNumber"
#define TEMP_MIN_BNAME              "TempMin"
#define TEMP_MAX_BNAME              "TempMax"
#define TEMP_AVG_BNAME              "TempAvg"
#define THRESHOLD_MEASUREMENT_BNAME "ThresholdMeasurement"
#define THRESHOLD_VALUE_BNAME       "ThresholdValue"
#define TRIGGERED_BNAME             "Triggered"

DEFINE_GQUARK("opc-thermal-plugin")

typedef struct plugin {
  /* user-friendly name of the plugin */
  gchar *name;
  /* OPC-UA namespace index */
  UA_UInt16 ns;
  /* an open62541 logger */
  UA_Logger *logger;
  /* copy of the server object */
  UA_Server *server;
  /* node id of the thermal object */
  UA_NodeId thermal_parent;
  /* callback id for getAreaStatus callback */
  guint cb_id;
  /* count number of times polling temperature values fails */
  gint counter;
  gchar *vapix_credentials;
  CURL *curl_h;
  /* mutex for curl_h */
  GMutex curl_mutex;
  /* keep track of data that needs to be rolled back in case of failure */
  rollback_data_t *rbd;
} plugin_t;

static plugin_t *plugin;

typedef struct property {
  gchar *name;
  gint32 value_type;
} property_t;

/* clang-format off */
static property_t thermal_properties[] = {
  {
    .name = ID_BNAME,
    .value_type = UA_TYPES_UINT32
  },
  {
    .name = PRESET_NBR_BNAME,
    .value_type = UA_TYPES_INT32
  },
  {
    .name = TEMP_AVG_BNAME,
    .value_type = UA_TYPES_INT32
  },
  {
    .name = TEMP_MAX_BNAME,
    .value_type = UA_TYPES_INT32
  },
  {
    .name = TEMP_MIN_BNAME,
    .value_type = UA_TYPES_INT32
  },
  {
    .name = THRESHOLD_VALUE_BNAME,
    .value_type = UA_TYPES_INT32
  },
  {
    .name = TRIGGERED_BNAME,
    .value_type = UA_TYPES_BOOLEAN
  },
  {
    .name = ENABLED_BNAME,
    .value_type = UA_TYPES_BOOLEAN
  },
  {
    .name = NAME_BNAME,
    .value_type = UA_TYPES_STRING
  },
  {
    .name = DETECTION_TYPE_BNAME,
    .value_type = UA_TYPES_STRING
  },
  {
    .name = THRESHOLD_MEASUREMENT_BNAME,
    .value_type = UA_TYPES_STRING
  },
  {
    .name = NULL,
    .value_type = 0
  },
};
/* clang-format on */

/* Local functions */
static gboolean
opc_write_property(UA_NodeId parent,
                   UA_QualifiedName browseName,
                   void *data,
                   const UA_DataType *type,
                   GError **err)
{
  UA_StatusCode retval;

  g_assert(type != NULL);
  g_assert(data != NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->server != NULL);
  g_assert(err == NULL || *err == NULL);

  retval = UA_Server_writeObjectProperty_scalar(plugin->server,
                                                parent,
                                                browseName,
                                                data,
                                                type);

  if (retval != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "UA_Server_writeObjectProperty_scalar(%.*s) failed: %s",
              (int) browseName.name.length,
              browseName.name.data,
              UA_StatusCode_name(retval));
    return FALSE;
  }

  return TRUE;
}

static gboolean
ua_server_add_themal_properties(UA_NodeId parent, GError **err)
{
  UA_StatusCode status;
  UA_VariableAttributes attr = UA_VariableAttributes_default;

  g_assert(plugin != NULL);
  g_assert(plugin->rbd != NULL);
  g_assert(plugin->server != NULL);
  g_assert(err == NULL || *err == NULL);

  for (gint i = 0; thermal_properties[i].name != NULL; i++) {
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;
    UA_Variant_setScalar(&attr.value,
                         NULL,
                         &UA_TYPES[thermal_properties[i].value_type]);

    attr.displayName = UA_LOCALIZEDTEXT("en-US", thermal_properties[i].name);
    attr.description = UA_LOCALIZEDTEXT("en-US", thermal_properties[i].name);

    status = UA_Server_addVariableNode_rb(
            plugin->server,
            UA_NODEID_NUMERIC(plugin->ns, 0),
            parent,
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
            UA_QUALIFIEDNAME(plugin->ns, thermal_properties[i].name),
            UA_NODEID_NUMERIC(0, UA_NS0ID_PROPERTYTYPE),
            attr,
            NULL,
            plugin->rbd,
            NULL);

    if (status != UA_STATUSCODE_GOOD) {
      SET_ERROR(err,
                -1,
                "Failed to add variable %s: %s",
                thermal_properties[i].name,
                UA_StatusCode_name(status));
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
ua_server_add_thermal_area(UA_UInt32 id,
                           gchar *name,
                           UA_Boolean enabled,
                           UA_Int32 preset_nbr,
                           UA_Int32 threshold_val,
                           gchar *threshold,
                           gchar *detection,
                           GError **err)
{
  UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
  gchar *title;
  UA_StatusCode status;
  gboolean ret = TRUE;
  UA_NodeId areaId;
  UA_String ua_name;
  UA_String ua_detection;
  UA_String ua_threshold;

  g_assert(plugin != NULL);
  g_assert(plugin->rbd != NULL);
  g_assert(plugin->server != NULL);
  g_assert(name != NULL);
  g_assert(threshold != NULL);
  g_assert(detection != NULL);
  g_assert(err == NULL || *err == NULL);

  title = g_strdup_printf("Thermal%u", id);
  oAttr.displayName = UA_LOCALIZEDTEXT("en-US", name);
  oAttr.description = UA_LOCALIZEDTEXT("en-US", THERMAL_DESCRIPTION);

  areaId = UA_NODEID_STRING(plugin->ns, title);

  status =
          UA_Server_addObjectNode_rb(plugin->server,
                                     areaId,
                                     plugin->thermal_parent,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                     UA_QUALIFIEDNAME(plugin->ns, title),
                                     UA_NODEID_NUMERIC(0,
                                                       UA_NS0ID_BASEOBJECTTYPE),
                                     oAttr,
                                     NULL,
                                     plugin->rbd,
                                     NULL);
  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "UA_Server_addObjectNode_rb() failed: %s",
              UA_StatusCode_name(status));
    ret = FALSE;
    goto err_out;
  }

  if (!ua_server_add_themal_properties(areaId, err)) {
    g_prefix_error(err, "ua_server_add_thermal_properties() failed: ");
    ret = FALSE;
    goto err_out;
  }

  ua_name = UA_STRING(name);
  ua_detection = UA_STRING(detection);
  ua_threshold = UA_STRING(threshold);

  if (!opc_write_property(areaId,
                          UA_QUALIFIEDNAME(plugin->ns, NAME_BNAME),
                          &ua_name,
                          &UA_TYPES[UA_TYPES_STRING],
                          err) ||
      !opc_write_property(areaId,
                          UA_QUALIFIEDNAME(plugin->ns, ENABLED_BNAME),
                          &enabled,
                          &UA_TYPES[UA_TYPES_BOOLEAN],
                          err) ||
      !opc_write_property(areaId,
                          UA_QUALIFIEDNAME(plugin->ns, THRESHOLD_VALUE_BNAME),
                          &threshold_val,
                          &UA_TYPES[UA_TYPES_INT32],
                          err) ||
      !opc_write_property(areaId,
                          UA_QUALIFIEDNAME(plugin->ns, PRESET_NBR_BNAME),
                          &preset_nbr,
                          &UA_TYPES[UA_TYPES_INT32],
                          err) ||
      !opc_write_property(areaId,
                          UA_QUALIFIEDNAME(plugin->ns, ID_BNAME),
                          &id,
                          &UA_TYPES[UA_TYPES_UINT32],
                          err) ||
      !opc_write_property(areaId,
                          UA_QUALIFIEDNAME(plugin->ns,
                                           THRESHOLD_MEASUREMENT_BNAME),
                          &ua_threshold,
                          &UA_TYPES[UA_TYPES_STRING],
                          err) ||
      !opc_write_property(areaId,
                          UA_QUALIFIEDNAME(plugin->ns, DETECTION_TYPE_BNAME),
                          &ua_detection,
                          &UA_TYPES[UA_TYPES_STRING],
                          err)) {
    g_prefix_error(err, "opc_write_property() failed: ");
    ret = FALSE;
    goto err_out;
  }

err_out:
  g_clear_pointer(&title, g_free);

  return ret;
}

static void
free_thermal_area(gpointer data)
{
  thermal_area_t *values = (thermal_area_t *) data;

  if (values == NULL) {
    return;
  }

  g_clear_pointer(&values->name, g_free);
  g_clear_pointer(&values->detectionType, g_free);
  g_clear_pointer(&values->measurement, g_free);
  g_clear_pointer(&values, g_free);
}

static gboolean
add_thermal_areas(GError **err)
{
  gboolean retval = TRUE;
  GList *lst = NULL;

  g_assert(plugin != NULL);
  g_assert(plugin->curl_h != NULL);
  g_assert(plugin->logger != NULL);
  g_assert(plugin->vapix_credentials != NULL);
  g_assert(err == NULL || *err == NULL);

  if (!vapix_get_thermal_areas(plugin->vapix_credentials,
                               plugin->curl_h,
                               &lst,
                               err)) {
    g_prefix_error(err, "vapix_get_thermal_areas() failed: ");
    retval = FALSE;
    goto err_out;
  }

  for (GList *iter = lst; iter != NULL; iter = iter->next) {
    thermal_area_t *values = (thermal_area_t *) iter->data;
    if (!ua_server_add_thermal_area(values->id,
                                    values->name,
                                    values->enabled,
                                    values->presetNbr,
                                    values->threshold,
                                    values->measurement,
                                    values->detectionType,
                                    err)) {
      g_prefix_error(err, "ua_server_add_thermal_area() failed: ");
      retval = FALSE;
      goto err_out;
    }
  }

err_out:
  g_list_free_full(lst, free_thermal_area);

  return retval;
}

static gboolean
ua_server_update_thermal(UA_UInt32 id,
                         UA_Int32 min,
                         UA_Int32 avg,
                         UA_Int32 max,
                         UA_Boolean triggered,
                         GError **err)
{
  gchar *title;
  UA_NodeId areaId;
  gboolean ret = TRUE;

  g_assert(NULL != plugin);
  g_assert(NULL != plugin->server);
  g_assert(err == NULL || *err == NULL);

  title = g_strdup_printf("Thermal%u", id);

  areaId = UA_NODEID_STRING(plugin->ns, title);

  if (!opc_write_property(areaId,
                          UA_QUALIFIEDNAME(plugin->ns, TEMP_MIN_BNAME),
                          &min,
                          &UA_TYPES[UA_TYPES_INT32],
                          err) ||
      !opc_write_property(areaId,
                          UA_QUALIFIEDNAME(plugin->ns, TEMP_AVG_BNAME),
                          &avg,
                          &UA_TYPES[UA_TYPES_INT32],
                          err) ||
      !opc_write_property(areaId,
                          UA_QUALIFIEDNAME(plugin->ns, TEMP_MAX_BNAME),
                          &max,
                          &UA_TYPES[UA_TYPES_INT32],
                          err) ||
      !opc_write_property(areaId,
                          UA_QUALIFIEDNAME(plugin->ns, TRIGGERED_BNAME),
                          &triggered,
                          &UA_TYPES[UA_TYPES_BOOLEAN],
                          err)) {
    g_prefix_error(err, "opc_write_property() failed: ");
    ret = G_SOURCE_REMOVE;
  }

  g_clear_pointer(&title, g_free);

  return ret;
}

/* Retry to poll for temperature values */
static gboolean
check_counter(void)
{
  g_assert(plugin != NULL);

  plugin->counter++;

  if (plugin->counter < NBR_OF_RETRIES) {
    return G_SOURCE_CONTINUE;
  } else {
    plugin->cb_id = 0;
    return G_SOURCE_REMOVE;
  }
}

static void
free_thermal_area_values(gpointer data)
{
  thermal_area_values_t *values = (thermal_area_values_t *) data;

  if (values == NULL) {
    return;
  }

  g_clear_pointer(&values, g_free);
}

static gboolean
update_thermal_cb(G_GNUC_UNUSED gpointer userdata)
{
  GError *lerr = NULL;
  gboolean ret = G_SOURCE_CONTINUE;
  GList *lst = NULL;

  g_assert(plugin != NULL);
  g_assert(plugin->logger != NULL);
  g_assert(plugin->curl_h != NULL);
  g_assert(plugin->vapix_credentials != NULL);

  g_mutex_lock(&plugin->curl_mutex);

  if (!vapix_get_thermal_area_status(plugin->vapix_credentials,
                                     plugin->curl_h,
                                     &lst,
                                     &lerr)) {
    LOG_E(plugin->logger,
          "vapix_get_thermal_area_status() failed: %s",
          GERROR_MSG(lerr));
    g_clear_error(&lerr);
    g_mutex_unlock(&plugin->curl_mutex);
    ret = check_counter();
    goto err_out;
  }

  g_mutex_unlock(&plugin->curl_mutex);

  for (GList *iter = lst; iter != NULL; iter = iter->next) {
    thermal_area_values_t *values = (thermal_area_values_t *) iter->data;
    if (!ua_server_update_thermal(values->id,
                                  (gint) values->min,
                                  (gint) values->avg,
                                  (gint) values->max,
                                  values->triggered,
                                  &lerr)) {
      LOG_E(plugin->logger,
            "ua_server_update_thermal() failed: %s",
            GERROR_MSG(lerr));
      g_clear_error(&lerr);
      ret = G_SOURCE_REMOVE;
      goto err_out;
    }
  }

  plugin->counter = 0;

err_out:
  g_list_free_full(lst, free_thermal_area_values);

  return ret;
}

/* callback executed when the 'Set Scale' method */
static UA_StatusCode
thermal_change_scale_cb(G_GNUC_UNUSED UA_Server *server,
                        G_GNUC_UNUSED const UA_NodeId *sessionId,
                        G_GNUC_UNUSED void *sessionHandle,
                        G_GNUC_UNUSED const UA_NodeId *methodId,
                        G_GNUC_UNUSED void *methodContext,
                        G_GNUC_UNUSED const UA_NodeId *objectId,
                        G_GNUC_UNUSED void *objectContext,
                        G_GNUC_UNUSED size_t inputSize,
                        const UA_Variant *input,
                        G_GNUC_UNUSED size_t outputSize,
                        G_GNUC_UNUSED UA_Variant *output)
{
  UA_String scale;
  gchar *scale_lower;
  GError *lerr = NULL;
  UA_StatusCode status = UA_STATUSCODE_GOOD;

  g_assert(plugin != NULL);
  g_assert(plugin->logger != NULL);
  g_assert(input != NULL);

  scale = *(UA_String *) input[0].data;
  scale_lower = g_ascii_strdown((gchar *) scale.data, scale.length);

  if (g_strcmp0(scale_lower, "celsius") != 0 &&
      g_strcmp0(scale_lower, "fahrenheit") != 0) {
    LOG_E(plugin->logger, "Scale: %s is not supported", scale_lower);
    status = UA_STATUSCODE_BADINVALIDARGUMENT;
    goto err_out;
  }

  g_mutex_lock(&plugin->curl_mutex);

  if (!vapix_set_temperature_scale(plugin->vapix_credentials,
                                   plugin->curl_h,
                                   scale_lower,
                                   &lerr)) {
    LOG_E(plugin->logger,
          "vapix_set_temperature_scale() failed: %s",
          GERROR_MSG(lerr));
    g_clear_error(&lerr);
    status = UA_STATUSCODE_BADCOMMUNICATIONERROR;
  }

  g_mutex_unlock(&plugin->curl_mutex);

err_out:
  g_clear_pointer(&scale_lower, g_free);

  return status;
}

static UA_StatusCode
thermal_add_scale_method(GError **err)
{
  UA_StatusCode status;
  UA_MethodAttributes mattr;
  UA_Argument in_arg;

  g_assert(plugin != NULL);
  g_assert(plugin->rbd != NULL);
  g_assert(plugin->server != NULL);
  g_assert(err == NULL || *err == NULL);

  /* prepare a node for the Activate method */
  UA_Argument_init(&in_arg);
  in_arg.description =
          UA_LOCALIZEDTEXT("en-US", "Temperature Scale: Celsius or Fahrenheit");
  in_arg.name = UA_STRING("Scale");
  in_arg.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
  in_arg.valueRank = UA_VALUERANK_SCALAR;

  mattr = UA_MethodAttributes_default;
  mattr.description = UA_LOCALIZEDTEXT("en-US", "Change Temperature Scale");
  mattr.displayName = UA_LOCALIZEDTEXT("en-US", "Set Scale");
  mattr.executable = true;
  mattr.userExecutable = true;

  status = UA_Server_addMethodNode_rb(plugin->server,
                                      UA_NODEID_NUMERIC(plugin->ns, 0),
                                      plugin->thermal_parent,
                                      UA_NODEID_NUMERIC(0,
                                                        UA_NS0ID_HASCOMPONENT),
                                      UA_QUALIFIEDNAME(plugin->ns,
                                                       "Set Scale Method"),
                                      mattr,
                                      &thermal_change_scale_cb,
                                      1,
                                      &in_arg,
                                      0,
                                      NULL,
                                      NULL,
                                      plugin->rbd,
                                      NULL);

  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "UA_Server_addMethodNode_rb() failed, error code: %s",
              UA_StatusCode_name(status));
  }

  return status;
}

static gboolean
add_thermal_object(GError **err)
{
  UA_StatusCode status;
  UA_ObjectAttributes attr = UA_ObjectAttributes_default;

  g_assert(plugin != NULL);
  g_assert(plugin->rbd != NULL);
  g_assert(plugin->server != NULL);
  g_assert(err == NULL || *err == NULL);

  attr.displayName = UA_LOCALIZEDTEXT("en-US", THERMAL_OBJECT_DESCRIPTION);
  attr.description = UA_LOCALIZEDTEXT("en-US", THERMAL_OBJECT_DESCRIPTION);

  status =
          UA_Server_addObjectNode_rb(plugin->server,
                                     UA_NODEID_NUMERIC(plugin->ns, 0),
                                     UA_NODEID_NUMERIC(0,
                                                       UA_NS0ID_OBJECTSFOLDER),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                     UA_QUALIFIEDNAME(plugin->ns,
                                                      "ThermalAreas"),
                                     UA_NODEID_NUMERIC(0,
                                                       UA_NS0ID_BASEOBJECTTYPE),
                                     attr,
                                     NULL,
                                     plugin->rbd,
                                     &plugin->thermal_parent);
  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "Failed to add object 'ThermalAreas': %s",
              UA_StatusCode_name(status));
    return FALSE;
  }

  status = thermal_add_scale_method(err);

  if (status != UA_STATUSCODE_GOOD) {
    g_prefix_error(err, "Failed to add 'Set Scale' method");
    return FALSE;
  }

  return TRUE;
}

static void
plugin_cleanup(void)
{
  g_assert(plugin != NULL);
  g_assert(plugin->logger != NULL);

  if (plugin->cb_id != 0 && !g_source_remove(plugin->cb_id)) {
    LOG_E(plugin->logger,
          "Failed to remove timed vapix retrieval function from main loop");
  }

  if (plugin->curl_h != NULL) {
    curl_easy_cleanup(plugin->curl_h);
  }

  plugin->logger = NULL;
  plugin->server = NULL;
  g_mutex_clear(&plugin->curl_mutex);
  g_clear_pointer(&plugin->name, g_free);
  g_clear_pointer(&plugin->vapix_credentials, g_free);

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
  plugin->server = server;
  g_mutex_init(&plugin->curl_mutex);

  plugin->curl_h = curl_easy_init();
  if (plugin->curl_h == NULL) {
    SET_ERROR(err, -1, "curl_easy_init() failed");
    goto err_out;
  }

  /* obtain credentials to make VAPIX calls */
  plugin->vapix_credentials =
          vapix_get_credentials("vapix-thermometry-user", err);
  if (plugin->vapix_credentials == NULL) {
    g_prefix_error(err, "Failed to get the VAPIX credentials: ");
    goto err_out;
  }

  /* If thermometry isn't supported don't return false */
  if (!vapix_get_supported_versions(plugin->vapix_credentials,
                                    plugin->curl_h,
                                    err)) {
    g_prefix_error(err, "No supported versions available for 'thermometry': ");
    goto err_out;
  }

  plugin->ns = UA_Server_addNamespace(server, THERMAL_NAMESPACE_URI);

  /* Add thermal-object and scale variable */
  if (!add_thermal_object(err)) {
    g_prefix_error(err, "add_thermal_object() failed: ");
    goto err_out;
  }

  if (!add_thermal_areas(err)) {
    g_prefix_error(err, "add_thermal_areas() failed: ");
    goto err_out;
  }

  plugin->cb_id = g_timeout_add_seconds(1, update_thermal_cb, NULL);

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
