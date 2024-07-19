/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#include <iostream>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/iam/IAMClient.h>
#include <aws/iam/model/CreateRoleRequest.h>
#include <aws/iam/model/DeleteRoleRequest.h>
#include <aws/iam/model/ListPoliciesRequest.h>
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

#define CMD_NAME(s, internal) std::sprintf((s), "_AWS_IAM_%p", (internal))

static char VAR_READ_ONLY_MSG[] = "var is read-only";

typedef struct {
    Tcl_Interp *interp;
    char *handle;
    char *varname;
    Aws::IAM::IAMClient *item;
} aws_sdk_tcl_iam_trace_t;

static Tcl_HashTable aws_sdk_tcl_iam_NameToInternal_HT;
static Tcl_Mutex aws_sdk_tcl_iam_NameToInternal_HT_Mutex;
static int aws_sdk_tcl_iam_ModuleInitialized;

static char client_usage[] =
        "Usage iamClient <method> <args>, where method can be:\n"
        "  create_role role_name policy\n"
        "  delete_role role_name\n"
        "  list_policies\n"
        "  destroy\n"
        ;


static int
aws_sdk_tcl_iam_RegisterName(const char *name, Aws::IAM::IAMClient *internal) {

    Tcl_HashEntry *entryPtr;
    int newEntry;

    Tcl_MutexLock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);
    entryPtr = Tcl_CreateHashEntry(&aws_sdk_tcl_iam_NameToInternal_HT, (char *) name, &newEntry);
    if (newEntry) {
        Tcl_SetHashValue(entryPtr, (ClientData) internal);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> RegisterName: name=%s internal=%p %s\n", name, internal,
                newEntry ? "entered into" : "already in"));

    return !!newEntry;
}

static int
aws_sdk_tcl_iam_UnregisterName(const char *name) {

    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_iam_NameToInternal_HT, (char *) name);
    if (entryPtr != nullptr) {
        Tcl_DeleteHashEntry(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> UnregisterName: name=%s entryPtr=%p\n", name, entryPtr));

    return entryPtr != nullptr;
}

static struct Aws::IAM::IAMClient *
aws_sdk_tcl_iam_GetInternalFromName(const char *name) {
    Aws::IAM::IAMClient *internal = nullptr;
    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_iam_NameToInternal_HT, (char *) name);
    if (entryPtr != nullptr) {
        internal = (Aws::IAM::IAMClient *) Tcl_GetHashValue(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);

    return internal;
}

int aws_sdk_tcl_iam_Destroy(Tcl_Interp *interp, const char *handle) {
    Aws::IAM::IAMClient *client = aws_sdk_tcl_iam_GetInternalFromName(handle);
    if (!aws_sdk_tcl_iam_UnregisterName(handle)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    delete client;
    Tcl_DeleteCommand(interp, handle);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

int aws_sdk_tcl_iam_CreateRole(Tcl_Interp *interp, const char *handle, const char *role_name, const char *policy) {
    Aws::IAM::IAMClient *client = aws_sdk_tcl_iam_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    Aws::IAM::Model::CreateRoleRequest request;

    request.SetRoleName(role_name);
    request.SetAssumeRolePolicyDocument(policy);

    Aws::IAM::Model::CreateRoleOutcome outcome = client->CreateRole(request);
    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
    else {
        const Aws::IAM::Model::Role iamRole = outcome.GetResult().GetRole();
        Tcl_Obj *dictPtr = Tcl_NewDictObj();
        Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("role_name", -1), Tcl_NewStringObj(iamRole.GetRoleName().c_str(), -1));
        Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("role_id", -1), Tcl_NewStringObj(iamRole.GetRoleId().c_str(), -1));
        Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("arn", -1), Tcl_NewStringObj(iamRole.GetArn().c_str(), -1));
        Tcl_SetObjResult(interp, dictPtr);
        return TCL_OK;
    }
}

