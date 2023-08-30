/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#include <iostream>
#include <aws/core/Aws.h>
#include <aws/iam/IAMClient.h>
#include <aws/iam/model/CreateRoleRequest.h>
#include <aws/iam/model/DeleteRoleRequest.h>
#include <aws/iam/model/ListPoliciesRequest.h>
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

#define CMD_NAME(s, internal) std::sprintf((s), "_AWS_IAM_%p", (internal))

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

static int aws_sdk_tcl_iam_CreateCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
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

    auto *client = new Aws::IAM::IAMClient(clientConfig);
    char handle[80];
    CMD_NAME(handle, client);
    aws_sdk_tcl_iam_RegisterName(handle, client);

    Tcl_CreateObjCommand(interp, handle,
                         (Tcl_ObjCmdProc *) aws_sdk_tcl_iam_ClientObjCmd,
                         nullptr,
                         nullptr);
//                                 (Tcl_CmdDeleteProc*) aws_sdk_tcl_iam_clientObjCmdDeleteProc);

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

static void aws_sdk_tcl_iam_ExitHandler(ClientData unused) {
    Tcl_MutexLock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);
    Tcl_DeleteHashTable(&aws_sdk_tcl_iam_NameToInternal_HT);
    Tcl_MutexUnlock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);

}


void aws_sdk_tcl_iam_InitModule() {
    Tcl_MutexLock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);
    if (!aws_sdk_tcl_iam_ModuleInitialized) {
        Aws::SDKOptions options;
        Aws::InitAPI(options);
        Tcl_InitHashTable(&aws_sdk_tcl_iam_NameToInternal_HT, TCL_STRING_KEYS);
        Tcl_CreateThreadExitHandler(aws_sdk_tcl_iam_ExitHandler, nullptr);
        aws_sdk_tcl_iam_ModuleInitialized = 1;
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_iam_NameToInternal_HT_Mutex);
}

int Aws_sdk_tcl_iam_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.6", 0) == nullptr) {
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
