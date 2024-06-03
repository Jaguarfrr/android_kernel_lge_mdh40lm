/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __TC1_PARTITION_H__
#define __TC1_PARTITION_H__

#include "lg_partition.h"

#define TC1_GET_NAME(fname)	("LGE_"#fname)
#define TC1_FAC_NAME(fname)	LGE_##fname


#define TC1_FAC_IMEI_LEN		LGE_FAC_IMEI_LEN
#define TC1_FAC_NETWORK_CODE_LEN	LGE_FAC_NETWORK_CODE_LEN

#endif
