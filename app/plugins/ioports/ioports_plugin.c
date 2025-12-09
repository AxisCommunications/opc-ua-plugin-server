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
#include <errno.h>
#include <gio/gio.h>
#include <glib.h>

#include "error.h"
#include "ioports_nodeids.h"
#include "ioports_ns.h"
#include "ioports_plugin.h"
#include "ioports_vapix.h"
#include "log.h"
#include "ua_utils.h"
#include "vapix_utils.h"

#define UA_PLUGIN_NAME "opc-ioports-plugin"

#define ERR_NOT_INITIALIZED "The " UA_PLUGIN_NAME " is not initialized"
#define ERR_NO_NAME         "The " UA_PLUGIN_NAME " was not given a name"

#define IOP_HT_LOCK(mtx)   (g_mutex_lock(&plugin->mtx))
#define IOP_HT_UNLOCK(mtx) (g_mutex_unlock(&plugin->mtx))

#define IOP_DBUS_CFG_SERVICE "com.axis.Configuration.Legacy.IOControl1.IOPort"
#define IOP_LABEL_FMT        "I/O Port %d"

#define IOP_STATE_CHANGE             0
#define IOP_CFG_CHANGE               1
#define IOP_STATE_CHANGE_EV_SEVERITY 100

/* parameters monitored for value changes via AxEvent */
#define IOP_CFG_CHANGE_NAME  "Name"
#define IOP_CFG_CHANGE_USAGE "Usage"
#define IOP_CFG_CHANGE_DIR   "Direction"
/* normal state changes have have different names depending on direction */
#define IOP_CFG_CHANGE_NS_IN  "Trig"
#define IOP_CFG_CHANGE_NS_OUT "Active"

#define NAME_PROP   0
#define USAGE_PROP  1
#define STATE_PROP  0
#define NSTATE_PROP 1

DEFINE_GQUARK(UA_PLUGIN_NAME)

typedef struct plugin {
  /* pointer to the server instance */
  UA_Server *server;
  /* user-friendly name of the plugin */
  gchar *name;
  /* OPC-UA namespace index */
  UA_UInt16 ns;
  /* keep track of data that needs to be rolled back in case of failure */
  rollback_data_t *rbd;

  /* an open62541 logger */
  UA_Logger *logger;
  /* hash table with the I/O Ports returned by VAPIX 'getPorts' */
  GHashTable *iop_ht;
  GMutex iop_mtx;

  /* AxEvent handlers */
  /* monitor I/O port state changes */
  AXEventHandler *iopstate_evh;
  /* monitor I/O port configuration changes */
  AXEventHandler *iopcfg_evh;
  /* event subscriptions for state and configuration changes respectively */
  guint event_subs[2];

  gchar *vapix_credentials;
  CURL *curl_h;
} plugin_t;

typedef struct ioport_proptype_map {
  gchar *browse_name;
  UA_DataType *data_type_array;
  guint16 data_type_index;
} ioport_proptype_map_t;

/* structure passed in as nodeContext to the I/O port object node contructor */
typedef struct ua_ioport_obj {
  UA_Boolean configurable;
  UA_IOPortDirectionType direction;
  UA_Boolean disabled;
  UA_UInt32 index;
  UA_String name;
  UA_IOPortStateType normalState;
  UA_IOPortStateType state;
  UA_String usage;
} ua_ioport_obj_t;

static plugin_t *plugin;

static const ioport_proptype_map_t IOPort_obj_type_map[] = {
  { CONFIGURABLE_BNAME, UA_TYPES, UA_TYPES_BOOLEAN },
  { DIRECTION_BNAME, UA_TYPES_IOP, UA_TYPES_IOP_IOPORTDIRECTIONTYPE },
  { DISABLED_BNAME, UA_TYPES, UA_TYPES_BOOLEAN },
  { INDEX_BNAME, UA_TYPES, UA_TYPES_INT32 },
  { NAME_BNAME, UA_TYPES, UA_TYPES_STRING },
  { NORMALSTATE_BNAME, UA_TYPES_IOP, UA_TYPES_IOP_IOPORTSTATETYPE },
  { STATE_BNAME, UA_TYPES_IOP, UA_TYPES_IOP_IOPORTSTATETYPE },
  { USAGE_BNAME, UA_TYPES, UA_TYPES_STRING },
  { NULL, NULL, 0 }
};

/* Local functions */
/* Wrapper around g_ascii_strtoll() using decimal base & with error handling */
static gint64
ascii_strtoll_dec(const gchar *nptr, GError **error)
{
  gchar *endptr = NULL;
  gint64 value = 0;

  g_assert(nptr != NULL);
  g_assert(error == NULL || *error == NULL);

  if (*nptr == '\0') {
    SET_ERROR(error, -1, "Empty string");
    return 0;
  }

  errno = 0;
  value = g_ascii_strtoll(nptr, &endptr, 10);

  /* out of range */
  if (errno == ERANGE) {
    SET_ERROR(error, -1, "String '%s' out of gint64 range", nptr);
    return 0;
  }

  /* no characters consumed */
  if (endptr == nptr) {
    SET_ERROR(error, -1, "Failed converting '%s': no valid digits", nptr);
    return 0;
  }

  /* invalid trailing characters */
  if (*endptr != '\0') {
    SET_ERROR(error,
              -1,
              "Failed converting '%s': trailing junk at: '%s'",
              nptr,
              endptr);
    return 0;
  }

  return value;
}

static gpointer
get_member_from_browsename(const ua_ioport_obj_t *ioport, const gchar *bname)
{
  g_assert(ioport != NULL);
  g_assert(bname != NULL);

  if (g_strcmp0(bname, CONFIGURABLE_BNAME) == 0) {
    return (gpointer) &ioport->configurable;
  } else if (g_strcmp0(bname, DIRECTION_BNAME) == 0) {
    return (gpointer) &ioport->direction;
  } else if (g_strcmp0(bname, DISABLED_BNAME) == 0) {
    return (gpointer) &ioport->disabled;
  } else if (g_strcmp0(bname, INDEX_BNAME) == 0) {
    return (gpointer) &ioport->index;
  } else if (g_strcmp0(bname, NAME_BNAME) == 0) {
    return (gpointer) &ioport->name;
  } else if (g_strcmp0(bname, NORMALSTATE_BNAME) == 0) {
    return (gpointer) &ioport->normalState;
  } else if (g_strcmp0(bname, STATE_BNAME) == 0) {
    return (gpointer) &ioport->state;
  } else if (g_strcmp0(bname, USAGE_BNAME) == 0) {
    return (gpointer) &ioport->usage;
  }

  return NULL;
}

/* Find out and return the nodeId of an ioport object property node given its
 * browseName.
 *
 * Input parameters:
 *  * server
 *  * start_node  - the parent nodeId to start descending from
 *  * referenceTypeId - UA_NS0ID_ORGANIZES | UA_NS0ID_HASPROPERTY
 *  * browse_name - the browse name of the child node (property) we are looking
 *                  for
 *
 * Output parameters:
 *  * out_nodeId - the nodeId of the child node
 *  * error      - a GError in case of error */
static gboolean
iop_ua_get_nodeid_from_browsename(UA_Server *server,
                                  const UA_NodeId *start_node,
                                  UA_UInt32 referenceTypeId,
                                  gchar *browse_name,
                                  UA_NodeId *out_nodeId,
                                  GError **error)
{
  UA_RelativePathElement rpe;
  UA_BrowsePath bp;
  UA_BrowsePathResult bpr;
  UA_StatusCode status;

  gboolean found = FALSE;

  g_assert(server != NULL);
  g_assert(start_node != NULL);
  g_assert(referenceTypeId == UA_NS0ID_HASPROPERTY ||
           referenceTypeId == UA_NS0ID_ORGANIZES);
  g_assert(browse_name != NULL);
  g_assert(out_nodeId != NULL);
  g_assert(error == NULL || *error == NULL);

  g_assert(plugin != NULL);

  UA_RelativePathElement_init(&rpe);
  rpe.referenceTypeId = UA_NODEID_NUMERIC(0, referenceTypeId);
  rpe.isInverse = FALSE;
  rpe.includeSubtypes = FALSE;
  rpe.targetName = UA_QUALIFIEDNAME(plugin->ns, browse_name);

  UA_BrowsePath_init(&bp);
  bp.startingNode = *start_node;
  bp.relativePath.elementsSize = 1;
  bp.relativePath.elements = &rpe;

  UA_BrowsePathResult_init(&bpr);
  bpr = UA_Server_translateBrowsePathToNodeIds(server, &bp);
  if (bpr.statusCode != UA_STATUSCODE_GOOD || bpr.targetsSize < 1) {
    SET_ERROR(error,
              -1,
              "Unable to find nodeId of obj property '%s', err: %s",
              browse_name,
              UA_StatusCode_name(bpr.statusCode));
    goto bail_out;

  } else {
    status = UA_NodeId_copy(&bpr.targets[0].targetId.nodeId, out_nodeId);
    if (status != UA_STATUSCODE_GOOD) {
      SET_ERROR(error,
                -1,
                "UA_NodeId_copy() failed: %s",
                UA_StatusCode_name(status));
      goto bail_out;
    }

    found = TRUE;
  }

bail_out:
  UA_BrowsePathResult_clear(&bpr);

  return found;
}

