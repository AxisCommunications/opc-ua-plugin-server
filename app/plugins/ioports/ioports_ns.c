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

#include <glib.h>

#include "error.h"
#include "ioports_nodeids.h"
#include "ioports_ns.h"
#include "ua_utils.h"

#define UA_NS0_NAMESPACE "http://opcfoundation.org/UA/"

/* I/O port: object type */
#define IOP_OBJECT_TYPE_BNAME "IOPortObjType"

/* I/O port: event types */
#define IOP_EVENT_TYPE_BNAME        "IOPEventType"
#define IOP_DIR_EVENT_BNAME         "IOPDirectionEventType"
#define IOP_NORMALSTATE_EVENT_BNAME "IOPNormalStateEventType"
#define IOP_STATE_EVENT_BNAME       "IOPStateEventType"

/* I/O port: root object */
#define IOP_ROOT_BNAME "I/O Ports"

/* I/O port: direction values */
#define IOP_DIR_INPUT  "Input"
#define IOP_DIR_OUTPUT "Output"
/* I/O port: state value */
#define IOP_STATE_OPEN   "Open"
#define IOP_STATE_CLOSED "Closed"

#define UA_ENUM_STRINGS   "EnumStrings"
#define IOP_NR_EVENTTYPES 3

typedef struct IOP_property_node {
  UA_NodeId node_id;
  UA_Byte access_level;
  UA_NodeId data_type;
  UA_LocalizedText d_name; /* display name */
  UA_QualifiedName q_name; /* qualified name */
} IOP_property_node_t;

typedef struct IOP_eventtype_node {
  UA_NodeId node_id;
  UA_LocalizedText d_name; /* display name */
  UA_QualifiedName q_name; /* qualified name */
} IOP_eventtype_node_t;

/* clang-format off */
static UA_DataTypeArray customUA_TYPES_IOP = {
  NULL,
  UA_TYPES_IOP_COUNT,
  UA_TYPES_IOP,
  UA_FALSE
};
/* clang-format on */

static UA_StatusCode
ioports_add_port_state_type(UA_Server *server,
                            UA_UInt16 *ns,
                            rollback_data_t *rbd)
{
  UA_StatusCode retv = UA_STATUSCODE_GOOD;
  UA_DataTypeAttributes attr = UA_DataTypeAttributes_default;
  UA_VariableAttributes vattr = UA_VariableAttributes_default;
  UA_UInt32 array_dimensions[1];
  UA_LocalizedText enum_strings[2];

  g_assert(server != NULL);
  g_assert(ns != NULL);
  g_assert(rbd != NULL);

  attr.displayName = UA_LOCALIZEDTEXT("", UA_TYPE_IOP_STATETYPE_NAME);
  retv = UA_Server_addDataTypeNode_rb(
          server,
          UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTSTATETYPE),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_ENUMERATION),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_HASSUBTYPE),
          UA_QUALIFIEDNAME(ns[1], UA_TYPE_IOP_STATETYPE_NAME),
          attr,
          NULL,
          rbd,
          NULL);

  if (retv != UA_STATUSCODE_GOOD) {
    return retv;
  }

  /* add the children nodes */
  vattr.userAccessLevel = 1;
  vattr.accessLevel = UA_ACCESSLEVELMASK_READ;
  vattr.valueRank = UA_VALUERANK_ONE_DIMENSION;
  vattr.arrayDimensionsSize = 1;

  array_dimensions[0] = 2;
  vattr.arrayDimensions = &array_dimensions[0];
  vattr.dataType = UA_NODEID_NUMERIC(ns[0], UA_NS0ID_LOCALIZEDTEXT);

  enum_strings[0] = UA_LOCALIZEDTEXT("", IOP_STATE_OPEN);
  enum_strings[1] = UA_LOCALIZEDTEXT("", IOP_STATE_CLOSED);

  UA_Variant_setArray(&vattr.value,
                      &enum_strings,
                      (UA_Int32) 2,
                      &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
  vattr.displayName = UA_LOCALIZEDTEXT("", UA_ENUM_STRINGS);

  retv = UA_Server_addVariableNode_rb(
          server,
          UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTSTATETYPE_ENUMSTRINGS),
          UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTSTATETYPE),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_HASPROPERTY),
          UA_QUALIFIEDNAME(ns[0], UA_ENUM_STRINGS),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_PROPERTYTYPE),
          vattr,
          NULL,
          rbd,
          NULL);

  return retv;
}

