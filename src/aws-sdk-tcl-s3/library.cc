/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#include <iostream>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include "aws/s3/model/PutObjectRequest.h"
#include "aws/s3/model/GetObjectRequest.h"
#include "aws/s3/model/DeleteObjectRequest.h"
#include "aws/s3/model/HeadObjectRequest.h"
#include <aws/s3/model/Object.h>
#include <cstdio>
#include <fstream>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/s3/model/DeleteBucketRequest.h>
#include <aws/s3/model/HeadBucketRequest.h>
#include <aws/s3/model/Delete.h>
#include <aws/s3/model/DeleteObjectsRequest.h>
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

#define CheckArgs(min,max,n,msg) \
                 if ((objc < min) || (objc >max)) { \
                     Tcl_WrongNumArgs(interp, n, objv, msg); \
                     return TCL_ERROR; \
                 }

#define CMD_NAME(s,internal) std::sprintf((s), "_AWS_S3_%p", (internal))

static char VAR_READ_ONLY_MSG[] = "var is read-only";

typedef struct {
    Tcl_Interp *interp;
    char *handle;
    char *varname;
    Aws::S3::S3Client *item;
} aws_sdk_tcl_s3_trace_t;

static Tcl_HashTable aws_sdk_tcl_s3_NameToInternal_HT;
static Tcl_Mutex     aws_sdk_tcl_s3_NameToInternal_HT_Mutex;
static int           aws_sdk_tcl_s3_ModuleInitialized;

static char s3_client_usage[] =
    "Usage s3Client <method> <args>, where method can be:\n"
    "   ls bucket ?key?                 \n"
    "   put_text bucket key text        \n"
    "   put bucket key input_file       \n"
    "   get bucket key ?output_file?    \n"
    "   delete bucket key               \n"
    "   batch_delete bucket keys        \n"
    "   exists bucket key               \n"
    "   create_bucket bucket            \n"
    "   delete_bucket bucket            \n"
    "   exists_bucket bucket            \n"
    "   list_buckets                    \n"
    "   destroy                         \n"
;


static int
aws_sdk_tcl_s3_RegisterName(const char *name, Aws::S3::S3Client *internal) {

    Tcl_HashEntry *entryPtr;
    int newEntry;

    Tcl_MutexLock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);
    entryPtr = Tcl_CreateHashEntry(&aws_sdk_tcl_s3_NameToInternal_HT, (char*) name, &newEntry);
    if (newEntry) {
        Tcl_SetHashValue(entryPtr, (ClientData)internal);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> RegisterName: name=%s internal=%p %s\n", name, internal, newEntry ? "entered into" : "already in"));

    return !!newEntry;
}

