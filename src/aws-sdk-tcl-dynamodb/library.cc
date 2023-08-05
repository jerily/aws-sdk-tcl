#include <iostream>
#include <aws/core/Aws.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/GetItemRequest.h>
#include <cstdio>
#include <fstream>
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

#define CMD_NAME(s, internal) std::sprintf((s), "_AWS_DDB_%p", (internal))

static Tcl_HashTable aws_sdk_tcl_dynamodb_NameToInternal_HT;
static Tcl_Mutex aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex;
static int aws_sdk_tcl_dynamodb_ModuleInitialized;

static char s3_client_usage[] =
        "Usage s3Client <method> <args>, where method can be:\n"
        "   ls bucket ?key?                 \n"
        "   put_text bucket key text        \n"
        "   put bucket key input_file       \n"
        "   get bucket key ?output_file?    \n"
        "   delete bucket key               \n";


static int
aws_sdk_tcl_dynamodb_RegisterName(const char *name, Aws::DynamoDB::DynamoDBClient *internal) {

    Tcl_HashEntry *entryPtr;
    int newEntry;

    Tcl_MutexLock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);
    entryPtr = Tcl_CreateHashEntry(&aws_sdk_tcl_dynamodb_NameToInternal_HT, (char *) name, &newEntry);
    if (newEntry) {
        Tcl_SetHashValue(entryPtr, (ClientData) internal);
    }
    Tcl_MutexUnlock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);

    DBG(fprintf(stderr, "--> RegisterName: name=%s internal=%p %s\n", name, internal,
                newEntry ? "entered into" : "already in"));

    return !!newEntry;
}