/* Get the parent nodeId (the ioport object node id) of an ioport object
 * property node.
 *
 * Input parameters:
 *  * server
 *  * nodeId - the browse name of the property node
 *
 * Output parameters:
 *  * parent_nodeId - the nodeId of the parent node (the ioport object node the
 *                    property belongs to)
 *  * error         - a GError in case of an error */
static gboolean
iop_ua_get_iop_prop_parent(UA_Server *server,
                           const UA_NodeId *nodeId,
                           UA_NodeId *parent_nodeId,
                           GError **error)
{
  gboolean retval = FALSE;
  UA_BrowseDescription bd;
  UA_BrowseResult br;
  UA_StatusCode status;

  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(parent_nodeId != NULL);
  g_assert(error == NULL || *error == NULL);

  /* find our parent node (the I/O port object we belong to) */
  UA_BrowseDescription_init(&bd);
  bd.nodeId = *nodeId;
  bd.browseDirection = UA_BROWSEDIRECTION_INVERSE;
  bd.resultMask =
          UA_BROWSERESULTMASK_REFERENCETYPEID | UA_BROWSERESULTMASK_ISFORWARD;

  br = UA_Server_browse(server, 0, &bd);
  if (br.statusCode != UA_STATUSCODE_GOOD) {
    SET_ERROR(error,
              -1,
              "UA_Server_browse() failed: %s",
              UA_StatusCode_name(br.statusCode));
    goto err_out;
  }

  status = UA_NodeId_copy(&br.references[0].nodeId.nodeId, parent_nodeId);
  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(error,
              -1,
              "UA_NodeId_copy() failed: %s",
              UA_StatusCode_name(status));
    goto err_out;
  }

  retval = TRUE;

err_out:
  UA_BrowseResult_clear(&br);

  return retval;
}

/* Given the nodeId of any ioport object property, find out and return the
 * nodeId of another property (sibling node) of the same object.
 *
 * Input parameters:
 *  * server
 *  * nodeId     - the nodeId of any ioport object property
 *  * browseName - the Browse Name of the other property node for which we want
 *                 to get the nodeId
 *
 * Output parameters:
 *  * out_nodeId - the nodeId of the targeted 'browseName' property
 *  * error      - a GError in case an error occurs */
static gboolean
iop_ua_get_iop_prop_nodeid(UA_Server *server,
                           const UA_NodeId *nodeId,
                           gchar *browse_name,
                           UA_NodeId *out_nodeId,
                           GError **error)
{
  gboolean retval = FALSE;
  UA_NodeId parent = UA_NODEID_NULL;

  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(browse_name != NULL);
  g_assert(out_nodeId != NULL);
  g_assert(error == NULL || *error == NULL);

  g_assert(plugin != NULL);

  /* find our parent node (the I/O port object we belong to) */
  if (!iop_ua_get_iop_prop_parent(server, nodeId, &parent, error)) {
    g_prefix_error(error, "iop_ua_get_iop_prop_parent() failed: ");
    goto err_out;
  }

  /* having now the starting point, browse for 'browse_name'
   * to find its nodeId */
  if (!iop_ua_get_nodeid_from_browsename(server,
                                         &parent,
                                         UA_NS0ID_HASPROPERTY,
                                         browse_name,
                                         out_nodeId,
                                         error)) {
    g_prefix_error(error,
                   "iop_ua_get_nodeid_from_browsename() failed nodeId lookup "
                   "of '%s': ",
                   browse_name);
    goto err_out;
  }

  retval = TRUE;

err_out:
  UA_NodeId_clear(&parent);

  return retval;
}

/* Given the nodeId of any ioport object property, return the value of any other
 * property node given its browseName.
 *
 * Input parameters:
 *  * server
 *  * nodeId      - the nodeId of any ioport object property
 *  * browse_name - the Browse Name of the other property for which we want to
 *                  fetch the value
 *
 * Output parameters:
 *  * ua_value - the value (UA_Variant) of the requested property node
 *  * error    - a GError in case an error occurs */
static gboolean
iop_ua_get_iop_prop_value(UA_Server *server,
                          const UA_NodeId *nodeId,
                          gchar *browse_name,
                          UA_Variant *ua_value,
                          GError **error)
{
  gboolean retval = FALSE;
  UA_StatusCode ua_status = UA_STATUSCODE_BAD;
  UA_NodeId parent = UA_NODEID_NULL;

  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(browse_name != NULL);
  g_assert(ua_value != NULL);
  g_assert(error == NULL || *error == NULL);

  g_assert(plugin != NULL);

  /* find our parent node (the I/O port object we belong to) */
  if (!iop_ua_get_iop_prop_parent(server, nodeId, &parent, error)) {
    g_prefix_error(error, "iop_ua_get_iop_prop_parent() failed: ");
    goto err_out;
  }

  /* read the value of the 'browse_name' property */
  ua_status = UA_Server_readObjectProperty(server,
                                           parent,
                                           UA_QUALIFIEDNAME(plugin->ns,
                                                            browse_name),
                                           ua_value);
  if (ua_status != UA_STATUSCODE_GOOD) {
    SET_ERROR(error,
              -1,
              "UA_Server_readObjectProperty(...'%s'...) failed: %s",
              browse_name,
              UA_StatusCode_name(ua_status));
    goto err_out;
  }

  retval = TRUE;

err_out:
  UA_NodeId_clear(&parent);

  return retval;
}

/* Given any property node of an ioport object, returns the 'Index' value of
 * the respective ioport object.
 *
 * Input parameters:
 *  * server -
 *  * nodeId - the nodeId of any ioport object property
 *
 * Output parameters:
 *  * iop_index - the value of the 'Index' property of the same ioport object
 *  * error     - a GError if an error occurs */
static gboolean
iop_ua_get_iop_index(UA_Server *server,
                     const UA_NodeId *nodeId,
                     UA_UInt32 *iop_index,
                     GError **error)
{
  UA_Variant ua_value;

  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(iop_index != NULL);
  g_assert(error == NULL || *error == NULL);

  g_assert(plugin != NULL);

  if (!iop_ua_get_iop_prop_value(server,
                                 nodeId,
                                 INDEX_BNAME,
                                 &ua_value,
                                 error)) {
    g_prefix_error(error, "Could not fetch 'Index' property: ");
    return FALSE;
  }
  *iop_index = *(UA_UInt32 *) ua_value.data;
  UA_Variant_clear(&ua_value);

  return TRUE;
}

/**
 * Adds a new I/O Port object to the server.
 *
 * Input parameters:
 *  * server    - OPC-UA server instance
 *  * port_nr   - port index (0-based indexing)
 *  * port_data - a structure describing the properties of the I/O Port object
 *
 * Output parameters:
 *  * error - a GError if an error occurs
 *
 * Return value:
 *  TRUE if successful, FALSE otherwise
 */
static gboolean
iop_add_ioport_object(UA_Server *server,
                      guint32 port_nr,
                      const ioport_obj_t *port_data,
                      GError **err)
{
  gboolean ret = TRUE;
  UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
  UA_StatusCode status;
  ua_ioport_obj_t node_ctx;
  gchar *label;

  g_assert(server != NULL);
  g_assert(port_data != NULL);
  g_assert(err == NULL || *err == NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->rbd != NULL);

  /* NOTE: the web GUI uses 1-based indexing */
  label = g_strdup_printf(IOP_LABEL_FMT, port_nr + 1);
  oAttr.displayName = UA_LOCALIZEDTEXT("", label);
  oAttr.description = UA_LOCALIZEDTEXT("", "I/O port");

  /* node context to be passed on to the iop_ua_obj_constructor() callback */
  node_ctx.index = port_nr;
  node_ctx.configurable = port_data->configurable;
  node_ctx.direction = port_data->direction;
  node_ctx.disabled = port_data->readonly;
  node_ctx.name = UA_STRING(port_data->name);
  node_ctx.normalState = port_data->normal_state;
  node_ctx.state = port_data->state;
  node_ctx.usage = UA_STRING(port_data->usage);

  /* create an object instance for the given IO port and add it to the
   * "I/O Ports" parent object */
  status = UA_Server_addObjectNode_rb(server,
                                      UA_NODEID_NUMERIC(plugin->ns, 0),
                                      UA_NODEID_NUMERIC(plugin->ns,
                                                        UA_IOPID_IOPORTS),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                      UA_QUALIFIEDNAME(plugin->ns, label),
                                      UA_NODEID_NUMERIC(plugin->ns,
                                                        UA_IOPID_IOPORTOBJTYPE),
                                      oAttr,
                                      &node_ctx,
                                      plugin->rbd,
                                      NULL);
  if (status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "UA_Server_addObjectNode_rb('%s') failed: %s",
              label,
              UA_StatusCode_name(status));
    ret = FALSE;
  }

  g_clear_pointer(&label, g_free);

  return ret;
}

/* callback backend - gets the string value of the 'Name' or 'Usage'
 * property of an IO port */