int aws_sdk_tcl_iam_DeleteRole(Tcl_Interp *interp, const char *handle, const char *role_name) {
    Aws::IAM::IAMClient *client = aws_sdk_tcl_iam_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    Aws::IAM::Model::DeleteRoleRequest request;

    request.SetRoleName(role_name);

    Aws::IAM::Model::DeleteRoleOutcome outcome = client->DeleteRole(request);
    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

int aws_sdk_tcl_iam_ListPolicies(Tcl_Interp *interp, const char *handle) {
    Aws::IAM::IAMClient *client = aws_sdk_tcl_iam_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    Aws::IAM::Model::ListPoliciesRequest request;

    Tcl_Obj *listPtr = Tcl_NewListObj(0, nullptr);
    bool done = false;
    while (!done) {
        auto outcome = client->ListPolicies(request);
        if (!outcome.IsSuccess()) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
            return TCL_ERROR;
        }
        const Aws::String DATE_FORMAT("%Y-%m-%d");
        const auto &policies = outcome.GetResult().GetPolicies();
        for (const auto &policy: policies) {
            Tcl_Obj *dictPtr = Tcl_NewDictObj();
            Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("policy_name", -1), Tcl_NewStringObj(policy.GetPolicyName().c_str(), -1));
            Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("policy_id", -1), Tcl_NewStringObj(policy.GetPolicyId().c_str(), -1));
            Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("arn", -1), Tcl_NewStringObj(policy.GetArn().c_str(), -1));
            Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("description", -1), Tcl_NewStringObj(policy.GetDescription().c_str(), -1));
            Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("create_date", -1), Tcl_NewStringObj(policy.GetCreateDate().ToGmtString(DATE_FORMAT.c_str()).c_str(), -1));
            Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("update_date", -1), Tcl_NewStringObj(policy.GetUpdateDate().ToGmtString(DATE_FORMAT.c_str()).c_str(), -1));
            Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("attachment_count", -1), Tcl_NewIntObj(policy.GetAttachmentCount()));
            Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("is_attachable", -1), Tcl_NewBooleanObj(policy.GetIsAttachable()));
            Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("path", -1), Tcl_NewStringObj(policy.GetPath().c_str(), -1));
            Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("default_version_id", -1), Tcl_NewStringObj(policy.GetDefaultVersionId().c_str(), -1));

            Tcl_ListObjAppendElement(interp, listPtr, dictPtr);
        }

        if (outcome.GetResult().GetIsTruncated()) {
            request.SetMarker(outcome.GetResult().GetMarker());
        }
        else {
            done = true;
        }
    }
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

int aws_sdk_tcl_iam_ClientObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    static const char *clientMethods[] = {
            "destroy",
            "create_role",
            "delete_role",
            "list_policies",
            nullptr
    };

    enum clientMethod {
        m_destroy,
        m_createRole,
        m_deleteRole,
        m_listPolicies
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
                return aws_sdk_tcl_iam_Destroy(interp, handle);
            case m_createRole:
                CheckArgs(4, 4, 1, "create_role role_name policy");
                return aws_sdk_tcl_iam_CreateRole(interp, handle, Tcl_GetString(objv[2]), Tcl_GetString(objv[3]));
            case m_deleteRole:
                CheckArgs(3, 3, 1, "create_role role_name");
                return aws_sdk_tcl_iam_DeleteRole(interp, handle, Tcl_GetString(objv[2]));
            case m_listPolicies:
                CheckArgs(2, 2, 1, "list_policies");
                return aws_sdk_tcl_iam_ListPolicies(interp, handle);
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj("Unknown method", -1));
    return TCL_ERROR;
}