static int
aws_sdk_tcl_s3_UnregisterName(const char *name) {

    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_s3_NameToInternal_HT, (char*)name);
    if (entryPtr != nullptr) {
        Tcl_DeleteHashEntry(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> UnregisterName: name=%s entryPtr=%p\n", name, entryPtr));

    return entryPtr != nullptr;
}

static struct Aws::S3::S3Client *
aws_sdk_tcl_s3_GetInternalFromName(const char *name) {
    Aws::S3::S3Client *internal = nullptr;
    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_s3_NameToInternal_HT, (char*)name);
    if (entryPtr != nullptr) {
        internal = (Aws::S3::S3Client *)Tcl_GetHashValue(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);

    return internal;
}

int aws_sdk_tcl_s3_Destroy(Tcl_Interp *interp, const char *handle) {
    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handle);
    if (!aws_sdk_tcl_s3_UnregisterName(handle)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    delete client;
    Tcl_DeleteCommand(interp, handle);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

int aws_sdk_tcl_s3_List(Tcl_Interp *interp, const char *handle, const char *bucket_name, const char *key_name) {
    DBG(fprintf(stderr, "aws_sdk_tcl_s3_List: handle=%s bucket_name=%s key_name=%s\n", handle, bucket_name, key_name));
    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    const Aws::String bucket = bucket_name;

    Aws::S3::Model::ListObjectsRequest request;
    request.WithBucket(bucket);
    if (key_name) {
        const Aws::String key = key_name;
        request.WithPrefix(key);
    }

    auto outcome = client->ListObjects(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
    else {
        Aws::Vector<Aws::S3::Model::Object> objects =
                outcome.GetResult().GetContents();

        Tcl_Obj *listObj = Tcl_NewListObj(0, nullptr);
        for (Aws::S3::Model::Object &object: objects) {
            Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(object.GetKey().c_str(), -1));
        }
        Tcl_SetObjResult(interp, listObj);
        return TCL_OK;
    }
}

int aws_sdk_tcl_s3_PutText(Tcl_Interp *interp, const char *handle, const char *bucket_name, const char *key_name, const char *text) {
    DBG(fprintf(stderr, "PutText: handle=%s bucket_name=%s key_name=%s text=%s\n", handle, bucket_name, key_name, text));
    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    const Aws::String bucket = bucket_name;
    const Aws::String key = key_name;

    const std::shared_ptr<Aws::IOStream> inputData =
            Aws::MakeShared<Aws::StringStream>("");
    *inputData << text;

    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucket);
    request.SetKey(key);
    request.SetBody(inputData);
    Aws::S3::Model::PutObjectOutcome outcome = client->PutObject(request);
    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

int aws_sdk_tcl_s3_PutChannel(Tcl_Interp *interp, const char *handle, const char *bucket_name, const char *key_name, const char *filename) {

    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    const Aws::String bucket = bucket_name;
    const Aws::String key = key_name;

//    int mode;
//    Tcl_Channel channel = Tcl_GetChannel(interp, Tcl_GetString(objv[4]), &mode);
//    if (!(mode & TCL_READABLE)) {
//        Tcl_SetObjResult(interp, Tcl_NewStringObj("Channel not readable", -1));
//        return TCL_ERROR;
//    }

    std::shared_ptr<Aws::IOStream> inputData =
            Aws::MakeShared<Aws::FStream>("SampleAllocationTag",
                                          filename,
                                          std::ios_base::in | std::ios_base::binary);

    if (!*inputData) {
//        std::cerr << "Error unable to read file " << fileName << std::endl;
        Tcl_SetObjResult(interp, Tcl_NewStringObj("Error unable to read file", -1));
        return TCL_ERROR;
    }
//    const std::shared_ptr<Aws::IOStream> inputData =
//            Aws::MakeShared<Aws::StringStream>("");

//    int size = Tcl_GetChannelBufferSize(channel);
//    char buf[size];
//    while(!Tcl_Eof(channel)) {
//        fprintf(stderr, "size: %d\n", size);
//        int nread = Tcl_Read(channel, buf, size);
//        fprintf(stderr, "nread: %d\n", nread);
//        *inputData << buf;
//    }

    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucket);
    request.SetKey(key);
    request.SetBody(inputData);
    Aws::S3::Model::PutObjectOutcome outcome = client->PutObject(request);
    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

int aws_sdk_tcl_s3_Get(Tcl_Interp *interp, const char *handle, const char *bucket_name, const char *key_name, const char *filename) {
    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    const Aws::String bucket = bucket_name;
    const Aws::String key = key_name;

    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(bucket);
    request.SetKey(key);

    Aws::S3::Model::GetObjectOutcome outcome =
            client->GetObject(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    } else {
        Aws::IOStream &body = outcome.GetResult().GetBody();
        if (filename) {
            std::ofstream ofs;
            ofs.open(filename, std::ios::app | std::ios::binary);
            ofs << body.rdbuf();
//            int mode;
//            Tcl_Channel  channel = Tcl_GetChannel(interp, Tcl_GetString(objv[4]), &mode);
//            if (!(mode & TCL_WRITABLE)) {
//                return TCL_ERROR;
//            }
//            Tcl_Write(channel,ss.str().c_str(), -1);
        } else {
            std::ostringstream ss;
            ss << body.rdbuf();
            Tcl_SetObjResult(interp, Tcl_NewStringObj(ss.str().c_str(), -1));
        }
        return TCL_OK;
    }
}

int aws_sdk_tcl_s3_Delete(Tcl_Interp *interp, const char *handle, const char *bucket_name, const char *key_name) {
    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    const Aws::String bucket = bucket_name;
    const Aws::String key = key_name;

    Aws::S3::Model::DeleteObjectRequest request;

    request.WithKey(key)
            .WithBucket(bucket);

    Aws::S3::Model::DeleteObjectOutcome outcome =
            client->DeleteObject(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

int aws_sdk_tcl_s3_BatchDelete(Tcl_Interp *interp, const char *handle, const char *bucket_name, Tcl_Obj *listPtr) {
    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    const Aws::String bucket = bucket_name;
    Aws::S3::Model::DeleteObjectsRequest request;

    Aws::S3::Model::Delete deleteObject;
    Tcl_Size listLen;
    Tcl_ListObjLength(interp, listPtr, &listLen);
    for (int i = 0; i < listLen; i++) {
        Tcl_Obj *objPtr;
        Tcl_ListObjIndex(interp, listPtr, i, &objPtr);
        Aws::String objectKey = Tcl_GetString(objPtr);
        deleteObject.AddObjects(Aws::S3::Model::ObjectIdentifier().WithKey(objectKey));
    }

    request.SetDelete(deleteObject);
    request.SetBucket(bucket);

    Aws::S3::Model::DeleteObjectsOutcome outcome =
            client->DeleteObjects(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

int aws_sdk_tcl_s3_Exists(Tcl_Interp *interp, const char *handle, const char *bucket_name, const char *key_name) {
    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    const Aws::String bucket = bucket_name;
    const Aws::String key = key_name;

    Aws::S3::Model::HeadObjectRequest request;
    request.WithKey(key)
            .WithBucket(bucket);

    Aws::S3::Model::HeadObjectOutcome outcome =
            client->HeadObject(request);

    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(outcome.IsSuccess()));
    return TCL_OK;
}

int aws_sdk_tcl_s3_CreateBucket(Tcl_Interp *interp, const char *handle, const char *bucket_name) {
    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    const Aws::String bucket = bucket_name;

    Aws::S3::Model::CreateBucketRequest request;
    request.WithBucket(bucket);

    Aws::S3::Model::CreateBucketOutcome outcome =
            client->CreateBucket(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

int aws_sdk_tcl_s3_DeleteBucket(Tcl_Interp *interp, const char *handle, const char *bucket_name) {
    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    const Aws::String bucket = bucket_name;

    Aws::S3::Model::DeleteBucketRequest request;
    request.WithBucket(bucket);

    Aws::S3::Model::DeleteBucketOutcome outcome =
            client->DeleteBucket(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

int aws_sdk_tcl_s3_ExistsBucket(Tcl_Interp *interp, const char *handle, const char *bucket_name) {
    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    const Aws::String bucket = bucket_name;

    Aws::S3::Model::HeadBucketRequest request;
    request.WithBucket(bucket);

    Aws::S3::Model::HeadBucketOutcome outcome =
            client->HeadBucket(request);

    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(outcome.IsSuccess()));
    return TCL_OK;
}

int aws_sdk_tcl_s3_ListBuckets(Tcl_Interp *interp, const char *handle) {
    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    auto outcome = client->ListBuckets();
    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
    else {
        Tcl_Obj *listObj = Tcl_NewListObj(0, nullptr);
        for (auto &&b: outcome.GetResult().GetBuckets()) {
            Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(b.GetName().c_str(), -1));
        }
        Tcl_SetObjResult(interp, listObj);
        return TCL_OK;
    }

    return TCL_OK;
}

int aws_sdk_tcl_s3_ClientObjCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    static const char *clientMethods[] = {
            "destroy",
            "ls",
            "put_text",
            "put",
            "get",
            "delete",
            "batch_delete",
            "exists",
            "create_bucket",
            "delete_bucket",
            "exists_bucket",
            "list_buckets",
            nullptr
    };

    enum clientMethod {
        m_destroy,
        m_ls,
        m_putText,
        m_put,
        m_get,
        m_delete,
        m_batchDelete,
        m_exists,
        m_createBucket,
        m_deleteBucket,
        m_existsBucket,
        m_listBuckets
    };

    if (objc < 2) {
        Tcl_ResetResult(interp);
        Tcl_SetStringObj(Tcl_GetObjResult(interp), (s3_client_usage), -1);
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
                return aws_sdk_tcl_s3_Destroy(interp, handle);
            case m_ls:
                DBG(fprintf(stderr, "ListMethod\n"));
                CheckArgs(3,4,1,"ls bucket ?prefix?");
                return aws_sdk_tcl_s3_List(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        objc == 4 ? Tcl_GetString(objv[3]) : nullptr
                );
            case m_putText:
                DBG(fprintf(stderr, "PutTextMethod\n"));
                CheckArgs(5,5,1,"put_text bucket prefix text");
                return aws_sdk_tcl_s3_PutText(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3]),
                        Tcl_GetString(objv[4])
                );
            case m_put:
                DBG(fprintf(stderr, "PutMethod\n"));
                CheckArgs(5,5,1,"put bucket prefix filename");
                return aws_sdk_tcl_s3_PutChannel(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3]),
                        Tcl_GetString(objv[4])
                );
            case m_get:
                DBG(fprintf(stderr, "GetMethod\n"));
                CheckArgs(4,5,1,"get bucket prefix ?filename?");
                return aws_sdk_tcl_s3_Get(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3]),
                        objc == 5 ? Tcl_GetString(objv[4]) : nullptr
                );
            case m_delete:
                DBG(fprintf(stderr, "DeleteMethod\n"));
                CheckArgs(4,4,1,"delete bucket key");
                return aws_sdk_tcl_s3_Delete(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3])
                );
            case m_batchDelete:
                DBG(fprintf(stderr, "BatchDeleteMethod\n"));
                CheckArgs(4,4,1,"batch_delete bucket keys");
                return aws_sdk_tcl_s3_BatchDelete(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        objv[3]
                );
            case m_exists:
                DBG(fprintf(stderr, "ExistsMethod\n"));
                CheckArgs(4,4,1,"exists bucket key");
                return aws_sdk_tcl_s3_Exists(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3])
                );
            case m_createBucket:
                DBG(fprintf(stderr, "CreateBucketMethod\n"));
                CheckArgs(3,3,1,"create_bucket bucket");
                return aws_sdk_tcl_s3_CreateBucket(
                        interp,
                        handle,
                        Tcl_GetString(objv[2])
                );
            case m_deleteBucket:
                DBG(fprintf(stderr, "DeleteBucketMethod\n"));
                CheckArgs(3,3,1,"delete_bucket bucket");
                return aws_sdk_tcl_s3_DeleteBucket(
                        interp,
                        handle,
                        Tcl_GetString(objv[2])
                );
            case m_existsBucket:
                DBG(fprintf(stderr, "ExistsBucketMethod\n"));
                CheckArgs(3,3,1,"exists_bucket bucket");
                return aws_sdk_tcl_s3_ExistsBucket(
                        interp,
                        handle,
                        Tcl_GetString(objv[2])
                );
            case m_listBuckets:
                DBG(fprintf(stderr, "ListBucketsMethod\n"));
                CheckArgs(2,2,1,"list_buckets");
                return aws_sdk_tcl_s3_ListBuckets(
                        interp,
                        handle
                );
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj("Unknown method", -1));
    return TCL_ERROR;
}