static UA_StatusCode
iop_ua_get_string(UA_Server *server,
                  G_GNUC_UNUSED const UA_NodeId *sessionId,
                  G_GNUC_UNUSED void *sessionContext,
                  const UA_NodeId *nodeId,
                  G_GNUC_UNUSED void *nodeContext,
                  G_GNUC_UNUSED UA_Boolean includeSourceTimeStamp,
                  G_GNUC_UNUSED const UA_NumericRange *range,
                  UA_DataValue *dataValue,
                  gint property)
{
  GError *lerr = NULL;
  ioport_obj_t *iop = NULL;
  UA_StatusCode ua_status;
  UA_UInt32 iop_index;
  UA_String ua_string;

  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(dataValue != NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->iop_ht != NULL);
  g_assert(plugin->logger != NULL);

  /* property can only be one of 'Name' or 'Usage' */
  if ((property != NAME_PROP) && (property != USAGE_PROP)) {
    LOG_E(plugin->logger, "Invalid property: %d in read request!", property);
    return UA_STATUSCODE_BAD;
  }

  /* open62541 'false' (flags if dataValue holds any data) */
  dataValue->hasValue = FALSE;

  /* find the index (port number) of the I/O port node we belong to */
  if (!iop_ua_get_iop_index(server, nodeId, &iop_index, &lerr)) {
    LOG_E(plugin->logger,
          "iop_ua_get_iop_index() failed: %s",
          GERROR_MSG(lerr));
    g_clear_error(&lerr);
    return UA_STATUSCODE_BADNOTFOUND;
  }

  IOP_HT_LOCK(iop_mtx);
  iop = g_hash_table_lookup(plugin->iop_ht, &iop_index);
  if (!iop) {
    LOG_E(plugin->logger, "g_hash_table_lookup(port: %d) failed", iop_index);
    IOP_HT_UNLOCK(iop_mtx);
    return UA_STATUSCODE_BADINTERNALERROR;
  }

  ua_string = (property == NAME_PROP) ? UA_STRING(iop->name) :
                                        UA_STRING(iop->usage);
  IOP_HT_UNLOCK(iop_mtx);

  ua_status = UA_Variant_setScalarCopy(&dataValue->value,
                                       &ua_string,
                                       &UA_TYPES[UA_TYPES_STRING]);
  if (ua_status != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "UA_Variant_setScalarCopy() failed: %s",
          UA_StatusCode_name(ua_status));

    return ua_status;
  }

  dataValue->hasValue = TRUE;

  return UA_STATUSCODE_GOOD;
}

/* callback backend - sets the 'Name' or 'Usage' property of an IO port */
static UA_StatusCode
iop_ua_set_string(UA_Server *server,
                  G_GNUC_UNUSED const UA_NodeId *sessionId,
                  G_GNUC_UNUSED void *sessionContext,
                  const UA_NodeId *nodeId,
                  G_GNUC_UNUSED void *nodeContext,
                  G_GNUC_UNUSED const UA_NumericRange *range,
                  const UA_DataValue *dataValue,
                  gint property)
{
  gchar *new_string = NULL;
  GError *lerr = NULL;
  UA_UInt32 iop_index;
  UA_String *ua_str;
  UA_StatusCode ret = UA_STATUSCODE_GOOD;

  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(dataValue != NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->curl_h != NULL);
  g_assert(plugin->vapix_credentials != NULL);
  g_assert(plugin->logger != NULL);

  /* property can only be one of 'Name' or 'Usage' */
  if ((property != NAME_PROP) && (property != USAGE_PROP)) {
    LOG_E(plugin->logger, "Invalid property: %d in write request!", property);
    return UA_STATUSCODE_BAD;
  }

  /* get the new 'Name' or 'Usage' value from the OPC-UA write node request
   * (dataValue container) */
  ua_str = (UA_String *) dataValue->value.data;
  new_string = g_strndup((gchar *) ua_str->data, ua_str->length);
  if (new_string == NULL) {
    ret = UA_STATUSCODE_BAD;
    goto err_out;
  }

  if (!iop_ua_get_iop_index(server, nodeId, &iop_index, &lerr)) {
    LOG_E(plugin->logger,
          "iop_ua_get_iop_index() failed: %s",
          GERROR_MSG(lerr));

    ret = UA_STATUSCODE_BADNOTFOUND;
    goto err_out;
  }

  if (!iop_vapix_set_port(plugin->curl_h,
                          plugin->vapix_credentials,
                          iop_index,
                          (property == NAME_PROP) ? IO_VAPIX_JSON_NAME :
                                                    IO_VAPIX_JSON_USAGE,
                          new_string,
                          &lerr)) {
    LOG_E(plugin->logger, "iop_vapix_set_port() failed: %s", GERROR_MSG(lerr));

    ret = UA_STATUSCODE_BADINTERNALERROR;
    goto err_out;
  }

err_out:
  g_clear_error(&lerr);
  g_clear_pointer(&new_string, g_free);

  return ret;
}

/* callback backend - gets the value of the 'State' or 'NormalState' property of
 * an IO port */
static UA_StatusCode
iop_ua_get_state(UA_Server *server,
                 G_GNUC_UNUSED const UA_NodeId *sessionId,
                 G_GNUC_UNUSED void *sessionContext,
                 const UA_NodeId *nodeId,
                 G_GNUC_UNUSED void *nodeContext,
                 G_GNUC_UNUSED UA_Boolean includeSourceTimeStamp,
                 G_GNUC_UNUSED const UA_NumericRange *range,
                 UA_DataValue *dataValue,
                 gint property)
{
  GError *lerr = NULL;
  ioport_obj_t *iop = NULL;
  UA_IOPortStateType ua_state;
  UA_StatusCode ua_status;
  UA_UInt32 iop_index;

  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(dataValue != NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->iop_ht != NULL);
  g_assert(plugin->logger != NULL);

  /* property can only be one of 'State' or 'NormalState' */
  if ((property != STATE_PROP) && (property != NSTATE_PROP)) {
    LOG_E(plugin->logger, "Invalid property: %d in read request!", property);
    return UA_STATUSCODE_BAD;
  }

  /* open62541 'false' (flags if dataValue holds any data) */
  dataValue->hasValue = FALSE;

  if (!iop_ua_get_iop_index(server, nodeId, &iop_index, &lerr)) {
    LOG_E(plugin->logger,
          "iop_ua_get_iop_index() failed: %s",
          GERROR_MSG(lerr));
    g_clear_error(&lerr);
    return UA_STATUSCODE_BADNOTFOUND;
  }

  IOP_HT_LOCK(iop_mtx);
  iop = g_hash_table_lookup(plugin->iop_ht, &iop_index);
  if (!iop) {
    LOG_E(plugin->logger, "g_hash_table_lookup(port: %d) failed", iop_index);
    IOP_HT_UNLOCK(iop_mtx);
    return UA_STATUSCODE_BADINTERNALERROR;
  }

  ua_state = (property == STATE_PROP) ? iop->state : iop->normal_state;
  IOP_HT_UNLOCK(iop_mtx);

  ua_status =
          UA_Variant_setScalarCopy(&dataValue->value,
                                   &ua_state,
                                   &UA_TYPES_IOP[UA_TYPES_IOP_IOPORTSTATETYPE]);
  if (ua_status != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "UA_Variant_setScalarCopy() failed: %s",
          UA_StatusCode_name(ua_status));
    return ua_status;
  }

  dataValue->hasValue = TRUE;

  return UA_STATUSCODE_GOOD;
}

/* callback backend - sets the value of the 'State' or 'NormalState' property of
 * an IO port */
static UA_StatusCode
iop_ua_set_state(UA_Server *server,
                 G_GNUC_UNUSED const UA_NodeId *sessionId,
                 G_GNUC_UNUSED void *sessionContext,
                 const UA_NodeId *nodeId,
                 G_GNUC_UNUSED void *nodeContext,
                 G_GNUC_UNUSED const UA_NumericRange *range,
                 const UA_DataValue *dataValue,
                 gint property)
{
  GError *lerr = NULL;
  const gchar *new_state = NULL;
  UA_UInt32 iop_index;
  UA_IOPortStateType ua_newstate;

  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(dataValue != NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->curl_h != NULL);
  g_assert(plugin->vapix_credentials != NULL);
  g_assert(plugin->logger != NULL);

  /* property can only be one of 'State' or 'NormalState' */
  if ((property != STATE_PROP) && (property != NSTATE_PROP)) {
    LOG_E(plugin->logger, "Invalid property: %d in write request!", property);
    return UA_STATUSCODE_BAD;
  }

  /* get the requested state from the OPC-UA write node request */
  ua_newstate = *(UA_IOPortStateType *) dataValue->value.data;
  switch (ua_newstate) {
  case UA_IOPORTSTATETYPE_OPEN:
    new_state = IO_VAPIX_STATE_OPEN;
    break;

  case UA_IOPORTSTATETYPE_CLOSED:
    new_state = IO_VAPIX_STATE_CLOSED;
    break;

  default:
    LOG_E(plugin->logger,
          "Invalid port state value: %d in node write request!",
          ua_newstate);
    return UA_STATUSCODE_BAD;
  }

  /* find the index (port number) of the I/O port node we belong to */
  if (!iop_ua_get_iop_index(server, nodeId, &iop_index, &lerr)) {
    LOG_E(plugin->logger,
          "iop_ua_get_iop_index() failed: %s",
          GERROR_MSG(lerr));
    g_clear_error(&lerr);
    return UA_STATUSCODE_BADNOTFOUND;
  }

  if (!iop_vapix_set_port(plugin->curl_h,
                          plugin->vapix_credentials,
                          iop_index,
                          (property == STATE_PROP) ? IO_VAPIX_JSON_STATE :
                                                     IO_VAPIX_JSON_NSTATE,
                          new_state,
                          &lerr)) {
    LOG_E(plugin->logger, "iop_vapix_set_port() failed: %s", GERROR_MSG(lerr));
    g_clear_error(&lerr);
    return UA_STATUSCODE_BADINTERNALERROR;
  }

  return UA_STATUSCODE_GOOD;
}

