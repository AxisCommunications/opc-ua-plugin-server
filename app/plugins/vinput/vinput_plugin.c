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

#include <axsdk/axevent.h>
#include <curl/curl.h>
#include <gio/gio.h>
#include <glib.h>
#include <open62541/server.h>

#include "error.h"
#include "log.h"
#include "ua_utils.h"
#include "vapix_utils.h"
#include "vinput_plugin.h"
#include "vinput_vapix.h"

#define UA_PLUGIN_NAMESPACE      "http://www.axis.com/OpcUA/VirtualInput/"
#define UA_PLUGIN_NAME           "opc-vinput-plugin"
#define UA_VINP_OBJ_DISPLAY_NAME "VirtualInputs"
#define UA_VINP_OBJ_DESCRIPTION  "VirtualInputs"

#define UA_VINPUTID_VIRTUALINPUTS_STARTID 6100
#define VIN_BROWSE_NAME                   "VirtualInput-"
#define VIN_BROWSE_NAME_FMT               VIN_BROWSE_NAME "%d"

/* the max possible as of today, the actual nr. could be less on older f/w */
#define VINPUT_MAX_PORTS 64

#define ERR_NOT_INITIALIZED "The " UA_PLUGIN_NAME " is not initialized"
#define ERR_NO_NAME         "The " UA_PLUGIN_NAME " was not given a name"

DEFINE_GQUARK(UA_PLUGIN_NAME)

typedef struct plugin {
  UA_Server *server;
  /* user-friendly name of the plugin */
  gchar *name;
  /* OPC-UA namespace index */
  UA_UInt16 ns;
  /* an open62541 logger */
  UA_Logger *logger;
  rollback_data_t *rbd;

  AXEventHandler *event_handler;
  guint event_subscription;
  gboolean *vin_states;
  gchar *schema_version;
  gchar *vapix_credentials;
  CURL *curl_h;
} plugin_t;

static plugin_t *plugin;

/* Local functions */

/* Performs a rollback on the nodes added to the information model by deleting
 * them from the server. This must always be called before the server thread
 * is started. */
static gboolean
vin_ua_do_rollback(GError **err)
{
  g_assert(err == NULL || *err == NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->rbd != NULL);
  g_assert(plugin->server != NULL);

  return ua_utils_do_rollback(plugin->server, plugin->rbd, err);
}

static UA_StatusCode
vin_ua_activate_cb(G_GNUC_UNUSED UA_Server *server,
                   G_GNUC_UNUSED const UA_NodeId *sessionId,
                   G_GNUC_UNUSED void *sessionHandle,
                   G_GNUC_UNUSED const UA_NodeId *methodId,
                   G_GNUC_UNUSED void *methodContext,
                   G_GNUC_UNUSED const UA_NodeId *objectId,
                   G_GNUC_UNUSED void *objectContext,
                   G_GNUC_UNUSED size_t inputSize,
                   const UA_Variant *input,
                   G_GNUC_UNUSED size_t outputSize,
                   UA_Variant *output)
{
  UA_UInt32 port_nr;
  UA_Int32 duration;
  UA_StatusCode ua_status;
  GError *lerr = NULL;
  gboolean state_changed;

  g_assert(input != NULL);
  g_assert(output != NULL);

  port_nr = *(UA_UInt32 *) input[0].data;
  duration = *(UA_Int32 *) input[1].data;

  LOG_D(plugin->logger, "port_nr: %d, duration: %d", port_nr, duration);

  /* 'port_nr' must be in the range [1..VINPUT_MAX_PORTS] */
  if ((port_nr < 1) || (port_nr > VINPUT_MAX_PORTS)) {
    return UA_STATUSCODE_BADOUTOFRANGE;
  }

  ua_status = vin_set_port_state(plugin->curl_h,
                                 plugin->vapix_credentials,
                                 plugin->schema_version,
                                 port_nr,
                                 TRUE,
                                 duration,
                                 plugin->vin_states,
                                 &state_changed,
                                 &lerr);
  if (ua_status != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger, "vin_set_port_state() failed: %s", GERROR_MSG(lerr));
    g_clear_error(&lerr);
  } else {
    LOG_D(plugin->logger,
          "result: port_nr: %d set ACTIVE, state_changed: %d",
          port_nr,
          state_changed);
    ua_status = UA_Variant_setScalarCopy(output,
                                         &state_changed,
                                         &UA_TYPES[UA_TYPES_BOOLEAN]);
  }

  return ua_status;
}