char *aws_sdk_tcl_iam_VarTraceProc(ClientData clientData, Tcl_Interp *interp, const char *name1, const char *name2, int flags) {
    auto *trace = (aws_sdk_tcl_iam_trace_t *) clientData;
    if (trace->item == nullptr) {
        DBG(fprintf(stderr, "VarTraceProc: client has been deleted\n"));
        if (!Tcl_InterpDeleted(trace->interp)) {
            Tcl_UntraceVar(trace->interp, trace->varname, TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                           (Tcl_VarTraceProc*) aws_sdk_tcl_iam_VarTraceProc,
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
        DBG(fprintf(stderr, "VarTraceProc: TCL_TRACE_UNSETS\n"));
        aws_sdk_tcl_iam_Destroy(trace->interp, trace->handle);
        Tcl_Free((char *) trace->varname);
        Tcl_Free((char *) trace->handle);
        Tcl_Free((char *) trace);
    }
    return nullptr;
}

static int aws_sdk_tcl_iam_CreateCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
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

    auto *client = credentials_provider_ptr != nullptr ? new Aws::IAM::IAMClient(credentials_provider_ptr, client_config) : new Aws::IAM::IAMClient(client_config);
    char handle[80];
    CMD_NAME(handle, client);
    aws_sdk_tcl_iam_RegisterName(handle, client);

    Tcl_CreateObjCommand(interp, handle,
                         (Tcl_ObjCmdProc *) aws_sdk_tcl_iam_ClientObjCmd,
                         nullptr,
                         nullptr);
//                                 (Tcl_CmdDeleteProc*) aws_sdk_tcl_iam_clientObjCmdDeleteProc);


    if (objc == 3) {
        auto *trace = (aws_sdk_tcl_iam_trace_t *) Tcl_Alloc(sizeof(aws_sdk_tcl_iam_trace_t));
        trace->interp = interp;
        trace->varname = aws_sdk_strndup(Tcl_GetString(objv[2]), 80);
        trace->handle = aws_sdk_strndup(handle, 80);
        trace->item = client;
        const char *objVar = Tcl_GetString(objv[2]);
        Tcl_UnsetVar(interp, objVar, 0);
        Tcl_SetVar  (interp, objVar, handle, 0);
        Tcl_TraceVar(interp,objVar,TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                     (Tcl_VarTraceProc*) aws_sdk_tcl_iam_VarTraceProc,
                     (ClientData) trace);
    }


    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

static int aws_sdk_tcl_iam_DestroyCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "DestroyCmd\n"));
    CheckArgs(2, 2, 1, "handle");
    return aws_sdk_tcl_iam_Destroy(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_iam_CreateRoleCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CreateRoleCmd\n"));
    CheckArgs(4, 4, 1, "handle role_name policy");
    return aws_sdk_tcl_iam_CreateRole(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), Tcl_GetString(objv[3]));
}

static int aws_sdk_tcl_iam_DeleteRoleCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "DeleteRoleCmd\n"));
    CheckArgs(3, 3, 1, "handle role_name");
    return aws_sdk_tcl_iam_DeleteRole(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]));
}

static int aws_sdk_tcl_iam_ListPoliciesCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "ListPoliciesCmd\n"));
    CheckArgs(2, 2, 1, "handle");
    return aws_sdk_tcl_iam_ListPolicies(interp, Tcl_GetString(objv[1]));
}

static Aws::SDKOptions options;

static void aws_sdk_tcl_iam_ExitHandler(ClientData unused) {
    Tcl_MutexLock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);
    Tcl_DeleteHashTable(&aws_sdk_tcl_iam_NameToInternal_HT);
    Aws::ShutdownAPI(options);
    Tcl_MutexUnlock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);

}


void aws_sdk_tcl_iam_InitModule() {
    Tcl_MutexLock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);
    if (!aws_sdk_tcl_iam_ModuleInitialized) {
        Aws::InitAPI(options);
        Tcl_InitHashTable(&aws_sdk_tcl_iam_NameToInternal_HT, TCL_STRING_KEYS);
        Tcl_CreateThreadExitHandler(aws_sdk_tcl_iam_ExitHandler, nullptr);
        aws_sdk_tcl_iam_ModuleInitialized = 1;
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);
}

#if TCL_MAJOR_VERSION > 8
#define MIN_VERSION "9.0"
#else
#define MIN_VERSION "8.6"
#endif

int Aws_sdk_tcl_iam_Init(Tcl_Interp *interp) {

    if (Tcl_InitStubs(interp, MIN_VERSION, 0) == nullptr) {
        SetResult("Tcl_InitStubs failed");
        return TCL_ERROR;
    }

    aws_sdk_tcl_iam_InitModule();

    Tcl_CreateNamespace(interp, "::aws::iam", nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::iam::create", aws_sdk_tcl_iam_CreateCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::iam::destroy", aws_sdk_tcl_iam_DestroyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::iam::create_role", aws_sdk_tcl_iam_CreateRoleCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::iam::delete_role", aws_sdk_tcl_iam_DeleteRoleCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::iam::list_policies", aws_sdk_tcl_iam_ListPoliciesCmd, nullptr, nullptr);

    return Tcl_PkgProvide(interp, "awsiam", XSTR(VERSION));
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Aws_sdk_tcl_iam_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
