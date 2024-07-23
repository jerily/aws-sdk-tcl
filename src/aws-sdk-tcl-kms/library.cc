/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include <aws/kms/KMSClient.h>
#include <aws/kms/model/ListKeysRequest.h>
#include <aws/kms/model/CreateKeyRequest.h>
#include <aws/kms/model/DescribeKeyRequest.h>
#include <aws/kms/model/EnableKeyRequest.h>
#include <aws/kms/model/DisableKeyRequest.h>
#include <aws/kms/model/ScheduleKeyDeletionRequest.h>
#include <aws/kms/model/CancelKeyDeletionRequest.h>
#include <aws/kms/model/EncryptRequest.h>
#include <aws/kms/model/DecryptRequest.h>
#include <aws/kms/model/GenerateDataKeyRequest.h>
#include <aws/kms/model/GenerateRandomRequest.h>

#include "library.h"
#include "../common/common.h"

#ifndef TCL_SIZE_MAX
typedef int Tcl_Size;
# define Tcl_GetSizeIntFromObj Tcl_GetIntFromObj
# define Tcl_NewSizeIntObj Tcl_NewIntObj
# define TCL_SIZE_MAX      INT_MAX
# define TCL_SIZE_MODIFIER ""
#endif

#ifndef Tcl_BounceRefCount
#define Tcl_BounceRefCount(x) Tcl_IncrRefCount((x));Tcl_DecrRefCount((x))
#endif

#define XSTR(s) STR(s)
#define STR(s) #s

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

#define CMD_NAME(s,internal) std::sprintf((s), "_AWS_KMS_%p", (internal))

static char VAR_READ_ONLY_MSG[] = "var is read-only";

typedef struct {
    Tcl_Interp *interp;
    char *handle;
    char *varname;
    Aws::KMS::KMSClient *item;
} aws_sdk_tcl_kms_trace_t;

static Tcl_HashTable aws_sdk_tcl_kms_NameToInternal_HT;
static Tcl_Mutex     aws_sdk_tcl_kms_NameToInternal_HT_Mutex;
static int           aws_sdk_tcl_kms_ModuleInitialized;

static char kms_client_usage[] =
    "Usage kmsClient <method> <args>, where method can be:\n"
    "   list_keys                                          \n"
    "   create_key                                         \n"
    "   describe_key arn                                   \n"
    "   enable_key arn                                     \n"
    "   disable_key arn                                    \n"
    "   schedule_key_deletion arn ?pending_window_in_days? \n"
    "   cancel_key_deletion arn                            \n"
    "   encrypt arn plain_data                             \n"
    "   decrypt cipher_data                                \n"
    "   generate_data_key arn number_of_bytes              \n"
    "   generate_random number_of_bytes                    \n"
    "   destroy                                            \n"
;

