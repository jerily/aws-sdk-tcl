/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#include <iostream>
#include <aws/core/Aws.h>
#include <aws/sqs/SQSClient.h>
#include <aws/sqs/model/CreateQueueRequest.h>
#include <aws/sqs/model/DeleteQueueRequest.h>
#include <aws/sqs/model/ListQueuesRequest.h>
#include <aws/sqs/model/SendMessageRequest.h>
#include <aws/sqs/model/ReceiveMessageRequest.h>
#include <aws/sqs/model/SetQueueAttributesRequest.h>
#include <aws/sqs/model/ChangeMessageVisibilityRequest.h>
#include <aws/sqs/model/DeleteMessageRequest.h>
#include <aws/sqs/model/DeleteMessageBatchRequest.h>
#include <aws/sqs/model/GetQueueAttributesRequest.h>
#include <cstdio>
#include <fstream>
#include "library.h"

#define XSTR(s) STR(s)
#define STR(s) #s

#ifdef DEBUG
# define DBG(x) x
#else
# define DBG(x)
#endif

#define CheckArgs(min, max, n, msg) \
                 if ((objc < min) || (objc >max)) { \
                     Tcl_WrongNumArgs(interp, n, objv, msg); \
                     return TCL_ERROR; \
                 }

#define CMD_NAME(s, internal) std::sprintf((s), "_AWS_S3_%p", (internal))

static Tcl_HashTable aws_sdk_tcl_sqs_NameToInternal_HT;
static Tcl_Mutex aws_sdk_tcl_sqs_NameToInternal_HT_Mutex;
static int aws_sdk_tcl_sqs_ModuleInitialized;

static char client_usage[] =
        "Usage sqsClient <method> <args>, where method can be:\n"
        "  destroy\n"
        "  create_queue queue_name\n"
        "  delete_queue queue_url\n"
        "  list_queues\n"
        "  send_message queue_url message\n"
        "  receive_messages queue_url ?max_number_of_messages?\n"
        "  set_queue_attributes queue_url attributes_dict\n"
        "  change_message_visibility queue_url message_receipt_handle visibility_timeout_seconds\n"
        "  delete_message queue_url message_receipt_handle\n"
        "  delete_message_batch queue_url message_receipt_handles\n"
        "  get_queue_attributes queue_url\n";


static int
aws_sdk_tcl_sqs_RegisterName(const char *name, Aws::SQS::SQSClient *internal) {

    Tcl_HashEntry *entryPtr;
    int newEntry;

    Tcl_MutexLock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);
    entryPtr = Tcl_CreateHashEntry(&aws_sdk_tcl_sqs_NameToInternal_HT, (char *) name, &newEntry);
    if (newEntry) {
        Tcl_SetHashValue(entryPtr, (ClientData) internal);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> RegisterName: name=%s internal=%p %s\n", name, internal,
                newEntry ? "entered into" : "already in"));

    return !!newEntry;
}