static UA_StatusCode
ioports_add_port_dir_type(UA_Server *server,
                          UA_UInt16 *ns,
                          rollback_data_t *rbd)
{
  UA_StatusCode retv = UA_STATUSCODE_GOOD;
  UA_DataTypeAttributes attr = UA_DataTypeAttributes_default;
  UA_VariableAttributes vattr = UA_VariableAttributes_default;
  UA_UInt32 array_dimensions[1];
  UA_LocalizedText enum_strings[2];

  g_assert(server != NULL);
  g_assert(ns != NULL);

  attr.displayName = UA_LOCALIZEDTEXT("", UA_TYPE_IOP_DIRTYPE_NAME);
  retv = UA_Server_addDataTypeNode_rb(
          server,
          UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTDIRECTIONTYPE),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_ENUMERATION),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_HASSUBTYPE),
          UA_QUALIFIEDNAME(ns[1], UA_TYPE_IOP_DIRTYPE_NAME),
          attr,
          NULL,
          rbd,
          NULL);

  if (retv != UA_STATUSCODE_GOOD) {
    return retv;
  }

  /* add the children nodes */
  vattr.userAccessLevel = 1;
  vattr.accessLevel = UA_ACCESSLEVELMASK_READ;
  vattr.valueRank = UA_VALUERANK_ONE_DIMENSION;
  vattr.arrayDimensionsSize = 1;

  array_dimensions[0] = 2;
  vattr.arrayDimensions = &array_dimensions[0];
  vattr.dataType = UA_NODEID_NUMERIC(ns[0], UA_NS0ID_LOCALIZEDTEXT);

  enum_strings[0] = UA_LOCALIZEDTEXT("", IOP_DIR_INPUT);
  enum_strings[1] = UA_LOCALIZEDTEXT("", IOP_DIR_OUTPUT);

  UA_Variant_setArray(&vattr.value,
                      &enum_strings,
                      (UA_Int32) 2,
                      &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
  vattr.displayName = UA_LOCALIZEDTEXT("", UA_ENUM_STRINGS);

  retv = UA_Server_addVariableNode_rb(
          server,
          UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTDIRECTIONTYPE_ENUMSTRINGS),
          UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTDIRECTIONTYPE),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_HASPROPERTY),
          UA_QUALIFIEDNAME(ns[0], UA_ENUM_STRINGS),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_PROPERTYTYPE),
          vattr,
          NULL,
          rbd,
          NULL);

  return retv;
}

/**
 * Add definition nodes for our OPC-UA port events:
 *   - state change (open/closed)
 *   - normal state change (open/closed)
 *   - direction change (input/output)
 **/