/* callback executed when the 'Name' property of an IO port is read */
static UA_StatusCode
iop_ua_read_name_cb(UA_Server *server,
                    const UA_NodeId *sessionId,
                    void *sessionContext,
                    const UA_NodeId *nodeId,
                    void *nodeContext,
                    UA_Boolean includeSourceTimeStamp,
                    const UA_NumericRange *range,
                    UA_DataValue *dataValue)
{
  /* call backend function to get the value of the 'Name' property */
  return iop_ua_get_string(server,
                           sessionId,
                           sessionContext,
                           nodeId,
                           nodeContext,
                           includeSourceTimeStamp,
                           range,
                           dataValue,
                           NAME_PROP);
}

/* callback executed when the 'Name' property of an IO port is written */
static UA_StatusCode
iop_ua_write_name_cb(UA_Server *server,
                     const UA_NodeId *sessionId,
                     void *sessionContext,
                     const UA_NodeId *nodeId,
                     void *nodeContext,
                     const UA_NumericRange *range,
                     const UA_DataValue *dataValue)
{
  /* call backend function to set a new value for the 'Name' property */
  return iop_ua_set_string(server,
                           sessionId,
                           sessionContext,
                           nodeId,
                           nodeContext,
                           range,
                           dataValue,
                           NAME_PROP);
}

/* callback executed when the 'Usage' property of an IO port is read */
static UA_StatusCode
iop_ua_read_usage_cb(UA_Server *server,
                     const UA_NodeId *sessionId,
                     void *sessionContext,
                     const UA_NodeId *nodeId,
                     void *nodeContext,
                     UA_Boolean includeSourceTimeStamp,
                     const UA_NumericRange *range,
                     UA_DataValue *dataValue)
{
  /* call backend function to get the value of the 'Usage' property */
  return iop_ua_get_string(server,
                           sessionId,
                           sessionContext,
                           nodeId,
                           nodeContext,
                           includeSourceTimeStamp,
                           range,
                           dataValue,
                           USAGE_PROP);
}

/* callback executed when the 'Usage' property of an IO port is written */
static UA_StatusCode
iop_ua_write_usage_cb(UA_Server *server,
                      const UA_NodeId *sessionId,
                      void *sessionContext,
                      const UA_NodeId *nodeId,
                      void *nodeContext,
                      const UA_NumericRange *range,
                      const UA_DataValue *dataValue)
{
  /* call backend function to set the value of the 'Usage' property */
  return iop_ua_set_string(server,
                           sessionId,
                           sessionContext,
                           nodeId,
                           nodeContext,
                           range,
                           dataValue,
                           USAGE_PROP);
}

/* callback executed when the 'Direction' property of an IO port is read */
static UA_StatusCode
iop_ua_read_dir_cb(UA_Server *server,
                   G_GNUC_UNUSED const UA_NodeId *sessionId,
                   G_GNUC_UNUSED void *sessionContext,
                   const UA_NodeId *nodeId,
                   G_GNUC_UNUSED void *nodeContext,
                   G_GNUC_UNUSED UA_Boolean includeSourceTimeStamp,
                   G_GNUC_UNUSED const UA_NumericRange *range,
                   UA_DataValue *dataValue)
{
  GError *lerr = NULL;
  ioport_obj_t *iop = NULL;
  UA_IOPortDirectionType ua_dir;
  UA_StatusCode ua_status;
  UA_UInt32 iop_index;

  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(dataValue != NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->iop_ht != NULL);
  g_assert(plugin->logger != NULL);

  /* open62541 'false' (flags if dataValue holds any data) */
  dataValue->hasValue = FALSE;

  if (!iop_ua_get_iop_index(server, nodeId, &iop_index, &lerr)) {
    LOG_E(plugin->logger,
          "iop_ua_get_iop_index() failed: %s",
          GERROR_MSG(lerr));
    g_clear_error(&lerr);
    return UA_STATUSCODE_BADNOTFOUND;
  }

  IOP_HT_LOCK(iop_mtx);
  iop = g_hash_table_lookup(plugin->iop_ht, &iop_index);
  if (!iop) {
    LOG_E(plugin->logger, "g_hash_table_lookup(port: %d) failed", iop_index);
    IOP_HT_UNLOCK(iop_mtx);
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  ua_dir = iop->direction;
  IOP_HT_UNLOCK(iop_mtx);

  ua_status = UA_Variant_setScalarCopy(
          &dataValue->value,
          &ua_dir,
          &UA_TYPES_IOP[UA_TYPES_IOP_IOPORTDIRECTIONTYPE]);
  if (ua_status != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "UA_Variant_setScalarCopy() failed: %s",
          UA_StatusCode_name(ua_status));

    return ua_status;
  }

  dataValue->hasValue = TRUE;

  return UA_STATUSCODE_GOOD;
}

/* callback executed when the 'Direction' property of an IO port is written */
static UA_StatusCode
iop_ua_write_dir_cb(UA_Server *server,
                    G_GNUC_UNUSED const UA_NodeId *sessionId,
                    G_GNUC_UNUSED void *sessionContext,
                    const UA_NodeId *nodeId,
                    G_GNUC_UNUSED void *nodeContext,
                    G_GNUC_UNUSED const UA_NumericRange *range,
                    const UA_DataValue *dataValue)
{
  GError *lerr = NULL;
  const gchar *newdir = NULL;
  UA_IOPortDirectionType ua_newdir;
  UA_Byte access_level;
  UA_StatusCode ua_status;
  UA_UInt32 iop_index;
  UA_NodeId state_nodeId = UA_NODEID_NULL;
  UA_StatusCode ret = UA_STATUSCODE_GOOD;

  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(dataValue != NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->curl_h != NULL);
  g_assert(plugin->vapix_credentials != NULL);
  g_assert(plugin->logger != NULL);

  /* get the requested 'direction' from the OPC-UA write node request */
  ua_newdir = *(UA_IOPortDirectionType *) dataValue->value.data;
  switch (ua_newdir) {
  case UA_IOPORTDIRECTIONTYPE_INPUT:
    newdir = IO_VAPIX_DIR_INPUT;
    break;

  case UA_IOPORTDIRECTIONTYPE_OUTPUT:
    newdir = IO_VAPIX_DIR_OUTPUT;
    break;

  default:
    LOG_E(plugin->logger,
          "Invalid 'Direction' value: %d in node write request!",
          ua_newdir);
    ret = UA_STATUSCODE_BAD;
    goto err_out;
  }

  if (!iop_ua_get_iop_index(server, nodeId, &iop_index, &lerr)) {
    LOG_E(plugin->logger,
          "iop_ua_get_iop_index() failed: %s",
          GERROR_MSG(lerr));

    ret = UA_STATUSCODE_BADNOTFOUND;
    goto err_out;
  }

  if (!iop_vapix_set_port(plugin->curl_h,
                          plugin->vapix_credentials,
                          iop_index,
                          IO_VAPIX_JSON_DIR,
                          newdir,
                          &lerr)) {
    LOG_E(plugin->logger, "iop_vapix_set_port() failed: %s", GERROR_MSG(lerr));

    ret = UA_STATUSCODE_BADINTERNALERROR;
    goto err_out;
  }

  if (ua_newdir == UA_IOPORTDIRECTIONTYPE_OUTPUT) {
    /* port was configured as output, make the 'State' property r/w*/
    access_level = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  } else {
    /* port was configured as input, make the 'State' property r/o*/
    access_level = UA_ACCESSLEVELMASK_READ;
  }

  /* we need to find the nodeid of the 'State' property */
  if (!iop_ua_get_iop_prop_nodeid(server,
                                  nodeId,
                                  "State",
                                  &state_nodeId,
                                  &lerr)) {
    LOG_E(plugin->logger,
          "iop_ua_get_iop_prop_nodeid() failed: %s",
          GERROR_MSG(lerr));

    ret = UA_STATUSCODE_BADINTERNALERROR;
    goto err_out;
  }

  ua_status = UA_Server_writeAccessLevel(server, state_nodeId, access_level);
  if (ua_status != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "Failed to set the access level for port-%d - 'State' node: %s",
          iop_index,
          UA_StatusCode_name(ua_status));

    ret = ua_status;
    goto err_out;
  }

err_out:
  g_clear_error(&lerr);
  UA_NodeId_clear(&state_nodeId);

  return ret;
}

/* callback executed when the 'NormalState' property of an IO port is read */
static UA_StatusCode
iop_ua_read_normalstate_cb(UA_Server *server,
                           const UA_NodeId *sessionId,
                           void *sessionContext,
                           const UA_NodeId *nodeId,
                           void *nodeContext,
                           UA_Boolean includeSourceTimeStamp,
                           const UA_NumericRange *range,
                           UA_DataValue *dataValue)
{
  /* call backend function to get the value of the 'NormalState' property */
  return iop_ua_get_state(server,
                          sessionId,
                          sessionContext,
                          nodeId,
                          nodeContext,
                          includeSourceTimeStamp,
                          range,
                          dataValue,
                          NSTATE_PROP);
}