static UA_StatusCode
vin_ua_deactivate_cb(G_GNUC_UNUSED UA_Server *server,
                     G_GNUC_UNUSED const UA_NodeId *sessionId,
                     G_GNUC_UNUSED void *sessionHandle,
                     G_GNUC_UNUSED const UA_NodeId *methodId,
                     G_GNUC_UNUSED void *methodContext,
                     G_GNUC_UNUSED const UA_NodeId *objectId,
                     G_GNUC_UNUSED void *objectContext,
                     G_GNUC_UNUSED size_t inputSize,
                     const UA_Variant *input,
                     G_GNUC_UNUSED size_t outputSize,
                     UA_Variant *output)
{
  UA_UInt32 port_nr;
  gboolean state_changed;
  GError *lerr = NULL;
  UA_StatusCode ua_status;

  g_assert(input != NULL);
  g_assert(output != NULL);

  port_nr = *(UA_UInt32 *) input[0].data;

  LOG_D(plugin->logger, "port_nr: %d", port_nr);

  if ((port_nr < 1) || (port_nr > VINPUT_MAX_PORTS)) {
    return UA_STATUSCODE_BADOUTOFRANGE;
  }

  ua_status = vin_set_port_state(plugin->curl_h,
                                 plugin->vapix_credentials,
                                 plugin->schema_version,
                                 port_nr,
                                 FALSE,
                                 0,
                                 plugin->vin_states,
                                 &state_changed,
                                 &lerr);
  if (ua_status != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger, "vin_set_port_state() failed: %s", GERROR_MSG(lerr));
    g_clear_error(&lerr);
  } else {
    LOG_D(plugin->logger,
          "result: port_nr: %d set INACTIVE, state_changed: %d",
          port_nr,
          state_changed);
    ua_status = UA_Variant_setScalarCopy(output,
                                         &state_changed,
                                         &UA_TYPES[UA_TYPES_BOOLEAN]);
  }

  return ua_status;
}

