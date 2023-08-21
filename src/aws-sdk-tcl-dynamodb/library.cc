/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2023 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#include <iostream>
#include <aws/core/Aws.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/GetItemRequest.h>
#include <aws/dynamodb/model/QueryRequest.h>
#include <aws/dynamodb/model/ScanRequest.h>
#include <aws/dynamodb/model/CreateTableRequest.h>
#include <aws/dynamodb/model/DeleteTableRequest.h>
#include <aws/dynamodb/model/ListTablesRequest.h>
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

static char dynamodb_client_usage[] =
        "Usage dynamodbClient <method> <args>, where method can be:\n"
        "  put_item table item_dict                                                                             \n"
        "  get_item table key_dict                                                                              \n"
        "  query_items table query_dict ?scan_forward? ?limit? ?index_name?                                     \n"
        "  create_table table key_schema_dict ?provisioned_throughput_dict? ?global_secondary_indexes_list?     \n"
        "  delete_table table                                                                                   \n"
        "  list_tables                                                                                          \n"
        ;


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

Tcl_Obj* get_typed_obj_from_attribute_value(Tcl_Interp *interp, const Aws::DynamoDB::Model::AttributeValue& attribute_value);

Tcl_Obj* get_typed_obj_from_map(Tcl_Interp *interp, std::map<std::string, const std::shared_ptr<Aws::DynamoDB::Model::AttributeValue>> map_attr_value) {
    Tcl_Obj *dictPtr = Tcl_NewDictObj();
    for (auto const& x : map_attr_value) {
        Tcl_Obj *keyPtr = Tcl_NewStringObj(x.first.c_str(), -1);
        Tcl_Obj *valuePtr = get_typed_obj_from_attribute_value(interp, *x.second);
        Tcl_DictObjPut(interp, dictPtr, keyPtr, valuePtr);
    }
    return dictPtr;
}

Tcl_Obj* get_typed_obj_from_list(Tcl_Interp *interp, std::vector<std::shared_ptr<Aws::DynamoDB::Model::AttributeValue>> list_attr_value) {
    Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
    for (auto const& x : list_attr_value) {
        Tcl_Obj *valuePtr = get_typed_obj_from_attribute_value(interp,*x);
        Tcl_ListObjAppendElement(interp, listPtr, valuePtr);
    }
    return listPtr;
}

