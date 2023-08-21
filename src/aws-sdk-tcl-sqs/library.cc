/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#include <iostream>
#include <aws/core/Aws.h>
#include <aws/sqs/SQSClient.h>
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

int aws_sdk_tcl_sqs_ClientObjCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    static const char *clientMethods[] = {
            "destroy",
            nullptr
    };

    enum clientMethod {
        m_destroy
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

    return Tcl_PkgProvide(interp, "awssqs", "0.1");
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Aws_sdk_tcl_sqs_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