static gboolean
vin_ua_add_methods(const UA_NodeId parent, GError **err)
{
  UA_StatusCode status;
  UA_MethodAttributes mattr;
  /* input/output argument definitions for the UA Activate/Deactivate methods */
  UA_Argument in_args[2];
  UA_Argument out_arg;

  g_assert(plugin != NULL);
  g_assert(plugin->server != NULL);
  g_assert(plugin->rbd != NULL);
  g_assert(err == NULL || *err == NULL);

  /* prepare a node for the Activate method */
  UA_Argument_init(&in_args[0]);
  in_args[0].description =
          UA_LOCALIZEDTEXT("en-US", "Virtual Input port number (1..64)");
  in_args[0].name = UA_STRING("Virtual Input");
  in_args[0].dataType = UA_TYPES[UA_TYPES_UINT32].typeId;
  in_args[0].valueRank = UA_VALUERANK_SCALAR;

  UA_Argument_init(&in_args[1]);
  in_args[1].description =
          UA_LOCALIZEDTEXT("en-US", "Duration in seconds (-1 to ignore)");
  in_args[1].name = UA_STRING("Duration");
  in_args[1].dataType = UA_TYPES[UA_TYPES_INT32].typeId;
  in_args[1].valueRank = UA_VALUERANK_SCALAR;

  UA_Argument_init(&out_arg);
  out_arg.description = UA_LOCALIZEDTEXT("en-US", "State Changed");
  out_arg.name = UA_STRING("State Changed");
  out_arg.dataType = UA_TYPES[UA_TYPES_BOOLEAN].typeId;
  out_arg.valueRank = UA_VALUERANK_SCALAR;

  mattr = UA_MethodAttributes_default;
  mattr.description = UA_LOCALIZEDTEXT("en-US", "Activate Virtual Input");
  mattr.displayName = UA_LOCALIZEDTEXT("en-US", "Activate");
  mattr.executable = TRUE;
  mattr.userExecutable = TRUE;

  status = UA_Server_addMethodNode_rb(plugin->server,
                                      UA_NODEID_NUMERIC(plugin->ns, 0),
                                      parent,
                                      UA_NODEID_NUMERIC(0,
                                                        UA_NS0ID_HASCOMPONENT),
                                      UA_QUALIFIEDNAME(1, "Activate Method"),
                                      mattr,
                                      &vin_ua_activate_cb,
                                      2,
                                      in_args,
                                      1,
                                      &out_arg,
                                      NULL,
                                      plugin->rbd,
                                      NULL);
  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "UA_Server_addMethodNode_rb() failed, error code: %s",
              UA_StatusCode_name(status));
    return FALSE;
  }

  /* prepare a node for the Deactivate method */
  /* Note: has the same I/O arguments as the "Activate" method except it is
   * missing "duration". */
  mattr = UA_MethodAttributes_default;
  mattr.description = UA_LOCALIZEDTEXT("en-US", "Deactivate Virtual Input");
  mattr.displayName = UA_LOCALIZEDTEXT("en-US", "Deactivate");
  mattr.executable = TRUE;
  mattr.userExecutable = TRUE;

  status = UA_Server_addMethodNode_rb(plugin->server,
                                      UA_NODEID_NUMERIC(plugin->ns, 0),
                                      parent,
                                      UA_NODEID_NUMERIC(0,
                                                        UA_NS0ID_HASCOMPONENT),
                                      UA_QUALIFIEDNAME(1, "Deactivate Method"),
                                      mattr,
                                      &vin_ua_deactivate_cb,
                                      1,
                                      in_args,
                                      1,
                                      &out_arg,
                                      NULL,
                                      plugin->rbd,
                                      NULL);
  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "UA_Server_addMethodNode_rb() failed, error code: %s",
              UA_StatusCode_name(status));
    return FALSE;
  }

  return TRUE;
}

static UA_StatusCode
vin_ua_read_cb(UA_Server *server,
               G_GNUC_UNUSED const UA_NodeId *sessionId,
               G_GNUC_UNUSED void *sessionContext,
               const UA_NodeId *nodeId,
               G_GNUC_UNUSED void *nodeContext,
               G_GNUC_UNUSED UA_Boolean includeSourceTimeStamp,
               G_GNUC_UNUSED const UA_NumericRange *range,
               UA_DataValue *dataValue)
{
  UA_StatusCode ua_status = UA_STATUSCODE_BAD;
  UA_Boolean ua_vin_state;
  UA_UInt32 portnr;

  g_assert(plugin != NULL);
  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(dataValue != NULL);

  dataValue->hasValue = FALSE;

  portnr = nodeId->identifier.numeric - UA_VINPUTID_VIRTUALINPUTS_STARTID;
  g_assert((portnr >= 1) && (portnr <= VINPUT_MAX_PORTS));

  portnr--; /* adjust for 0-indexing in our array */

  LOG_D(plugin->logger,
        "cached VirtualInput-%d state: %d",
        portnr + 1,
        plugin->vin_states[portnr]);

  if (plugin->vin_states[portnr]) {
    ua_vin_state = TRUE;
  } else {
    ua_vin_state = FALSE;
  }

  ua_status = UA_Variant_setScalarCopy(&dataValue->value,
                                       &ua_vin_state,
                                       &UA_TYPES[UA_TYPES_BOOLEAN]);
  if (ua_status != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "UA_Variant_setScalarCopy() failed: %s",
          UA_StatusCode_name(ua_status));
    goto err_out;
  }

  dataValue->hasValue = TRUE;

err_out:

  return ua_status;
}

