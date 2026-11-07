#pragma once
#include "snt_piperlite.h"
#ifdef __cplusplus
extern "C" {
#endif
int amy_packed_init(snt_piperlite_model *,const void *,size_t);
void amy_packed_unbind(void);
#ifdef __cplusplus
}
#endif
