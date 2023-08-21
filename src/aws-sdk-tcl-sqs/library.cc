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
#include <cstdio>
#include <fstream>
#include "library.h"

#ifdef DEBUG
# define DBG(x) x
#else
# define DBG(x)
#endif

#define CheckArgs(min,max,n,msg) \
                 if ((objc < min) || (objc >max)) { \
                     Tcl_WrongNumArgs(interp, n, objv, msg); \
                     return TCL_ERROR; \
                 }

#define CMD_NAME(s,internal) std::sprintf((s), "_AWS_S3_%p", (internal))

static Tcl_HashTable aws_sdk_tcl_sqs_NameToInternal_HT;
static Tcl_Mutex     aws_sdk_tcl_sqs_NameToInternal_HT_Mutex;
static int           aws_sdk_tcl_sqs_ModuleInitialized;

static char client_usage[] =
    "Usage sqsClient <method> <args>, where method can be:\n"
    "  destroy\n"
    "  create_queue queue_name\n"
    "  delete_queue queue_url\n"
    "  list_queues\n"
    "  send_message queue_url message\n"
    "  receive_messages queue_url ?max_number_of_messages?\n"
;


static int
aws_sdk_tcl_sqs_RegisterName(const char *name, Aws::SQS::SQSClient *internal) {

    Tcl_HashEntry *entryPtr;
    int newEntry;

    Tcl_MutexLock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);
    entryPtr = Tcl_CreateHashEntry(&aws_sdk_tcl_sqs_NameToInternal_HT, (char*) name, &newEntry);
    if (newEntry) {
        Tcl_SetHashValue(entryPtr, (ClientData)internal);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> RegisterName: name=%s internal=%p %s\n", name, internal, newEntry ? "entered into" : "already in"));

    return !!newEntry;
}

static int
aws_sdk_tcl_sqs_UnregisterName(const char *name) {

    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_sqs_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_sqs_NameToInternal_HT, (char*)name);
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
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_sqs_NameToInternal_HT, (char*)name);
    if (entryPtr != nullptr) {
        internal = (Aws::SQS::SQSClient *)Tcl_GetHashValue(entryPtr);
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

int aws_sdk_tcl_sqs_ReceiveMessages(Tcl_Interp *interp, const char *handle, const char *queue_url, Tcl_Obj *maxNumberOfMessagesPtr) {
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
            Tcl_DictObjPut(interp, messagePtr, Tcl_NewStringObj("MessageId", -1), Tcl_NewStringObj(iter.GetMessageId().c_str(), -1));
            Tcl_DictObjPut(interp, messagePtr, Tcl_NewStringObj("ReceiptHandle", -1), Tcl_NewStringObj(iter.GetReceiptHandle().c_str(), -1));
            Tcl_DictObjPut(interp, messagePtr, Tcl_NewStringObj("Body", -1), Tcl_NewStringObj(iter.GetBody().c_str(), -1));
            Tcl_ListObjAppendElement(interp, listPtr, messagePtr);
        }
        Tcl_SetObjResult(interp, listPtr);
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_sqs_ClientObjCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    static const char *clientMethods[] = {
            "destroy",
            "create_queue",
            "delete_queue",
            "list_queues",
            "send_message",
            "receive_messages",
            nullptr
    };

    enum clientMethod {
        m_destroy,
        m_createQueue,
        m_deleteQueue,
        m_listQueues,
        m_sendMessage,
        m_receiveMessages
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
        switch ((enum clientMethod) methodIndex ) {
            case m_destroy:
                DBG(fprintf(stderr, "DestroyMethod\n"));
                CheckArgs(2,2,1,"destroy");
                return aws_sdk_tcl_sqs_Destroy(interp, handle);
            case m_createQueue:
                DBG(fprintf(stderr, "CreateQueueMethod\n"));
                CheckArgs(3,3,1,"create_queue queue_name");
                return aws_sdk_tcl_sqs_CreateQueue(
                        interp,
                        handle,
                        Tcl_GetString(objv[2])
                );
            case m_deleteQueue:
                DBG(fprintf(stderr, "DeleteQueueMethod\n"));
                CheckArgs(3,3,1,"delete_queue queue_url");
                return aws_sdk_tcl_sqs_DeleteQueue(
                        interp,
                        handle,
                        Tcl_GetString(objv[2])
                );
            case m_listQueues:
                DBG(fprintf(stderr, "ListQueuesMethod\n"));
                CheckArgs(2,2,1,"list_queues");
                return aws_sdk_tcl_sqs_ListQueues(interp, handle);
            case m_sendMessage:
                DBG(fprintf(stderr, "SendMessageMethod\n"));
                CheckArgs(4,4,1,"send_message queue_url message");
                return aws_sdk_tcl_sqs_SendMessage(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3])
                );
            case m_receiveMessages:
                DBG(fprintf(stderr, "ReceiveMessagesMethod\n"));
                CheckArgs(3,4,1,"receive_messages queue_url ?max_number_of_messages?");
                return aws_sdk_tcl_sqs_ReceiveMessages(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        objc == 4 ? objv[3] : nullptr
                );
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj("Unknown method", -1));
    return TCL_ERROR;
}

static int aws_sdk_tcl_sqs_CreateCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "CreateCmd\n"));

    CheckArgs(2,2,1,"config_dict");

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
                                 (Tcl_ObjCmdProc *)  aws_sdk_tcl_sqs_ClientObjCmd,
                                 nullptr,
                                 nullptr);
//                                 (Tcl_CmdDeleteProc*) aws_sdk_tcl_sqs_clientObjCmdDeleteProc);

    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

static int aws_sdk_tcl_sqs_DestroyCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "DestroyCmd\n"));
    CheckArgs(2,2,1,"handle");
    return aws_sdk_tcl_sqs_Destroy(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_sqs_CreateQueueCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "CreateQueueCmd\n"));
    CheckArgs(3,3,1,"handle queue_name");
    return aws_sdk_tcl_sqs_CreateQueue(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2])
    );
}

static int aws_sdk_tcl_sqs_DeleteQueueCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "DeleteQueueCmd\n"));
    CheckArgs(3,3,1,"handle queue_url");
    return aws_sdk_tcl_sqs_DeleteQueue(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2])
    );
}

static int aws_sdk_tcl_sqs_ListQueuesCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "ListQueuesCmd\n"));
    CheckArgs(2,2,1,"handle");
    return aws_sdk_tcl_sqs_ListQueues(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_sqs_SendMessageCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "SendMessageCmd\n"));
    CheckArgs(4,4,1,"handle queue_url message");
    return aws_sdk_tcl_sqs_SendMessage(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            Tcl_GetString(objv[3])
    );
}

static int aws_sdk_tcl_sqs_ReceiveMessagesCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "SendMessageCmd\n"));
    CheckArgs(3,4,1,"handle queue_url ?max_number_of_messages?");
    return aws_sdk_tcl_sqs_ReceiveMessages(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            objc == 4 ? objv[3] : nullptr
    );
}

static void aws_sdk_tcl_sqs_ExitHandler(ClientData unused)
{
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

    return Tcl_PkgProvide(interp, "awssqs", "0.1");
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Aws_sdk_tcl_sqs_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
