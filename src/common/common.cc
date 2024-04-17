#include "common.h"

std::tuple<int, Aws::Client::ClientConfiguration, std::shared_ptr<Aws::Auth::AWSCredentialsProvider>>
        get_client_config_and_credentials_provider(Tcl_Interp *interp, Tcl_Obj *const dict_ptr) {
    Aws::Client::ClientConfiguration clientConfig;
    Tcl_Obj *region;
    Tcl_Obj *region_key_ptr = Tcl_NewStringObj("region", -1);
    Tcl_IncrRefCount(region_key_ptr);
    if (TCL_OK != Tcl_DictObjGet(interp, dict_ptr, region_key_ptr, &region)) {
        Tcl_DecrRefCount(region_key_ptr);
        return {TCL_ERROR, nullptr, nullptr};
    }
    Tcl_DecrRefCount(region_key_ptr);
    Tcl_Obj *endpoint;
    Tcl_Obj *endpoint_key_ptr = Tcl_NewStringObj("endpoint", -1);
    Tcl_IncrRefCount(endpoint_key_ptr);
    if (TCL_OK != Tcl_DictObjGet(interp, dict_ptr, endpoint_key_ptr, &endpoint)) {
        Tcl_DecrRefCount(endpoint_key_ptr);
        return {TCL_ERROR, nullptr, nullptr};
    }
    Tcl_DecrRefCount(endpoint_key_ptr);
    Tcl_Obj *aws_access_key_id;
    Tcl_Obj *aws_access_key_id_key_ptr = Tcl_NewStringObj("aws_access_key_id", -1);
    Tcl_IncrRefCount(aws_access_key_id_key_ptr);
    if (TCL_OK != Tcl_DictObjGet(interp, dict_ptr, aws_access_key_id_key_ptr, &aws_access_key_id)) {
        Tcl_DecrRefCount(aws_access_key_id_key_ptr);
        return {TCL_ERROR, nullptr, nullptr};
    }
    Tcl_DecrRefCount(aws_access_key_id_key_ptr);
    Tcl_Obj *aws_secret_access_key;
    Tcl_Obj *aws_secret_access_key_key_ptr = Tcl_NewStringObj("aws_secret_access_key", -1);
    Tcl_IncrRefCount(aws_secret_access_key_key_ptr);
    if (TCL_OK != Tcl_DictObjGet(interp, dict_ptr, aws_secret_access_key_key_ptr, &aws_secret_access_key)) {
        Tcl_DecrRefCount(aws_secret_access_key_key_ptr);
        return {TCL_ERROR, nullptr, nullptr};
    }
    Tcl_DecrRefCount(aws_secret_access_key_key_ptr);
    Tcl_Obj *aws_session_token;
    Tcl_Obj *aws_session_token_key_ptr = Tcl_NewStringObj("aws_session_token", -1);
    Tcl_IncrRefCount(aws_session_token_key_ptr);
    if (TCL_OK != Tcl_DictObjGet(interp, dict_ptr, aws_session_token_key_ptr, &aws_session_token)) {
        Tcl_DecrRefCount(aws_session_token_key_ptr);
        return {TCL_ERROR, nullptr, nullptr};
    }
    Tcl_DecrRefCount(aws_session_token_key_ptr);
    if (region) {
        clientConfig.region = Tcl_GetString(region);
    }
    if (endpoint) {
        clientConfig.endpointOverride = Tcl_GetString(endpoint);
    }
    std::shared_ptr<Aws::Auth::AWSCredentialsProvider> credentials_provider_ptr = nullptr;
    if (aws_access_key_id && aws_secret_access_key) {
        Aws::Auth::AWSCredentials credentials = Aws::Auth::AWSCredentials(
                Tcl_GetString(aws_access_key_id),
                Tcl_GetString(aws_secret_access_key),
                aws_session_token ? Tcl_GetString(aws_session_token) : ""
        );

        credentials_provider_ptr = std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(credentials);
    }
    return {TCL_OK, clientConfig, credentials_provider_ptr};
}


char *aws_sdk_strndup(const char *s, size_t n) {
    if (s == NULL) {
        return NULL;
    }
    size_t l = strnlen(s, n);
    char *result = (char *) Tcl_Alloc(l + 1);
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, s, l);
    result[l] = '\0';
    return result;
}