static int
aws_sdk_tcl_sqs_UnregisterName(const char *name) {

    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_sqs_NameToInternal_HT, (char *) name);
    if (entryPtr != nullptr) {
        Tcl_DeleteHashEntry(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> UnregisterName: name=%s entryPtr=%p\n", name, entryPtr));

    return entryPtr != nullptr;
}

static struct Aws::SQS::SQSClient *
aws_sdk_tcl_sqs_GetInternalFromName(const char *name) {
    Aws::SQS::SQSClient *internal = nullptr;
    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_sqs_NameToInternal_HT, (char *) name);
    if (entryPtr != nullptr) {
        internal = (Aws::SQS::SQSClient *) Tcl_GetHashValue(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);

    return internal;
}

int aws_sdk_tcl_sqs_Destroy(Tcl_Interp *interp, const char *handle) {
    Aws::SQS::SQSClient *client = aws_sdk_tcl_sqs_GetInternalFromName(handle);
    if (!aws_sdk_tcl_sqs_UnregisterName(handle)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    delete client;
    Tcl_DeleteCommand(interp, handle);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

int aws_sdk_tcl_sqs_CreateQueue(Tcl_Interp *interp, const char *handle, const char *queue_name) {
    Aws::SQS::SQSClient *client = aws_sdk_tcl_sqs_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    Aws::SQS::Model::CreateQueueRequest request;
    request.SetQueueName(queue_name);
    auto outcome = client->CreateQueue(request);
    if (outcome.IsSuccess()) {
        auto queueUrl = outcome.GetResult().GetQueueUrl();
        Tcl_SetObjResult(interp, Tcl_NewStringObj(queueUrl.c_str(), -1));
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_sqs_DeleteQueue(Tcl_Interp *interp, const char *handle, const char *queue_url) {
    Aws::SQS::SQSClient *client = aws_sdk_tcl_sqs_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    Aws::SQS::Model::DeleteQueueRequest request;
    request.SetQueueUrl(queue_url);
    auto outcome = client->DeleteQueue(request);
    if (outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_sqs_ListQueues(Tcl_Interp *interp, const char *handle) {
    Aws::SQS::SQSClient *client = aws_sdk_tcl_sqs_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    Aws::SQS::Model::ListQueuesRequest request;

    auto outcome = client->ListQueues(request);
    if (outcome.IsSuccess()) {
        Tcl_Obj *listPtr = Tcl_NewListObj(0, nullptr);
        const auto &queue_urls = outcome.GetResult().GetQueueUrls();
        for (const auto &iter: queue_urls) {
            Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj(iter.c_str(), -1));
        }
        Tcl_SetObjResult(interp, listPtr);
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_sqs_SendMessage(Tcl_Interp *interp, const char *handle, const char *queue_url, const char *message) {
    Aws::SQS::SQSClient *client = aws_sdk_tcl_sqs_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    Aws::SQS::Model::SendMessageRequest request;
    request.SetQueueUrl(queue_url);
    request.SetMessageBody(message);
    auto outcome = client->SendMessage(request);
    if (outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_sqs_ReceiveMessages(Tcl_Interp *interp, const char *handle, const char *queue_url,
                                    Tcl_Obj *const maxNumberOfMessagesPtr) {
    Aws::SQS::SQSClient *client = aws_sdk_tcl_sqs_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    Aws::SQS::Model::ReceiveMessageRequest request;
    request.SetQueueUrl(queue_url);
    if (maxNumberOfMessagesPtr) {
        int maxNumberOfMessages;
        if (TCL_OK != Tcl_GetIntFromObj(interp, maxNumberOfMessagesPtr, &maxNumberOfMessages)) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("maxNumberOfMessages must be an integer", -1));
            return TCL_ERROR;
        }
        request.SetMaxNumberOfMessages(maxNumberOfMessages);
    } else {
        request.SetMaxNumberOfMessages(1);
    }
    auto outcome = client->ReceiveMessage(request);
    if (outcome.IsSuccess()) {
        Tcl_Obj *listPtr = Tcl_NewListObj(0, nullptr);
        const auto &messages = outcome.GetResult().GetMessages();
        for (const auto &iter: messages) {
            Tcl_Obj *messagePtr = Tcl_NewDictObj();
            Tcl_DictObjPut(interp, messagePtr, Tcl_NewStringObj("MessageId", -1),
                           Tcl_NewStringObj(iter.GetMessageId().c_str(), -1));
            Tcl_DictObjPut(interp, messagePtr, Tcl_NewStringObj("ReceiptHandle", -1),
                           Tcl_NewStringObj(iter.GetReceiptHandle().c_str(), -1));
            Tcl_DictObjPut(interp, messagePtr, Tcl_NewStringObj("Body", -1),
                           Tcl_NewStringObj(iter.GetBody().c_str(), -1));
            Tcl_ListObjAppendElement(interp, listPtr, messagePtr);
        }
        Tcl_SetObjResult(interp, listPtr);
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}


static int aws_sdk_tcl_sqs_SetQueueAttributes(Tcl_Interp *interp, const char *handle, const char *queue_url,
                                              Tcl_Obj *const dictPtr) {
    Aws::SQS::SQSClient *client = aws_sdk_tcl_sqs_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    Aws::SQS::Model::SetQueueAttributesRequest request;
    request.SetQueueUrl(queue_url);

    // DelaySeconds  The length of time, in seconds, for which the
    // delivery of all messages in the queue is delayed.
    // Valid values: An  integer from 0 to 900 (15 minutes). Default: 0.
    Tcl_Obj *delaySecondsPtr;
    if (TCL_OK != Tcl_DictObjGet(interp, dictPtr, Tcl_NewStringObj("DelaySeconds", -1), &delaySecondsPtr)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("error reading attributes_dict", -1));
        return TCL_ERROR;
    }
    if (delaySecondsPtr) {
        request.AddAttributes(Aws::SQS::Model::QueueAttributeName::DelaySeconds,
                              Tcl_GetString(delaySecondsPtr));
    }

    // MaximumMessageSize  The limit of how many bytes a message can
    // contain before Amazon SQS rejects it. Valid values: An  integer  from
    // 1,024  bytes  (1  KiB)  up  to  262,144  bytes (256 KiB). Default: 262,144 (256 KiB).
    Tcl_Obj *maximumMessageSizePtr;
    if (TCL_OK != Tcl_DictObjGet(interp, dictPtr, Tcl_NewStringObj("MaximumMessageSize", -1), &maximumMessageSizePtr)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("error reading attributes dict", -1));
        return TCL_ERROR;
    }
    if (maximumMessageSizePtr) {
        request.AddAttributes(Aws::SQS::Model::QueueAttributeName::MaximumMessageSize,
                              Tcl_GetString(maximumMessageSizePtr));
    }

    // MessageRetentionPeriod  The length of time, in seconds, for  which
    // Amazon  SQS retains a message. Valid values: An integer representing
    // seconds, from 60 (1 minute) to 1,209,600 (14  days). Default: 345,600 (4 days).
    Tcl_Obj *messageRetentionPeriodPtr;
    if (TCL_OK !=
        Tcl_DictObjGet(interp, dictPtr, Tcl_NewStringObj("MessageRetentionPeriod", -1), &messageRetentionPeriodPtr)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("error reading attributes dict", -1));
        return TCL_ERROR;
    }
    if (messageRetentionPeriodPtr) {
        request.AddAttributes(Aws::SQS::Model::QueueAttributeName::MessageRetentionPeriod,
                              Tcl_GetString(messageRetentionPeriodPtr));
    }

    // Policy   The  queue's  policy. A valid Amazon Web Services policy.
    Tcl_Obj *policyPtr;
    if (TCL_OK != Tcl_DictObjGet(interp, dictPtr, Tcl_NewStringObj("Policy", -1), &policyPtr)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("error reading attributes dict", -1));
        return TCL_ERROR;
    }
    if (policyPtr) {
        request.AddAttributes(Aws::SQS::Model::QueueAttributeName::Policy,
                              Tcl_GetString(policyPtr));
    }

    // ReceiveMessageWaitTimeSeconds  The length of time, in seconds, for
    // which  a  ``ReceiveMessage`` action waits for a message to arrive.
    // Valid values: An integer from 0 to 20 (seconds). Default: 0.
    Tcl_Obj *receiveMessageWaitTimeSecondsPtr;
    if (TCL_OK != Tcl_DictObjGet(interp, dictPtr, Tcl_NewStringObj("ReceiveMessageWaitTimeSeconds", -1),
                                 &receiveMessageWaitTimeSecondsPtr)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("error reading attributes dict", -1));
        return TCL_ERROR;
    }
    if (receiveMessageWaitTimeSecondsPtr) {
        request.AddAttributes(Aws::SQS::Model::QueueAttributeName::ReceiveMessageWaitTimeSeconds,
                              Tcl_GetString(receiveMessageWaitTimeSecondsPtr));
    }

    // VisibilityTimeout  The visibility timeout for the queue,  in  seconds.
    // Valid  values:  An integer from 0 to 43,200 (12 hours). Default: 30.
    Tcl_Obj *visibilityTimeoutPtr;
    if (TCL_OK != Tcl_DictObjGet(interp, dictPtr, Tcl_NewStringObj("VisibilityTimeout", -1), &visibilityTimeoutPtr)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("error reading attributes_dict", -1));
        return TCL_ERROR;
    }
    if (visibilityTimeoutPtr) {
        request.AddAttributes(Aws::SQS::Model::QueueAttributeName::VisibilityTimeout,
                              Tcl_GetString(visibilityTimeoutPtr));
    }

    const Aws::SQS::Model::SetQueueAttributesOutcome outcome = client->SetQueueAttributes(request);
    if (outcome.IsSuccess()) {
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

static int aws_sdk_tcl_sqs_ChangeMessageVisibility(
        Tcl_Interp *interp,
        const char *handle,
        const char *queue_url,
        const char *message_receipt_handle,
        Tcl_Obj *const visibilityTimeoutSecondsPtr
) {
    Aws::SQS::SQSClient *client = aws_sdk_tcl_sqs_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    int visibility_timeout_seconds;
    if (TCL_OK != Tcl_GetIntFromObj(interp, visibilityTimeoutSecondsPtr, &visibility_timeout_seconds)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("visibility_timeout_seconds must be an integer", -1));
        return TCL_ERROR;
    }

    Aws::SQS::Model::ChangeMessageVisibilityRequest request;
    request.SetQueueUrl(queue_url);
    request.SetReceiptHandle(message_receipt_handle);
    request.SetVisibilityTimeout(visibility_timeout_seconds);

    auto outcome = client->ChangeMessageVisibility(request);

    if (outcome.IsSuccess()) {
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

static int aws_sdk_tcl_sqs_DeleteMessage(
        Tcl_Interp *interp,
        const char *handle,
        const char *queue_url,
        const char *message_receipt_handle
) {
    Aws::SQS::SQSClient *client = aws_sdk_tcl_sqs_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::SQS::Model::DeleteMessageRequest request;
    request.SetQueueUrl(queue_url);
    request.SetReceiptHandle(message_receipt_handle);

    auto outcome = client->DeleteMessage(request);

    if (outcome.IsSuccess()) {
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

static int aws_sdk_tcl_sqs_DeleteMessageBatch(
        Tcl_Interp *interp,
        const char *handle,
        const char *queue_url,
        Tcl_Obj *const messageReceiptHandlesPtr
) {
    Aws::SQS::SQSClient *client = aws_sdk_tcl_sqs_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::SQS::Model::DeleteMessageBatchRequest request;
    request.SetQueueUrl(queue_url);

    int length;
    if (TCL_OK != Tcl_ListObjLength(interp, messageReceiptHandlesPtr, &length)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("messageReceiptHandles must be a list", -1));
        return TCL_ERROR;
    }

    int id = 1; // Ids must be unique within a batch delete request.
    for (int i = 0; i < length; i++) {
        Tcl_Obj *messageReceiptHandlePtr;
        if (TCL_OK != Tcl_ListObjIndex(interp, messageReceiptHandlesPtr, i, &messageReceiptHandlePtr)) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("failed to get message receipt handle from list", -1));
            return TCL_ERROR;
        }
        Aws::SQS::Model::DeleteMessageBatchRequestEntry entry;
        entry.SetId(std::to_string(id));
        id++;
        entry.SetReceiptHandle(Tcl_GetString(messageReceiptHandlePtr));
        request.AddEntries(entry);
    }

    auto outcome = client->DeleteMessageBatch(request);

    if (outcome.IsSuccess()) {
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

static int aws_sdk_tcl_sqs_GetQueueAttributes(
        Tcl_Interp *interp,
        const char *handle,
        const char *queue_url
) {
    Aws::SQS::SQSClient *client = aws_sdk_tcl_sqs_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::SQS::Model::GetQueueAttributesRequest request;
    request.SetQueueUrl(queue_url);
    request.AddAttributeNames(Aws::SQS::Model::QueueAttributeName::All);

    auto outcome = client->GetQueueAttributes(request);
    if (outcome.IsSuccess()) {
        Tcl_Obj *dictPtr = Tcl_NewDictObj();
        const auto &attributes = outcome.GetResult().GetAttributes();
        for (const auto &iter: attributes) {
            Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj(Aws::SQS::Model::QueueAttributeNameMapper::GetNameForQueueAttributeName(iter.first).c_str(), -1),
                           Tcl_NewStringObj(iter.second.c_str(), -1));
        }
        Tcl_SetObjResult(interp, dictPtr);
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_sqs_ClientObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    static const char *clientMethods[] = {
            "destroy",
            "create_queue",
            "delete_queue",
            "list_queues",
            "send_message",
            "receive_messages",
            "set_queue_attributes",
            "change_message_visibility",
            "delete_message",
            "delete_message_batch",
            "get_queue_attributes",
            nullptr
    };

    enum clientMethod {
        m_destroy,
        m_createQueue,
        m_deleteQueue,
        m_listQueues,
        m_sendMessage,
        m_receiveMessages,
        m_setQueueAttributes,
        m_changeMessageVisibility,
        m_deleteMessage,
        m_deleteMessageBatch,
        m_getQueueAttributes
    };

    if (objc < 2) {
        Tcl_ResetResult(interp);
        Tcl_SetStringObj(Tcl_GetObjResult(interp), (client_usage), -1);
        return TCL_ERROR;
    }
    Tcl_ResetResult(interp);

    int methodIndex;
    if (TCL_OK == Tcl_GetIndexFromObj(interp, objv[1], clientMethods, "method", 0, &methodIndex)) {
        Tcl_ResetResult(interp);
        const char *handle = Tcl_GetString(objv[0]);
        switch ((enum clientMethod) methodIndex) {
            case m_destroy:
                DBG(fprintf(stderr, "DestroyMethod\n"));
                CheckArgs(2, 2, 1, "destroy");
                return aws_sdk_tcl_sqs_Destroy(interp, handle);
            case m_createQueue:
                DBG(fprintf(stderr, "CreateQueueMethod\n"));
                CheckArgs(3, 3, 1, "create_queue queue_name");
                return aws_sdk_tcl_sqs_CreateQueue(
                        interp,
                        handle,
                        Tcl_GetString(objv[2])
                );
            case m_deleteQueue:
                DBG(fprintf(stderr, "DeleteQueueMethod\n"));
                CheckArgs(3, 3, 1, "delete_queue queue_url");
                return aws_sdk_tcl_sqs_DeleteQueue(
                        interp,
                        handle,
                        Tcl_GetString(objv[2])
                );
            case m_listQueues:
                DBG(fprintf(stderr, "ListQueuesMethod\n"));
                CheckArgs(2, 2, 1, "list_queues");
                return aws_sdk_tcl_sqs_ListQueues(interp, handle);
            case m_sendMessage:
                DBG(fprintf(stderr, "SendMessageMethod\n"));
                CheckArgs(4, 4, 1, "send_message queue_url message");
                return aws_sdk_tcl_sqs_SendMessage(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3])
                );
            case m_receiveMessages:
                DBG(fprintf(stderr, "ReceiveMessagesMethod\n"));
                CheckArgs(3, 4, 1, "receive_messages queue_url ?max_number_of_messages?");
                return aws_sdk_tcl_sqs_ReceiveMessages(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        objc == 4 ? objv[3] : nullptr
                );
            case m_setQueueAttributes:
                DBG(fprintf(stderr, "SetQueueAttributesMethod\n"));
                CheckArgs(4, 4, 1, "set_queue_attributes queue_url attributes_dict");
                return aws_sdk_tcl_sqs_SetQueueAttributes(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        objv[3]
                );
            case m_changeMessageVisibility:
                DBG(fprintf(stderr, "ChangeMessageVisibilityMethod\n"));
                CheckArgs(5, 5, 1,
                          "change_message_visibility queue_url message_receipt_handle visibility_timeout_seconds");
                return aws_sdk_tcl_sqs_ChangeMessageVisibility(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3]),
                        objv[4]
                );
            case m_deleteMessage:
                DBG(fprintf(stderr, "DeleteMessageMethod\n"));
                CheckArgs(4, 4, 1, "delete_message queue_url message_receipt_handle");
                return aws_sdk_tcl_sqs_DeleteMessage(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3])
                );
            case m_deleteMessageBatch:
                DBG(fprintf(stderr, "DeleteMessageBatchMethod\n"));
                CheckArgs(4, 4, 1, "delete_message_batch queue_url message_receipt_handles");
                return aws_sdk_tcl_sqs_DeleteMessageBatch(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        objv[3]
                );
            case m_getQueueAttributes:
                DBG(fprintf(stderr, "GetQueueAttributesMethod\n"));
                CheckArgs(3, 3, 1, "get_queue_attributes queue_url");
                return aws_sdk_tcl_sqs_GetQueueAttributes(
                        interp,
                        handle,
                        Tcl_GetString(objv[2])
                );
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj("Unknown method", -1));
    return TCL_ERROR;
}

static int aws_sdk_tcl_sqs_CreateCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CreateCmd\n"));

    CheckArgs(2, 2, 1, "config_dict");

    Aws::Client::ClientConfiguration clientConfig;
    Tcl_Obj *region;
    Tcl_Obj *endpoint;
    Tcl_DictObjGet(interp, objv[1], Tcl_NewStringObj("region", -1), &region);
    Tcl_DictObjGet(interp, objv[1], Tcl_NewStringObj("endpoint", -1), &endpoint);
    if (region) {
        clientConfig.region = Tcl_GetString(region);
    }
    if (endpoint) {
        clientConfig.endpointOverride = Tcl_GetString(endpoint);
    }

    auto *client = new Aws::SQS::SQSClient(clientConfig);
    char handle[80];
    CMD_NAME(handle, client);
    aws_sdk_tcl_sqs_RegisterName(handle, client);

    Tcl_CreateObjCommand(interp, handle,
                         (Tcl_ObjCmdProc *) aws_sdk_tcl_sqs_ClientObjCmd,
                         nullptr,
                         nullptr);
//                                 (Tcl_CmdDeleteProc*) aws_sdk_tcl_sqs_clientObjCmdDeleteProc);

    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

static int aws_sdk_tcl_sqs_DestroyCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "DestroyCmd\n"));
    CheckArgs(2, 2, 1, "handle");
    return aws_sdk_tcl_sqs_Destroy(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_sqs_CreateQueueCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CreateQueueCmd\n"));
    CheckArgs(3, 3, 1, "handle queue_name");
    return aws_sdk_tcl_sqs_CreateQueue(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2])
    );
}

