/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#include <iostream>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/ssm/SSMClient.h>
#include <aws/ssm/model/PutParameterRequest.h>
#include <aws/ssm/model/GetParameterRequest.h>
#include <aws/ssm/model/DeleteParameterRequest.h>
#include <cstdio>
#include <fstream>
#include "library.h"
#include "../common/common.h"

#ifndef TCL_SIZE_MAX
typedef int Tcl_Size;
# define Tcl_GetSizeIntFromObj Tcl_GetIntFromObj
# define Tcl_NewSizeIntObj Tcl_NewIntObj
# define TCL_SIZE_MAX      INT_MAX
# define TCL_SIZE_MODIFIER ""
#endif

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

#define CMD_NAME(s, internal) std::sprintf((s), "_AWS_SSM_%p", (internal))

static char VAR_READ_ONLY_MSG[] = "var is read-only";

typedef struct {
    Tcl_Interp *interp;
    char *handle;
    char *varname;
    Aws::SSM::SSMClient *item;
} aws_sdk_tcl_ssm_trace_t;

static Tcl_HashTable aws_sdk_tcl_ssm_NameToInternal_HT;
static Tcl_Mutex aws_sdk_tcl_ssm_NameToInternal_HT_Mutex;
static int aws_sdk_tcl_ssm_ModuleInitialized;

static char client_usage[] =
        "Usage ssmClient <method> <args>, where method can be:\n"
        "  destroy\n"
        "  put_parameter name value ?type?\n"
        "  get_parameter name\n"
        "  delete_parameter name\n"
        ;


static int
aws_sdk_tcl_ssm_RegisterName(const char *name, Aws::SSM::SSMClient *internal) {

    Tcl_HashEntry *entryPtr;
    int newEntry;

    Tcl_MutexLock(&aws_sdk_tcl_ssm_NameToInternal_HT_Mutex);
    entryPtr = Tcl_CreateHashEntry(&aws_sdk_tcl_ssm_NameToInternal_HT, (char *) name, &newEntry);
    if (newEntry) {
        Tcl_SetHashValue(entryPtr, (ClientData) internal);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_ssm_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> RegisterName: name=%s internal=%p %s\n", name, internal,
                newEntry ? "entered into" : "already in"));

    return !!newEntry;
}

static int
aws_sdk_tcl_ssm_UnregisterName(const char *name) {

    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_ssm_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_ssm_NameToInternal_HT, (char *) name);
    if (entryPtr != nullptr) {
        Tcl_DeleteHashEntry(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_ssm_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> UnregisterName: name=%s entryPtr=%p\n", name, entryPtr));

    return entryPtr != nullptr;
}

static struct Aws::SSM::SSMClient *
aws_sdk_tcl_ssm_GetInternalFromName(const char *name) {
    Aws::SSM::SSMClient *internal = nullptr;
    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_ssm_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_ssm_NameToInternal_HT, (char *) name);
    if (entryPtr != nullptr) {
        internal = (Aws::SSM::SSMClient *) Tcl_GetHashValue(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_ssm_NameToInternal_HT_Mutex);

    return internal;
}

int aws_sdk_tcl_ssm_Destroy(Tcl_Interp *interp, const char *handle) {
    Aws::SSM::SSMClient *client = aws_sdk_tcl_ssm_GetInternalFromName(handle);
    if (!aws_sdk_tcl_ssm_UnregisterName(handle)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    delete client;
    Tcl_DeleteCommand(interp, handle);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

int aws_sdk_tcl_ssm_PutParameter(Tcl_Interp *interp, const char *handle, const char *name, const char *value, Tcl_Obj *type) {
    Aws::SSM::SSMClient *client = aws_sdk_tcl_ssm_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::SSM::Model::PutParameterRequest request;
    request.SetName(name);
    request.SetValue(value);

    if (type) {
        Tcl_Size type_length;
        const char *typeStr = Tcl_GetStringFromObj(type, &type_length);
        if (type_length == 6 && strcmp(typeStr, "String") == 0) {
            request.SetType(Aws::SSM::Model::ParameterType::String);
        } else if (type_length == 10 && strcmp(typeStr, "StringList") == 0) {
            request.SetType(Aws::SSM::Model::ParameterType::StringList);
        } else if (type_length == 12 && strcmp(typeStr, "SecureString") == 0) {
            request.SetType(Aws::SSM::Model::ParameterType::SecureString);
        } else {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid type", -1));
            return TCL_ERROR;
        }
    }

    auto outcome = client->PutParameter(request);
    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }

    return TCL_OK;
}

int aws_sdk_tcl_ssm_GetParameter(Tcl_Interp *interp, const char *handle, const char *name) {
    Aws::SSM::SSMClient *client = aws_sdk_tcl_ssm_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::SSM::Model::GetParameterRequest request;
    request.SetName(name);

    auto outcome = client->GetParameter(request);
    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }

    const Aws::SSM::Model::Parameter &parameter = outcome.GetResult().GetParameter();
    Tcl_SetObjResult(interp, Tcl_NewStringObj(parameter.GetValue().c_str(), -1));
    return TCL_OK;
}

int aws_sdk_tcl_ssm_DeleteParameter(Tcl_Interp *interp, const char *handle, const char *name) {
    Aws::SSM::SSMClient *client = aws_sdk_tcl_ssm_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::SSM::Model::DeleteParameterRequest request;
    request.SetName(name);

    auto outcome = client->DeleteParameter(request);
    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
    return TCL_OK;
}


int aws_sdk_tcl_ssm_ClientObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    static const char *clientMethods[] = {
            "destroy",
            "put_parameter",
            "get_parameter",
            "delete_parameter",
            nullptr
    };

    enum clientMethod {
        m_destroy,
        m_putParameter,
        m_getParameter,
        m_deleteParameter
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
                return aws_sdk_tcl_ssm_Destroy(interp, handle);
            case m_putParameter:
                return aws_sdk_tcl_ssm_PutParameter(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3]),
                        objc == 5 ? objv[4]: nullptr);
            case m_getParameter:
                return aws_sdk_tcl_ssm_GetParameter(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]));
            case m_deleteParameter:
                return aws_sdk_tcl_ssm_DeleteParameter(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]));
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj("Unknown method", -1));
    return TCL_ERROR;
}

