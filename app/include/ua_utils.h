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

#ifndef __UA_UTILS_H__
#define __UA_UTILS_H__

#include <glib.h>
#include <open62541/types.h>

typedef struct rollback_data {
  /* used to save the existing 'customDataTypes' of the server configuration
   * (struct UA_ServerConfig) */
  const UA_DataTypeArray *saved_cdt;

  /* a list of 'struct UA_NodeId' that have been added to the server */
  GList *node_ids;
} rollback_data_t;

/* Performs a 'deep' free() to deallocate the 'rollback_data_t' structure with
 * its associated 'node_ids' list */
void
ua_utils_clear_rbd(rollback_data_t **rbd);

/* Loops over the rbd->node_ids list in reverse order and deletes the nodes
 * from the information model.
 * NOTE: the list is populated by pre-pending, traversing it in forward
 * direction will visit the nodes in the reverse order of their addition.
 * IMPORTANT: This can only be called before the server thread gets started
 * as it can change the server configuration. */
gboolean
ua_utils_do_rollback(UA_Server *server, rollback_data_t *rbd, GError **err);

/* wrapper around the open62541 UA_Server_addObjectNode()
 * If underlying UA_Server_addObjectNode() succeeds it also adds the nodeId to
 * the rbd (rollback data) */
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
                           UA_NodeId *outNewNodeId);

/* wrapper around the open62541 UA_Server_addDataTypeNode()
 * If underlying UA_Server_addDataTypeNode() succeeds it also adds the nodeId to
 * the rbd (rollback data) */
UA_StatusCode
UA_Server_addDataTypeNode_rb(UA_Server *server,
                             const UA_NodeId requestedNewNodeId,
                             const UA_NodeId parentNodeId,
                             const UA_NodeId referenceTypeId,
                             const UA_QualifiedName browseName,
                             const UA_DataTypeAttributes attr,
                             void *nodeContext,
                             rollback_data_t *rbd,
                             UA_NodeId *outNewNodeId);

/* wrapper around the open62541 UA_Server_addVariableNode()
 * If underlying UA_Server_addVariableNode() succeeds it also adds the nodeId to
 * the rbd (rollback data) */
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
                             UA_NodeId *outNewNodeId);

/* wrapper around the open62541 UA_Server_addObjectTypeNode()
 * If underlying UA_Server_addObjectTypeNode() succeeds it also adds the nodeId
 * to the rbd (rollback data) */
UA_StatusCode
UA_Server_addObjectTypeNode_rb(UA_Server *server,
                               const UA_NodeId requestedNewNodeId,
                               const UA_NodeId parentNodeId,
                               const UA_NodeId referenceTypeId,
                               const UA_QualifiedName browseName,
                               const UA_ObjectTypeAttributes attr,
                               void *nodeContext,
                               rollback_data_t *rbd,
                               UA_NodeId *outNewNodeId);

/* wrapper around the open62541 UA_Server_addMethodNode()
 * If underlying UA_Server_addMethodNode() succeeds it also adds the nodeId
 * to the rbd (rollback data) */
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
                           UA_NodeId *outNewNodeId);

#endif /* __UA_UTILS_H__ */