static UA_StatusCode
ioports_add_port_event_type(UA_Server *server,
                            UA_UInt16 *ns,
                            rollback_data_t *rbd)
{
  guint i;
  UA_StatusCode retv = UA_STATUSCODE_GOOD;
  UA_ObjectTypeAttributes oattr = UA_ObjectTypeAttributes_default;
  /* clang-format off */
  const IOP_eventtype_node_t IOP_eventtypes[IOP_NR_EVENTTYPES] = {
    {
      .node_id = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPDIRECTIONEVENTTYPE),
      .d_name = UA_LOCALIZEDTEXT("", IOP_DIR_EVENT_BNAME),
      .q_name = UA_QUALIFIEDNAME(ns[1], IOP_DIR_EVENT_BNAME),
    },
    {
      .node_id = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPNORMALSTATEEVENTTYPE),
      .d_name = UA_LOCALIZEDTEXT("", IOP_NORMALSTATE_EVENT_BNAME),
      .q_name = UA_QUALIFIEDNAME(ns[1], IOP_NORMALSTATE_EVENT_BNAME),
    },
    {
      .node_id = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPSTATEEVENTTYPE),
      .d_name = UA_LOCALIZEDTEXT("", IOP_STATE_EVENT_BNAME),
      .q_name = UA_QUALIFIEDNAME(ns[1], IOP_STATE_EVENT_BNAME),
    },
  };
  /* clang-format on */

  g_assert(server != NULL);
  g_assert(ns != NULL);

  oattr.isAbstract = TRUE;
  oattr.displayName = UA_LOCALIZEDTEXT("", IOP_EVENT_TYPE_BNAME);

  /* add object type (event types are object types) */
  retv |= UA_Server_addObjectTypeNode_rb(
          server,
          UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPEVENTTYPE),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_BASEEVENTTYPE),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_HASSUBTYPE),
          UA_QUALIFIEDNAME(ns[1], IOP_EVENT_TYPE_BNAME),
          oattr,
          NULL,
          rbd,
          NULL);

  if (retv != UA_STATUSCODE_GOOD) {
    return retv;
  }

  /* add reference: an I/O port object generates I/O port event types */
  retv |= UA_Server_addReference(
          server,
          UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPEVENTTYPE),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_GENERATESEVENT),
          UA_EXPANDEDNODEID_NUMERIC(ns[1], UA_IOPID_IOPORTOBJTYPE),
          FALSE);
  if (retv != UA_STATUSCODE_GOOD) {
    return retv;
  }

  /* add the particular I/O port event subtypes */
  for (i = 0; i < IOP_NR_EVENTTYPES; i++) {
    oattr.displayName = IOP_eventtypes[i].d_name;
    retv |= UA_Server_addObjectTypeNode_rb(
            server,
            IOP_eventtypes[i].node_id,
            UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPEVENTTYPE),
            UA_NODEID_NUMERIC(ns[0], UA_NS0ID_HASSUBTYPE),
            IOP_eventtypes[i].q_name,
            oattr,
            NULL,
            rbd,
            NULL);
    if (retv != UA_STATUSCODE_GOOD) {
      return retv;
    }
  }

  return retv;
}