Tcl_Obj* get_typed_obj_from_attribute_value(Tcl_Interp *interp, const Aws::DynamoDB::Model::AttributeValue& attribute_value) {
    Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
    switch (attribute_value.GetType()) {
        case Aws::DynamoDB::Model::ValueType::NUMBER:
            Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj("N", -1));
            Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj(attribute_value.GetN().c_str(), -1));
            break;
        case Aws::DynamoDB::Model::ValueType::STRING:
            Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj("S", -1));
            Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj(attribute_value.GetS().c_str(), -1));
            break;
        case Aws::DynamoDB::Model::ValueType::BOOL:
            Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj("BOOL", -1));
            Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewBooleanObj(attribute_value.GetBool()));
            break;
        case Aws::DynamoDB::Model::ValueType::ATTRIBUTE_MAP:
            Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj("M", -1));
            Tcl_ListObjAppendElement(interp, listPtr, get_typed_obj_from_map(interp, attribute_value.GetM()));
            break;
        case Aws::DynamoDB::Model::ValueType::ATTRIBUTE_LIST:
            Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj("L", -1));
            Tcl_ListObjAppendElement(interp, listPtr, get_typed_obj_from_list(interp, attribute_value.GetL()));
            break;
        default:
            Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj("__unknown__", -1));
            Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj("", -1));
            break;
    }
    return listPtr;
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
                               get_typed_obj_from_attribute_value(interp, i.second));
            }
        }
        Tcl_SetObjResult(interp, result);
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_dynamodb_QueryItems(
        Tcl_Interp *interp,
        const char *handle,
        const char *tableName,
        Tcl_Obj *dictPtr,
        Tcl_Obj *scanForwardPtr,
        Tcl_Obj *limitPtr,
        Tcl_Obj *indexNamePtr
        ) {

    DBG(fprintf(stderr, "aws_sdk_tcl_dynamodb_QueryItems: handle=%s tableName=%s dict=%s\n", handle, tableName,
                Tcl_GetString(dictPtr)));
    Aws::DynamoDB::DynamoDBClient *client = aws_sdk_tcl_dynamodb_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::DynamoDB::Model::QueryRequest request;
    request.SetTableName(tableName);
    if (scanForwardPtr) {
        int scanForward;
        Tcl_GetBooleanFromObj(interp, scanForwardPtr, &scanForward);
        request.SetScanIndexForward(scanForward);
    }
    int limit = 0;
    if (limitPtr) {
        Tcl_GetIntFromObj(interp, limitPtr, &limit);
        request.SetLimit(limit);
    }
    if (indexNamePtr) {
        request.SetIndexName(Tcl_GetString(indexNamePtr));
    }

    Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> attributeValues;
    Aws::String keyConditionExpression;

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
        attributeValues.emplace(":" + attribute_key, *value);

        if (keyConditionExpression.length() > 0) {
            keyConditionExpression.append(" AND ");
        }
        keyConditionExpression.append(attribute_key);
        keyConditionExpression.append(" = :");
        keyConditionExpression.append(attribute_key);
    }
    Tcl_DictObjDone(&search);

    request.SetKeyConditionExpression(keyConditionExpression);
    request.SetExpressionAttributeValues(attributeValues);

    Tcl_Obj *resultListPtr = Tcl_NewListObj(0, nullptr);
    int count = 0;

    // "exclusiveStartKey" is used for pagination.
    Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> exclusiveStartKey;
    do {
        if (!exclusiveStartKey.empty()) {
            request.SetExclusiveStartKey(exclusiveStartKey);
            exclusiveStartKey.clear();
        }
        // Perform Query operation.
        const Aws::DynamoDB::Model::QueryOutcome &outcome = client->Query(request);
        if (outcome.IsSuccess()) {
            // Reference the retrieved items.
            const Aws::Vector<Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>> &items = outcome.GetResult().GetItems();
            if (!items.empty()) {
                for (const auto &item: items) {
                    // Output each retrieved field and its value.
                    Tcl_Obj *itemDictPtr = Tcl_NewDictObj();
                    for (const auto &i: item) {
                        Tcl_DictObjPut(interp, itemDictPtr, Tcl_NewStringObj(i.first.c_str(), -1),
                                       get_typed_obj_from_attribute_value(interp, i.second));
                        count++;
                    }
                    Tcl_ListObjAppendElement(interp, resultListPtr, itemDictPtr);
                }
            }

            // If LastEvaluatedKey presents in the output, it means there are more items
            exclusiveStartKey = outcome.GetResult().GetLastEvaluatedKey();
        }
        else {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
            return TCL_ERROR;
        }
    } while (!exclusiveStartKey.empty() && (limitPtr == nullptr || count < limit));

    Tcl_SetObjResult(interp, resultListPtr);
    return TCL_OK;
}

Aws::DynamoDB::Model::ScalarAttributeType get_attribute_type(const char *type) {
    switch(type[0]) {
        case 'S':
            return Aws::DynamoDB::Model::ScalarAttributeType::S;
        case 'N':
            return Aws::DynamoDB::Model::ScalarAttributeType::N;
        case 'B':
            return Aws::DynamoDB::Model::ScalarAttributeType::B;
        default:
            return Aws::DynamoDB::Model::ScalarAttributeType::S;
    }
}

Aws::DynamoDB::Model::KeyType get_key_type(const char *type) {
    switch(type[0]) {
        case 'H':
            return Aws::DynamoDB::Model::KeyType::HASH;
        case 'R':
            return Aws::DynamoDB::Model::KeyType::RANGE;
        default:
            return Aws::DynamoDB::Model::KeyType::HASH;
    }
}