static int
aws_sdk_tcl_dynamodb_UnregisterName(const char *name) {

    Tcl_HashEntry *entryPtr;
    int newEntry;

    Tcl_MutexLock(&aws_sdk_tcl_dynamodb_NameToInternal_HT_Mutex);
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_dynamodb_NameToInternal_HT, (char *) name);
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
    entryPtr = Tcl_FindHashEntry(&aws_sdk_tcl_dynamodb_NameToInternal_HT, (char *) name);
    if (entryPtr != nullptr) {
        internal = (Aws::DynamoDB::DynamoDBClient *) Tcl_GetHashValue(entryPtr);
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

std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> set_attribute_value(Tcl_Interp *interp, Tcl_Obj *listPtr);

std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> set_attribute_value_to_list(Tcl_Interp *interp, Tcl_Obj *listPtr) {
    std::vector<std::shared_ptr<Aws::DynamoDB::Model::AttributeValue>> list_attr_value;
    int length;
    Tcl_ListObjLength(interp, listPtr, &length);
    DBG(fprintf(stderr, "set_attribute_value_to_list, length: %d\n", length));
    for (int i = 0; i < length; i++) {
        Tcl_Obj *list_valuePtr;
        Tcl_ListObjIndex(interp, listPtr, i, &list_valuePtr);
        auto list_attrValue = set_attribute_value(interp, list_valuePtr);
        list_attr_value.push_back(list_attrValue);
    }
    DBG(fprintf(stderr, "set_attribute_value_to_list, size: %lu\n", list_attr_value.size()));
    return std::make_shared<Aws::DynamoDB::Model::AttributeValue>(Aws::DynamoDB::Model::AttributeValue().SetL(list_attr_value));
}

std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> set_attribute_value_to_map(Tcl_Interp *interp, Tcl_Obj *listPtr) {
    std::map<std::string, const std::shared_ptr<Aws::DynamoDB::Model::AttributeValue>> map_attr_value;
    int length;
    Tcl_ListObjLength(interp, listPtr, &length);
    DBG(fprintf(stderr, "set_attribute_value_to_map, length: %d\n", length));
    for (int i = 0; i < length; i+=2) {
        Tcl_Obj *list_keyPtr, *list_valuePtr;
        Tcl_ListObjIndex(interp, listPtr, i, &list_keyPtr);
        Tcl_ListObjIndex(interp, listPtr, i+1, &list_valuePtr);
        auto map_attrValue = set_attribute_value(interp, list_valuePtr);

        auto result = map_attr_value.insert(std::make_pair(Tcl_GetString(list_keyPtr), map_attrValue));

        // result variable holds a pair, where the first element is an iterator to the inserted element
        // and the second element is a bool indicating whether insertion took place
        if (!result.second) {
            DBG(fprintf(stderr, "Duplicate key in map\n"));
        }
    }
    DBG(fprintf(stderr, "set_attribute_value_to_map, map size: %lu\n", map_attr_value.size()));
    return std::make_shared<Aws::DynamoDB::Model::AttributeValue>(Aws::DynamoDB::Model::AttributeValue().SetM(map_attr_value));
}

std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> set_attribute_value(Tcl_Interp *interp, Tcl_Obj *listPtr) {
    Tcl_Obj *typePtr, *valuePtr;
    Tcl_ListObjIndex(interp, listPtr, 0, &typePtr);
    Tcl_ListObjIndex(interp, listPtr, 1, &valuePtr);
    int typeLength;
    const char *type = Tcl_GetStringFromObj(typePtr, &typeLength);
    switch(type[0]) {
        case 'S':
            return std::make_shared<Aws::DynamoDB::Model::AttributeValue>(Aws::DynamoDB::Model::AttributeValue().SetS(Tcl_GetString(valuePtr)));
        case 'N':
            if (typeLength == 1) {
                return std::make_shared<Aws::DynamoDB::Model::AttributeValue>(Aws::DynamoDB::Model::AttributeValue().SetN(Tcl_GetString(valuePtr)));
            } else if (0 == strcmp("NULL", type)) {
                return std::make_shared<Aws::DynamoDB::Model::AttributeValue>(Aws::DynamoDB::Model::AttributeValue().SetNull(true));
            }
        case 'B':
            if (typeLength == 1) {
//                return Aws::DynamoDB::Model::AttributeValue().SetB(Tcl_GetString(valuePtr));
            } else if (0 == strcmp("BOOL", type)) {
                return std::make_shared<Aws::DynamoDB::Model::AttributeValue>(Aws::DynamoDB::Model::AttributeValue().SetBool(Tcl_GetString(valuePtr)));
            }
        case 'M':
            return set_attribute_value_to_map(interp, valuePtr);
        case 'L':
            return set_attribute_value_to_list(interp, valuePtr);
    }
    return std::make_shared<Aws::DynamoDB::Model::AttributeValue>(Aws::DynamoDB::Model::AttributeValue().SetNull(true));
}

int aws_sdk_tcl_dynamodb_PutItem(Tcl_Interp *interp, const char *handle, const char *tableName, Tcl_Obj *dictPtr) {
    DBG(fprintf(stderr, "aws_sdk_tcl_dynamodb_PutItem: handle=%s tableName=%s dict=%s\n", handle, tableName,
                Tcl_GetString(dictPtr)));
    Aws::DynamoDB::DynamoDBClient *client = aws_sdk_tcl_dynamodb_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::DynamoDB::Model::PutItemRequest putItemRequest;
    putItemRequest.SetTableName(tableName);

    Tcl_DictSearch search;
    Tcl_Obj *key, *spec;
    int done;
    if (Tcl_DictObjFirst(interp, dictPtr, &search,
                         &key, &spec, &done) != TCL_OK) {
        return TCL_ERROR;
    }
    for (; !done; Tcl_DictObjNext(&search, &key, &spec, &done)) {
        int length;
        Tcl_ListObjLength(interp, spec, &length);
        if (length != 2) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid attribute value", -1));
            return TCL_ERROR;
        }
        Aws::String attribute_key = Tcl_GetString(key);
        DBG(fprintf(stderr, "key=%s spec=%s\n", attribute_key.c_str(), Tcl_GetString(spec)));
        auto value = set_attribute_value(interp, spec);
        DBG(fprintf(stderr, "done\n"));
        putItemRequest.AddItem(attribute_key, *value);
    }
    Tcl_DictObjDone(&search);

    DBG(fprintf(stderr, "putItemRequest ready\n"));

    const Aws::DynamoDB::Model::PutItemOutcome outcome = client->PutItem(
            putItemRequest);
    if (outcome.IsSuccess()) {
//        std::cout << "Successfully added Item!" << std::endl;
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(true));
        return TCL_OK;
    } else {
//        std::cerr << outcome.GetError().GetMessage() << std::endl;
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

Tcl_Obj* get_dict_obj_from_attribute_value(Aws::DynamoDB::Model::AttributeValue attribute_value);

Tcl_Obj* get_dict_obj_from_map(std::map<std::string, const std::shared_ptr<Aws::DynamoDB::Model::AttributeValue>> map_attr_value) {
    Tcl_Obj *dictPtr = Tcl_NewDictObj();
    for (auto const& x : map_attr_value) {
        Tcl_Obj *keyPtr = Tcl_NewStringObj(x.first.c_str(), -1);
        Tcl_Obj *valuePtr = get_dict_obj_from_attribute_value(*x.second);
        Tcl_DictObjPut(NULL, dictPtr, keyPtr, valuePtr);
    }
    return dictPtr;
}

Tcl_Obj* get_dict_obj_from_list(std::vector<std::shared_ptr<Aws::DynamoDB::Model::AttributeValue>> list_attr_value) {
    Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
    for (auto const& x : list_attr_value) {
        Tcl_Obj *valuePtr = get_dict_obj_from_attribute_value(*x);
        Tcl_ListObjAppendElement(NULL, listPtr, valuePtr);
    }
    return listPtr;
}

Tcl_Obj* get_dict_obj_from_attribute_value(Aws::DynamoDB::Model::AttributeValue attribute_value) {
    switch (attribute_value.GetType()) {
        case Aws::DynamoDB::Model::ValueType::NUMBER:
            return Tcl_NewStringObj(attribute_value.GetN().c_str(), -1);
        case Aws::DynamoDB::Model::ValueType::STRING:
            return Tcl_NewStringObj(attribute_value.GetS().c_str(), -1);
        case Aws::DynamoDB::Model::ValueType::BOOL:
            return Tcl_NewBooleanObj(attribute_value.GetBool());
        case Aws::DynamoDB::Model::ValueType::ATTRIBUTE_MAP:
            return get_dict_obj_from_map(attribute_value.GetM());
        case Aws::DynamoDB::Model::ValueType::ATTRIBUTE_LIST:
            return get_dict_obj_from_list(attribute_value.GetL());
        default:
            return Tcl_NewStringObj("__unknown__", -1);
    }
}

int aws_sdk_tcl_dynamodb_GetItem(Tcl_Interp *interp, const char *handle, const char *tableName, Tcl_Obj *dictPtr) {
    DBG(fprintf(stderr, "aws_sdk_tcl_dynamodb_GetItem: handle=%s tableName=%s dict=%s\n", handle, tableName,
                Tcl_GetString(dictPtr)));
    Aws::DynamoDB::DynamoDBClient *client = aws_sdk_tcl_dynamodb_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::DynamoDB::Model::GetItemRequest getItemRequest;
    getItemRequest.SetTableName(tableName);

    Tcl_DictSearch search;
    Tcl_Obj *key, *spec;
    int done;
    if (Tcl_DictObjFirst(interp, dictPtr, &search,
                         &key, &spec, &done) != TCL_OK) {
        return TCL_ERROR;
    }
    for (; !done; Tcl_DictObjNext(&search, &key, &spec, &done)) {
        int length;
        Tcl_ListObjLength(interp, spec, &length);
        if (length != 2) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid attribute value", -1));
            return TCL_ERROR;
        }
        Aws::String attribute_key = Tcl_GetString(key);
        DBG(fprintf(stderr, "key=%s spec=%s\n", attribute_key.c_str(), Tcl_GetString(spec)));
        auto value = set_attribute_value(interp, spec);
        DBG(fprintf(stderr, "done\n"));
        getItemRequest.AddKey(attribute_key, *value);
    }
    Tcl_DictObjDone(&search);

    DBG(fprintf(stderr, "getItemRequest ready\n"));

    const Aws::DynamoDB::Model::GetItemOutcome outcome = client->GetItem(
            getItemRequest);
    if (outcome.IsSuccess()) {
        Tcl_Obj *result = Tcl_NewDictObj();
        // Reference the retrieved fields/values.
        const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> &item = outcome.GetResult().GetItem();
        if (!item.empty()) {
            // Output each retrieved field and its value.
            for (const auto &i: item) {
//                std::cout << "Values: " << i.first << ": " << i.second.GetS()<< std::endl;
                Tcl_DictObjPut(interp, result, Tcl_NewStringObj(i.first.c_str(), -1),
                               get_dict_obj_from_attribute_value(i.second));
            }
        }
        Tcl_SetObjResult(interp, result);
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_dynamodb_ClientObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
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
        switch ((enum clientMethod) methodIndex) {
            case m_destroy:
                return aws_sdk_tcl_dynamodb_Destroy(interp, handle);
            case m_putItem:
                CheckArgs(4, 4, 1, "put_item table item_dict");
                return aws_sdk_tcl_dynamodb_PutItem(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        objv[3]
                );
            case m_getItem:
                CheckArgs(4, 4, 1, "get_item table key_dict");
                return aws_sdk_tcl_dynamodb_GetItem(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        objv[3]
                );
            case m_updateItem:
                break;
            case m_deleteItem:
                break;
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj("Unknown method", -1));
    return TCL_ERROR;
}

static int aws_sdk_tcl_dynamodb_CreateCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CreateCmd\n"));

    CheckArgs(2, 2, 1, "config_dict");

    Aws::Client::ClientConfiguration clientConfig;
    Tcl_Obj *profile;
    Tcl_Obj *region;
    Tcl_Obj *endpoint;
    Tcl_DictObjGet(interp, objv[1], Tcl_NewStringObj("profile", -1), &profile);
    Tcl_DictObjGet(interp, objv[1], Tcl_NewStringObj("region", -1), &region);
    Tcl_DictObjGet(interp, objv[1], Tcl_NewStringObj("endpoint", -1), &endpoint);
    if (profile) {
        clientConfig.profileName = Tcl_GetString(profile);
    }
    if (region) {
        clientConfig.region = Tcl_GetString(region);
    }
    if (endpoint) {
        clientConfig.endpointOverride = Tcl_GetString(endpoint);
    }

    auto *client = new Aws::DynamoDB::DynamoDBClient(clientConfig);
    char handle[80];
    CMD_NAME(handle, client);
    aws_sdk_tcl_dynamodb_RegisterName(handle, client);

    Tcl_CreateObjCommand(interp, handle,
                         (Tcl_ObjCmdProc *) aws_sdk_tcl_dynamodb_ClientObjCmd,
                         nullptr,
                         nullptr);
//                                 (Tcl_CmdDeleteProc*) aws_sdk_tcl_dynamodb_clientObjCmdDeleteProc);

    Tcl_SetObjResult(interp, Tcl_NewStringObj(handle, -1));
    return TCL_OK;
}