/* callback executed when the 'NormalState' property of an IO port is written */
static UA_StatusCode
iop_ua_write_normalstate_cb(UA_Server *server,
                            const UA_NodeId *sessionId,
                            void *sessionContext,
                            const UA_NodeId *nodeId,
                            void *nodeContext,
                            const UA_NumericRange *range,
                            const UA_DataValue *dataValue)
{
  /* call backend function to set the value of the 'NormalState' property */
  return iop_ua_set_state(server,
                          sessionId,
                          sessionContext,
                          nodeId,
                          nodeContext,
                          range,
                          dataValue,
                          NSTATE_PROP);
}

/* callback executed when the 'State' property of an IO port is read */
static UA_StatusCode
iop_ua_read_state_cb(UA_Server *server,
                     const UA_NodeId *sessionId,
                     void *sessionContext,
                     const UA_NodeId *nodeId,
                     void *nodeContext,
                     UA_Boolean includeSourceTimeStamp,
                     const UA_NumericRange *range,
                     UA_DataValue *dataValue)
{
  /* call backend function to get the value of the 'State' property */
  return iop_ua_get_state(server,
                          sessionId,
                          sessionContext,
                          nodeId,
                          nodeContext,
                          includeSourceTimeStamp,
                          range,
                          dataValue,
                          STATE_PROP);
}

/* callback executed when the 'State' property of an IO port is written */
static UA_StatusCode
iop_ua_write_state_cb(UA_Server *server,
                      const UA_NodeId *sessionId,
                      void *sessionContext,
                      const UA_NodeId *nodeId,
                      void *nodeContext,
                      const UA_NumericRange *range,
                      const UA_DataValue *dataValue)
{
  /* call backend function to set the value of the 'State' property */
  return iop_ua_set_state(server,
                          sessionId,
                          sessionContext,
                          nodeId,
                          nodeContext,
                          range,
                          dataValue,
                          STATE_PROP);
}

/* Constructor callback for IOPortObjType object nodes.
 * It loops over all the object property nodes and:
 *   - initializes each node with the appropriate value from the data provided
 *     via the nodeContext
 *   - sets the access level (R/O or R/W) for the node according to applicable
 *     conditions
 *   - attaches UA_DataSource callbacks (to handle read/write access) to
 *     applicable property nodes */
static UA_StatusCode
iop_ua_obj_constructor(UA_Server *server,
                       G_GNUC_UNUSED const UA_NodeId *sessionId,
                       G_GNUC_UNUSED void *sessionContext,
                       G_GNUC_UNUSED const UA_NodeId *typeNodeId,
                       G_GNUC_UNUSED void *typeNodeContext,
                       const UA_NodeId *nodeId,
                       void **nodeContext)
{
  const ioport_proptype_map_t *iop_obj = &IOPort_obj_type_map[0];
  const ua_ioport_obj_t *node_ctx = (ua_ioport_obj_t *) *nodeContext;
  GError *lerr = NULL;
  UA_Byte access_level;
  UA_StatusCode ua_status;
  UA_NodeId prop_nodeId = UA_NODEID_NULL;

  /* clang-format off */
  UA_DataSource iop_dir_cb = {
    .read = iop_ua_read_dir_cb,
    .write = iop_ua_write_dir_cb
  };
  UA_DataSource iop_name_cb = {
    .read = iop_ua_read_name_cb,
    .write = iop_ua_write_name_cb
  };
  UA_DataSource iop_usage_cb = {
    .read = iop_ua_read_usage_cb,
    .write = iop_ua_write_usage_cb
  };
  UA_DataSource iop_state_cb = {
    .read = iop_ua_read_state_cb,
    .write = iop_ua_write_state_cb
  };
  UA_DataSource iop_normalstate_cb = {
    .read = iop_ua_read_normalstate_cb,
    .write = iop_ua_write_normalstate_cb
  };
  /* clang-format on */

  g_assert(server != NULL);
  g_assert(nodeId != NULL);
  g_assert(nodeContext != NULL && *nodeContext != NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->logger != NULL);

  /* loop over the 'properties' of this object (children nodes) */
  while (iop_obj->browse_name != NULL) {
    /* update node value with data provided via the nodeContext */
    ua_status = UA_Server_writeObjectProperty_scalar(
            server,
            *nodeId,
            UA_QUALIFIEDNAME(plugin->ns, iop_obj->browse_name),
            get_member_from_browsename(node_ctx, iop_obj->browse_name),
            &iop_obj->data_type_array[iop_obj->data_type_index]);
    if (ua_status != UA_STATUSCODE_GOOD) {
      LOG_E(plugin->logger,
            "UA_Server_writeObjectProperty_scalar(\"%s\") failed: %s",
            iop_obj->browse_name,
            UA_StatusCode_name(ua_status));
      goto err_out;
    }

    /* we need the nodeId of the child property node (prop_nodeId) to be able
     * to update the access level mask */
    if (!iop_ua_get_nodeid_from_browsename(server,
                                           nodeId,
                                           UA_NS0ID_HASPROPERTY,
                                           iop_obj->browse_name,
                                           &prop_nodeId,
                                           &lerr)) {
      LOG_E(plugin->logger,
            "Failed to get nodeId of property '%s': %s",
            iop_obj->browse_name,
            GERROR_MSG(lerr));

      ua_status = UA_STATUSCODE_BAD;
      goto err_out;
    }

    access_level = UA_ACCESSLEVELMASK_READ;

    /* set the correct access levels on certain properties */
    if (node_ctx->disabled) {
      /* all property nodes are R/O if the port is disabled */
      ua_status = UA_Server_writeAccessLevel(server, prop_nodeId, access_level);
      if (ua_status != UA_STATUSCODE_GOOD) {
        LOG_E(plugin->logger,
              "Unable to set the access level for property '%s': %s",
              iop_obj->browse_name,
              UA_StatusCode_name(ua_status));
        goto err_out;
      }
    } else {
      /* 'Direction' - can be r/o or r/w depending on 'Configurable' */
      if (g_strcmp0(iop_obj->browse_name, DIRECTION_BNAME) == 0) {
        if (node_ctx->configurable) {
          access_level = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
        } else {
          access_level = UA_ACCESSLEVELMASK_READ;
        }

        ua_status =
                UA_Server_writeAccessLevel(server, prop_nodeId, access_level);
        if (ua_status != UA_STATUSCODE_GOOD) {
          LOG_E(plugin->logger,
                "Unable to set the access level for property '%s': %s",
                iop_obj->browse_name,
                UA_StatusCode_name(ua_status));
          goto err_out;
        }
      } /* 'Direction' property */

      /* 'State' - can be r/o or r/w depending on 'Direction' */
      if (g_strcmp0(iop_obj->browse_name, STATE_BNAME) == 0) {
        if (node_ctx->direction == UA_IOPORTDIRECTIONTYPE_INPUT) {
          access_level = UA_ACCESSLEVELMASK_READ;
        } else {
          access_level = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
        }

        ua_status =
                UA_Server_writeAccessLevel(server, prop_nodeId, access_level);
        if (ua_status != UA_STATUSCODE_GOOD) {
          LOG_E(plugin->logger,
                "Unable to set the access level for property '%s': %s",
                iop_obj->browse_name,
                UA_StatusCode_name(ua_status));
          goto err_out;
        }
      } /* 'State' property */
    }   /* if disabled */

    /* attach dataSource callbacks to each property where it makes sense */

    /* attach access callbacks to the 'Name' property */
    if (g_strcmp0(iop_obj->browse_name, NAME_BNAME) == 0) {
      ua_status = UA_Server_setVariableNode_dataSource(server,
                                                       prop_nodeId,
                                                       iop_name_cb);
      if (ua_status != UA_STATUSCODE_GOOD) {
        LOG_E(plugin->logger,
              "Unable to set dataSource callback for property '%s': %s",
              iop_obj->browse_name,
              UA_StatusCode_name(ua_status));
        goto err_out;
      }
    }

    /* attach access callbacks to the 'Usage' property */
    if (g_strcmp0(iop_obj->browse_name, USAGE_BNAME) == 0) {
      ua_status = UA_Server_setVariableNode_dataSource(server,
                                                       prop_nodeId,
                                                       iop_usage_cb);
      if (ua_status != UA_STATUSCODE_GOOD) {
        LOG_E(plugin->logger,
              "Unable to set dataSource callback for property '%s': %s",
              iop_obj->browse_name,
              UA_StatusCode_name(ua_status));
        goto err_out;
      }
    }

    /* attach access callback to the 'Direction' property */
    if (g_strcmp0(iop_obj->browse_name, DIRECTION_BNAME) == 0) {
      ua_status = UA_Server_setVariableNode_dataSource(server,
                                                       prop_nodeId,
                                                       iop_dir_cb);
      if (ua_status != UA_STATUSCODE_GOOD) {
        LOG_E(plugin->logger,
              "Unable to set dataSource callback for property '%s': %s",
              iop_obj->browse_name,
              UA_StatusCode_name(ua_status));
        goto err_out;
      }
    }

    /* attach access callback to the 'State' property */
    if (g_strcmp0(iop_obj->browse_name, STATE_BNAME) == 0) {
      ua_status = UA_Server_setVariableNode_dataSource(server,
                                                       prop_nodeId,
                                                       iop_state_cb);
      if (ua_status != UA_STATUSCODE_GOOD) {
        LOG_E(plugin->logger,
              "Unable to set dataSource callback for property '%s': %s",
              iop_obj->browse_name,
              UA_StatusCode_name(ua_status));
        goto err_out;
      }
    }

    /* attach access callback to the 'NormalState' property */
    if (g_strcmp0(iop_obj->browse_name, NORMALSTATE_BNAME) == 0) {
      ua_status = UA_Server_setVariableNode_dataSource(server,
                                                       prop_nodeId,
                                                       iop_normalstate_cb);
      if (ua_status != UA_STATUSCODE_GOOD) {
        LOG_E(plugin->logger,
              "Unable to set dataSource callback for property '%s': %s",
              iop_obj->browse_name,
              UA_StatusCode_name(ua_status))
        goto err_out;
      }
    }

    /* done with this property node */
    UA_NodeId_clear(&prop_nodeId);

    /* go to next entry */
    iop_obj++;
  } /* while */

  /* set the EventNotifier attribute for this I/O port object node */
  ua_status = UA_Server_writeEventNotifier(server,
                                           *nodeId,
                                           UA_EVENTNOTIFIER_SUBSCRIBE_TO_EVENT);
  if (ua_status != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "UA_Server_writeEventNotifier() failed: %s",
          UA_StatusCode_name(ua_status));
    goto err_out;
  }

  return ua_status;