int aws_sdk_tcl_dynamodb_Scan(
        Tcl_Interp *interp,
        const char *handle,
        const char *tableName,
        Tcl_Obj *projectionExpressionDictPtr
) {
    DBG(fprintf(stderr, "aws_sdk_tcl_dynamodb_Scan: handle=%s tableName=%s projection_expression_dict=%s\n", handle, tableName,
                Tcl_GetString(projectionExpressionDictPtr)));
    Aws::DynamoDB::DynamoDBClient *client = aws_sdk_tcl_dynamodb_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::DynamoDB::Model::ScanRequest request;
    request.SetTableName(tableName);


//    if (!projectionExpression.empty())
//        request.SetProjectionExpression(projectionExpression);

    // Perform scan on table.
    const Aws::DynamoDB::Model::ScanOutcome &outcome = client->Scan(request);
    Tcl_Obj *resultListPtr = Tcl_NewListObj(0, nullptr);
    if (outcome.IsSuccess()) {
        // Reference the retrieved items.
        const Aws::Vector<Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>> &items = outcome.GetResult().GetItems();
        if (!items.empty()) {
            for (const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> &itemMap: items) {
                Tcl_Obj *itemDictPtr = Tcl_NewDictObj();
                for (const auto &i: itemMap) {
                    Tcl_DictObjPut(interp, itemDictPtr, Tcl_NewStringObj(i.first.c_str(), -1),
                                   get_typed_obj_from_attribute_value(interp, i.second));
                }
                Tcl_ListObjAppendElement(interp, resultListPtr, itemDictPtr);
            }
        }
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, resultListPtr);
    return TCL_OK;

}