static int aws_sdk_tcl_dynamodb_DestroyCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "DestroyCmd\n"));
    CheckArgs(2, 2, 1, "handle");
    return aws_sdk_tcl_dynamodb_Destroy(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_dynamodb_PutItemCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "PutItemCmd\n"));
    CheckArgs(4, 4, 1, "handle_name table item_dict");
    return aws_sdk_tcl_dynamodb_PutItem(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), objv[3]);
}

static int aws_sdk_tcl_dynamodb_GetItemCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "GetItemCmd\n"));
    CheckArgs(4, 4, 1, "handle_name table key_dict");
    return aws_sdk_tcl_dynamodb_GetItem(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]), objv[3]);
}

static void aws_sdk_tcl_dynamodb_ExitHandler(ClientData unused) {
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

int Aws_sdk_tcl_dynamodb_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.6", 0) == nullptr) {
        return TCL_ERROR;
    }

    aws_sdk_tcl_dynamodb_InitModule();

    Tcl_CreateNamespace(interp, "::aws::dynamodb", nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::dynamodb::create", aws_sdk_tcl_dynamodb_CreateCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::dynamodb::destroy", aws_sdk_tcl_dynamodb_DestroyCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::dynamodb::put_item", aws_sdk_tcl_dynamodb_PutItemCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::dynamodb::get_item", aws_sdk_tcl_dynamodb_GetItemCmd, nullptr, nullptr);

    return Tcl_PkgProvide(interp, "aws_sdk_tcl_dynamodb", "0.1");
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Aws_sdk_tcl_dynamodb_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