err_out:
  g_clear_error(&lerr);
  UA_NodeId_clear(&prop_nodeId);

  return ua_status;
}

/**
 * Attaches lifecycle callbacks (an object constructor in particular) to nodes
 * of IOPortObjType.
 *
 * Input parameters:
 *  * server  - OPC-UA server instance
 *  * ns      - the namespace index of the IOPortObjType
 *
 * Return value:
 *  * UA_STATUSCODE_GOOD if successful or a different UA_StatusCode otherwise
 */
static UA_StatusCode
set_ioport_lifecycle_cb(UA_Server *server, UA_UInt16 ns)
{
  UA_NodeTypeLifecycle lifecycle;

  g_assert(server != NULL);

  lifecycle.constructor = iop_ua_obj_constructor;
  lifecycle.destructor = NULL;

  return UA_Server_setNodeTypeLifecycle(
          server,
          UA_NODEID_NUMERIC(ns, UA_IOPID_IOPORTOBJTYPE),
          lifecycle);
}

/**
 * Creates a new OPC-UA event object to be emitted when an I/O port changes its
 * state.
 *
 * Input parameters:
 *  * server          - OPC-UA server instance
 *  * eventSourceName - browse name of originating node
 *  * ua_state        - the new state of the I/O port
 *  * eventTypeId     - the type ID of the event
 *  * eventSeverity   - event severity
 *
 * Output parameters:
 *  * outId           - node ID of the OPC-UA event object
 *
 * Returns:
 *  * UA_STATUSCODE_GOOD if successful or a different UA_StatusCode otherwise
 */
static UA_StatusCode
iop_ua_create_event(UA_Server *server,
                    UA_String eventSourceName,
                    UA_IOPortStateType ua_state,
                    UA_UInt32 eventTypeId,
                    UA_UInt16 eventSeverity,
                    UA_NodeId *outId)
{
  UA_StatusCode ret;
  UA_DateTime eventTime;
  UA_LocalizedText eventMessage;

  g_assert(server != NULL);
  g_assert(outId != NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->logger != NULL);

  ret = UA_Server_createEvent(server,
                              UA_NODEID_NUMERIC(plugin->ns, eventTypeId),
                              outId);
  if (ret != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "UA_Server_createEvent() failed: %s",
          UA_StatusCode_name(ret));
    return ret;
  }

  /* Set the Event Attributes */
  /* Setting the Time is required or else the event will not show up in
   * UAExpert! */
  eventTime = UA_DateTime_now();
  ret = UA_Server_writeObjectProperty_scalar(server,
                                             *outId,
                                             UA_QUALIFIEDNAME(0, "Time"),
                                             &eventTime,
                                             &UA_TYPES[UA_TYPES_DATETIME]);
  if (ret != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "UA_Server_writeObjectProperty_scalar('Time') failed: %s",
          UA_StatusCode_name(ret));
    return ret;
  }

  ret = UA_Server_writeObjectProperty_scalar(server,
                                             *outId,
                                             UA_QUALIFIEDNAME(0, "Severity"),
                                             &eventSeverity,
                                             &UA_TYPES[UA_TYPES_UINT16]);
  if (ret != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "UA_Server_writeObjectProperty_scalar('Severity') failed: %s",
          UA_StatusCode_name(ret));
    return ret;
  }

  if (ua_state == UA_IOPORTSTATETYPE_OPEN) {
    eventMessage = UA_LOCALIZEDTEXT("en-US", "New state: OPEN");
  } else if (ua_state == UA_IOPORTSTATETYPE_CLOSED) {
    eventMessage = UA_LOCALIZEDTEXT("en-US", "New state: CLOSED");
  }

  ret = UA_Server_writeObjectProperty_scalar(server,
                                             *outId,
                                             UA_QUALIFIEDNAME(0, "Message"),
                                             &eventMessage,
                                             &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
  if (ret != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "UA_Server_writeObjectProperty_scalar('Message') failed: %s",
          UA_StatusCode_name(ret));
    return ret;
  }

  ret = UA_Server_writeObjectProperty_scalar(server,
                                             *outId,
                                             UA_QUALIFIEDNAME(0, "SourceName"),
                                             &eventSourceName,
                                             &UA_TYPES[UA_TYPES_STRING]);
  if (ret != UA_STATUSCODE_GOOD) {
    LOG_E(plugin->logger,
          "UA_Server_writeObjectProperty_scalar('SourceName') failed: %s",
          UA_StatusCode_name(ret));
    return ret;
  }

  return UA_STATUSCODE_GOOD;
}

/**
 * Returns the new state for an I/O port given the new 'active' state and the
 * configured 'Normal state'.
 *
 * Input parameters:
 *  * active       - the 'active' (true/false) state as indicated by the AxEvent
 *  * normal_state - the currently configured 'Normal state'
 *
 * Returns:
 *  * the new corresponding state (open/closed)
 */
static UA_IOPortStateType
iop_new_state(gboolean active, UA_IOPortStateType normal_state)
{
  UA_IOPortStateType new_state;

  if (active) {
    if (normal_state == UA_IOPORTSTATETYPE_OPEN) {
      new_state = UA_IOPORTSTATETYPE_CLOSED;
    } else {
      new_state = UA_IOPORTSTATETYPE_OPEN;
    }
  } else {
    if (normal_state == UA_IOPORTSTATETYPE_OPEN) {
      new_state = UA_IOPORTSTATETYPE_OPEN;
    } else {
      new_state = UA_IOPORTSTATETYPE_CLOSED;
    }
  }

  return new_state;
}

/**
 * AXSubscriptionCallback to handle I/O port state change events.
 * Interprets the AxEvent to update our local cache holding the current state
 * of the I/O ports and emits an OPC-UA event according to the AxEvent.
 */
static void
iop_state_ev_cb(G_GNUC_UNUSED guint subscription,
                AXEvent *event,
                G_GNUC_UNUSED gpointer user_data)
{
  const AXEventKeyValueSet *key_value_set;
  gint port;
  ioport_obj_t *iop = NULL;
  gboolean active;
  gchar *topic2 = NULL;
  gchar *id_str = NULL;
  GError *lerr = NULL;
  UA_StatusCode retval;
  UA_NodeId iop_node = UA_NODEID_NULL;
  UA_NodeId eventNodeId;
  UA_NodeId ioports_obj = UA_NODEID_NUMERIC(plugin->ns, UA_IOPID_IOPORTS);
  UA_IOPortStateType ua_state;

  g_assert(event != NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->iop_ht != NULL);
  g_assert(plugin->logger != NULL);
  g_assert(plugin->server != NULL);

  /* extract the AXEventKeyValueSet from the event. */
  key_value_set = ax_event_get_key_value_set(event);
  if (key_value_set == NULL) {
    LOG_E(plugin->logger, "ax_event_get_key_value_set() failed, event ignored");
    goto err_out;
  }

  /* fetch the I/O port number */
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
                                          "state",
                                          NULL,
                                          &active,
                                          &lerr)) {
    LOG_E(plugin->logger,
          "'active' key missing from event: %s",
          GERROR_MSG(lerr));
    goto err_out;
  }

  /* fetch the 'topic2' key: the value ("Port" or "OutputPort") will tell us
   * if the port is an input or an output and will also help us filter out
   * virtual ports for example. */
  if (!ax_event_key_value_set_get_string(key_value_set,
                                         "topic2",
                                         "tnsaxis",
                                         &topic2,
                                         &lerr)) {
    LOG_E(plugin->logger,
          "'topic2' key missing from event: %s",
          GERROR_MSG(lerr));
    goto err_out;
  }

  LOG_D(plugin->logger,
        "I/O port: %d (\"topic2:%s\"), active: %d",
        port,
        topic2,
        active);

  if ((g_strcmp0(topic2, "Port") == 0 ||
       g_strcmp0(topic2, "OutputPort") == 0) &&
      (port > -1)) {
    /* look up this port in our hash table and update the 'state' accordingly */
    IOP_HT_LOCK(iop_mtx);
    iop = g_hash_table_lookup(plugin->iop_ht, &port);
    if (!iop) {
      LOG_W(plugin->logger, "port: %d not found, ignoring AxEvent!", port);
      IOP_HT_UNLOCK(iop_mtx);
      goto err_out;
    }

    /* update the current state of the port in our cache based on the AxEvent
     * ('active') we received and the current value of the "NormalState"
     * property of the port. */
    iop->state = iop_new_state(active, iop->normal_state);
    ua_state = iop->state;
    IOP_HT_UNLOCK(iop_mtx);

    LOG_D(plugin->logger,
          "I/O port: %d, new state: %s",
          port,
          ua_state == UA_IOPORTSTATETYPE_OPEN ? "OPEN" : "CLOSED");

    /* NOTE: web GUI uses 1-based indexing */
    id_str = g_strdup_printf(IOP_LABEL_FMT, port + 1);

    /* find the node id corresponding to the browseName */
    if (!iop_ua_get_nodeid_from_browsename(plugin->server,
                                           &ioports_obj,
                                           UA_NS0ID_ORGANIZES,
                                           id_str,
                                           &iop_node,
                                           &lerr)) {
      LOG_E(plugin->logger,
            "iop_ua_get_nodeid_from_browsename() failed: %s",
            GERROR_MSG(lerr));
      goto err_out;
    }

    /* set up a node representation of the OPC-UA event */
    retval = iop_ua_create_event(plugin->server,
                                 UA_STRING(id_str),
                                 ua_state,
                                 UA_IOPID_IOPSTATEEVENTTYPE,
                                 IOP_STATE_CHANGE_EV_SEVERITY,
                                 &eventNodeId);
    if (retval != UA_STATUSCODE_GOOD) {
      LOG_E(plugin->logger,
            "iop_ua_create_event() failed: %s",
            UA_StatusCode_name(retval));
      goto err_out;
    }

    /* trigger the OPC-UA event */
    retval = UA_Server_triggerEvent(plugin->server,
                                    eventNodeId,
                                    iop_node, /* event origin node */
                                    NULL,
                                    UA_TRUE);
    if (retval != UA_STATUSCODE_GOOD) {
      LOG_E(plugin->logger,
            "UA_Server_triggerEvent failed: %s",
            UA_StatusCode_name(retval));
      goto err_out;
    }
  }

