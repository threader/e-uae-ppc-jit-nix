/*
 * UAE - The Un*x Amiga Emulator
 *
 * PPAPI module and instance for running in Chrome.
 *
 * Copyright 2012, 2013 Christian Stefansen
 */

#ifndef SRC_GUI_HTML_PPAPI_H_
#define SRC_GUI_HTML_PPAPI_H_

#include <stddef.h>

#include "ppapi/c/ppb_instance.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t NaCl_LoadUrl(const char* name, char** data);
const void *NaCl_GetInterface(const char *interface_name);
PP_Instance NaCl_GetInstance(void);

#ifdef __cplusplus
}
#endif

#endif  // SRC_GUI_HTML_PPAPI_H_
