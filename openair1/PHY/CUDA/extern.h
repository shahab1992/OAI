#ifndef __MODULATION_EXTERN_CU_H__
#define __MODULATION_EXTERN_CU_H_

#include "defs_struct.h"

#include <stdint.h>
#include <stdio.h>

#ifndef CUFFT_H
#define CUFFT_H
#include "cufft.h"
#endif
extern dl_cu_t dl_cu[10];
extern ul_cu_t ul_cu[10];
extern int device_count;
#endif
