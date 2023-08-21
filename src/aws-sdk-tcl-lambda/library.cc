/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#include <iostream>
#include <aws/core/Aws.h>
#include <aws/lambda/LambdaClient.h>
#include <cstdio>
#include <fstream>
#include <aws/lambda/model/ListFunctionsRequest.h>
#include <aws/lambda/model/GetFunctionRequest.h>
#include <aws/lambda/model/CreateFunctionRequest.h>
#include <aws/lambda/model/DeleteFunctionRequest.h>
#include <aws/lambda/model/InvokeRequest.h>
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
        "Usage lambdaClient <method> <args>, where method can be:\n"
        "   list_functions\n"
        "   get_function function_name\n"
        "   create_function function_name function_code_path handler runtime execution_role_arn ?timeout?\n"
        "   invoke_function function_name payload_json ?invocation_type?\n"
        "   delete_function function_name\n"
        "   destroy\n"
        ;


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

Tcl_Obj *get_dict_from_function_configuration(Tcl_Interp *interp, const Aws::Lambda::Model::FunctionConfiguration &functionConfiguration) {
    Tcl_Obj *dictPtr = Tcl_NewDictObj();
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("FunctionName", -1),
                   Tcl_NewStringObj(functionConfiguration.GetFunctionName().c_str(), -1));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("FunctionArn", -1),
                   Tcl_NewStringObj(functionConfiguration.GetFunctionArn().c_str(), -1));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("Description", -1),
                   Tcl_NewStringObj(functionConfiguration.GetDescription().c_str(), -1));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("Runtime", -1),
                   Tcl_NewStringObj(Aws::Lambda::Model::RuntimeMapper::GetNameForRuntime(functionConfiguration.GetRuntime()).c_str(), -1));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("Role", -1),
                   Tcl_NewStringObj(functionConfiguration.GetRole().c_str(), -1));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("Handler", -1),
                   Tcl_NewStringObj(functionConfiguration.GetHandler().c_str(), -1));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("CodeSize", -1),
                   Tcl_NewLongObj(functionConfiguration.GetCodeSize()));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("Timeout", -1),
                   Tcl_NewIntObj(functionConfiguration.GetTimeout()));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("MemorySize", -1),
                   Tcl_NewIntObj(functionConfiguration.GetMemorySize()));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("LastModified", -1),
                   Tcl_NewStringObj(functionConfiguration.GetLastModified().c_str(), -1));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("CodeSha256", -1),
                   Tcl_NewStringObj(functionConfiguration.GetCodeSha256().c_str(), -1));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("KmsKeyArn", -1),
                   Tcl_NewStringObj(functionConfiguration.GetKMSKeyArn().c_str(), -1));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("PackageType", -1),
                   Tcl_NewStringObj(Aws::Lambda::Model::PackageTypeMapper::GetNameForPackageType(functionConfiguration.GetPackageType()).c_str(), -1));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("Version", -1),
                   Tcl_NewStringObj(functionConfiguration.GetVersion().c_str(), -1));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("RevisionId", -1),
                   Tcl_NewStringObj(functionConfiguration.GetRevisionId().c_str(), -1));

    // vpc config
    Tcl_Obj *vpcConfigDictPtr = Tcl_NewDictObj();
    Tcl_DictObjPut(interp, vpcConfigDictPtr, Tcl_NewStringObj("VpcId", -1),
                   Tcl_NewStringObj(functionConfiguration.GetVpcConfig().GetVpcId().c_str(), -1));
    Tcl_Obj *subnetsListObj = Tcl_NewListObj(0, nullptr);
    for (const Aws::String &subnetId: functionConfiguration.GetVpcConfig().GetSubnetIds()) {
        Tcl_ListObjAppendElement(interp, subnetsListObj, Tcl_NewStringObj(subnetId.c_str(), -1));
    }
    Tcl_DictObjPut(interp, vpcConfigDictPtr, Tcl_NewStringObj("SubnetIds", -1), subnetsListObj);
    Tcl_Obj *securityGroupsListObj = Tcl_NewListObj(0, nullptr);
    for (const Aws::String &securityGroupId: functionConfiguration.GetVpcConfig().GetSecurityGroupIds()) {
        Tcl_ListObjAppendElement(interp, securityGroupsListObj, Tcl_NewStringObj(securityGroupId.c_str(), -1));
    }
    Tcl_DictObjPut(interp, vpcConfigDictPtr, Tcl_NewStringObj("SecurityGroupIds", -1), securityGroupsListObj);
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("VpcConfig", -1), vpcConfigDictPtr);

    // environment
    Tcl_Obj *environmentDictPtr = Tcl_NewDictObj();
    Tcl_Obj *variablesDictPtr = Tcl_NewDictObj();
    for (const auto &variable: functionConfiguration.GetEnvironment().GetVariables()) {
        Tcl_DictObjPut(interp, variablesDictPtr, Tcl_NewStringObj(variable.first.c_str(), -1),
                       Tcl_NewStringObj(variable.second.c_str(), -1));
    }
    Tcl_DictObjPut(interp, environmentDictPtr, Tcl_NewStringObj("Variables", -1), variablesDictPtr);
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("Environment", -1), environmentDictPtr);

    // tracing config
    Tcl_Obj *tracingConfigDictPtr = Tcl_NewDictObj();
    Tcl_DictObjPut(interp, tracingConfigDictPtr, Tcl_NewStringObj("Mode", -1),
                   Tcl_NewStringObj(Aws::Lambda::Model::TracingModeMapper::GetNameForTracingMode(functionConfiguration.GetTracingConfig().GetMode()).c_str(), -1));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("TracingConfig", -1), tracingConfigDictPtr);

    // architectures
    Tcl_Obj *architecturesListObj = Tcl_NewListObj(0, nullptr);
    for (const auto &architecture: functionConfiguration.GetArchitectures()) {
        Tcl_ListObjAppendElement(interp, architecturesListObj, Tcl_NewStringObj(Aws::Lambda::Model::ArchitectureMapper::GetNameForArchitecture(architecture).c_str(), -1));
    }
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("Architectures", -1), architecturesListObj);

    // ephemeral storage
    Tcl_Obj *ephemeralStorageDictPtr = Tcl_NewDictObj();
    Tcl_DictObjPut(interp, ephemeralStorageDictPtr, Tcl_NewStringObj("Size", -1),
                   Tcl_NewIntObj(functionConfiguration.GetEphemeralStorage().GetSize()));
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("EphemeralStorage", -1), ephemeralStorageDictPtr);

    // file system configs
    Tcl_Obj *fileSystemConfigsListObj = Tcl_NewListObj(0, nullptr);
    for (const auto &fileSystemConfig: functionConfiguration.GetFileSystemConfigs()) {
        Tcl_Obj *fileSystemConfigDictPtr = Tcl_NewDictObj();
        Tcl_DictObjPut(interp, fileSystemConfigDictPtr, Tcl_NewStringObj("Arn", -1),
                       Tcl_NewStringObj(fileSystemConfig.GetArn().c_str(), -1));
        Tcl_DictObjPut(interp, fileSystemConfigDictPtr, Tcl_NewStringObj("LocalMountPath", -1),
                       Tcl_NewStringObj(fileSystemConfig.GetLocalMountPath().c_str(), -1));
        Tcl_ListObjAppendElement(interp, fileSystemConfigsListObj, fileSystemConfigDictPtr);
    }
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("FileSystemConfigs", -1), fileSystemConfigsListObj);

    // layers
    Tcl_Obj *layersListObj = Tcl_NewListObj(0, nullptr);
    for (const auto &layer: functionConfiguration.GetLayers()) {
        Tcl_Obj *layerDictPtr = Tcl_NewDictObj();
        Tcl_DictObjPut(interp, layerDictPtr, Tcl_NewStringObj("Arn", -1),
                       Tcl_NewStringObj(layer.GetArn().c_str(), -1));
        Tcl_DictObjPut(interp, layerDictPtr, Tcl_NewStringObj("CodeSize", -1),
                       Tcl_NewLongObj(layer.GetCodeSize()));
        Tcl_DictObjPut(interp, layerDictPtr, Tcl_NewStringObj("SigningProfileVersionArn", -1),
                       Tcl_NewStringObj(layer.GetSigningProfileVersionArn().c_str(), -1));
        Tcl_DictObjPut(interp, layerDictPtr, Tcl_NewStringObj("SigningJobArn", -1),
                       Tcl_NewStringObj(layer.GetSigningJobArn().c_str(), -1));
        Tcl_ListObjAppendElement(interp, layersListObj, layerDictPtr);
    }
    Tcl_DictObjPut(interp, dictPtr, Tcl_NewStringObj("Layers", -1), layersListObj);

    return dictPtr;
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
                Tcl_ListObjAppendElement(interp, resultObj,get_dict_from_function_configuration(interp, functionConfiguration));
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