static int aws_sdk_tcl_kms_RegisterName(const char *name, Aws::KMS::KMSClient *internal) {

    Tcl_HashEntry *entryPtr;
    int newEntry;

    Tcl_MutexLock(&aws_sdk_tcl_kms_NameToInternal_HT_Mutex);
    entryPtr = Tcl_CreateHashEntry(&aws_sdk_tcl_kms_NameToInternal_HT, (char*) name, &newEntry);
    if (newEntry) {
        Tcl_SetHashValue(entryPtr, (ClientData)internal);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_kms_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> RegisterName: name=%s internal=%p %s\n", name, internal, newEntry ? "entered into" : "already in"));

    return !!newEntry;
}

static int aws_sdk_tcl_kms_UnregisterName(const char *name) {

    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_kms_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_kms_NameToInternal_HT, (char*)name);
    if (entryPtr != nullptr) {
        Tcl_DeleteHashEntry(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_kms_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> UnregisterName: name=%s entryPtr=%p\n", name, entryPtr));

    return entryPtr != nullptr;
}

static struct Aws::KMS::KMSClient *aws_sdk_tcl_kms_GetInternalFromName(const char *name) {
    Aws::KMS::KMSClient *internal = nullptr;
    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_kms_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_kms_NameToInternal_HT, (char*)name);
    if (entryPtr != nullptr) {
        internal = (Aws::KMS::KMSClient *)Tcl_GetHashValue(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_kms_NameToInternal_HT_Mutex);

    return internal;
}

int aws_sdk_tcl_kms_Destroy(Tcl_Interp *interp, const char *handle) {
    Aws::KMS::KMSClient *client = aws_sdk_tcl_kms_GetInternalFromName(handle);
    if (!aws_sdk_tcl_kms_UnregisterName(handle)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    Aws::KMS::KMSClient::ShutdownSdkClient(client, -1);
    delete client;
    Tcl_DeleteCommand(interp, handle);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

int aws_sdk_tcl_kms_ListKeys(Tcl_Interp *interp, const char *handle) {
    DBG(fprintf(stderr, "aws_sdk_tcl_kms_ListKeys: handle=%s\n", handle));
    Aws::KMS::KMSClient *client = aws_sdk_tcl_kms_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Tcl_Obj *rc = Tcl_NewListObj(0, NULL);

    Aws::KMS::Model::ListKeysRequest request;
    request.SetLimit(1000);
    bool want_more = true;
    while (want_more) {

        Aws::KMS::Model::ListKeysOutcome outcome = client->ListKeys(request);

        if (!outcome.IsSuccess()) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
            Tcl_BounceRefCount(rc);
            return TCL_ERROR;
        }

        for (size_t i = 0; i < outcome.GetResult().GetKeys().size(); i++) {
            const char *arn = outcome.GetResult().GetKeys().at(i).GetKeyArn().c_str();
            Tcl_ListObjAppendElement(interp, rc, Tcl_NewStringObj(arn, -1));
        }

        want_more = outcome.GetResult().GetTruncated();
        if (want_more) {
            request.SetMarker(outcome.GetResult().GetNextMarker());
        }

    }

    Tcl_SetObjResult(interp, rc);
    return TCL_OK;
}

int aws_sdk_tcl_kms_CreateKey(Tcl_Interp *interp, const char *handle) {
    DBG(fprintf(stderr, "aws_sdk_tcl_kms_CreateKey: handle=%s\n", handle));
    Aws::KMS::KMSClient *client = aws_sdk_tcl_kms_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::KMS::Model::CreateKeyRequest request;

    Aws::KMS::Model::CreateKeyOutcome outcome = client->CreateKey(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }

    const char *arn = outcome.GetResult().GetKeyMetadata().GetArn().c_str();
    Tcl_SetObjResult(interp, Tcl_NewStringObj(arn, -1));
    return TCL_OK;
}

Tcl_Obj *aws_sdk_tcl_json2dict(const Aws::Utils::Json::JsonView json) {

    if (json.IsObject()) {

        Tcl_Obj *rc = Tcl_NewDictObj();
        Aws::Map<Aws::String, Aws::Utils::Json::JsonView> map = json.GetAllObjects();
        for (const auto &entry: map) {
            Tcl_DictObjPut(NULL, rc, Tcl_NewStringObj(entry.first.c_str(), -1),
                aws_sdk_tcl_json2dict(entry.second));
        }
        return rc;

    } else if (json.IsBool()) {

        return Tcl_NewBooleanObj(json.AsBool() ? 1 : 0);

    } else if (json.IsIntegerType()) {

        return Tcl_NewWideIntObj(json.AsInt64());

    } else if (json.IsFloatingPointType()) {

        return Tcl_NewDoubleObj(json.AsDouble());

    } else if (json.IsNull()) {

        return Tcl_NewStringObj("", 0);

    } else if (json.IsListType()) {

        Tcl_Obj *rc = Tcl_NewListObj(0, NULL);
        Aws::Utils::Array<Aws::Utils::Json::JsonView> entries = json.AsArray();
        for(unsigned i = 0; i < entries.GetLength(); i++) {
            Tcl_ListObjAppendElement(NULL, rc, aws_sdk_tcl_json2dict(entries[i]));
        }
        return rc;

    } else if (json.IsString()) {

        return Tcl_NewStringObj(json.AsString().c_str(), -1);

    }

    return Tcl_NewStringObj("unknown", -1);

}

int aws_sdk_tcl_kms_DescribeKey(Tcl_Interp *interp, const char *handle, const char *arn) {
    DBG(fprintf(stderr, "aws_sdk_tcl_kms_DescribeKey: handle=%s arn=%s\n", handle, arn));
    Aws::KMS::KMSClient *client = aws_sdk_tcl_kms_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::KMS::Model::DescribeKeyRequest request;
    request.SetKeyId(arn);

    Aws::KMS::Model::DescribeKeyOutcome outcome = client->DescribeKey(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }

    Aws::Utils::Json::JsonValue json = outcome.GetResult().GetKeyMetadata().Jsonize();

    Tcl_SetObjResult(interp, aws_sdk_tcl_json2dict(json.View()));
    return TCL_OK;
}

int aws_sdk_tcl_kms_EnableKey(Tcl_Interp *interp, const char *handle, const char *arn) {
    DBG(fprintf(stderr, "aws_sdk_tcl_kms_EnableKey: handle=%s arn=%s\n", handle, arn));
    Aws::KMS::KMSClient *client = aws_sdk_tcl_kms_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::KMS::Model::EnableKeyRequest request;
    request.SetKeyId(arn);

    Aws::KMS::Model::EnableKeyOutcome outcome = client->EnableKey(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(arn, -1));
    return TCL_OK;
}

int aws_sdk_tcl_kms_DisableKey(Tcl_Interp *interp, const char *handle, const char *arn) {
    DBG(fprintf(stderr, "aws_sdk_tcl_kms_DisableKey: handle=%s arn=%s\n", handle, arn));
    Aws::KMS::KMSClient *client = aws_sdk_tcl_kms_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::KMS::Model::DisableKeyRequest request;
    request.SetKeyId(arn);

    Aws::KMS::Model::DisableKeyOutcome outcome = client->DisableKey(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(arn, -1));
    return TCL_OK;
}

int aws_sdk_tcl_kms_ScheduleKeyDeletion(Tcl_Interp *interp, const char *handle, const char *arn, int pending_window) {
    DBG(fprintf(stderr, "aws_sdk_tcl_kms_ScheduleKeyDeletion: handle=%s arn=%s pending_window=%d\n", handle, arn, pending_window));
    Aws::KMS::KMSClient *client = aws_sdk_tcl_kms_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::KMS::Model::ScheduleKeyDeletionRequest request;
    request.SetKeyId(arn);
    if (pending_window != -1) {
        request.SetPendingWindowInDays(pending_window);
    }

    Aws::KMS::Model::ScheduleKeyDeletionOutcome outcome = client->ScheduleKeyDeletion(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(outcome.GetResult().GetDeletionDate().Seconds()));
    return TCL_OK;
}

int aws_sdk_tcl_kms_CancelKeyDeletion(Tcl_Interp *interp, const char *handle, const char *arn) {
    DBG(fprintf(stderr, "aws_sdk_tcl_kms_CancelKeyDeletion: handle=%s arn=%s\n", handle, arn));
    Aws::KMS::KMSClient *client = aws_sdk_tcl_kms_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::KMS::Model::CancelKeyDeletionRequest request;
    request.SetKeyId(arn);

    Aws::KMS::Model::CancelKeyDeletionOutcome outcome = client->CancelKeyDeletion(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(arn, -1));
    return TCL_OK;
}

int aws_sdk_tcl_kms_Encrypt(Tcl_Interp *interp, const char *handle, const char *arn, const unsigned char *buffer, Tcl_Size size) {
    DBG(fprintf(stderr, "aws_sdk_tcl_kms_Encrypt: handle=%s arn=%s buffer=%p size=%" TCL_SIZE_MODIFIER "d\n", handle, arn, (void *)buffer, size));
    Aws::KMS::KMSClient *client = aws_sdk_tcl_kms_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::Utils::ByteBuffer aws_buffer(buffer, size);

    Aws::KMS::Model::EncryptRequest request;
    request.SetKeyId(arn);
    request.SetPlaintext(aws_buffer);

    Aws::KMS::Model::EncryptOutcome outcome = client->Encrypt(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }

    const Aws::Utils::ByteBuffer cipher = outcome.GetResult().GetCiphertextBlob();

    Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(cipher.GetUnderlyingData(), cipher.GetLength()));
    return TCL_OK;
}

int aws_sdk_tcl_kms_Decrypt(Tcl_Interp *interp, const char *handle, const unsigned char *buffer, Tcl_Size size) {
    DBG(fprintf(stderr, "aws_sdk_tcl_kms_Encrypt: handle=%s buffer=%p size=%" TCL_SIZE_MODIFIER "d\n", handle, (void *)buffer, size));
    Aws::KMS::KMSClient *client = aws_sdk_tcl_kms_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::Utils::ByteBuffer aws_buffer(buffer, size);

    Aws::KMS::Model::DecryptRequest request;
    request.SetCiphertextBlob(aws_buffer);

    Aws::KMS::Model::DecryptOutcome outcome = client->Decrypt(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }

    const Aws::Utils::CryptoBuffer plain = outcome.GetResult().GetPlaintext();

    Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(plain.GetUnderlyingData(), plain.GetLength()));
    return TCL_OK;
}

int aws_sdk_tcl_kms_GenerateDataKey(Tcl_Interp *interp, const char *handle, const char *arn, int num_bytes) {
    DBG(fprintf(stderr, "aws_sdk_tcl_kms_GenerateDataKey: handle=%s arn=%s num_bytes=%d\n", handle, arn, num_bytes));
    Aws::KMS::KMSClient *client = aws_sdk_tcl_kms_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::KMS::Model::GenerateDataKeyRequest request;
    request.SetKeyId(arn);
    request.SetNumberOfBytes(num_bytes);

    Aws::KMS::Model::GenerateDataKeyOutcome outcome = client->GenerateDataKey(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }

    Tcl_Obj *objv[2];

    const Aws::Utils::CryptoBuffer plain = outcome.GetResult().GetPlaintext();
    objv[0] = Tcl_NewByteArrayObj(plain.GetUnderlyingData(), plain.GetLength());

    const Aws::Utils::ByteBuffer cipher = outcome.GetResult().GetCiphertextBlob();
    objv[1] = Tcl_NewByteArrayObj(cipher.GetUnderlyingData(), cipher.GetLength());

    Tcl_SetObjResult(interp, Tcl_NewListObj(2, objv));
    return TCL_OK;
}

int aws_sdk_tcl_kms_GenerateRandom(Tcl_Interp *interp, const char *handle, int num_bytes) {
    DBG(fprintf(stderr, "aws_sdk_tcl_kms_GenerateRandom: handle=%s num_bytes=%d\n", handle, num_bytes));
    Aws::KMS::KMSClient *client = aws_sdk_tcl_kms_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::KMS::Model::GenerateRandomRequest request;
    request.SetNumberOfBytes(num_bytes);

    Aws::KMS::Model::GenerateRandomOutcome outcome = client->GenerateRandom(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }

    const Aws::Utils::CryptoBuffer data = outcome.GetResult().GetPlaintext();

    Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(data.GetUnderlyingData(), data.GetLength()));
    return TCL_OK;
}