char *aws_sdk_tcl_ssm_VarTraceProc(ClientData clientData, Tcl_Interp *interp, const char *name1, const char *name2, int flags) {
    auto *trace = (aws_sdk_tcl_ssm_trace_t *) clientData;
    if (trace->item == nullptr) {
        DBG(fprintf(stderr, "VarTraceProc: client has been deleted\n"));
        if (!Tcl_InterpDeleted(trace->interp)) {
            Tcl_UntraceVar(trace->interp, trace->varname, TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                           (Tcl_VarTraceProc*) aws_sdk_tcl_ssm_VarTraceProc,
                           (ClientData) clientData);
        }
        Tcl_Free((char *) trace->varname);
        Tcl_Free((char *) trace->handle);
        Tcl_Free((char *) trace);
        return nullptr;
    }
    if (flags & TCL_TRACE_WRITES) {
        DBG(fprintf(stderr, "VarTraceProc: TCL_TRACE_WRITES\n"));
        Tcl_SetVar2(trace->interp, name1, name2, trace->handle, TCL_LEAVE_ERR_MSG);
        return VAR_READ_ONLY_MSG;
    }
    if (flags & TCL_TRACE_UNSETS) {
        fprintf(stderr, "VarTraceProc: TCL_TRACE_UNSETS\n");
        aws_sdk_tcl_ssm_Destroy(trace->interp, trace->handle);
        Tcl_Free((char *) trace->varname);
        Tcl_Free((char *) trace->handle);
        Tcl_Free((char *) trace);
    }
    return nullptr;
}

static int aws_sdk_tcl_ssm_CreateCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CreateCmd\n"));

    CheckArgs(2, 3, 1, "config_dict ?varname?");

    auto result = get_client_config_and_credentials_provider(interp, objv[1]);
    int status = std::get<0>(result);
    if (TCL_OK != status) {
        SetResult("Invalid config_dict");
        return TCL_ERROR;
    }
    Aws::Client::ClientConfiguration client_config = std::get<1>(result);
    std::shared_ptr<Aws::Auth::AWSCredentialsProvider> credentials_provider_ptr = std::get<2>(result);

    auto *client = credentials_provider_ptr != nullptr ? new Aws::SSM::SSMClient(credentials_provider_ptr, client_config) : new Aws::SSM::SSMClient(client_config);
    char handle[80];
    CMD_NAME(handle, client);
    aws_sdk_tcl_ssm_RegisterName(handle, client);

    Tcl_CreateObjCommand(interp, handle,
                         (Tcl_ObjCmdProc *) aws_sdk_tcl_ssm_ClientObjCmd,
                         nullptr,
                         nullptr);