char *aws_sdk_tcl_s3_VarTraceProc(ClientData clientData, Tcl_Interp *interp, const char *name1, const char *name2, int flags) {
    auto *trace = (aws_sdk_tcl_s3_trace_t *) clientData;
    if (trace->item == nullptr) {
        DBG(fprintf(stderr, "VarTraceProc: client has been deleted\n"));
        if (!Tcl_InterpDeleted(trace->interp)) {
            Tcl_UntraceVar(trace->interp, trace->varname, TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                           (Tcl_VarTraceProc*) aws_sdk_tcl_s3_VarTraceProc,
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
        aws_sdk_tcl_s3_Destroy(trace->interp, trace->handle);
        Tcl_Free((char *) trace->varname);
        Tcl_Free((char *) trace->handle);
        Tcl_Free((char *) trace);
    }
    return nullptr;
}

static int aws_sdk_tcl_s3_CreateCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
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

    auto *client = credentials_provider_ptr != nullptr ? new Aws::S3::S3Client(credentials_provider_ptr, Aws::MakeShared<Aws::S3::S3EndpointProvider>(Aws::S3::S3Client::ALLOCATION_TAG), client_config) : new Aws::S3::S3Client(client_config);
    char handle[80];
    CMD_NAME(handle, client);
    aws_sdk_tcl_s3_RegisterName(handle, client);

    Tcl_CreateObjCommand(interp, handle,
                                 (Tcl_ObjCmdProc *)  aws_sdk_tcl_s3_ClientObjCmd,
                                 nullptr,
                                 nullptr);
//                                 (Tcl_CmdDeleteProc*) aws_sdk_tcl_s3_clientObjCmdDeleteProc);

    if (objc == 3) {
        auto *trace = (aws_sdk_tcl_s3_trace_t *) Tcl_Alloc(sizeof(aws_sdk_tcl_s3_trace_t));
        trace->interp = interp;
        trace->varname = aws_sdk_strndup(Tcl_GetString(objv[2]), 80);
        trace->handle = aws_sdk_strndup(handle, 80);
        trace->item = client;
        const char *objVar = Tcl_GetString(objv[2]);
        Tcl_UnsetVar(interp, objVar, 0);
        Tcl_SetVar  (interp, objVar, handle, 0);
        Tcl_TraceVar(interp,objVar,TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                     (Tcl_VarTraceProc*) aws_sdk_tcl_s3_VarTraceProc,
                     (ClientData) trace);
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

static int aws_sdk_tcl_s3_DestroyCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "DestroyCmd\n"));
    CheckArgs(2,2,1,"handle");
    return aws_sdk_tcl_s3_Destroy(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_s3_ListCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr,"ListCmd\n"));
    CheckArgs(3,4,1,"handle_name bucket ?key?");
    return aws_sdk_tcl_s3_List(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), objc == 4 ? Tcl_GetString(objv[3]) : nullptr);
}