err_out:
  g_clear_error(&lerr);
  g_clear_pointer(&topic2, g_free);
  g_clear_pointer(&id_str, g_free);
  UA_NodeId_clear(&iop_node);

  /* the callback must always free 'event', NULL-case handled by the API */
  ax_event_free(event);

  return;
}

/**
 * AXSubscriptionCallback to handle I/O port configuration change events.
 * Interprets the AxEvent and updates our local cache holding the current state
 * of the I/O ports.
 */
static void
iop_cfg_ev_cb(G_GNUC_UNUSED guint subscription,
              AXEvent *event,
              G_GNUC_UNUSED gpointer user_data)
{
  const AXEventKeyValueSet *key_value_set;
  gint64 port_nr;
  ioport_obj_t *iop = NULL;
  gchar *cfg_changes = NULL;
  gchar *cfg_changes_unq = NULL;
  gchar **strv;
  gchar *param, *val;
  gchar *id_str = NULL;
  gchar *iop_index = NULL;
  GError *lerr = NULL;

  g_assert(event != NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->iop_ht != NULL);
  g_assert(plugin->logger != NULL);

  /* extract the AXEventKeyValueSet from the event. */
  key_value_set = ax_event_get_key_value_set(event);
  if (key_value_set == NULL) {
    LOG_E(plugin->logger, "ax_event_get_key_value_set() failed, event ignored");
    goto err_out;
  }

  /* get the value of the 'configuration_changes' key */
  if (!ax_event_key_value_set_get_string(key_value_set,
                                         "configuration_changes",
                                         NULL,
                                         &cfg_changes,
                                         &lerr)) {
    LOG_E(plugin->logger,
          "'configuration_changes' key missing from event: %s",
          GERROR_MSG(lerr));
    goto err_out;
  }

  /* get the value of the 'id' key */
  if (!ax_event_key_value_set_get_string(key_value_set,
                                         "id",
                                         NULL,
                                         &id_str,
                                         &lerr)) {
    LOG_E(plugin->logger, "'id' key missing from event: %s", GERROR_MSG(lerr));
    goto err_out;
  }

  /* extract the port number (index) from the 'id' key of the AxEvent which is
   * of the following form:
   * /com/axis/Configuration/Legacy/IOControl/IOPort/<port index> */
  iop_index = g_strrstr(id_str, "/");
  if (!iop_index) {
    LOG_E(plugin->logger, "Can't parse AxEvent for a port index!");
    goto err_out;
  }

  /* iop_index + 1: skip over the '/' character */
  port_nr = ascii_strtoll_dec(iop_index + 1, &lerr);
  if (lerr != NULL) {
    LOG_E(plugin->logger,
          "Invalid port index in AxEvent: %s",
          GERROR_MSG(lerr));
    goto err_out;
  }

  /* The string describing the parameter change is expected to be in form of:
   * `"%s=%s"`. For example a name change of an I/O port can look like this:
   * "Name=Port 01" */

  /* strip away the double quotes */
  cfg_changes_unq = g_shell_unquote(cfg_changes, &lerr);
  if (lerr != NULL) {
    LOG_E(plugin->logger, "g_shell_unquote() failed: %s", GERROR_MSG(lerr));
    goto err_out;
  }

  /* split the string into 2 tokens (parameter and value)
   * using '=' as separator */
  strv = g_strsplit(cfg_changes_unq, "=", 2);
  if (strv == NULL) {
    LOG_E(plugin->logger, "Can't parse AxEvent key: 'configuration_changes'!");
    goto err_out;
  }

  /* We expect to have 2 tokens in our 'strv' array of strings. This also
   * implies they are not NULL. */
  if (g_strv_length(strv) != 2) {
    LOG_E(plugin->logger,
          "Unexpected result parsing AxEvent key: 'configuration_changes'!");
    goto err_out;
  }

  param = *strv;
  val = *(strv + 1);

  LOG_D(plugin->logger,
        "configuration_changes: %s, id: %s ==> port: %" G_GINT64_FORMAT
        ", param: %s, val: %s",
        cfg_changes,
        id_str,
        port_nr,
        param,
        val);

  /* update our cached information in 'iop_ht' */
  IOP_HT_LOCK(iop_mtx);
  iop = g_hash_table_lookup(plugin->iop_ht, &port_nr);
  if (iop == NULL) {
    LOG_W(plugin->logger,
          "port: %" G_GINT64_FORMAT " not found, ignoring AxEvent!",
          port_nr);
    IOP_HT_UNLOCK(iop_mtx);
    goto err_out;
  }

  if (g_strcmp0(param, IOP_CFG_CHANGE_NAME) == 0) {
    /* 'Name' got changed */
    g_clear_pointer(&iop->name, g_free);
    iop->name = g_strdup(val);
  } else if (g_strcmp0(param, IOP_CFG_CHANGE_USAGE) == 0) {
    /* 'Usage' got changed */
    g_clear_pointer(&iop->usage, g_free);
    iop->usage = g_strdup(val);
  } else if (g_strcmp0(param, IOP_CFG_CHANGE_DIR) == 0) {
    /* 'Direction' got changed */
    if (g_strcmp0(val, IO_VAPIX_DIR_INPUT) == 0) {
      iop->direction = UA_IOPORTDIRECTIONTYPE_INPUT;
    } else {
      iop->direction = UA_IOPORTDIRECTIONTYPE_OUTPUT;
    }
  } else if ((g_strcmp0(param, IOP_CFG_CHANGE_NS_OUT) == 0) ||
             (g_strcmp0(param, IOP_CFG_CHANGE_NS_IN) == 0)) {
    /* 'Normal state' got changed */
    if (g_strcmp0(val, IO_VAPIX_STATE_OPEN) == 0) {
      iop->normal_state = UA_IOPORTSTATETYPE_CLOSED;
    } else {
      iop->normal_state = UA_IOPORTSTATETYPE_OPEN;
    }
  }
  IOP_HT_UNLOCK(iop_mtx);

err_out:
  g_strfreev(strv);
  g_clear_pointer(&cfg_changes_unq, g_free);
  g_clear_pointer(&cfg_changes, g_free);
  g_clear_pointer(&id_str, g_free);
  g_clear_error(&lerr);

  /* the callback must always free 'event', NULL-case handled by the API */
  ax_event_free(event);

  return;
}

/* Subscribe to the events we are interested in:
 * - port state change events: emitted when an I/O port transitions between
 *   open/closed
 * - port configuration change events: emitted when a configuration parameter
 *   for a certain I/O port gets changed (for example a change of the name,
 *   usage or direction)*/
