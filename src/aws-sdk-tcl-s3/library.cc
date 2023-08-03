#include <iostream>
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/Object.h>
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

static Tcl_HashTable aws_sdk_tcl_s3_NameToInternal_HT;
static Tcl_Mutex     aws_sdk_tcl_s3_NameToInternal_HT_Mutex;
static int           aws_sdk_tcl_s3_ModuleInitialized;
static int
aws_sdk_tcl_s3_RegisterName(const char *name, struct bert_ctx *ctx) {

    Tcl_HashEntry *entryPtr;
    int newEntry;

    Tcl_MutexLock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);
    entryPtr = Tcl_CreateHashEntry(&aws_sdk_tcl_s3_NameToInternal_HT, (char*) name, &newEntry);
    if (newEntry) {
        Tcl_SetHashValue(entryPtr, (ClientData)ctx);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> RegisterName: name=%s ctx=%p %s\n", name, ctx, newEntry ? "entered into" : "already in"));

    return 0;
}

static struct bert_ctx *
aws_sdk_tcl_s3_GetInternalFromName(const char *name) {
    struct bert_ctx *ctx = NULL;
    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_s3_NameToInternal_HT, (char*)name);
    if (entryPtr != NULL) {
        ctx = (struct bert_ctx *)Tcl_GetHashValue(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_s3_NameToInternal_HT_Mutex);

    return ctx;
}

static int aws_sdk_tcl_s3_ListCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr,"ListCmd\n"));

    CheckArgs(3,3,1,"handle_name bucket");

    const Aws::String bucketName = Aws::String(Tcl_GetString(objv[2]));

    Aws::Client::ClientConfiguration clientConfig;
        // Optional: Set to the AWS Region (overrides config file).
        // clientConfig.region = "us-east-1";

    Aws::S3::S3Client s3_client(clientConfig);

    Aws::S3::Model::ListObjectsRequest request;
    request.WithBucket(bucketName);

    auto outcome = s3_client.ListObjects(request);

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
    Tcl_CreateObjCommand(interp, "::aws::s3::ls", aws_sdk_tcl_s3_ListCmd, NULL, NULL);

    return Tcl_PkgProvide(interp, "aws_sdk_tcl_s3", "0.1");
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Tbert_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
