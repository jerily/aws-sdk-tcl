#include <iostream>
#include <aws/core/Aws.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <cstdio>
#include <fstream>
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

#define CMD_NAME(s,internal) std::sprintf((s), "_AWS_DDB_%p", (internal))

static Tcl_HashTable aws_sdk_tcl_dynamodb_NameToInternal_HT;
static Tcl_Mutex     aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex;
static int           aws_sdk_tcl_dynamodb_ModuleInitialized;

static char s3_client_usage[] =
    "Usage s3Client <method> <args>, where method can be:\n"
    "   ls bucket ?key?                 \n"
    "   put_text bucket key text        \n"
    "   put bucket key input_file       \n"
    "   get bucket key ?output_file?    \n"
    "   delete bucket key               \n"
;


static int
aws_sdk_tcl_dynamodb_RegisterName(const char *name, Aws::DynamoDB::DynamoDBClient *internal) {

    Tcl_HashEntry *entryPtr;
    int newEntry;

    Tcl_MutexLock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);
    entryPtr = Tcl_CreateHashEntry(&aws_sdk_tcl_dynamodb_NameToInternal_HT, (char*) name, &newEntry);
    if (newEntry) {
        Tcl_SetHashValue(entryPtr, (ClientData)internal);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> RegisterName: name=%s internal=%p %s\n", name, internal, newEntry ? "entered into" : "already in"));

    return !!newEntry;
}

static int
aws_sdk_tcl_dynamodb_UnregisterName(const char *name) {

    Tcl_HashEntry *entryPtr;
    int newEntry;

    Tcl_MutexLock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_dynamodb_NameToInternal_HT, (char*)name);
    if (entryPtr != nullptr) {
        Tcl_DeleteHashEntry(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> UnregisterName: name=%s entryPtr=%p\n", name, entryPtr));

    return entryPtr != nullptr;
}

static struct Aws::DynamoDB::DynamoDBClient *
aws_sdk_tcl_dynamodb_GetInternalFromName(const char *name) {
    Aws::DynamoDB::DynamoDBClient *internal = nullptr;
    Tcl_HashEntry *entryPtr;

    Tcl_MutexLock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_dynamodb_NameToInternal_HT, (char*)name);
    if (entryPtr != nullptr) {
        internal = (Aws::DynamoDB::DynamoDBClient *)Tcl_GetHashValue(entryPtr);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);

    return internal;
}

int aws_sdk_tcl_dynamodb_Destroy(Tcl_Interp *interp, const char *handle) {
    Aws::DynamoDB::DynamoDBClient *client = aws_sdk_tcl_dynamodb_GetInternalFromName(handle);
    if (!aws_sdk_tcl_dynamodb_UnregisterName(handle)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    delete client;
    Tcl_DeleteCommand(interp, handle);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

int aws_sdk_tcl_dynamodb_PutItem(Tcl_Interp *interp, const char *handle, const char *bucket_name, const char *key_name) {
    DBG(fprintf(stderr, "aws_sdk_tcl_dynamodb_PutItem: handle=%s bucket_name=%s key_name=%s\n", handle, bucket_name, key_name));
    Aws::DynamoDB::DynamoDBClient *client = aws_sdk_tcl_dynamodb_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }
    // TODO: implement dynamodb put item using the aws-sdk-cpp

    return TCL_OK;
}

int aws_sdk_tcl_dynamodb_ClientObjCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    static const char *clientMethods[] = {
            "destroy",
            "put_item",
            "get_item",
            "update_item",
            "delete_item",
            nullptr
    };

    enum clientMethod {
        m_destroy,
        m_putItem,
        m_getItem,
        m_updateItem,
        m_deleteItem,
    };

    if (objc < 2) {
        Tcl_ResetResult(interp);
        Tcl_SetStringObj(Tcl_GetObjResult(interp), (s3_client_usage), -1);
        return TCL_ERROR;
    }
    Tcl_ResetResult(interp);

    int methodIndex;
    if (TCL_OK == Tcl_GetIndexFromObj(interp, objv[1], clientMethods, "method", 0, &methodIndex) != TCL_OK) {
        Tcl_ResetResult(interp);
        const char *handle = Tcl_GetString(objv[0]);
        switch ((enum clientMethod) methodIndex ) {
            case m_destroy:
                return aws_sdk_tcl_dynamodb_Destroy(interp, handle);
            case m_putItem:
                return aws_sdk_tcl_dynamodb_PutItem(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3])
                );
            case m_getItem:
                break;
            case m_updateItem:
                break;
            case m_deleteItem:
                break;
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj("Unknown method", -1));
    return TCL_ERROR;
}