int aws_sdk_tcl_lambda_GetFunction(Tcl_Interp *interp, const char *handle, const char *function_name) {
    DBG(fprintf(stderr, "aws_sdk_tcl_lambda_GetFunction: handle=%s function_name=%s\n", handle, function_name));
    Aws::Lambda::LambdaClient *client = aws_sdk_tcl_lambda_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::Lambda::Model::GetFunctionRequest request;
    request.SetFunctionName(function_name);
    Aws::Lambda::Model::GetFunctionOutcome outcome = client->GetFunction(request);
    if (outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, get_dict_from_function_configuration(interp, outcome.GetResult().GetConfiguration()));
        return TCL_OK;
    }
    else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_lambda_CreateFunction(
        Tcl_Interp *interp,
        const char *handle,
        const char *function_name,
        const char *function_code_path,
        const char *handler,
        const char *runtime,
        const char *execution_role_arn,
        Tcl_Obj *timeoutPtr
        ) {
    DBG(fprintf(stderr, "aws_sdk_tcl_lambda_CreateFunction: handle=%s function_name=%s\n", handle, function_name));
    Aws::Lambda::LambdaClient *client = aws_sdk_tcl_lambda_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::Lambda::Model::CreateFunctionRequest request;
    request.SetFunctionName(function_name);
    request.SetHandler(handler);
    request.SetRuntime(Aws::Lambda::Model::RuntimeMapper::GetRuntimeForName(runtime));
    request.SetRole(execution_role_arn);

    if (timeoutPtr) {
        int timeout;
        Tcl_GetIntFromObj(interp, timeoutPtr, &timeout);
        request.SetTimeout(timeout);
    }

    Aws::Lambda::Model::FunctionCode code;
    std::ifstream ifstream(function_code_path,std::ios_base::in | std::ios_base::binary);
    if (!ifstream.is_open()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("Error opening code file", -1));
        return TCL_ERROR;
    }

    Aws::StringStream buffer;
    buffer << ifstream.rdbuf();

    code.SetZipFile(Aws::Utils::ByteBuffer((unsigned char *) buffer.str().c_str(),
                                           buffer.str().length()));
    request.SetCode(code);

    Aws::Lambda::Model::CreateFunctionOutcome outcome = client->CreateFunction(request);
    if (outcome.IsSuccess()) {
        return TCL_OK;
    }
    else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_lambda_DeleteFunction(
        Tcl_Interp *interp,
        const char *handle,
        const char *function_name
) {
    DBG(fprintf(stderr, "aws_sdk_tcl_lambda_DeleteFunction: handle=%s function_name=%s\n", handle, function_name));
    Aws::Lambda::LambdaClient *client = aws_sdk_tcl_lambda_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::Lambda::Model::DeleteFunctionRequest request;
    request.SetFunctionName(function_name);
    Aws::Lambda::Model::DeleteFunctionOutcome outcome = client->DeleteFunction(request);

    if (outcome.IsSuccess()) {
        return TCL_OK;
    }
    else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_lambda_InvokeFunction(
        Tcl_Interp *interp,
        const char *handle,
        const char *function_name,
        const char *payload_json,
        const char *invocation_type
) {
    DBG(fprintf(stderr, "aws_sdk_tcl_lambda_DeleteFunction: handle=%s function_name=%s\n", handle, function_name));
    Aws::Lambda::LambdaClient *client = aws_sdk_tcl_lambda_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    std::shared_ptr<Aws::IOStream> payload = Aws::MakeShared<Aws::StringStream>("FunctionTest");
    *payload << payload_json;

    Aws::Lambda::Model::InvokeRequest request;
    request.SetFunctionName(function_name);
    if (invocation_type) {
        request.SetInvocationType(Aws::Lambda::Model::InvocationTypeMapper::GetInvocationTypeForName(invocation_type));
    }
    request.SetContentType("application/json");
    request.SetBody(payload);
    Aws::Lambda::Model::InvokeOutcome outcome = client->Invoke(request);

    if (outcome.IsSuccess()) {
        std::ostringstream ss;
        ss << outcome.GetResult().GetPayload().rdbuf();
        Tcl_SetObjResult(interp, Tcl_NewStringObj(ss.str().c_str(), -1));
        return TCL_OK;
    }
    else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_lambda_ClientObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    static const char *clientMethods[] = {
            "destroy",
            "list_functions",
            "get_function",
            "create_function",
            "delete_function",
            "invoke_function",
            nullptr
    };

    enum clientMethod {
        m_destroy,
        m_listFunctions,
        m_getFunction,
        m_createFunction,
        m_deleteFunction,
        m_invokeFunction
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
            case m_getFunction:
                CheckArgs(3, 3, 1, "get_function function_name");
                return aws_sdk_tcl_lambda_GetFunction(
                        interp,
                        handle,
                        Tcl_GetString(objv[2])
                );
            case m_createFunction:
                CheckArgs(7, 8, 1, "create_function function_name function_code_path handler runtime execution_role_arn ?timeout?");
                return aws_sdk_tcl_lambda_CreateFunction(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3]),
                        Tcl_GetString(objv[4]),
                        Tcl_GetString(objv[5]),
                        Tcl_GetString(objv[6]),
                        objc == 8 ? objv[7] : nullptr
                );
            case m_deleteFunction:
                CheckArgs(3, 3, 1, "delete_function function_name");
                return aws_sdk_tcl_lambda_DeleteFunction(
                        interp,
                        handle,
                        Tcl_GetString(objv[2])
                );
            case m_invokeFunction:
                CheckArgs(4, 5, 1, "invoke_function function_name payload_json ?invocation_type?");
                return aws_sdk_tcl_lambda_InvokeFunction(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3]),
                        objc == 5 ? Tcl_GetString(objv[4]) : nullptr
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
    CheckArgs(2, 2, 1, "handle_name");
    return aws_sdk_tcl_lambda_ListFunctions(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_lambda_GetFunctionCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "GetFunctionCmd\n"));
    CheckArgs(3, 3, 1, "handle_name function_name");
    return aws_sdk_tcl_lambda_GetFunction(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]));
}