static int aws_sdk_tcl_sqs_DeleteQueueCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "DeleteQueueCmd\n"));
    CheckArgs(3, 3, 1, "handle queue_url");
    return aws_sdk_tcl_sqs_DeleteQueue(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2])
    );
}

static int aws_sdk_tcl_sqs_ListQueuesCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "ListQueuesCmd\n"));
    CheckArgs(2, 2, 1, "handle");
    return aws_sdk_tcl_sqs_ListQueues(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_sqs_SendMessageCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "SendMessageCmd\n"));
    CheckArgs(4, 4, 1, "handle queue_url message");
    return aws_sdk_tcl_sqs_SendMessage(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            Tcl_GetString(objv[3])
    );
}

static int
aws_sdk_tcl_sqs_ReceiveMessagesCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "ReceiveMessagesCmd\n"));
    CheckArgs(3, 4, 1, "handle queue_url ?max_number_of_messages?");
    return aws_sdk_tcl_sqs_ReceiveMessages(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            objc == 4 ? objv[3] : nullptr
    );
}

static int
aws_sdk_tcl_sqs_SetQueueAttributesCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "SetQueueAttributesCmd\n"));
    CheckArgs(4, 4, 1, "handle queue_url attributes_dict");
    return aws_sdk_tcl_sqs_SetQueueAttributes(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            objv[3]
    );
}