static UA_StatusCode
ioports_add_port_obj_type(UA_Server *server,
                          UA_UInt16 *ns,
                          rollback_data_t *rbd)
{
  UA_StatusCode retv = UA_STATUSCODE_GOOD;
  UA_ObjectTypeAttributes oattr = UA_ObjectTypeAttributes_default;
  UA_VariableAttributes vattr = UA_VariableAttributes_default;
  guint i;

  /* clang-format off */
  const IOP_property_node_t IOP_properties[IOP_OBJ_NR_PROPS] = {
    { /* "Configurable" property node */
      .node_id = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTOBJTYPE_CONFIGURABLE),
      .access_level = UA_ACCESSLEVELMASK_READ,
      .data_type = UA_NODEID_NUMERIC(ns[0], UA_NS0ID_BOOLEAN),
      .d_name = UA_LOCALIZEDTEXT("", CONFIGURABLE_BNAME),
      .q_name = UA_QUALIFIEDNAME(ns[1], CONFIGURABLE_BNAME),
    },
    { /* "Direction" property node */
      .node_id = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTOBJTYPE_DIRECTION),
      .access_level = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE,
      .data_type = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTDIRECTIONTYPE),
      .d_name = UA_LOCALIZEDTEXT("", DIRECTION_BNAME),
      .q_name = UA_QUALIFIEDNAME(ns[1], DIRECTION_BNAME),
    },
    { /* "Disabled" property node */
      .node_id = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTOBJTYPE_DISABLED),
      .access_level = UA_ACCESSLEVELMASK_READ,
      .data_type = UA_NODEID_NUMERIC(ns[0], UA_NS0ID_BOOLEAN),
      .d_name = UA_LOCALIZEDTEXT("", DISABLED_BNAME),
      .q_name = UA_QUALIFIEDNAME(ns[1], DISABLED_BNAME),
    },
    { /* Index property node */
      .node_id = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTOBJTYPE_INDEX),
      .access_level = UA_ACCESSLEVELMASK_READ,
      .data_type = UA_NODEID_NUMERIC(ns[0], UA_NS0ID_INT32),
      .d_name = UA_LOCALIZEDTEXT("", INDEX_BNAME),
      .q_name = UA_QUALIFIEDNAME(ns[1], INDEX_BNAME),
    },
    { /* Name property node */
      .node_id = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTOBJTYPE_NAME),
      .access_level = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE,
      .data_type = UA_NODEID_NUMERIC(ns[0], UA_NS0ID_STRING),
      .d_name = UA_LOCALIZEDTEXT("", NAME_BNAME),
      .q_name = UA_QUALIFIEDNAME(ns[1], NAME_BNAME),
    },
    { /* NormalState property node */
      .node_id = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTOBJTYPE_NORMALSTATE),
      .access_level = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE,
      .data_type = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTSTATETYPE),
      .d_name = UA_LOCALIZEDTEXT("", NORMALSTATE_BNAME),
      .q_name = UA_QUALIFIEDNAME(ns[1], NORMALSTATE_BNAME),
    },
    { /* State property node */
      .node_id = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTOBJTYPE_STATE),
      .access_level = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE,
      .data_type = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTSTATETYPE),
      .d_name = UA_LOCALIZEDTEXT("", STATE_BNAME),
      .q_name = UA_QUALIFIEDNAME(ns[1], STATE_BNAME),
    },
    { /* Usage property node */
      .node_id = UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTOBJTYPE_USAGE),
      .access_level = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE,
      .data_type = UA_NODEID_NUMERIC(ns[0], UA_NS0ID_STRING),
      .d_name = UA_LOCALIZEDTEXT("", USAGE_BNAME),
      .q_name = UA_QUALIFIEDNAME(ns[1], USAGE_BNAME),
    },
  };
  /* clang-format on */

  g_assert(server != NULL);
  g_assert(ns != NULL);

  oattr.displayName = UA_LOCALIZEDTEXT("", IOP_OBJECT_TYPE_BNAME);
  retv = UA_Server_addObjectTypeNode_rb(
          server,
          UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTOBJTYPE),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_BASEOBJECTTYPE),
          UA_NODEID_NUMERIC(ns[0], UA_NS0ID_HASSUBTYPE),
          UA_QUALIFIEDNAME(ns[1], IOP_OBJECT_TYPE_BNAME),
          oattr,
          NULL,
          rbd,
          NULL);
  if (retv != UA_STATUSCODE_GOOD) {
    return retv;
  }

  /* add I/O port object type properties: */
  for (i = 0; i < IOP_OBJ_NR_PROPS; i++) {
    vattr.accessLevel = IOP_properties[i].access_level;
    vattr.dataType = IOP_properties[i].data_type;
    vattr.displayName = IOP_properties[i].d_name;
    retv = UA_Server_addVariableNode_rb(
            server,
            IOP_properties[i].node_id,
            UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTOBJTYPE),
            UA_NODEID_NUMERIC(ns[0], UA_NS0ID_HASPROPERTY),
            IOP_properties[i].q_name,
            UA_NODEID_NUMERIC(ns[0], UA_NS0ID_PROPERTYTYPE),
            vattr,
            NULL,
            rbd,
            NULL);

    if (retv != UA_STATUSCODE_GOOD) {
      return retv;
    }

    retv = UA_Server_addReference(
            server,
            IOP_properties[i].node_id,
            UA_NODEID_NUMERIC(ns[0], UA_NS0ID_HASMODELLINGRULE),
            UA_EXPANDEDNODEID_NUMERIC(ns[0], UA_NS0ID_MODELLINGRULE_MANDATORY),
            TRUE);
    if (retv != UA_STATUSCODE_GOOD) {
      return retv;
    }
  } /* for i: 0..IOP_OBJ_NR_PROPS */

  return retv;
}