char *aws_sdk_tcl_kms_VarTraceProc(ClientData clientData, Tcl_Interp *interp, const char *name1, const char *name2, int flags) {
    auto *trace = (aws_sdk_tcl_kms_trace_t *) clientData;
    if (trace->item == nullptr) {
        DBG(fprintf(stderr, "VarTraceProc: client has been deleted\n"));
        if (!Tcl_InterpDeleted(trace->interp)) {
            Tcl_UntraceVar(trace->interp, trace->varname, TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                           (Tcl_VarTraceProc*) aws_sdk_tcl_kms_VarTraceProc,
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
        aws_sdk_tcl_kms_Destroy(trace->interp, trace->handle);
        Tcl_Free((char *) trace->varname);
        Tcl_Free((char *) trace->handle);
        Tcl_Free((char *) trace);
    }
    return nullptr;
}

int aws_sdk_tcl_kms_ClientObjCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {

    static const char *clientMethods[] = {
            "destroy",
            "list_keys",
            "create_key",
            "describe_key",
            "enable_key",
            "disable_key",
            "schedule_key_deletion",
            "cancel_key_deletion",
            "encrypt",
            "decrypt",
            "generate_data_key",
            "generate_random",
            nullptr
    };

    enum clientMethod {
        m_destroy,
        m_listKeys,
        m_createKey,
        m_describeKey,
        m_enableKey,
        m_disableKey,
        m_scheduleKeyDeletion,
        m_cancelKeyDeletion,
        m_encrypt,
        m_decrypt,
        m_generateDataKey,
        m_generateRandom
    };

    Tcl_ResetResult(interp);

    if (objc < 2) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(kms_client_usage, -1));
        return TCL_ERROR;
    }

    int methodIndex;
    if (Tcl_GetIndexFromObj(interp, objv[1], clientMethods, "method", 0, &methodIndex) != TCL_OK) {
        return TCL_ERROR;
    }

    const char *handle = Tcl_GetString(objv[0]);
    switch ((enum clientMethod) methodIndex ) {
        case m_destroy:
            DBG(fprintf(stderr, "DestroyMethod\n"));
            CheckArgs(2,2,1,"destroy");
            return aws_sdk_tcl_kms_Destroy(interp, handle);
        case m_listKeys:
            DBG(fprintf(stderr, "ListKeysMethod\n"));
            CheckArgs(2,2,1,"list_keys");
            return aws_sdk_tcl_kms_ListKeys(interp, handle);
        case m_createKey:
            DBG(fprintf(stderr, "CreateKeyMethod\n"));
            CheckArgs(2,2,1,"create_key");
            return aws_sdk_tcl_kms_CreateKey(interp, handle);
        case m_describeKey:
            DBG(fprintf(stderr, "DescribeKeyMethod\n"));
            CheckArgs(3,3,1,"describe_key arn");
            return aws_sdk_tcl_kms_DescribeKey(interp, handle, Tcl_GetString(objv[2]));
        case m_enableKey:
            DBG(fprintf(stderr, "EnableKeyMethod\n"));
            CheckArgs(3,3,1,"enable_key arn");
            return aws_sdk_tcl_kms_EnableKey(interp, handle, Tcl_GetString(objv[2]));
        case m_disableKey:
            DBG(fprintf(stderr, "DisableKeyMethod\n"));
            CheckArgs(3,3,1,"disable_key arn");
            return aws_sdk_tcl_kms_DisableKey(interp, handle, Tcl_GetString(objv[2]));
        case m_scheduleKeyDeletion: {
            DBG(fprintf(stderr, "ScheduleKeyDeletionMethod\n"));
            CheckArgs(3,4,1,"schedule_key_deletion arn ?pending_window_in_days?");
            int pending_window = -1;
            if (objc > 3 && Tcl_GetIntFromObj(interp, objv[3], &pending_window) != TCL_OK) {
                return TCL_ERROR;
            }
            return aws_sdk_tcl_kms_ScheduleKeyDeletion(interp, handle, Tcl_GetString(objv[2]), pending_window);
        }
        case m_cancelKeyDeletion:
            DBG(fprintf(stderr, "CancelKeyDeletionMethod\n"));
            CheckArgs(3,3,1,"cancel_key_deletion arn");
            return aws_sdk_tcl_kms_CancelKeyDeletion(interp, handle, Tcl_GetString(objv[2]));
        case m_encrypt: {
            DBG(fprintf(stderr, "EncryptMethod\n"));
            CheckArgs(4,4,1,"encrypt arn plain_data");
            Tcl_Size size;
            const unsigned char *buffer = Tcl_GetByteArrayFromObj(objv[3], &size);
            return aws_sdk_tcl_kms_Encrypt(interp, handle, Tcl_GetString(objv[2]), buffer, size);
        }
        case m_decrypt: {
            DBG(fprintf(stderr, "DecryptMethod\n"));
            CheckArgs(3,3,1,"decrypt cipher_data");
            Tcl_Size size;
            const unsigned char *buffer = Tcl_GetByteArrayFromObj(objv[2], &size);
            return aws_sdk_tcl_kms_Decrypt(interp, handle, buffer, size);
        }
        case m_generateDataKey: {
            DBG(fprintf(stderr, "GenerateDataKeyMethod\n"));
            CheckArgs(4,4,1,"generate_data_key arn number_of_bytes");
            int num_bytes;
            if (Tcl_GetIntFromObj(interp, objv[3], &num_bytes) != TCL_OK) {
                return TCL_ERROR;
            }
            return aws_sdk_tcl_kms_GenerateDataKey(interp, handle, Tcl_GetString(objv[2]), num_bytes);
        }
        case m_generateRandom: {
            DBG(fprintf(stderr, "GenerateRandomMethod\n"));
            CheckArgs(3,3,1,"generate_random number_of_bytes");
            int num_bytes;
            if (Tcl_GetIntFromObj(interp, objv[2], &num_bytes) != TCL_OK) {
                return TCL_ERROR;
            }
            return aws_sdk_tcl_kms_GenerateRandom(interp, handle, num_bytes);
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj("Unknown method", -1));
    return TCL_ERROR;
}

static int aws_sdk_tcl_kms_CreateCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "CreateCmd\n"));

    CheckArgs(2,3,1,"config_dict ?varname?");

    auto result = get_client_config_and_credentials_provider(interp, objv[1]);
    int status = std::get<0>(result);
    if (TCL_OK != status) {
        SetResult("Invalid config_dict");
        return TCL_ERROR;
    }
    Aws::Client::ClientConfiguration client_config = std::get<1>(result);
    std::shared_ptr<Aws::Auth::AWSCredentialsProvider> credentials_provider_ptr = std::get<2>(result);

    auto *client = credentials_provider_ptr != nullptr ?
        new Aws::KMS::KMSClient(credentials_provider_ptr,
            Aws::MakeShared<Aws::KMS::KMSEndpointProvider>(Aws::KMS::KMSClient::ALLOCATION_TAG),
            client_config) :
        new Aws::KMS::KMSClient(client_config);

    char handle[80];
    CMD_NAME(handle, client);
    aws_sdk_tcl_kms_RegisterName(handle, client);

    Tcl_CreateObjCommand(interp, handle,
                                 (Tcl_ObjCmdProc *)  aws_sdk_tcl_kms_ClientObjCmd,
                                 nullptr,
                                 nullptr);

    if (objc == 3) {
        auto *trace = (aws_sdk_tcl_kms_trace_t *) Tcl_Alloc(sizeof(aws_sdk_tcl_kms_trace_t));
        trace->interp = interp;
        trace->varname = aws_sdk_strndup(Tcl_GetString(objv[2]), 80);
        trace->handle = aws_sdk_strndup(handle, 80);
        trace->item = client;
        const char *objVar = Tcl_GetString(objv[2]);
        Tcl_UnsetVar(interp, objVar, 0);
        Tcl_SetVar  (interp, objVar, handle, 0);
        Tcl_TraceVar(interp,objVar,TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                     (Tcl_VarTraceProc*) aws_sdk_tcl_kms_VarTraceProc,
                     (ClientData) trace);
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

static int aws_sdk_tcl_kms_DestroyCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "DestroyCmd\n"));
    CheckArgs(2,2,1,"handle");
    return aws_sdk_tcl_kms_Destroy(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_kms_ListKeysCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "ListKeysCmd\n"));
    CheckArgs(2,2,1,"handle");
    return aws_sdk_tcl_kms_ListKeys(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_kms_CreateKeyCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "CreateKeyCmd\n"));
    CheckArgs(2,2,1,"handle");
    return aws_sdk_tcl_kms_ListKeys(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_kms_DescribeKeyCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "DescribeKeyCmd\n"));
    CheckArgs(3,3,1,"handle arn");
    return aws_sdk_tcl_kms_DescribeKey(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]));
}

