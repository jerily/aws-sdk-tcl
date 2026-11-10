#ifndef AWS_SDK_TCL_SES_LIBRARY_H
#define AWS_SDK_TCL_SES_LIBRARY_H

#ifdef USE_NAVISERVER
#include "ns.h"
#else
#include <tcl.h>
#endif

extern "C" {
DLLEXPORT int Aws_sdk_tcl_ses_Init(Tcl_Interp *interp);
#ifdef USE_NAVISERVER
NS_EXTERN int Ns_ModuleVersion;
NS_EXTERN int Ns_ModuleInit(const char *server, const char *module);
#endif
}
#endif