static UA_StatusCode
ioports_add_ioports_root(UA_Server *server, UA_UInt16 *ns, rollback_data_t *rbd)
{
  UA_StatusCode retv = UA_STATUSCODE_GOOD;
  UA_ObjectAttributes oattr = UA_ObjectAttributes_default;

  g_assert(server != NULL);
  g_assert(ns != NULL);

  oattr.displayName = UA_LOCALIZEDTEXT("", IOP_ROOT_BNAME);
  oattr.description = UA_LOCALIZEDTEXT("", IOP_ROOT_BNAME);

  retv = UA_Server_addObjectNode_rb(server,
                                    UA_NODEID_NUMERIC(ns[1], UA_IOPID_IOPORTS),
                                    UA_NODEID_NUMERIC(0,
                                                      UA_NS0ID_OBJECTSFOLDER),
                                    UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                    UA_QUALIFIEDNAME(ns[1], IOP_ROOT_BNAME),
                                    UA_NODEID_NUMERIC(ns[0],
                                                      UA_NS0ID_BASEOBJECTTYPE),
                                    oattr,
                                    NULL,
                                    rbd,
                                    NULL);

  if (retv != UA_STATUSCODE_GOOD) {
    return retv;
  }

  /* set the event notifier attribute for 'I/O Ports' object node */
  retv = UA_Server_writeEventNotifier(server,
                                      UA_NODEID_NUMERIC(ns[1],
                                                        UA_IOPID_IOPORTS),
                                      UA_EVENTNOTIFIER_SUBSCRIBE_TO_EVENT);
  return retv;
}

UA_StatusCode
ioports_ns(UA_Server *server, rollback_data_t *rbd)
{
  UA_StatusCode retVal = UA_STATUSCODE_GOOD;
  UA_UInt16 ns[2];
  gint i;

  g_return_val_if_fail(server != NULL, UA_STATUSCODE_BAD);
  g_return_val_if_fail(rbd != NULL, UA_STATUSCODE_BAD);

  ns[0] = UA_Server_addNamespace(server, UA_NS0_NAMESPACE);
  ns[1] = UA_Server_addNamespace(server, UA_PLUGIN_NAMESPACE);

#if UA_TYPES_IOP_COUNT > 0
  for (i = 0; i < UA_TYPES_IOP_COUNT; i++) {
    UA_TYPES_IOP[i].typeId.namespaceIndex = ns[1];
    UA_TYPES_IOP[i].binaryEncodingId.namespaceIndex = ns[1];
  }
#endif

  /* Load custom datatype definitions into the server */
  if (UA_TYPES_IOP_COUNT > 0) {
    customUA_TYPES_IOP.next = UA_Server_getConfig(server)->customDataTypes;

    /* also save the original pointer value in case we need to roll back */
    rbd->saved_cdt = UA_Server_getConfig(server)->customDataTypes;

    UA_Server_getConfig(server)->customDataTypes = &customUA_TYPES_IOP;
  }

  if ((retVal = ioports_add_port_state_type(server, ns, rbd)) !=
      UA_STATUSCODE_GOOD) {
    return retVal;
  }
  if ((retVal = ioports_add_port_dir_type(server, ns, rbd)) !=
      UA_STATUSCODE_GOOD) {
    return retVal;
  }
  if ((retVal = ioports_add_port_obj_type(server, ns, rbd)) !=
      UA_STATUSCODE_GOOD) {
    return retVal;
  }
  if ((retVal = ioports_add_port_event_type(server, ns, rbd)) !=
      UA_STATUSCODE_GOOD) {
    return retVal;
  }

  /* Root "I/O Ports" object */
  if ((retVal = ioports_add_ioports_root(server, ns, rbd)) !=
      UA_STATUSCODE_GOOD) {
    return retVal;
  }

  return retVal;
}
