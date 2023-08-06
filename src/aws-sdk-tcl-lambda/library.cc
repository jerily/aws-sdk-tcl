/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-License-Identifier: MIT.
 */
#include <iostream>
#include <aws/core/Aws.h>
#include <aws/lambda/LambdaClient.h>
#include <cstdio>
#include <fstream>
#include <aws/lambda/model/ListFunctionsRequest.h>
#include "library.h"

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

#define CMD_NAME(s, internal) std::sprintf((s), "_AWS_LAMBDA_%p", (internal))

static Tcl_HashTable aws_sdk_tcl_lambda_NameToInternal_HT;
static Tcl_Mutex aws_sdk_tcl_lambda_NameToInternal_HT_Mutex;
static int aws_sdk_tcl_lambda_ModuleInitialized;

static char lambda_client_usage[] =
        "Usage lambdaClient <method> <args>, where method can be:\n";


static int
aws_sdk_tcl_lambda_RegisterName(const char *name, Aws::Lambda::LambdaClient *internal) {

    Tcl_HashEntry *entryPtr;
    int newEntry;

    Tcl_MutexLock(&aws_sdk_tcl_lambda_NameToInternal_HT_Mutex);
    entryPtr = Tcl_CreateHashEntry(&aws_sdk_tcl_lambda_NameToInternal_HT, (char *) name, &newEntry);
    if (newEntry) {
        Tcl_SetHashValue(entryPtr, (ClientData) internal);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_lambda_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> RegisterName: name=%s internal=%p %s\n", name, internal,
                newEntry ? "entered into" : "already in"));

    return !!newEntry;
}

