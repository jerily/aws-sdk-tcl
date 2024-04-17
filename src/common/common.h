#ifndef COMMON_H
#define COMMON_H

#include <tcl.h>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/client/ClientConfiguration.h>

#define SetResult(str) Tcl_ResetResult(interp); \
                     Tcl_SetStringObj(Tcl_GetObjResult(interp), (str), -1)

std::tuple<int, Aws::Client::ClientConfiguration, std::shared_ptr<Aws::Auth::AWSCredentialsProvider>>
        get_client_config_and_credentials_provider(Tcl_Interp *interp, Tcl_Obj *dict_ptr);

char *aws_sdk_strndup(const char *s, size_t n);

#endif // COMMON_H