static int
aws_sdk_tcl_sqs_ChangeMessageVisibilityCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "ChangeMessageVisibilityCmd\n"));
    CheckArgs(5, 5, 1, "handle queue_url message_receipt_handle visibility_timeout_seconds");
    return aws_sdk_tcl_sqs_ChangeMessageVisibility(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            Tcl_GetString(objv[3]),
            objv[4]
    );
}

static int
aws_sdk_tcl_sqs_DeleteMessageCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "DeleteMessageCmd\n"));
    CheckArgs(4, 4, 1, "handle queue_url message_receipt_handle");
    return aws_sdk_tcl_sqs_DeleteMessage(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            Tcl_GetString(objv[3])
    );
}

static int
aws_sdk_tcl_sqs_DeleteMessageBatchCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "DeleteMessageBatchCmd\n"));
    CheckArgs(4, 4, 1, "handle queue_url message_receipt_handles");
    return aws_sdk_tcl_sqs_DeleteMessageBatch(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            objv[3]
    );
}

static int
aws_sdk_tcl_sqs_GetQueueAttributesCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "GetQueueAttributesCmd\n"));
    CheckArgs(3, 3, 1, "handle queue_url");
    return aws_sdk_tcl_sqs_GetQueueAttributes(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2])
    );
}

static void aws_sdk_tcl_sqs_ExitHandler(ClientData unused) {
    Tcl_MutexLock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);
    Tcl_DeleteHashTable(&aws_sdk_tcl_sqs_NameToInternal_HT);
    Tcl_MutexUnlock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);

}