static int aws_sdk_tcl_s3_PutTextCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "PutCmd\n"));
    CheckArgs(5,5,1,"handle_name bucket key text");
    return aws_sdk_tcl_s3_PutText(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), Tcl_GetString(objv[3]), Tcl_GetString(objv[4]));
}


static int aws_sdk_tcl_s3_PutChannelCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "PutChannelCmd\n"));
    CheckArgs(5,5,1,"handle_name bucket key filename");
    return aws_sdk_tcl_s3_PutChannel(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), Tcl_GetString(objv[3]), Tcl_GetString(objv[4]));
}

static int aws_sdk_tcl_s3_GetCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "GetCmd\n"));
    CheckArgs(4,5,1,"handle_name bucket key ?filename?");
    return aws_sdk_tcl_s3_Get(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), Tcl_GetString(objv[3]), objc == 5 ? Tcl_GetString(objv[4]) : nullptr);
}

static int aws_sdk_tcl_s3_DeleteCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "DeleteCmd\n"));
    CheckArgs(4,4,1,"handle_name bucket key");
    return aws_sdk_tcl_s3_Delete(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), Tcl_GetString(objv[3]));
}

static int aws_sdk_tcl_s3_BatchDeleteCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "BatchDeleteCmd\n"));
    CheckArgs(4,4,1,"handle_name bucket keys");
    return aws_sdk_tcl_s3_BatchDelete(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), objv[3]);
}

