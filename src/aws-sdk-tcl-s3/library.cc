#include <iostream>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/ListObjectsRequest.h>
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
    Aws::S3::S3Client *internal = NULL;
    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_s3_NameToInternal_HT, (char*)name);
    if (entryPtr != NULL) {
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

//    Aws::S3::S3Client s3Client(clientConfig);
    Aws::S3::S3Client *s3Client = new Aws::S3::S3Client(clientConfig);
    char handle[80];
    CMD_NAME(handle, s3Client);
    aws_sdk_tcl_s3_RegisterName(handle, s3Client);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

static int aws_sdk_tcl_s3_ListCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr,"ListCmd\n"));

    CheckArgs(3,4,1,"handle_name bucket ?key?");

    const char *handleName = Tcl_GetString(objv[1]);
    const Aws::String bucketName = Tcl_GetString(objv[2]);

    Aws::S3::S3Client *s3Client = aws_sdk_tcl_s3_GetInternalFromName(handleName);
    Aws::S3::Model::ListObjectsRequest request;
    request.WithBucket(bucketName);
    if (objc == 4) {
        const Aws::String keyName = Tcl_GetString(objv[3]);
        request.WithPrefix(keyName);
    }

    auto outcome = s3Client->ListObjects(request);

    if (!outcome.IsSuccess()) {
        std::cerr << "Error: ListObjects: " <<
                  outcome.GetError().GetMessage() << std::endl;
    }
    else {
        Aws::Vector<Aws::S3::Model::Object> objects =
                outcome.GetResult().GetContents();

        for (Aws::S3::Model::Object &object: objects) {
            std::cout << object.GetKey() << std::endl;
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(outcome.IsSuccess() ? 1 : 0));
    return TCL_OK;
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
        Tcl_CreateThreadExitHandler(aws_sdk_tcl_s3_ExitHandler, NULL);
        aws_sdk_tcl_s3_ModuleInitialized = 1;
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);
}

int Aws_sdk_tcl_s3_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.6", 0) == NULL) {
        return TCL_ERROR;
    }

    aws_sdk_tcl_s3_InitModule();

    Tcl_CreateNamespace(interp, "::aws::s3", NULL, NULL);
    Tcl_CreateObjCommand(interp, "::aws::s3::create", aws_sdk_tcl_s3_CreateCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::aws::s3::ls", aws_sdk_tcl_s3_ListCmd, NULL, NULL);

    return Tcl_PkgProvide(interp, "aws_sdk_tcl_s3", "0.1");
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Tbert_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
