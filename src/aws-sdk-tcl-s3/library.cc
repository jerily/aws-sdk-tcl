#include <iostream>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include "aws/s3/model/PutObjectRequest.h"
#include "aws/s3/model/GetObjectRequest.h"
#include "aws/s3/model/DeleteObjectRequest.h"
#include <aws/s3/model/Object.h>
#include <cstdio>
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

static Tcl_HashTable aws_sdk_tcl_s3_NameToInternal_HT;
static Tcl_Mutex     aws_sdk_tcl_s3_NameToInternal_HT_Mutex;
static int           aws_sdk_tcl_s3_ModuleInitialized;
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

    return 0;
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

static int aws_sdk_tcl_s3_CreateCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "CreateCmd\n"));

    CheckArgs(2,2,1,"config_dict");

    Aws::Client::ClientConfiguration clientConfig;
    // Optional: Set to the AWS Region (overrides config file).
    // clientConfig.region = "us-east-1";

//    Aws::S3::S3Client client(clientConfig);
    auto *client = new Aws::S3::S3Client(clientConfig);
    char handle[80];
    CMD_NAME(handle, client);
    aws_sdk_tcl_s3_RegisterName(handle, client);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

static int aws_sdk_tcl_s3_ListCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr,"ListCmd\n"));

    CheckArgs(3,4,1,"handle_name bucket ?key?");

    const char *handleName = Tcl_GetString(objv[1]);
    const Aws::String bucket = Tcl_GetString(objv[2]);

    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handleName);
    Aws::S3::Model::ListObjectsRequest request;
    request.WithBucket(bucket);
    if (objc == 4) {
        const Aws::String key = Tcl_GetString(objv[3]);
        request.WithPrefix(key);
    }

    auto outcome = client->ListObjects(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
//        std::cerr << "Error: ListObjects: " <<
//                  outcome.GetError().GetMessage() << std::endl;
        return TCL_ERROR;
    }
    else {
        Aws::Vector<Aws::S3::Model::Object> objects =
                outcome.GetResult().GetContents();

        Tcl_Obj *listObj = Tcl_NewListObj(0, nullptr);
        for (Aws::S3::Model::Object &object: objects) {
//            std::cout << object.GetKey() << std::endl;
            Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(object.GetKey().c_str(), -1));
        }
        Tcl_SetObjResult(interp, listObj);
        return TCL_OK;
    }

}

static int aws_sdk_tcl_s3_PutCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "PutCmd\n"));

    CheckArgs(5,5,1,"handle_name bucket key text");

    const char *handleName = Tcl_GetString(objv[1]);
    const Aws::String bucket = Tcl_GetString(objv[2]);
    const Aws::String key = Tcl_GetString(objv[3]);

    const std::shared_ptr<Aws::IOStream> inputData =
            Aws::MakeShared<Aws::StringStream>("");
    *inputData << Tcl_GetString(objv[4]);

    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handleName);
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


static int aws_sdk_tcl_s3_GetCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "GetCmd\n"));

    CheckArgs(4,5,1,"handle_name bucket key ?chan?");

    const char *handleName = Tcl_GetString(objv[1]);
    const Aws::String bucket = Tcl_GetString(objv[2]);
    const Aws::String key = Tcl_GetString(objv[3]);

    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(bucket);
    request.SetKey(key);
    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handleName);

    Aws::S3::Model::GetObjectOutcome outcome =
            client->GetObject(request);

    if (!outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    } else {
        Aws::IOStream &body = outcome.GetResult().GetBody();
        std::ostringstream ss;
        ss << body.rdbuf();
        if (objc == 5) {
            int mode;
            Tcl_Channel  channel = Tcl_GetChannel(interp, Tcl_GetString(objv[4]), &mode);
            if (!(mode & TCL_WRITABLE)) {
                return TCL_ERROR;
            }
            Tcl_WriteObj(channel, Tcl_NewStringObj(ss.str().c_str(), -1));
        } else {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(ss.str().c_str(), -1));
        }
        return TCL_OK;
    }
}

static int aws_sdk_tcl_s3_DeleteCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "DeleteCmd\n"));

    CheckArgs(4,4,1,"handle_name bucket key");

    const char *handleName = Tcl_GetString(objv[1]);
    const Aws::String bucket = Tcl_GetString(objv[2]);
    const Aws::String key = Tcl_GetString(objv[3]);

    Aws::S3::S3Client *client = aws_sdk_tcl_s3_GetInternalFromName(handleName);
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
    if (Tcl_InitStubs(interp, "8.6", 0) == nullptr) {
        return TCL_ERROR;
    }

    aws_sdk_tcl_s3_InitModule();

    Tcl_CreateNamespace(interp, "::aws::s3", nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::create", aws_sdk_tcl_s3_CreateCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::ls", aws_sdk_tcl_s3_ListCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::put", aws_sdk_tcl_s3_PutCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::get", aws_sdk_tcl_s3_GetCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::delete", aws_sdk_tcl_s3_DeleteCmd, nullptr, nullptr);

    return Tcl_PkgProvide(interp, "aws_sdk_tcl_s3", "0.1");
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Tbert_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