static int aws_sdk_tcl_s3_ExistsCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "ExistsCmd\n"));
    CheckArgs(4,4,1,"handle_name bucket key");
    return aws_sdk_tcl_s3_Exists(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), Tcl_GetString(objv[3]));
}

static int aws_sdk_tcl_s3_CreateBucketCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "CreateBucketCmd\n"));
    CheckArgs(3,3,1,"handle_name bucket");
    return aws_sdk_tcl_s3_CreateBucket(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]));
}

static int aws_sdk_tcl_s3_DeleteBucketCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "DeleteBucketCmd\n"));
    CheckArgs(3,3,1,"handle_name bucket");
    return aws_sdk_tcl_s3_DeleteBucket(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]));
}

static int aws_sdk_tcl_s3_ExistsBucketCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "ExistsBucketCmd\n"));
    CheckArgs(3,3,1,"handle_name bucket");
    return aws_sdk_tcl_s3_ExistsBucket(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]));
}

static int aws_sdk_tcl_s3_ListBucketsCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "ListBucketsCmd\n"));
    CheckArgs(2,2,1,"handle_name");
    return aws_sdk_tcl_s3_ListBuckets(interp, Tcl_GetString(objv[1]));
}

static void aws_sdk_tcl_s3_ExitHandler(ClientData unused)
{
    Tcl_MutexLock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);
    Tcl_DeleteHashTable(&aws_sdk_tcl_s3_NameToInternal_HT);
    Tcl_MutexUnlock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);

}