static int aws_sdk_tcl_kms_EnableKeyCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "EnableKeyCmd\n"));
    CheckArgs(3,3,1,"handle arn");
    return aws_sdk_tcl_kms_EnableKey(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]));
}

static int aws_sdk_tcl_kms_DisableKeyCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "DisableKeyCmd\n"));
    CheckArgs(3,3,1,"handle arn");
    return aws_sdk_tcl_kms_DisableKey(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]));
}

static int aws_sdk_tcl_kms_ScheduleKeyDeletionCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "ScheduleKeyDeletionCmd\n"));
    CheckArgs(3,4,1,"handle arn ?pending_window_in_days?");
    int pending_window = -1;
    if (objc > 3 && Tcl_GetIntFromObj(interp, objv[3], &pending_window) != TCL_OK) {
        return TCL_ERROR;
    }
    return aws_sdk_tcl_kms_ScheduleKeyDeletion(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), pending_window);
}

static int aws_sdk_tcl_kms_CancelKeyDeletionCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "CancelKeyDeletionCmd\n"));
    CheckArgs(3,3,1,"handle arn");
    return aws_sdk_tcl_kms_CancelKeyDeletion(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]));
}

static int aws_sdk_tcl_kms_EncryptCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "EncryptCmd\n"));
    CheckArgs(4,4,1,"handle arn plain_data");
    Tcl_Size size;
    const unsigned char *buffer = Tcl_GetByteArrayFromObj(objv[3], &size);
    return aws_sdk_tcl_kms_Encrypt(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), buffer, size);
}