static gboolean
iop_subscribe_events(GError **err)
{
  AXEventKeyValueSet *key_value_set = NULL;
  guint subscription;

  g_assert(plugin != NULL);
  g_assert(plugin->iopstate_evh != NULL);
  g_assert(plugin->iopcfg_evh != NULL);
  g_assert(err == NULL || *err == NULL);

  key_value_set = ax_event_key_value_set_new();
  if (key_value_set == NULL) {
    SET_ERROR(err, -1, "ax_event_key_value_set_new() failed!");
    return FALSE;
  }

  /* Initialize an AXEventKeyValueSet to match I/O port state events:
   *     tns1:topic0=Device
   *     tnsaxis:topic1=IO
   *     port=*
   *     active=*     <-- Subscribe to all states */
  /* clang-format off */
  if (!ax_event_key_value_set_add_key_values(key_value_set, err,
                            "topic0", "tns1",    "Device", AX_VALUE_TYPE_STRING,
                            "topic1", "tnsaxis", "IO",     AX_VALUE_TYPE_STRING,
                            "port",   NULL,      NULL,     AX_VALUE_TYPE_INT,
                            "state",  NULL,      NULL,     AX_VALUE_TYPE_BOOL,
                            NULL)) {
    g_prefix_error(err, "ax_event_key_value_set_add_key_values() failed: ");
    ax_event_key_value_set_free(key_value_set);
    return FALSE;
  }
  /* clang-format on */

  if (!ax_event_handler_subscribe(plugin->iopstate_evh,
                                  key_value_set,
                                  &subscription,
                                  (AXSubscriptionCallback) iop_state_ev_cb,
                                  plugin,
                                  err)) {
    g_prefix_error(err, "ax_event_handler_subscribe() failed: ");
    ax_event_key_value_set_free(key_value_set);
    return FALSE;
  }

  plugin->event_subs[IOP_STATE_CHANGE] = subscription;
  ax_event_key_value_set_free(key_value_set);

  /* prepare a new AXEventKeyValueSet */
  key_value_set = ax_event_key_value_set_new();
  if (key_value_set == NULL) {
    SET_ERROR(err, -1, "ax_event_key_value_set_new() failed!");
    return FALSE;
  }

  /* Initialize an AXEventKeyValueSet that matches I/O port configuration
   * change events:
   *    tns1:topic0=Device
   *    tnsaxis:topic1=Configuration
   *    service=com.axis.Configuration.Legacy.IOControl1.IOPort
   *    configuration_changes=* <-- the parameter and its new/changed value
   *    id=* <-- the port id */
  /* clang-format off */
  if (!ax_event_key_value_set_add_key_values(key_value_set, err,
               "topic0",  "tns1",    "Device",             AX_VALUE_TYPE_STRING,
               "topic1",  "tnsaxis", "Configuration",      AX_VALUE_TYPE_STRING,
               "service", NULL,      IOP_DBUS_CFG_SERVICE, AX_VALUE_TYPE_STRING,
               NULL)) {
    g_prefix_error(err, "ax_event_key_value_set_add_key_values() failed: ");
    ax_event_key_value_set_free(key_value_set);
    return FALSE;
  }
  /* clang-format on */

  if (!ax_event_handler_subscribe(plugin->iopcfg_evh,
                                  key_value_set,
                                  &subscription,
                                  (AXSubscriptionCallback) iop_cfg_ev_cb,
                                  plugin,
                                  err)) {
    g_prefix_error(err, "ax_event_handler_subscribe() failed: ");
    ax_event_key_value_set_free(key_value_set);
    return FALSE;
  }

  plugin->event_subs[IOP_CFG_CHANGE] = subscription;
  ax_event_key_value_set_free(key_value_set);

  return TRUE;
}

static gboolean
iop_ua_do_rollback(GError **err)
{
  g_assert(err == NULL || *err == NULL);
  g_assert(plugin != NULL);
  g_assert(plugin->rbd != NULL);
  g_assert(plugin->server != NULL);

  return ua_utils_do_rollback(plugin->server, plugin->rbd, err);
}

static void
plugin_cleanup(void)
{
  GError *lerr = NULL;

  g_assert(plugin != NULL);
  g_assert(plugin->logger != NULL);

  if (plugin->curl_h != NULL) {
    curl_easy_cleanup(plugin->curl_h);
  }

  IOP_HT_LOCK(iop_mtx);
  if (plugin->iop_ht) {
    g_hash_table_destroy(plugin->iop_ht);
  }
  IOP_HT_UNLOCK(iop_mtx);
  g_mutex_clear(&plugin->iop_mtx);

  if (plugin->iopstate_evh != NULL) {
    /* unsubscribe from I/O port state change events */
    if (!ax_event_handler_unsubscribe_and_notify(
                plugin->iopstate_evh,
                plugin->event_subs[IOP_STATE_CHANGE],
                NULL,
                NULL,
                &lerr)) {
      LOG_E(plugin->logger,
            "ax_event_handler_unsubscribe_and_notify() failed: %s",
            GERROR_MSG(lerr));
      g_clear_error(&lerr);
    }
    ax_event_handler_free(plugin->iopstate_evh);
  }

  if (plugin->iopcfg_evh != NULL) {
    /* unsubscribe from I/O port configuration change events */
    if (!ax_event_handler_unsubscribe_and_notify(
                plugin->iopcfg_evh,
                plugin->event_subs[IOP_CFG_CHANGE],
                NULL,
                NULL,
                &lerr)) {
      LOG_E(plugin->logger,
            "ax_event_handler_unsubscribe_and_notify() failed: %s",
            GERROR_MSG(lerr));
      g_clear_error(&lerr);
    }
    ax_event_handler_free(plugin->iopcfg_evh);
  }

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
  size_t ns_idx;
  UA_StatusCode ua_status;
  GHashTableIter ht_iter;
  gpointer key;
  gpointer value;
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
  plugin->iop_ht = NULL;
  plugin->server = server;
  plugin->rbd = g_new0(rollback_data_t, 1);
  g_mutex_init(&plugin->iop_mtx);

  /* allocate a curl handle that we are going to use throughout our requests */
  plugin->curl_h = curl_easy_init();
  if (plugin->curl_h == NULL) {
    SET_ERROR(err, -1, "curl_easy_init() failed!");
    goto err_out;
  }

  /* obtain credentials to make VAPIX calls */
  plugin->vapix_credentials = vapix_get_credentials("vapix-ioports-user", err);
  if (plugin->vapix_credentials == NULL) {
    g_prefix_error(err, "Failed to get the VAPIX credentials: ");
    goto err_out;
  }

  /* check API version compatibility */
  if (!iop_vapix_check_api_ver(plugin->curl_h,
                               plugin->vapix_credentials,
                               err)) {
    g_prefix_error(err, "iop_vapix_check_api_ver() failed: ");
    goto err_out;
  }

  /* add the "I/O Ports" namespace to the information model */
  ua_status = ioports_ns(server, plugin->rbd);
  if (ua_status != UA_STATUSCODE_GOOD) {
    g_prefix_error(err, "ns_ioports() failed: ");
    goto err_out;
  }

  ua_status = UA_Server_getNamespaceByName(server,
                                           UA_STRING(UA_PLUGIN_NAMESPACE),
                                           &ns_idx);
  if (ua_status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "UA_Server_getNamespaceByName('%s') failed: %s",
              UA_PLUGIN_NAMESPACE,
              UA_StatusCode_name(ua_status));
    return FALSE;
  }
  plugin->ns = ns_idx;

  /* add a constructor callback for the IOPortObjType object type */
  ua_status = set_ioport_lifecycle_cb(server, plugin->ns);
  if (ua_status != UA_STATUSCODE_GOOD) {
    SET_ERROR(err,
              -1,
              "Failed to install constructor for IOPortType nodes: %s",
              UA_StatusCode_name(ua_status));
    goto err_out;
  }

  IOP_HT_LOCK(iop_mtx);
  /* Fetch the available I/O ports on the device */
  if (!iop_vapix_get_ports(plugin->curl_h,
                           plugin->vapix_credentials,
                           &plugin->iop_ht,
                           err)) {
    g_prefix_error(err, "iop_vapix_get_ports() failed: ");
    IOP_HT_UNLOCK(iop_mtx);
    goto err_out;
  }

  /* iterate over the hash table elements and add the ports to the UA
   * information model */
  g_hash_table_iter_init(&ht_iter, plugin->iop_ht);
  while (g_hash_table_iter_next(&ht_iter, &key, &value)) {
    if (!iop_add_ioport_object(server,
                               *(guint32 *) key,
                               (ioport_obj_t *) value,
                               err)) {
      g_prefix_error(err, "iop_add_ioport_object() failed: ");
      IOP_HT_UNLOCK(iop_mtx);
      goto err_out;
    }
  }
  IOP_HT_UNLOCK(iop_mtx);

  plugin->iopstate_evh = ax_event_handler_new();
  if (!plugin->iopstate_evh) {
    SET_ERROR(err, -1, "Could not allocate AXEventHandler!");
    goto err_out;
  }

  plugin->iopcfg_evh = ax_event_handler_new();
  if (!plugin->iopcfg_evh) {
    SET_ERROR(err, -1, "Could not allocate AXEventHandler!");
    goto err_out;
  }

  if (!iop_subscribe_events(err)) {
    g_prefix_error(err, "iop_subscribe_events() failed: ");
    goto err_out;
  }

  /* the information model was successfully populated so now we can free up our
   * rollback data since we no longer need it */
  ua_utils_clear_rbd(&plugin->rbd);

  return TRUE;

err_out:
  /* remove any nodes created so far */
  if (!iop_ua_do_rollback(&lerr)) {
    /* NOTE: if we land here we might just as well terminate the whole
     * application, something is totally out of whack */
    LOG_E(plugin->logger, "iop_ua_do_rollback() failed: %s", GERROR_MSG(lerr));
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