void aws_sdk_tcl_s3_InitModule() {
    Tcl_MutexLock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);
    if (!aws_sdk_tcl_s3_ModuleInitialized) {
        Aws::SDKOptions options;
        Aws::InitAPI(options);
        Tcl_InitHashTable(&aws_sdk_tcl_s3_NameToInternal_HT, TCL_STRING_KEYS);
        Tcl_CreateThreadExitHandler(aws_sdk_tcl_s3_ExitHandler, nullptr);
        aws_sdk_tcl_s3_ModuleInitialized = 1;
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);
}

int Aws_sdk_tcl_s3_Init(Tcl_Interp *interp) {

    int major, minor, patchLevel, type;
    Tcl_GetVersion(&major, &minor, &patchLevel, &type);

    const char *version = major == 9 ? "9.0" : "8.6";
    if (Tcl_InitStubs(interp, version, 0) == nullptr) {
        return TCL_ERROR;
    }

    aws_sdk_tcl_s3_InitModule();

    Tcl_CreateNamespace(interp, "::aws::s3", nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::create", aws_sdk_tcl_s3_CreateCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::destroy", aws_sdk_tcl_s3_DestroyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::ls", aws_sdk_tcl_s3_ListCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::put_text", aws_sdk_tcl_s3_PutTextCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::put", aws_sdk_tcl_s3_PutChannelCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::get", aws_sdk_tcl_s3_GetCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::delete", aws_sdk_tcl_s3_DeleteCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::batch_delete", aws_sdk_tcl_s3_BatchDeleteCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::exists", aws_sdk_tcl_s3_ExistsCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::create_bucket", aws_sdk_tcl_s3_CreateBucketCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::delete_bucket", aws_sdk_tcl_s3_DeleteBucketCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::exists_bucket", aws_sdk_tcl_s3_ExistsBucketCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::list_buckets", aws_sdk_tcl_s3_ListBucketsCmd, nullptr, nullptr);

    return Tcl_PkgProvide(interp, "awss3", XSTR(VERSION));
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Aws_sdk_tcl_s3_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