int aws_sdk_tcl_dynamodb_CreateTable(
        Tcl_Interp *interp,
        const char *handle,
        const char *tableName,
        Tcl_Obj *keySchemaDictPtr,
        Tcl_Obj *provisionedThroughputDictPtr,
        Tcl_Obj *globalSecondaryIndexesListPtr
        ) {
    DBG(fprintf(stderr, "aws_sdk_tcl_dynamodb_CreateTable: handle=%s tableName=%s dict=%s\n", handle, tableName,
                Tcl_GetString(keySchemaDictPtr)));
    Aws::DynamoDB::DynamoDBClient *client = aws_sdk_tcl_dynamodb_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::DynamoDB::Model::CreateTableRequest request;
    request.SetTableName(tableName);

    Tcl_DictSearch search;
    Tcl_Obj *key, *spec;
    int done;
    if (Tcl_DictObjFirst(interp, keySchemaDictPtr, &search,
                         &key, &spec, &done) != TCL_OK) {
        return TCL_ERROR;
    }
    for (; !done; Tcl_DictObjNext(&search, &key, &spec, &done)) {
        int length;
        Tcl_ListObjLength(interp, spec, &length);
        if (length != 1 && length != 2) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid key schema definition", -1));
            return TCL_ERROR;
        }
        Tcl_Obj *attrTypePtr;
        Tcl_ListObjIndex(interp, spec, 0, &attrTypePtr);

        Aws::String keyName = Tcl_GetString(key);
        Aws::DynamoDB::Model::AttributeDefinition hashKey;
        hashKey.SetAttributeName(keyName);
        hashKey.SetAttributeType(get_attribute_type(Tcl_GetString(attrTypePtr))); // Aws::DynamoDB::Model::ScalarAttributeType::S
        request.AddAttributeDefinitions(hashKey);

        if (length == 2) {
            Tcl_Obj *keyTypePtr;
            Tcl_ListObjIndex(interp, spec, 1, &keyTypePtr);
            Aws::DynamoDB::Model::KeySchemaElement keySchemaElement;
            keySchemaElement.WithAttributeName(keyName).WithKeyType(
                    get_key_type(Tcl_GetString(keyTypePtr))); // Aws::DynamoDB::Model::KeyType::HASH
            request.AddKeySchema(keySchemaElement);
        }
    }
    Tcl_DictObjDone(&search);

    // Provisioned or On-Demand
    Aws::DynamoDB::Model::BillingMode billingMode = Aws::DynamoDB::Model::BillingMode::PAY_PER_REQUEST;
    if (provisionedThroughputDictPtr) {
        int dictSize;
        if (TCL_ERROR == Tcl_DictObjSize(interp, provisionedThroughputDictPtr, &dictSize)) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid provisionedThroughput definition", -1));
            return TCL_ERROR;
        }
        if (dictSize > 0) {
            if (dictSize != 2) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid provisionedThroughput definition", -1));
                return TCL_ERROR;
            }
            Tcl_Obj *readCapacityUnitsPtr, *writeCapacityUnitsPtr;
            Tcl_DictObjGet(interp, provisionedThroughputDictPtr, Tcl_NewStringObj("ReadCapacityUnits", -1), &readCapacityUnitsPtr);
            Tcl_DictObjGet(interp, provisionedThroughputDictPtr, Tcl_NewStringObj("WriteCapacityUnits", -1), &writeCapacityUnitsPtr);
            if (readCapacityUnitsPtr == nullptr || writeCapacityUnitsPtr == nullptr) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid provisionedThroughput definition", -1));
                return TCL_ERROR;
            }
            int readCapacityUnits = 0, writeCapacityUnits = 0;
            Tcl_GetIntFromObj(interp, readCapacityUnitsPtr, &readCapacityUnits);
            Tcl_GetIntFromObj(interp, writeCapacityUnitsPtr, &writeCapacityUnits);
            Aws::DynamoDB::Model::ProvisionedThroughput throughput;
            throughput.WithReadCapacityUnits(readCapacityUnits).WithWriteCapacityUnits(writeCapacityUnits);
            request.SetProvisionedThroughput(throughput);
            billingMode = Aws::DynamoDB::Model::BillingMode::PROVISIONED;
        }
    }
    request.SetBillingMode(billingMode);

    if (globalSecondaryIndexesListPtr) {
        int listSize;
        if (TCL_ERROR == Tcl_ListObjLength(interp, globalSecondaryIndexesListPtr, &listSize)) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid globalSecondaryIndexes definition, must be list of dicts", -1));
            return TCL_ERROR;
        }
        if (listSize > 0) {
            std::vector<Aws::DynamoDB::Model::GlobalSecondaryIndex> globalSecondaryIndexes;
            for (int i = 0; i < listSize; i++) {
                Tcl_Obj *globalSecondaryIndexDictPtr;
                Tcl_ListObjIndex(interp, globalSecondaryIndexesListPtr, i, &globalSecondaryIndexDictPtr);
                int dictSize;
                if (TCL_ERROR == Tcl_DictObjSize(interp, globalSecondaryIndexDictPtr, &dictSize)) {
                    Tcl_SetObjResult(interp,
                                     Tcl_NewStringObj("Invalid globalSecondaryIndex definition, must be dict", -1));
                    return TCL_ERROR;
                }
                if (dictSize > 0) {
                    if (dictSize != 2) {
                        Tcl_SetObjResult(interp, Tcl_NewStringObj(
                                "Invalid globalSecondaryIndex definition, must be dict with IndexName and KeySchema",
                                -1));
                        return TCL_ERROR;
                    }
                    Tcl_Obj *indexNamePtr, *keySchemaPtr;
                    Tcl_DictObjGet(interp, globalSecondaryIndexDictPtr, Tcl_NewStringObj("IndexName", -1),
                                   &indexNamePtr);
                    Tcl_DictObjGet(interp, globalSecondaryIndexDictPtr, Tcl_NewStringObj("KeySchema", -1),
                                   &keySchemaPtr);
                    if (indexNamePtr == nullptr || keySchemaPtr == nullptr) {
                        Tcl_SetObjResult(interp, Tcl_NewStringObj(
                                "Invalid globalSecondaryIndex definition, must be dict with IndexName and KeySchema",
                                -1));
                        return TCL_ERROR;
                    }

                    Aws::String indexName = Tcl_GetString(indexNamePtr);
                    Aws::DynamoDB::Model::GlobalSecondaryIndex globalSecondaryIndex;
                    globalSecondaryIndex.SetIndexName(indexName);

                    // KeySchema
                    int keySchemaLength;
                    Tcl_ListObjLength(interp, keySchemaPtr, &keySchemaLength);
                    if (keySchemaLength == 0) {
                        Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid globalSecondaryIndexes definition", -1));
                        return TCL_ERROR;
                    }

                    std::vector<Aws::DynamoDB::Model::KeySchemaElement> keySchema;
                    for (int j = 0; j < keySchemaLength; j++) {
                        Tcl_Obj *keySchemaObjPtr;
                        Tcl_ListObjIndex(interp, keySchemaPtr, j, &keySchemaObjPtr);
                        int keySchemaObjLength;
                        Tcl_ListObjLength(interp, keySchemaObjPtr, &keySchemaObjLength);
                        if (keySchemaObjLength != 2) {
                            Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid globalSecondaryIndex definition, key schema element is a pair of the name of the key and its type", -1));
                            return TCL_ERROR;
                        }
                        Tcl_Obj *keyNamePtr, *keyTypePtr;
                        Tcl_ListObjIndex(interp, keySchemaObjPtr, 0, &keyNamePtr);
                        Tcl_ListObjIndex(interp, keySchemaObjPtr, 1, &keyTypePtr);

                        Aws::String keyName = Tcl_GetString(keyNamePtr);
                        Aws::DynamoDB::Model::KeySchemaElement keySchemaElement;
                        keySchemaElement.WithAttributeName(keyName).WithKeyType(
                                get_key_type(Tcl_GetString(keyTypePtr))); // Aws::DynamoDB::Model::KeyType::HASH
                        keySchema.push_back(keySchemaElement);
                    }
                    globalSecondaryIndex.SetKeySchema(keySchema);
                    globalSecondaryIndex.SetProjection(Aws::DynamoDB::Model::Projection().WithProjectionType(
                            Aws::DynamoDB::Model::ProjectionType::ALL));
                    globalSecondaryIndexes.push_back(globalSecondaryIndex);
                }
                request.SetGlobalSecondaryIndexes(globalSecondaryIndexes);
            }
        }
    }

    const Aws::DynamoDB::Model::CreateTableOutcome &outcome = client->CreateTable(request);

    if (outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(true));
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_dynamodb_DeleteTable(Tcl_Interp *interp, const char *handle, const char *tableName) {
    DBG(fprintf(stderr, "aws_sdk_tcl_dynamodb_DeleteTable: handle=%s tableName=%s\n", handle, tableName));
    Aws::DynamoDB::DynamoDBClient *client = aws_sdk_tcl_dynamodb_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::DynamoDB::Model::DeleteTableRequest request;
    request.SetTableName(tableName);

    auto outcome = client->DeleteTable(request);

    if (outcome.IsSuccess()) {
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(true));
        return TCL_OK;
    } else {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
        return TCL_ERROR;
    }
}

