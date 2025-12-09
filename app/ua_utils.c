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
#include <open62541/server.h>

#include "error.h"
#include "ua_utils.h"

DEFINE_GQUARK("ua-utils")

/* Local functions */

static UA_StatusCode
add_nodeid_to_rbd(const UA_NodeId *node_id, rollback_data_t *rbd)
{
  UA_StatusCode ua_status;
  UA_NodeId *rb_nodeid;

  g_assert(node_id != NULL);
  g_assert(rbd != NULL);

  /* allocate a UA_NodeId struct and add it to our rollback data
   * (rbd->node_ids) */
  rb_nodeid = UA_NodeId_new();
  if (rb_nodeid == NULL) {
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }

  ua_status = UA_NodeId_copy(node_id, rb_nodeid);
  if (ua_status != UA_STATUSCODE_GOOD) {
    UA_NodeId_delete(rb_nodeid);
    return ua_status;
  }

  /* add the allocated NodeId to our list */
  rbd->node_ids = g_list_prepend(rbd->node_ids, rb_nodeid);

  return UA_STATUSCODE_GOOD;
}

/* Exported functions */

void
ua_utils_clear_rbd(rollback_data_t **rbd)
{
  g_return_if_fail(rbd != NULL);

  if (*rbd == NULL) {
    return;
  }

  if ((*rbd)->node_ids != NULL) {
    /* free up the allocated UA_NodeId structures from the 'node_ids' list */
    g_clear_list(&((*rbd)->node_ids), (GDestroyNotify) UA_NodeId_delete);
  }

  /* free up the rollback_data_t structure itself */
  g_clear_pointer(rbd, g_free);

  return;
}