static int aws_sdk_tcl_kms_DecryptCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "DecryptCmd\n"));
    CheckArgs(3,3,1,"handle cipher_data");
    Tcl_Size size;
    const unsigned char *buffer = Tcl_GetByteArrayFromObj(objv[2], &size);
    return aws_sdk_tcl_kms_Decrypt(interp, Tcl_GetString(objv[1]), buffer, size);
}

static int aws_sdk_tcl_kms_GenerateDataKeyCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "GenerateDataKeyCmd\n"));
    CheckArgs(4,4,1,"handle arn number_of_bytes");
    int num_bytes;
    if (Tcl_GetIntFromObj(interp, objv[3], &num_bytes) != TCL_OK) {
        return TCL_ERROR;
    }
    return aws_sdk_tcl_kms_GenerateDataKey(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), num_bytes);
}

static int aws_sdk_tcl_kms_GenerateRandomCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "GenerateRandomCmd\n"));
    CheckArgs(3,3,1,"handle number_of_bytes");
    int num_bytes;
    if (Tcl_GetIntFromObj(interp, objv[2], &num_bytes) != TCL_OK) {
        return TCL_ERROR;
    }
    return aws_sdk_tcl_kms_GenerateRandom(interp, Tcl_GetString(objv[1]), num_bytes);
}