int aws_sdk_tcl_dynamodb_ListTables(Tcl_Interp *interp, const char *handle) {
    DBG(fprintf(stderr, "aws_sdk_tcl_dynamodb_ListTables: handle=%s\n", handle));
    Aws::DynamoDB::DynamoDBClient *client = aws_sdk_tcl_dynamodb_GetInternalFromName(handle);
    if (!client) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("handle not found", -1));
        return TCL_ERROR;
    }

    Aws::DynamoDB::Model::ListTablesRequest listTablesRequest;
    listTablesRequest.SetLimit(50);
    Tcl_Obj *listPtr = Tcl_NewListObj(0, nullptr);
    do {
        const Aws::DynamoDB::Model::ListTablesOutcome &outcome = client->ListTables(listTablesRequest);
        if (!outcome.IsSuccess()) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(outcome.GetError().GetMessage().c_str(), -1));
            return TCL_ERROR;
        }

        for (const auto &tableName: outcome.GetResult().GetTableNames()) {
            Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewStringObj(tableName.c_str(), -1));
        }
        listTablesRequest.SetExclusiveStartTableName(
                outcome.GetResult().GetLastEvaluatedTableName());

    } while (!listTablesRequest.GetExclusiveStartTableName().empty());
    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

Tcl_Obj *get_simple_obj_from_typed(Tcl_Interp *interp, Tcl_Obj *spec);

Tcl_Obj *get_simple_obj_from_typed_map(Tcl_Interp *interp, Tcl_Obj *attrValuePtr) {
    Tcl_Obj *dictPtr = Tcl_NewDictObj();
    Tcl_DictSearch search;
    Tcl_Obj *key, *spec;
    int done;
    if (Tcl_DictObjFirst(interp, attrValuePtr, &search,
                         &key, &spec, &done) != TCL_OK) {
        return nullptr;
    }
    for (; !done; Tcl_DictObjNext(&search, &key, &spec, &done)) {
        int length;
        Tcl_ListObjLength(interp, spec, &length);
        if (length != 2) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid attribute value", -1));
            return nullptr;
        }
        Tcl_DictObjPut(interp, dictPtr, key, get_simple_obj_from_typed(interp, spec));
    }
    Tcl_DictObjDone(&search);
    return dictPtr;
}