static UA_StatusCode
vin_ua_write_cb(UA_Server *server,
                G_GNUC_UNUSED const UA_NodeId *sessionId,
                G_GNUC_UNUSED void *sessionContext,
                const UA_NodeId *nodeId,
                G_GNUC_UNUSED void *nodeContext,
                G_GNUC_UNUSED const UA_NumericRange *range,
                const UA_DataValue *dataValue)
{
  GError *lerr = NULL;
  gboolean state_changed;
  UA_UInt32 portnr;
  UA_Boolean new_state;
  UA_StatusCode ua_status;

  g_assert(plugin != NULL);
  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(dataValue != NULL);

  portnr = nodeId->identifier.numeric - UA_VINPUTID_VIRTUALINPUTS_STARTID;
  g_assert((portnr >= 1) && (portnr <= VINPUT_MAX_PORTS));

  new_state = *(UA_Boolean *) dataValue->value.data;
  LOG_D(plugin->logger, "vinput: %d OPC-UA new state: %d", portnr, new_state);

  ua_status = vin_set_port_state(plugin->curl_h,
                                 plugin->vapix_credentials,
                                 plugin->schema_version,
                                 portnr,
                                 new_state,
                                 -1,
                                 plugin->vin_states,
                                 &state_changed,
                                 &lerr);
  if (ua_status != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger, "vin_set_port_state() failed: %s", GERROR_MSG(lerr));
    g_clear_error(&lerr);
  }

  LOG_D(plugin->logger, "return: %s", UA_StatusCode_name(ua_status));

  return ua_status;
}

static gboolean
vin_ua_add_object(UA_NodeId *outId, GError **err)
{
  UA_StatusCode status;
  UA_ObjectAttributes attr = UA_ObjectAttributes_default;

  g_assert(plugin != NULL);
  g_assert(plugin->server != NULL);
  g_assert(plugin->rbd != NULL);
  g_assert(outId != NULL);
  g_assert(err == NULL || *err == NULL);

  attr.displayName = UA_LOCALIZEDTEXT("en-US", UA_VINP_OBJ_DISPLAY_NAME);
  attr.description = UA_LOCALIZEDTEXT("en-US", UA_VINP_OBJ_DESCRIPTION);

  status =
          UA_Server_addObjectNode_rb(plugin->server,
                                     UA_NODEID_NUMERIC(plugin->ns, 0),
                                     UA_NODEID_NUMERIC(0,
                                                       UA_NS0ID_OBJECTSFOLDER),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                     UA_QUALIFIEDNAME(plugin->ns,
                                                      UA_VINP_OBJ_DISPLAY_NAME),
                                     UA_NODEID_NUMERIC(0,
                                                       UA_NS0ID_BASEOBJECTTYPE),
                                     attr,
                                     NULL,
                                     plugin->rbd,
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
vin_ua_add_instances(UA_NodeId parent, GError **err)
{
  UA_StatusCode retVal = UA_STATUSCODE_GOOD;
  /* clang-format off */
  UA_DataSource ua_vinp_cb = {
    .read = vin_ua_read_cb,
    .write = vin_ua_write_cb
  };
  /* clang-format on */
  UA_VariableAttributes vattr = UA_VariableAttributes_default;
  gint i = 0;
  gchar *port_name = NULL;

  g_assert(plugin != NULL);
  g_assert(plugin->server != NULL);
  g_assert(plugin->rbd != NULL);
  g_assert(err == NULL || *err == NULL);

  vattr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  vattr.dataType = UA_TYPES[UA_TYPES_BOOLEAN].typeId;

  for (i = 1; i <= VINPUT_MAX_PORTS; i++) {
    port_name = g_strdup_printf(VIN_BROWSE_NAME_FMT, i);

    LOG_D(plugin->logger, "Adding virtual input: %s", port_name);

    /* add variable nodes for all existing VirtualInput ports */
    vattr.displayName = UA_LOCALIZEDTEXT("", port_name);
    retVal |= UA_Server_addVariableNode_rb(
            plugin->server,
            UA_NODEID_NUMERIC(plugin->ns,
                              UA_VINPUTID_VIRTUALINPUTS_STARTID + i),
            parent,
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
            UA_QUALIFIEDNAME(plugin->ns, port_name),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
            vattr,
            NULL,
            plugin->rbd,
            NULL);

    retVal |= UA_Server_setVariableNode_dataSource(
            plugin->server,
            UA_NODEID_NUMERIC(plugin->ns,
                              UA_VINPUTID_VIRTUALINPUTS_STARTID + i),
            ua_vinp_cb);

    g_clear_pointer(&port_name, g_free);
  }

  if (retVal != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "Unable to add 'VirtualInput' ports to 'VirtualInputs' object!");
    goto bail_out;
  }

bail_out:
  return (retVal == UA_STATUSCODE_GOOD);
}

static void
vin_event_cb(G_GNUC_UNUSED guint subscription,
             AXEvent *event,
             G_GNUC_UNUSED gpointer user_data)
{
  const AXEventKeyValueSet *key_value_set;
  gint port;
  gboolean active;
  GError *lerr = NULL;

  g_assert(plugin != NULL);
  g_assert(plugin->vin_states != NULL);
  g_assert(event != NULL);

  /* Extract the AXEventKeyValueSet from the event. */
  key_value_set = ax_event_get_key_value_set(event);

  if (key_value_set == NULL) {
    goto err_out;
  }

  /* fetch the VirtualInput port number */
  if (!ax_event_key_value_set_get_integer(key_value_set,
                                          "port",
                                          NULL,
                                          &port,
                                          &lerr)) {
    LOG_E(plugin->logger,
          "'port' key missing from event: %s",
          GERROR_MSG(lerr));
    goto err_out;
  }

  /* fetch the 'active' state of port */
  if (!ax_event_key_value_set_get_boolean(key_value_set,
                                          "active",
                                          NULL,
                                          &active,
                                          &lerr)) {
    LOG_E(plugin->logger,
          "'active' key missing from event: %s",
          GERROR_MSG(lerr));
    goto err_out;
  }

  /* !NOTE!: the D-Bus numbering of the Virtual Inputs starts from 1 not 0! */
  plugin->vin_states[port - 1] = active;

  LOG_D(plugin->logger, "VirtualInput-%d: %d", port, active);

err_out:
  g_clear_error(&lerr);
  /* the callback must always free 'event', NULL-case handled by the API */
  ax_event_free(event);

  return;
}

static gboolean
vin_subscribe_event(GError **err)
{
  AXEventKeyValueSet *key_value_set = NULL;
  guint subscription;

  g_assert(plugin != NULL);
  g_assert(err == NULL || *err == NULL);

  key_value_set = ax_event_key_value_set_new();
  if (key_value_set == NULL) {
    SET_ERROR(err, -1, "ax_event_key_value_set_new() failed!");
    return FALSE;
  }

  /* Initialize an AXEventKeyValueSet that matches 'VirtualInput' events.
   *
   * tns1:topic0=Device
   * tnsaxis:topic1=IO
   * tnsaxis:topic2=VirtualInput
   * active=*     <-- Subscribe to all states
   */
  /* clang-format off */
  if (!ax_event_key_value_set_add_key_values(key_value_set, err,
                    "topic0", "tns1",    "Device",       AX_VALUE_TYPE_STRING,
                    "topic1", "tnsaxis", "IO",           AX_VALUE_TYPE_STRING,
                    "topic2", "tnsaxis", "VirtualInput", AX_VALUE_TYPE_STRING,
                    "port",   NULL,      NULL,           AX_VALUE_TYPE_INT,
                    "active", NULL,      NULL,           AX_VALUE_TYPE_BOOL,
                    NULL)) {
    g_prefix_error(err, "ax_event_key_value_set_add_key_values() failed: ");
    ax_event_key_value_set_free(key_value_set);
    return FALSE;
  }
  /* clang-format on */

  if (!ax_event_handler_subscribe(plugin->event_handler,
                                  key_value_set,
                                  &subscription,
                                  (AXSubscriptionCallback) vin_event_cb,
                                  plugin,
                                  err)) {
    g_prefix_error(err, "ax_event_handler_subscribe() failed: ");
    ax_event_key_value_set_free(key_value_set);
    return FALSE;
  }

  plugin->event_subscription = subscription;

  /* The key/value set is no longer needed */
  ax_event_key_value_set_free(key_value_set);

  LOG_D(plugin->logger,
        "Device/IO/VirtualInput subscr. id: %d",
        plugin->event_subscription);

  return TRUE;
}

static void
plugin_cleanup(void)
{
  GError *lerr = NULL;

  g_assert(plugin != NULL);

  if (plugin->curl_h != NULL) {
    curl_easy_cleanup(plugin->curl_h);
  }

  if (plugin->event_handler != NULL) {
    if (plugin->event_subscription > 0) {
      if (!ax_event_handler_unsubscribe_and_notify(plugin->event_handler,
                                                   plugin->event_subscription,
                                                   NULL,
                                                   NULL,
                                                   &lerr)) {
        LOG_E(plugin->logger,
              "ax_event_handler_unsubscribe_and_notify() failed: %s",
              GERROR_MSG(lerr));
        g_clear_error(&lerr);
      }
    }

    ax_event_handler_free(plugin->event_handler);
  }

  g_clear_pointer(&plugin->name, g_free);
  g_clear_pointer(&plugin->vin_states, g_free);
  g_clear_pointer(&plugin->vapix_credentials, g_free);
  g_clear_pointer(&plugin->schema_version, g_free);

  /* free up allocated rollback data, if any */
  ua_utils_clear_rbd(&plugin->rbd);

  g_clear_pointer(&plugin, g_free);

  return;
}

/* Exported functions */
gboolean
opc_ua_create(UA_Server *server,
              UA_Logger *logger,
              G_GNUC_UNUSED gpointer *params,
              GError **err)
{
  UA_NodeId vinp_obj_node;
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
  plugin->server = server;
  plugin->ns = UA_Server_addNamespace(server, UA_PLUGIN_NAMESPACE);
  plugin->rbd = g_new0(rollback_data_t, 1);

  /* allocate an array of booleans to keep track of the vinput states */
  plugin->vin_states = g_new0(gboolean, VINPUT_MAX_PORTS);

  /* allocate a curl handle that we are going to use throughout our requests */
  plugin->curl_h = curl_easy_init();
  if (plugin->curl_h == NULL) {
    SET_ERROR(err, -1, "curl_easy_init() failed");
    goto err_out;
  }

  /* subscribe to "VirtualInput" events */
  plugin->event_handler = ax_event_handler_new();
  if (!plugin->event_handler) {
    SET_ERROR(err, -1, "Could not allocate AXEventHandler!");
    goto err_out;
  }

  if (!vin_subscribe_event(err)) {
    g_prefix_error(err, "vin_subscribe_event() failed: ");
    goto err_out;
  }

  /* obtain credentials to make VAPIX calls */
  plugin->vapix_credentials =
          vapix_get_credentials("vapix-virtualinput-user", err);
  if (plugin->vapix_credentials == NULL) {
    g_prefix_error(err, "Failed to get the VAPIX credentials: ");
    goto err_out;
  }

  plugin->schema_version = vin_get_schema_version(plugin->curl_h,
                                                  plugin->vapix_credentials,
                                                  err);
  if (plugin->schema_version == NULL) {
    g_prefix_error(err, "Failed to get VAPIX schema version: ");
    goto err_out;
  }
  LOG_D(plugin->logger, "plugin->schema_version: %s", plugin->schema_version);

  /* add the VirtualInputs object to the opc-ua server */
  if (!vin_ua_add_object(&vinp_obj_node, err)) {
    g_prefix_error(err, "vin_ua_add_object() failed: ");
    goto err_out;
  }

  /* add variable nodes to the object node */
  if (!vin_ua_add_instances(vinp_obj_node, err)) {
    g_prefix_error(err, "vin_ua_add_instances() failed: ");
    goto err_out;
  }

  /* add methods (Activate/Deactivate) to the object node */
  if (!vin_ua_add_methods(vinp_obj_node, err)) {
    g_prefix_error(err, "vin_ua_add_methods() failed: ");
    goto err_out;
  }

  /* the information model was successfully populated so now we can free up our
   * rollback data since we no longer need it */
  ua_utils_clear_rbd(&plugin->rbd);

  return TRUE;

err_out:
  /* remove any nodes created so far */
  if (!vin_ua_do_rollback(&lerr)) {
    /* NOTE: if we land here we might just as well terminate the whole
     * application, something is totally out of whack */
    LOG_E(plugin->logger, "vin_ua_do_rollback() failed: %s", GERROR_MSG(lerr));
    g_clear_error(&lerr);
  }

  /* clean up potentially allocated resources */
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