static int aws_sdk_tcl_lambda_CreateFunctionCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CreateFunctionCmd\n"));
    CheckArgs(7, 8, 1, "handle_name function_name function_code_path function_handler runtime execution_role_arn ?timeout?");
    return aws_sdk_tcl_lambda_CreateFunction(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            Tcl_GetString(objv[3]),
            Tcl_GetString(objv[4]),
            Tcl_GetString(objv[5]),
            Tcl_GetString(objv[6]),
            objc == 8 ? objv[7] : nullptr
            );
}

static int aws_sdk_tcl_lambda_DeleteFunctionCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "DeleteFunctionCmd\n"));
    CheckArgs(3, 3, 1, "handle_name function_name");
    return aws_sdk_tcl_lambda_DeleteFunction(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2])
    );
}

static int aws_sdk_tcl_lambda_InvokeFunctionCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "InvokeFunctionCmd\n"));
    CheckArgs(4, 5, 1, "handle_name function_name payload_json ?invocation_type?");
    return aws_sdk_tcl_lambda_InvokeFunction(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            Tcl_GetString(objv[3]),
            objc == 5 ? Tcl_GetString(objv[4]) : nullptr
    );
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
    Tcl_CreateObjCommand(interp, "::aws::lambda::get_function", aws_sdk_tcl_lambda_GetFunctionCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::lambda::create_function", aws_sdk_tcl_lambda_CreateFunctionCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::lambda::delete_function", aws_sdk_tcl_lambda_DeleteFunctionCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::lambda::invoke_function", aws_sdk_tcl_lambda_InvokeFunctionCmd, nullptr, nullptr);

    return Tcl_PkgProvide(interp, "awslambda", "0.1");
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Aws_sdk_tcl_lambda_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