Tcl_Obj *get_simple_obj_from_typed_list(Tcl_Interp *interp, Tcl_Obj *attrValuePtr) {
    Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
    int length;
    Tcl_ListObjLength(interp, attrValuePtr, &length);
    for (int i = 0; i < length; i++) {
        Tcl_Obj *valuePtr;
        Tcl_ListObjIndex(interp, attrValuePtr, i, &valuePtr);
        Tcl_ListObjAppendElement(interp, listPtr, get_simple_obj_from_typed(interp, valuePtr));
    }
    return listPtr;
}

Tcl_Obj *get_simple_obj_from_typed(Tcl_Interp *interp, Tcl_Obj *spec) {
    Tcl_Obj *attrTypePtr;
    Tcl_ListObjIndex(interp, spec, 0, &attrTypePtr);
    Tcl_Obj *attrValuePtr;
    Tcl_ListObjIndex(interp, spec, 1, &attrValuePtr);
    int typeLength;
    const char *type = Tcl_GetStringFromObj(attrTypePtr, &typeLength);
    switch (type[0]) {
        case 'S':
            return Tcl_NewStringObj(Tcl_GetString(attrValuePtr), -1);
        case 'N':
            int intValue;
            Tcl_GetIntFromObj(interp, attrValuePtr, &intValue);
            return Tcl_NewIntObj(intValue);
        case 'B':
            if (typeLength == 4 && 0 == strcmp(type, "BOOL")) {
                int flag;
                Tcl_GetBooleanFromObj(interp, attrValuePtr, &flag);
                return Tcl_NewBooleanObj(flag);
            } else {
                int bytelen;
                const unsigned char *bytes = Tcl_GetByteArrayFromObj(attrValuePtr, &bytelen);
                return Tcl_NewByteArrayObj(bytes, bytelen);
            }
        case 'M':
            return get_simple_obj_from_typed_map(interp, attrValuePtr);
        case 'L':
            return get_simple_obj_from_typed_list(interp, attrValuePtr);
        default:
            return nullptr;
    }

}

int aws_sdk_tcl_dynamodb_TypedItemToSimple(Tcl_Interp *interp, Tcl_Obj *dictPtr) {
    Tcl_Obj *resultDictPtr = Tcl_NewDictObj();
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
        Tcl_Obj *valuePtr = get_simple_obj_from_typed(interp, spec);
        if (valuePtr == nullptr) {
            Tcl_DictObjDone(&search);
            Tcl_SetObjResult(interp, Tcl_NewStringObj("Invalid attribute value", -1));
            return TCL_ERROR;
        }
        Tcl_DictObjPut(interp, resultDictPtr, key, valuePtr);
    }
    Tcl_DictObjDone(&search);
    Tcl_SetObjResult(interp, resultDictPtr);
    return TCL_OK;
}

int aws_sdk_tcl_dynamodb_ClientObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    static const char *clientMethods[] = {
            "destroy",
            "put_item",
            "get_item",
            "query_items",
            "scan",
            "update_item",
            "delete_item",
            "create_table",
            "delete_table",
            "list_tables",
            nullptr
    };

    enum clientMethod {
        m_destroy,
        m_putItem,
        m_getItem,
        m_queryItems,
        m_scan,
        m_updateItem,
        m_deleteItem,
        m_createTable,
        m_deleteTable,
        m_listTables
    };

    if (objc < 2) {
        Tcl_ResetResult(interp);
        Tcl_SetStringObj(Tcl_GetObjResult(interp), (dynamodb_client_usage), -1);
        return TCL_ERROR;
    }
    Tcl_ResetResult(interp);

    int methodIndex;
    if (TCL_OK == Tcl_GetIndexFromObj(interp, objv[1], clientMethods, "method", 0, &methodIndex)) {
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
            case m_queryItems:
                CheckArgs(4, 7, 1, "get_item table query_dict ?scan_forward? ?limit? ?index_name?");
                return aws_sdk_tcl_dynamodb_QueryItems(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        objv[3],
                        objc > 4 ? objv[4] : nullptr,
                        objc > 5 ? objv[5] : nullptr,
                        objc > 6 ? objv[6] : nullptr
                );
            case m_scan:
                CheckArgs(3, 4, 1, "scan table ?projection_expression_dict?");
                return aws_sdk_tcl_dynamodb_Scan(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        objc > 3 ? objv[3] : nullptr
                );
            case m_updateItem:
                break;
            case m_deleteItem:
                break;
            case m_createTable:
                CheckArgs(4, 6, 1, "create_table table key_schema_dict ?provisioned_throughput_dict? ?global_secondary_indexes_list?");
                return aws_sdk_tcl_dynamodb_CreateTable(
                        interp,
                        handle,
                        Tcl_GetString(objv[2]),
                        objv[3],
                        objc > 4 ? objv[4] : nullptr,
                        objc > 5 ? objv[5] : nullptr
                );
            case m_deleteTable:
                CheckArgs(3, 3, 1, "delete_table table");
                return aws_sdk_tcl_dynamodb_DeleteTable(
                        interp,
                        handle,
                        Tcl_GetString(objv[2])
                );
            case m_listTables:
                CheckArgs(2, 2, 1, "list_tables");
                return aws_sdk_tcl_dynamodb_ListTables(
                        interp,
                        handle
                );
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj("Unknown method", -1));
    return TCL_ERROR;
}