void aws_sdk_tcl_sqs_InitModule() {
    Tcl_MutexLock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);
    if (!aws_sdk_tcl_sqs_ModuleInitialized) {
        Aws::SDKOptions options;
        Aws::InitAPI(options);
        Tcl_InitHashTable(&aws_sdk_tcl_sqs_NameToInternal_HT, TCL_STRING_KEYS);
        Tcl_CreateThreadExitHandler(aws_sdk_tcl_sqs_ExitHandler, nullptr);
        aws_sdk_tcl_sqs_ModuleInitialized = 1;
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);
}

int Aws_sdk_tcl_sqs_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.6", 0) == nullptr) {
        return TCL_ERROR;
    }

    aws_sdk_tcl_sqs_InitModule();

    Tcl_CreateNamespace(interp, "::aws::sqs", nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::sqs::create", aws_sdk_tcl_sqs_CreateCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::sqs::destroy", aws_sdk_tcl_sqs_DestroyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::sqs::create_queue", aws_sdk_tcl_sqs_CreateQueueCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::sqs::delete_queue", aws_sdk_tcl_sqs_DeleteQueueCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::sqs::list_queues", aws_sdk_tcl_sqs_ListQueuesCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::sqs::send_message", aws_sdk_tcl_sqs_SendMessageCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::sqs::receive_messages", aws_sdk_tcl_sqs_ReceiveMessagesCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::sqs::set_queue_attributes", aws_sdk_tcl_sqs_SetQueueAttributesCmd, nullptr,nullptr);
    Tcl_CreateObjCommand(interp, "::aws::sqs::change_message_visibility", aws_sdk_tcl_sqs_ChangeMessageVisibilityCmd,nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::sqs::delete_message", aws_sdk_tcl_sqs_DeleteMessageCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::sqs::delete_message_batch", aws_sdk_tcl_sqs_DeleteMessageBatchCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::sqs::get_queue_attributes", aws_sdk_tcl_sqs_GetQueueAttributesCmd, nullptr, nullptr);

    return Tcl_PkgProvide(interp, "awssqs", XSTR(VERSION));
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Aws_sdk_tcl_sqs_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