gboolean
ua_utils_do_rollback(UA_Server *server, rollback_data_t *rbd, GError **err)
{
  gboolean ret = FALSE;
  GList *l;
  UA_StatusCode ua_status;
  UA_ServerConfig *config;

  g_return_val_if_fail(server != NULL, FALSE);
  g_return_val_if_fail(rbd != NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  config = UA_Server_getConfig(server);
  if (config == NULL) {
    /* Unlikely, documentation says:
     * "Get the configuration. Always succeeds as this simply
     * resolves a pointer." */
    return ret;
  }

  if (rbd->saved_cdt != NULL) {
    /* restore the custom data type arrays registered with the server */
    config->customDataTypes = rbd->saved_cdt;
  }

  l = g_list_first(rbd->node_ids);
  while (l != NULL) {
    if (l->data) {
      ua_status = UA_Server_deleteNode(server, *(UA_NodeId *) l->data, TRUE);
      if (ua_status != UA_STATUSCODE_GOOD) {
        SET_ERROR(err,
                  -1,
                  "UA_Server_deleteNode() failed: %s",
                  UA_StatusCode_name(ua_status));
        goto err_out;
      }
    }
    l = g_list_next(l);
  }

  ret = TRUE;

err_out:

  return ret;
}

UA_StatusCode
UA_Server_addObjectNode_rb(UA_Server *server,
                           const UA_NodeId requestedNewNodeId,
                           const UA_NodeId parentNodeId,
                           const UA_NodeId referenceTypeId,
                           const UA_QualifiedName browseName,
                           const UA_NodeId typeDefinition,
                           const UA_ObjectAttributes attr,
                           void *nodeContext,
                           rollback_data_t *rbd,
                           UA_NodeId *outNewNodeId)
{
  UA_StatusCode ua_status;
  UA_NodeId out_nodeid = UA_NODEID_NULL;

  g_return_val_if_fail(server != NULL, UA_STATUSCODE_BAD);
  g_return_val_if_fail(rbd != NULL, UA_STATUSCODE_BAD);

  ua_status = UA_Server_addObjectNode(server,
                                      requestedNewNodeId,
                                      parentNodeId,
                                      referenceTypeId,
                                      browseName,
                                      typeDefinition,
                                      attr,
                                      nodeContext,
                                      &out_nodeid);
  if (ua_status == UA_STATUSCODE_GOOD) {
    if (outNewNodeId) {
      /* caller wants to know the NodeId of the new node */
      *outNewNodeId = out_nodeid;
    }

    ua_status |= add_nodeid_to_rbd(&out_nodeid, rbd);
  }

  return ua_status;
}

UA_StatusCode
UA_Server_addDataTypeNode_rb(UA_Server *server,
                             const UA_NodeId requestedNewNodeId,
                             const UA_NodeId parentNodeId,
                             const UA_NodeId referenceTypeId,
                             const UA_QualifiedName browseName,
                             const UA_DataTypeAttributes attr,
                             void *nodeContext,
                             rollback_data_t *rbd,
                             UA_NodeId *outNewNodeId)
{
  UA_StatusCode ua_status;
  UA_NodeId out_nodeid = UA_NODEID_NULL;

  g_return_val_if_fail(server != NULL, UA_STATUSCODE_BAD);
  g_return_val_if_fail(rbd != NULL, UA_STATUSCODE_BAD);

  ua_status = UA_Server_addDataTypeNode(server,
                                        requestedNewNodeId,
                                        parentNodeId,
                                        referenceTypeId,
                                        browseName,
                                        attr,
                                        nodeContext,
                                        &out_nodeid);
  if (ua_status == UA_STATUSCODE_GOOD) {
    if (outNewNodeId) {
      /* caller wants to know the NodeId of the new node */
      *outNewNodeId = out_nodeid;
    }

    ua_status |= add_nodeid_to_rbd(&out_nodeid, rbd);
  }

  return ua_status;
}

UA_StatusCode
UA_Server_addVariableNode_rb(UA_Server *server,
                             const UA_NodeId requestedNewNodeId,
                             const UA_NodeId parentNodeId,
                             const UA_NodeId referenceTypeId,
                             const UA_QualifiedName browseName,
                             const UA_NodeId typeDefinition,
                             const UA_VariableAttributes attr,
                             void *nodeContext,
                             rollback_data_t *rbd,
                             UA_NodeId *outNewNodeId)
{
  UA_StatusCode ua_status;
  UA_NodeId out_nodeid = UA_NODEID_NULL;

  g_return_val_if_fail(server != NULL, UA_STATUSCODE_BAD);
  g_return_val_if_fail(rbd != NULL, UA_STATUSCODE_BAD);

  ua_status = UA_Server_addVariableNode(server,
                                        requestedNewNodeId,
                                        parentNodeId,
                                        referenceTypeId,
                                        browseName,
                                        typeDefinition,
                                        attr,
                                        nodeContext,
                                        &out_nodeid);
  if (ua_status == UA_STATUSCODE_GOOD) {
    if (outNewNodeId) {
      /* caller wants to know the NodeId of the new node */
      *outNewNodeId = out_nodeid;
    }

    ua_status |= add_nodeid_to_rbd(&out_nodeid, rbd);
  }

  return ua_status;
}

UA_StatusCode
UA_Server_addObjectTypeNode_rb(UA_Server *server,
                               const UA_NodeId requestedNewNodeId,
                               const UA_NodeId parentNodeId,
                               const UA_NodeId referenceTypeId,
                               const UA_QualifiedName browseName,
                               const UA_ObjectTypeAttributes attr,
                               void *nodeContext,
                               rollback_data_t *rbd,
                               UA_NodeId *outNewNodeId)
{
  UA_StatusCode ua_status;
  UA_NodeId out_nodeid = UA_NODEID_NULL;

  g_return_val_if_fail(server != NULL, UA_STATUSCODE_BAD);
  g_return_val_if_fail(rbd != NULL, UA_STATUSCODE_BAD);

  ua_status = UA_Server_addObjectTypeNode(server,
                                          requestedNewNodeId,
                                          parentNodeId,
                                          referenceTypeId,
                                          browseName,
                                          attr,
                                          nodeContext,
                                          &out_nodeid);
  if (ua_status == UA_STATUSCODE_GOOD) {
    if (outNewNodeId) {
      /* caller wants to know the NodeId of the new node */
      *outNewNodeId = out_nodeid;
    }

    ua_status |= add_nodeid_to_rbd(&out_nodeid, rbd);
  }

  return ua_status;
}

UA_StatusCode
UA_Server_addMethodNode_rb(UA_Server *server,
                           const UA_NodeId requestedNewNodeId,
                           const UA_NodeId parentNodeId,
                           const UA_NodeId referenceTypeId,
                           const UA_QualifiedName browseName,
                           const UA_MethodAttributes attr,
                           UA_MethodCallback method,
                           size_t inputArgumentsSize,
                           const UA_Argument *inputArguments,
                           size_t outputArgumentsSize,
                           const UA_Argument *outputArguments,
                           void *nodeContext,
                           rollback_data_t *rbd,
                           UA_NodeId *outNewNodeId)
{
  UA_StatusCode ua_status;
  UA_NodeId out_nodeid = UA_NODEID_NULL;

  g_return_val_if_fail(server != NULL, UA_STATUSCODE_BAD);
  g_return_val_if_fail(rbd != NULL, UA_STATUSCODE_BAD);

  ua_status = UA_Server_addMethodNode(server,
                                      requestedNewNodeId,
                                      parentNodeId,
                                      referenceTypeId,
                                      browseName,
                                      attr,
                                      method,
                                      inputArgumentsSize,
                                      inputArguments,
                                      outputArgumentsSize,
                                      outputArguments,
                                      nodeContext,
                                      &out_nodeid);

  if (ua_status == UA_STATUSCODE_GOOD) {
    if (outNewNodeId) {
      /* caller wants to know the NodeId of the new node */
      *outNewNodeId = out_nodeid;
    }

    ua_status |= add_nodeid_to_rbd(&out_nodeid, rbd);
  }

  return ua_status;
}