static int aws_sdk_tcl_dynamodb_CreateCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]) {
    DBG(fprintf(stderr, "CreateCmd\n"));

    CheckArgs(2,2,1,"config_dict");

    Aws::Client::ClientConfiguration clientConfig;
    // Optional: Set to the AWS Region (overrides config file).
    // clientConfig.region = "us-east-1";

//    Aws::DynamoDB::DynamoDBClient client(clientConfig);
    auto *client = new Aws::DynamoDB::DynamoDBClient(clientConfig);
    char handle[80];
    CMD_NAME(handle, client);
    aws_sdk_tcl_dynamodb_RegisterName(handle, client);

    Tcl_CreateObjCommand(interp, handle,
                                 (Tcl_ObjCmdProc *)  aws_sdk_tcl_dynamodb_ClientObjCmd,
                                 nullptr,
                                 nullptr);
//                                 (Tcl_CmdDeleteProc*) aws_sdk_tcl_dynamodb_clientObjCmdDeleteProc);

    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

static int aws_sdk_tcl_dynamodb_DestroyCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "DestroyCmd\n"));
    CheckArgs(2,2,1,"handle");
    return aws_sdk_tcl_dynamodb_Destroy(interp, Tcl_GetString(objv[1]));
}


static int aws_sdk_tcl_dynamodb_PutItemCmd(ClientData  clientData, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[] ) {
    DBG(fprintf(stderr, "PutChannelCmd\n"));
    CheckArgs(5,5,1,"handle_name bucket key filename");
    return aws_sdk_tcl_dynamodb_PutItem(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), Tcl_GetString(objv[3]));
}

static void aws_sdk_tcl_dynamodb_ExitHandler(ClientData unused)
{
    Tcl_MutexLock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);
    Tcl_DeleteHashTable(&aws_sdk_tcl_dynamodb_NameToInternal_HT);
    Tcl_MutexUnlock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);

}


void aws_sdk_tcl_dynamodb_InitModule() {
    Tcl_MutexLock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);
    if (!aws_sdk_tcl_dynamodb_ModuleInitialized) {
        Aws::SDKOptions options;
        Aws::InitAPI(options);
        Tcl_InitHashTable(&aws_sdk_tcl_dynamodb_NameToInternal_HT, TCL_STRING_KEYS);
        Tcl_CreateThreadExitHandler(aws_sdk_tcl_dynamodb_ExitHandler, nullptr);
        aws_sdk_tcl_dynamodb_ModuleInitialized = 1;
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);
}

int aws_sdk_tcl_dynamodb_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.6", 0) == nullptr) {
        return TCL_ERROR;
    }

    aws_sdk_tcl_dynamodb_InitModule();

    Tcl_CreateNamespace(interp, "::aws::s3", nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::create", aws_sdk_tcl_dynamodb_CreateCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::destroy", aws_sdk_tcl_dynamodb_DestroyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::s3::putItem", aws_sdk_tcl_dynamodb_PutItemCmd, nullptr, nullptr);

    return Tcl_PkgProvide(interp, "aws_sdk_tcl_s3", "0.1");
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Tbert_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