//                                 (Tcl_CmdDeleteProc*) aws_sdk_tcl_ssm_clientObjCmdDeleteProc);


    if (objc == 3) {
        auto *trace = (aws_sdk_tcl_ssm_trace_t *) Tcl_Alloc(sizeof(aws_sdk_tcl_ssm_trace_t));
        trace->interp = interp;
        trace->varname = aws_sdk_strndup(Tcl_GetString(objv[2]), 80);
        trace->handle = aws_sdk_strndup(handle, 80);
        trace->item = client;
        const char *objVar = Tcl_GetString(objv[2]);
        Tcl_UnsetVar(interp, objVar, 0);
        Tcl_SetVar  (interp, objVar, handle, 0);
        Tcl_TraceVar(interp,objVar,TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                     (Tcl_VarTraceProc*) aws_sdk_tcl_ssm_VarTraceProc,
                     (ClientData) trace);
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

static int aws_sdk_tcl_ssm_DestroyCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "DestroyCmd\n"));
    CheckArgs(2, 2, 1, "handle");
    return aws_sdk_tcl_ssm_Destroy(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_ssm_PutParameterCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "PutParameterCmd\n"));
    CheckArgs(4, 5, 1, "handle name value ?type?");
    return aws_sdk_tcl_ssm_PutParameter(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            Tcl_GetString(objv[3]),
            objc == 5 ? objv[4]: nullptr
    );
}

static int aws_sdk_tcl_ssm_GetParameterCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "GetParameterCmd\n"));
    CheckArgs(3, 4, 1, "handle name");
    return aws_sdk_tcl_ssm_GetParameter(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2])
    );
}

static int aws_sdk_tcl_ssm_DeleteParameterCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "DeleteParameterCmd\n"));
    CheckArgs(3, 3, 1, "handle name");
    return aws_sdk_tcl_ssm_DeleteParameter(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2])
    );
}

static void aws_sdk_tcl_ssm_ExitHandler(ClientData unused) {
    Tcl_MutexLock(&aws_sdk_tcl_ssm_NameToInternal_HT_Mutex);
    Tcl_DeleteHashTable(&aws_sdk_tcl_ssm_NameToInternal_HT);
    Tcl_MutexUnlock(&aws_sdk_tcl_ssm_NameToInternal_HT_Mutex);

}


void aws_sdk_tcl_ssm_InitModule() {
    Tcl_MutexLock(&aws_sdk_tcl_ssm_NameToInternal_HT_Mutex);
    if (!aws_sdk_tcl_ssm_ModuleInitialized) {
        Aws::SDKOptions options;
        Aws::InitAPI(options);
        Tcl_InitHashTable(&aws_sdk_tcl_ssm_NameToInternal_HT, TCL_STRING_KEYS);
        Tcl_CreateThreadExitHandler(aws_sdk_tcl_ssm_ExitHandler, nullptr);
        aws_sdk_tcl_ssm_ModuleInitialized = 1;
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_ssm_NameToInternal_HT_Mutex);
}

int Aws_sdk_tcl_ssm_Init(Tcl_Interp *interp) {

    int major, minor, patchLevel, type;
    Tcl_GetVersion(&major, &minor, &patchLevel, &type);

    const char *version = major == 9 ? "9.0" : "8.6";
    if (Tcl_InitStubs(interp, version, 0) == nullptr) {
        return TCL_ERROR;
    }

    aws_sdk_tcl_ssm_InitModule();

    Tcl_CreateNamespace(interp, "::aws::ssm", nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::ssm::create", aws_sdk_tcl_ssm_CreateCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::ssm::destroy", aws_sdk_tcl_ssm_DestroyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::ssm::put_parameter", aws_sdk_tcl_ssm_PutParameterCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::ssm::get_parameter", aws_sdk_tcl_ssm_GetParameterCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::ssm::delete_parameter", aws_sdk_tcl_ssm_DeleteParameterCmd, nullptr, nullptr);

    return Tcl_PkgProvide(interp, "awsssm", XSTR(PROJECT_VERSION));
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Aws_sdk_tcl_ssm_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