static int aws_sdk_tcl_dynamodb_CreateCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
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

static int aws_sdk_tcl_dynamodb_QueryItemsCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "QueryItemsCmd\n"));
    CheckArgs(4, 7, 1, "handle_name table query_dict ?scan_forward? ?limit? ?index_name?");
    return aws_sdk_tcl_dynamodb_QueryItems(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            objv[3],
            objc > 4 ? objv[4] : nullptr,
            objc > 5 ? objv[5] : nullptr,
            objc > 6 ? objv[6] : nullptr
    );
}

static int aws_sdk_tcl_dynamodb_ScanCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CreateTableCmd\n"));
    CheckArgs(3, 4, 1, "handle_name table ?projection_expression?");
    return aws_sdk_tcl_dynamodb_Scan(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            objc > 3 ? objv[3] : nullptr
    );
}

static int aws_sdk_tcl_dynamodb_CreateTableCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "CreateTableCmd\n"));
    CheckArgs(4, 6, 1, "handle_name table key_schema_dict ?provisioned_throughput_dict? ?global_secondary_indexes_list?");
    return aws_sdk_tcl_dynamodb_CreateTable(
            interp,
            Tcl_GetString(objv[1]),
            Tcl_GetString(objv[2]),
            objv[3],
            objc > 4 ? objv[4] : nullptr,
            objc > 5 ? objv[5] : nullptr
            );
}

static int aws_sdk_tcl_dynamodb_DeleteTableCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "DeleteTableCmd\n"));
    CheckArgs(3, 3, 1, "handle_name table");
    return aws_sdk_tcl_dynamodb_DeleteTable(interp, Tcl_GetString(objv[1]), Tcl_GetString(objv[2]));
}

static int aws_sdk_tcl_dynamodb_ListTablesCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "ListTablesCmd\n"));
    CheckArgs(2, 2, 1, "handle_name");
    return aws_sdk_tcl_dynamodb_ListTables(interp, Tcl_GetString(objv[1]));
}

static int aws_sdk_tcl_dynamodb_TypedItemToSimpleCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    DBG(fprintf(stderr, "TypedItemToSimpleCmd\n"));
    CheckArgs(2, 2, 1, "item_dict");
    return aws_sdk_tcl_dynamodb_TypedItemToSimple(interp, objv[1]);
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
    Tcl_CreateObjCommand(interp, "::aws::dynamodb::query_items", aws_sdk_tcl_dynamodb_QueryItemsCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::dynamodb::scan", aws_sdk_tcl_dynamodb_ScanCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::dynamodb::create_table", aws_sdk_tcl_dynamodb_CreateTableCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::dynamodb::delete_table", aws_sdk_tcl_dynamodb_DeleteTableCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::dynamodb::list_tables", aws_sdk_tcl_dynamodb_ListTablesCmd, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "::aws::dynamodb::typed_item_to_simple", aws_sdk_tcl_dynamodb_TypedItemToSimpleCmd, nullptr, nullptr);

    return Tcl_PkgProvide(interp, "awsdynamodb", "0.1");
}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Aws_sdk_tcl_dynamodb_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif
