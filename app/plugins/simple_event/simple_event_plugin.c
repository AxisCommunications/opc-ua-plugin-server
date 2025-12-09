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
#include <glib.h>
#include <open62541/server.h>

#include "error.h"
#include "log.h"
#include "simple_event_plugin.h"
#include "ua_utils.h"

#define UA_PLUGIN_NAMESPACE "http://www.axis.com/OpcUA/SimpleEvent/"
#define UA_PLUGIN_NAME      "opc-simple-event-plugin"

#define TIME_PROPERTY                  "Time"
#define SEVERITY_PROPERTY              "Severity"
#define MESSAGE_PROPERTY               "Message"
#define SOURCE_NAME_PROPERTY           "SourceName"
#define ACCESSED_VARIABLE_NAME         "Accessed"
#define UA_LIVESTREAM_OBJ_DISPLAY_NAME "LiveStreamAccessed"
#define UA_LIVESTREAM_OBJ_DESCRIPTION  "Livestream Accessed Object"

#define SEVERITY 500

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
  /* Axis event handler */
  AXEventHandler *event_handler;
  /* Axis event sub id */
  guint sub_id;
  /* event object node id */
  UA_NodeId event_obj;
  /* keep track of data that needs to be rolled back in case of failure */
  rollback_data_t *rbd;
} plugin_t;

static plugin_t *plugin;

/* Local functions */
static gboolean
create_opc_event(UA_Server *server,
                 const gchar *source_name,
                 UA_UInt16 eventSeverity,
                 UA_LocalizedText *eventMessage,
                 UA_DateTime ax_eventTime,
                 UA_NodeId *eventId,
                 GError **err)
{
  UA_StatusCode ret;
  UA_String eventSourceName;
  gchar *reason = "";

  g_assert(server != NULL);
  g_assert(source_name != NULL);
  g_assert(eventMessage != NULL);
  g_assert(eventId != NULL);
  g_assert(err == NULL || *err == NULL);

  ret = UA_Server_createEvent(server,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE),
                              eventId);

  if (ret != UA_STATUSCODE_GOOD) {
    SET_ERROR(err, -1, "Failed to create event: %s", UA_StatusCode_name(ret));
    return FALSE;
  }

  /* Set properties for base event type */

  /* Set the event Time */
  ret = UA_Server_writeObjectProperty_scalar(server,
                                             *eventId,
                                             UA_QUALIFIEDNAME(0, TIME_PROPERTY),
                                             &ax_eventTime,
                                             &UA_TYPES[UA_TYPES_DATETIME]);

  if (ret != UA_STATUSCODE_GOOD) {
    reason = TIME_PROPERTY;
    goto out;
  }

  /* Set the event Severity */
  ret = UA_Server_writeObjectProperty_scalar(
          server,
          *eventId,
          UA_QUALIFIEDNAME(0, SEVERITY_PROPERTY),
          &eventSeverity,
          &UA_TYPES[UA_TYPES_UINT16]);

  if (ret != UA_STATUSCODE_GOOD) {
    reason = SEVERITY_PROPERTY;
    goto out;
  }

  /* Set the event Message */
  ret = UA_Server_writeObjectProperty_scalar(server,
                                             *eventId,
                                             UA_QUALIFIEDNAME(0,
                                                              MESSAGE_PROPERTY),
                                             eventMessage,
                                             &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);

  if (ret != UA_STATUSCODE_GOOD) {
    reason = MESSAGE_PROPERTY;
    goto out;
  }

  /* Set the event SourceName */
  eventSourceName = UA_STRING((gchar *) source_name);
  ret = UA_Server_writeObjectProperty_scalar(
          server,
          *eventId,
          UA_QUALIFIEDNAME(0, SOURCE_NAME_PROPERTY),
          &eventSourceName,
          &UA_TYPES[UA_TYPES_STRING]);

  if (ret != UA_STATUSCODE_GOOD) {
    reason = SOURCE_NAME_PROPERTY;
    goto out;
  }

  return TRUE;

out:
  SET_ERROR(err,
            -1,
            "Failed to create event '%s': %s",
            reason,
            UA_StatusCode_name(ret));

  return FALSE;
}

static gboolean
trigger_opc_event(UA_Server *server,
                  UA_UInt16 eventSeverity,
                  const gchar *source_name,
                  UA_LocalizedText *eventMessage,
                  UA_DateTime ax_eventTime,
                  GError **err)
{
  UA_StatusCode ret;
  UA_NodeId eventNodeId;

  g_assert(server != NULL);
  g_assert(source_name != NULL);
  g_assert(eventMessage != NULL);
  g_assert(err == NULL || *err == NULL);
  g_assert(plugin != NULL);

  LOG_I(plugin->logger, "Try to create event %s ...", eventMessage->text.data);

  /* Write the event */
  if (!create_opc_event(server,
                        source_name,
                        eventSeverity,
                        eventMessage,
                        ax_eventTime,
                        &eventNodeId,
                        err)) {
    g_prefix_error(err, "create_opc_event() failed: ");
    return FALSE;
  }

  /* Trigger an event */
  ret = UA_Server_triggerEvent(server,
                               eventNodeId,
                               plugin->event_obj,
                               NULL,
                               UA_TRUE);

  if (ret != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "%s Failed to trigger event: %s",
              __func__,
              UA_StatusCode_name(ret));
    return FALSE;
  }

  LOG_I(plugin->logger,
        "Event: %s created successfully",
        eventMessage->text.data);

  return TRUE;
};

static void
simple_opc_event_cb(G_GNUC_UNUSED guint subscription,
                    AXEvent *event,
                    gpointer user_data)
{
  const AXEventKeyValueSet *key_value_set;
  gboolean active;
  gchar *s1 = NULL;
  UA_Server *server = (UA_Server *) user_data;
  GError *lerr = NULL;
  UA_StatusCode status;

  g_assert(event != NULL);
  g_assert(server != NULL);
  g_assert(plugin != NULL);

  key_value_set = ax_event_get_key_value_set(event);

  if (key_value_set == NULL) {
    LOG_E(plugin->logger, "ax_event_get_key_value_set() failed: returned NULL");
    goto out;
  }

  if (!ax_event_key_value_set_get_string(key_value_set,
                                         "topic1",
                                         "tnsaxis",
                                         &s1,
                                         &lerr)) {
    LOG_E(plugin->logger,
          "ax_event_key_value_set_get_string() failed: %s",
          GERROR_MSG(lerr));
    g_clear_error(&lerr);
    goto out;
  }

  if (!ax_event_key_value_set_get_boolean(key_value_set,
                                          "accessed",
                                          NULL,
                                          &active,
                                          NULL)) {
    LOG_E(plugin->logger,
          "ax_event_key_value_set_get_boolean() failed: %s",
          GERROR_MSG(lerr));
    g_clear_error(&lerr);
    goto out;
  }

  LOG_D(plugin->logger, "%s: Accessed=%s", s1, active ? "true" : "false");

  if (active) {
    GDateTime *d = ax_event_get_time_stamp2(event);
    UA_UInt16 severity = SEVERITY;
    UA_LocalizedText eventMsg;
    const gchar *source_name = UA_LIVESTREAM_OBJ_DISPLAY_NAME;

    eventMsg = UA_LOCALIZEDTEXT("en-US", s1);

    if (!trigger_opc_event(server,
                           severity,
                           source_name,
                           &eventMsg,
                           UA_DateTime_fromUnixTime(g_date_time_to_unix(d)),
                           &lerr)) {
      LOG_E(plugin->logger, "Event failure: %s", GERROR_MSG(lerr));
      g_clear_error(&lerr);
      goto out;
    }
  }

  status = UA_Server_writeObjectProperty_scalar(
          server,
          plugin->event_obj,
          UA_QUALIFIEDNAME(plugin->ns, ACCESSED_VARIABLE_NAME),
          &active,
          &UA_TYPES[UA_TYPES_BOOLEAN]);

  if (status != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "UA_Server_writeObjectProperty_scalar() failed: %s",
          UA_StatusCode_name(status));
    goto out;
  }

out:
  g_clear_pointer(&s1, g_free);
  ax_event_free(event);
}

/* This is how the 'LiveStreamAccessed' event looks like
 *
 * <MESSAGE > ---- Event ------------------------
 * <MESSAGE > < Property >
 * <MESSAGE > Global Declaration Id: 139
 * <MESSAGE > Local Declaration Id: 81
 * <MESSAGE > Producer Id: 25
 * <MESSAGE > Timestamp: 1742564428.730070
 * <MESSAGE > [accessed = '0' (Accessed)] {onvif-data} {property-state}
 * <MESSAGE > [tns1:topic0 = 'VideoSource']
 * <MESSAGE > [tnsaxis:topic1 = 'LiveStreamAccessed' (Live stream accessed)]
 * <MESSAGE > -----------------------------------
 */
static gboolean
setup_ax_event(UA_Server *server, AXSubscriptionCallback cb, GError **err)
{
  gboolean retval;
  AXEventKeyValueSet *key_value_set = NULL;

  g_assert(server != NULL);
  g_assert(cb != NULL);
  g_assert(err == NULL || *err == NULL);
  g_assert(plugin != NULL);

  /* Set keys and namespaces for the event to be subscribed */
  key_value_set = ax_event_key_value_set_new();

  if (key_value_set == NULL) {
    LOG_E(plugin->logger, "ax_event_key_value_set_new() failed: returned NULL");
    return FALSE;
  }

  retval = ax_event_key_value_set_add_key_values(key_value_set,
                                                 err,
                                                 "topic0",
                                                 "tns1",
                                                 "VideoSource",
                                                 AX_VALUE_TYPE_STRING,
                                                 "topic1",
                                                 "tnsaxis",
                                                 "LiveStreamAccessed",
                                                 AX_VALUE_TYPE_STRING,
                                                 NULL);

  if (!retval) {
    goto out;
  }

  plugin->event_handler = ax_event_handler_new();

  if (plugin->event_handler == NULL) {
    LOG_E(plugin->logger, "ax_event_handler_new() failed: returned NULL");
    retval = FALSE;
    goto out;
  }

  retval = ax_event_handler_subscribe(plugin->event_handler,
                                      key_value_set,
                                      &plugin->sub_id,
                                      cb,
                                      server,
                                      err);

out:
  ax_event_key_value_set_free(key_value_set);

  return retval;
}

/* The variable will change according to the latest value received from the
 * axevent */
static gboolean
create_event_object(UA_Server *server, GError **err)
{
  UA_StatusCode status;
  UA_ObjectAttributes attr = UA_ObjectAttributes_default;
  UA_VariableAttributes vattr = UA_VariableAttributes_default;
  UA_Boolean init_val = FALSE;

  g_assert(plugin != NULL);
  g_assert(plugin->rbd != NULL);
  g_assert(server != NULL);
  g_assert(err == NULL || *err == NULL);

  attr.displayName = UA_LOCALIZEDTEXT("en-US", UA_LIVESTREAM_OBJ_DISPLAY_NAME);
  attr.description = UA_LOCALIZEDTEXT("en-US", UA_LIVESTREAM_OBJ_DESCRIPTION);

  status = UA_Server_addObjectNode_rb(
          server,
          UA_NODEID_NUMERIC(plugin->ns, 0),
          UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
          UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
          UA_QUALIFIEDNAME(plugin->ns, UA_LIVESTREAM_OBJ_DISPLAY_NAME),
          UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
          attr,
          NULL,
          plugin->rbd,
          &plugin->event_obj);

  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "UA_Server_addObjectNode_rb() failed: %s",
              UA_StatusCode_name(status));
    return FALSE;
  }

  /* set the EventNotifier attribute for the 'LiveStreamAccessed' object node */
  status = UA_Server_writeEventNotifier(server,
                                        plugin->event_obj,
                                        UA_EVENTNOTIFIER_SUBSCRIBE_TO_EVENT);
  if (status != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "UA_Server_writeEventNotifier() failed: %s",
          UA_StatusCode_name(status));
    return FALSE;
  }

  vattr.accessLevel = UA_ACCESSLEVELMASK_READ;
  UA_Variant_setScalar(&vattr.value, &init_val, &UA_TYPES[UA_TYPES_BOOLEAN]);

  attr.displayName = UA_LOCALIZEDTEXT("en-US", ACCESSED_VARIABLE_NAME);
  attr.description = UA_LOCALIZEDTEXT("en-US", ACCESSED_VARIABLE_NAME);

  status =
          UA_Server_addVariableNode_rb(server,
                                       UA_NODEID_NUMERIC(plugin->ns, 0),
                                       plugin->event_obj,
                                       UA_NODEID_NUMERIC(0,
                                                         UA_NS0ID_HASPROPERTY),
                                       UA_QUALIFIEDNAME(plugin->ns,
                                                        ACCESSED_VARIABLE_NAME),
                                       UA_NODEID_NUMERIC(0,
                                                         UA_NS0ID_PROPERTYTYPE),
                                       vattr,
                                       NULL,
                                       plugin->rbd,
                                       NULL);
  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "UA_Server_addVariableNode_rb() failed: %s",
              UA_StatusCode_name(status));
    return FALSE;
  }

  return TRUE;
}

static void
plugin_cleanup(void)
{
  g_assert(plugin != NULL);

  if (plugin->event_handler != NULL) {
    if (plugin->sub_id) {
      ax_event_handler_unsubscribe_and_notify(plugin->event_handler,
                                              plugin->sub_id,
                                              NULL,
                                              NULL,
                                              NULL);
    }

    ax_event_handler_free(plugin->event_handler);
  }

  /* If the node is created the namespace is never 0 */
  if (plugin->event_obj.namespaceIndex) {
    UA_NodeId_clear(&plugin->event_obj);
  }

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

  if (!create_event_object(server, err)) {
    g_prefix_error(err, "create_event_object() failed: ");
    goto err_out;
  }

  if (!setup_ax_event(server, simple_opc_event_cb, err)) {
    g_prefix_error(err, "setup_ax_event() failed: ");
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