static int
aws_sdk_tcl_lambda_UnregisterName(const char *name) {

    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_lambda_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_lambda_NameToInternal_HT, (char *) name);
    if (entryPtr != nullptr) {
        Tcl_DeleteHashEntry(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_lambda_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> UnregisterName: name=%s entryPtr=%p\n", name, entryPtr));

    return entryPtr != nullptr;
}

static struct Aws::Lambda::LambdaClient *
aws_sdk_tcl_lambda_GetInternalFromName(const char *name) {
    Aws::Lambda::LambdaClient *internal = nullptr;
    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_lambda_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_lambda_NameToInternal_HT, (char *) name);
    if (entryPtr != nullptr) {
        internal = (Aws::Lambda::LambdaClient *) Tcl_GetHashValue(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_lambda_NameToInternal_HT_Mutex);

    return internal;
}

int aws_sdk_tcl_lambda_Destroy(Tcl_Interp *interp, const char *handle) {
    Aws::Lambda::LambdaClient *client = aws_sdk_tcl_lambda_GetInternalFromName(handle);
    if (!aws_sdk_tcl_lambda_UnregisterName(handle)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    delete client;
    Tcl_DeleteCommand(interp, handle);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

int aws_sdk_tcl_lambda_ListFunctions(Tcl_Interp *interp, const char *handle) {
    DBG(fprintf(stderr, "aws_sdk_tcl_lambda_ListFunctions: handle=%s\n", handle));
    Aws::Lambda::LambdaClient *client = aws_sdk_tcl_lambda_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Tcl_Obj *resultObj = Tcl_NewListObj(0, nullptr);
    Aws::String marker;

    do {
        Aws::Lambda::Model::ListFunctionsRequest request;
        if (!marker.empty()) {
            request.SetMarker(marker);
        }

        Aws::Lambda::Model::ListFunctionsOutcome outcome = client->ListFunctions(
                request);

        if (outcome.IsSuccess()) {
            const Aws::Lambda::Model::ListFunctionsResult &result = outcome.GetResult();
//            std::cout << result.GetFunctions().size() << " lambda functions were retrieved." << std::endl;

            for (const Aws::Lambda::Model::FunctionConfiguration &functionConfiguration: result.GetFunctions()) {
                Tcl_Obj *dictPtr = Tcl_NewDictObj();
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("function_name", -1),
                               Tcl_NewStringObj(functionConfiguration.GetFunctionName().c_str(), -1));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("function_arn", -1),
                               Tcl_NewStringObj(functionConfiguration.GetFunctionArn().c_str(), -1));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("description", -1),
                                 Tcl_NewStringObj(functionConfiguration.GetDescription().c_str(), -1));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("runtime", -1),
                                 Tcl_NewStringObj(Aws::Lambda::Model::RuntimeMapper::GetNameForRuntime(functionConfiguration.GetRuntime()).c_str(), -1));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("role", -1),
                                    Tcl_NewStringObj(functionConfiguration.GetRole().c_str(), -1));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("handler", -1),
                                    Tcl_NewStringObj(functionConfiguration.GetHandler().c_str(), -1));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("code_size", -1),
                                    Tcl_NewLongObj(functionConfiguration.GetCodeSize()));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("timeout", -1),
                                    Tcl_NewIntObj(functionConfiguration.GetTimeout()));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("memory_size", -1),
                                    Tcl_NewIntObj(functionConfiguration.GetMemorySize()));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("last_modified", -1),
                                    Tcl_NewStringObj(functionConfiguration.GetLastModified().c_str(), -1));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("code_sha_256", -1),
                               Tcl_NewStringObj(functionConfiguration.GetCodeSha256().c_str(), -1));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("kms_key_arn", -1),
                               Tcl_NewStringObj(functionConfiguration.GetKMSKeyArn().c_str(), -1));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("package_type", -1),
                               Tcl_NewStringObj(Aws::Lambda::Model::PackageTypeMapper::GetNameForPackageType(functionConfiguration.GetPackageType()).c_str(), -1));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("version", -1),
                               Tcl_NewStringObj(functionConfiguration.GetVersion().c_str(), -1));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("revision_id", -1),
                               Tcl_NewStringObj(functionConfiguration.GetRevisionId().c_str(), -1));

                // vpc config
                Tcl_Obj *vpcConfigDictPtr = Tcl_NewDictObj();
                Tcl_DictObjPut(interp, vpcConfigDictPtr, Tcl_NewStringObj("vpc_id", -1),
                               Tcl_NewStringObj(functionConfiguration.GetVpcConfig().GetVpcId().c_str(), -1));
                Tcl_Obj *subnetsListObj = Tcl_NewListObj(0, nullptr);
                for (const Aws::String &subnetId: functionConfiguration.GetVpcConfig().GetSubnetIds()) {
                    Tcl_ListObjAppendElement(interp, subnetsListObj, Tcl_NewStringObj(subnetId.c_str(), -1));
                }
                Tcl_DictObjPut(interp, vpcConfigDictPtr, Tcl_NewStringObj("subnet_ids", -1), subnetsListObj);
                Tcl_Obj *securityGroupsListObj = Tcl_NewListObj(0, nullptr);
                for (const Aws::String &securityGroupId: functionConfiguration.GetVpcConfig().GetSecurityGroupIds()) {
                    Tcl_ListObjAppendElement(interp, securityGroupsListObj, Tcl_NewStringObj(securityGroupId.c_str(), -1));
                }
                Tcl_DictObjPut(interp, vpcConfigDictPtr, Tcl_NewStringObj("security_group_ids", -1), securityGroupsListObj);
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("vpc_config", -1), vpcConfigDictPtr);

                // environment
                Tcl_Obj *environmentDictPtr = Tcl_NewDictObj();
                Tcl_Obj *variablesDictPtr = Tcl_NewDictObj();
                for (const auto &variable: functionConfiguration.GetEnvironment().GetVariables()) {
                    Tcl_DictObjPut(interp, variablesDictPtr, Tcl_NewStringObj(variable.first.c_str(), -1),
                                   Tcl_NewStringObj(variable.second.c_str(), -1));
                }
                Tcl_DictObjPut(interp, environmentDictPtr, Tcl_NewStringObj("variables", -1), variablesDictPtr);
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("environment", -1), environmentDictPtr);

                // tracing config
                Tcl_Obj *tracingConfigDictPtr = Tcl_NewDictObj();
                Tcl_DictObjPut(interp, tracingConfigDictPtr, Tcl_NewStringObj("mode", -1),
                               Tcl_NewStringObj(Aws::Lambda::Model::TracingModeMapper::GetNameForTracingMode(functionConfiguration.GetTracingConfig().GetMode()).c_str(), -1));
                Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("tracing_config", -1), tracingConfigDictPtr);

                // architectures
                Tcl_Obj *architecturesListObj = Tcl_NewListObj(0, nullptr);
                for (const auto &architecture: functionConfiguration.GetArchitectures()) {
                    Tcl_ListObjAppendElement(interp, architecturesListObj, Tcl_NewStringObj(Aws::Lambda::Model::ArchitectureMapper::GetNameForArchitecture(architecture).c_str(), -1));
                }

                // ephemeral storage
                Tcl_Obj *ephemeralStorageDictPtr = Tcl_NewDictObj();
                Tcl_DictObjPut(interp, ephemeralStorageDictPtr, Tcl_NewStringObj("size", -1),
                               Tcl_NewIntObj(functionConfiguration.GetEphemeralStorage().GetSize()));

                Tcl_ListObjAppendElement(interp, resultObj,dictPtr);
            }
            marker = result.GetNextMarker();
        }
        else {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
            return TCL_ERROR;
        }
    } while (!marker.empty());

    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