static Aws::SDKOptions options;

static void aws_sdk_tcl_kms_ExitHandler(ClientData unused)
{
    Tcl_MutexLock(&aws_sdk_tcl_kms_NameToInternal_HT_Mutex);
    Tcl_DeleteHashTable(&aws_sdk_tcl_kms_NameToInternal_HT);
    Aws::ShutdownAPI(options);
    Tcl_MutexUnlock(&aws_sdk_tcl_kms_NameToInternal_HT_Mutex);
}

void aws_sdk_tcl_kms_InitModule() {
    Tcl_MutexLock(&aws_sdk_tcl_kms_NameToInternal_HT_Mutex);
    if (!aws_sdk_tcl_kms_ModuleInitialized) {
        Aws::InitAPI(options);
        Tcl_InitHashTable(&aws_sdk_tcl_kms_NameToInternal_HT, TCL_STRING_KEYS);
        Tcl_CreateThreadExitHandler(aws_sdk_tcl_kms_ExitHandler, nullptr);
        aws_sdk_tcl_kms_ModuleInitialized = 1;
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_kms_NameToInternal_HT_Mutex);
}

#if TCL_MAJOR_VERSION > 8
#define MIN_VERSION "9.0"
#else
#define MIN_VERSION "8.6"
#endif

int Aws_sdk_tcl_kms_Init(Tcl_Interp *interp) {

    if (Tcl_InitStubs(interp, MIN_VERSION, 0) == nullptr) {
        SetResult("Tcl_InitStubs failed");
        return TCL_ERROR;
    }

    aws_sdk_tcl_kms_InitModule();

    Tcl_CreateNamespace(interp, "::aws::kms", nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::create", aws_sdk_tcl_kms_CreateCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::destroy", aws_sdk_tcl_kms_DestroyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::list_keys", aws_sdk_tcl_kms_ListKeysCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::create_key", aws_sdk_tcl_kms_CreateKeyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::describe_key", aws_sdk_tcl_kms_DescribeKeyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::enable_key", aws_sdk_tcl_kms_EnableKeyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::disable_key", aws_sdk_tcl_kms_DisableKeyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::schedule_key_deletion", aws_sdk_tcl_kms_ScheduleKeyDeletionCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::cancel_key_deletion", aws_sdk_tcl_kms_CancelKeyDeletionCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::encrypt", aws_sdk_tcl_kms_EncryptCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::decrypt", aws_sdk_tcl_kms_DecryptCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::generate_data_key", aws_sdk_tcl_kms_GenerateDataKeyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::kms::generate_random", aws_sdk_tcl_kms_GenerateRandomCmd, nullptr, nullptr);

    return Tcl_PkgProvide(interp, "awskms", XSTR(VERSION));
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Aws_sdk_tcl_kms_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