int aws_sdk_tcl_lambda_ClientObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    static const char *clientMethods[] = {
            "destroy",
            "list_functions",
            nullptr
    };

    enum clientMethod {
        m_destroy,
        m_listFunctions,
    };

    if (objc < 2) {
        Tcl_ResetResult(interp);
        Tcl_SetStringObj(Tcl_GetObjResult(interp), (lambda_client_usage), -1);
        return TCL_ERROR;
    }
    Tcl_ResetResult(interp);

    int methodIndex;
    if (TCL_OK == Tcl_GetIndexFromObj(interp, objv[1], clientMethods, "method", 0, &methodIndex)) {
        Tcl_ResetResult(interp);
        const char *handle = Tcl_GetString(objv[0]);
        switch ((enum clientMethod) methodIndex) {
            case m_destroy:
                return aws_sdk_tcl_lambda_Destroy(interp, handle);
            case m_listFunctions:
                CheckArgs(2, 2, 1, "list_functions");
                return aws_sdk_tcl_lambda_ListFunctions(
                        interp,
                        handle
                );
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj("Unknown method", -1));
    return TCL_ERROR;
}

static int aws_sdk_tcl_lambda_CreateCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
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

    auto *client = new Aws::Lambda::LambdaClient(clientConfig);
    char handle[80];
    CMD_NAME(handle, client);
    aws_sdk_tcl_lambda_RegisterName(handle, client);

    Tcl_CreateObjCommand(interp, handle,
                         (Tcl_ObjCmdProc *) aws_sdk_tcl_lambda_ClientObjCmd,
                         nullptr,
                         nullptr);
//                                 (Tcl_CmdDeleteProc*) aws_sdk_tcl_lambda_clientObjCmdDeleteProc);

    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

static int aws_sdk_tcl_lambda_DestroyCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "DestroyCmd\n"));
    CheckArgs(2, 2, 1, "handle");
    return aws_sdk_tcl_lambda_Destroy(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_lambda_ListFunctionsCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "ListFunctionsCmd\n"));
    CheckArgs(4, 4, 1, "handle_name");
    return aws_sdk_tcl_lambda_ListFunctions(interp, Tcl_GetString(objv[1]));
}

static void aws_sdk_tcl_lambda_ExitHandler(ClientData unused) {
    Tcl_MutexLock(&aws_sdk_tcl_lambda_NameToInternal_HT_Mutex);
    Tcl_DeleteHashTable(&aws_sdk_tcl_lambda_NameToInternal_HT);
    Tcl_MutexUnlock(&aws_sdk_tcl_lambda_NameToInternal_HT_Mutex);

}


void aws_sdk_tcl_lambda_InitModule() {
    Tcl_MutexLock(&aws_sdk_tcl_lambda_NameToInternal_HT_Mutex);
    if (!aws_sdk_tcl_lambda_ModuleInitialized) {
        Aws::SDKOptions options;
        Aws::InitAPI(options);
        Tcl_InitHashTable(&aws_sdk_tcl_lambda_NameToInternal_HT, TCL_STRING_KEYS);
        Tcl_CreateThreadExitHandler(aws_sdk_tcl_lambda_ExitHandler, nullptr);
        aws_sdk_tcl_lambda_ModuleInitialized = 1;
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_lambda_NameToInternal_HT_Mutex);
}

int Aws_sdk_tcl_lambda_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.6", 0) == nullptr) {
        return TCL_ERROR;
    }

    aws_sdk_tcl_lambda_InitModule();

    Tcl_CreateNamespace(interp, "::aws::lambda", nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::lambda::create", aws_sdk_tcl_lambda_CreateCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::lambda::destroy", aws_sdk_tcl_lambda_DestroyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::lambda::list_functions", aws_sdk_tcl_lambda_ListFunctionsCmd, nullptr, nullptr);

    return Tcl_PkgProvide(interp, "awslambda", "0.1");
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Aws_sdk_tcl_lambda_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
