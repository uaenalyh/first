/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

/**
 * @addtogroup hwmgmt_configs
 *
 * @{
 */

 /**
 * @file
 * @brief This file defines MACROs of maximum number of physical CPU and path of root file-system.
 */

#define MAX_PCPU_NUM 4U /**< Maximum number of physical CPUs on the platform. */

#define ROOTFS_0            "root=/dev/sda3 " /**< Path of root file-system. */

#ifndef CONFIG_RELEASE
#define SOS_BOOTARGS_DIFF      \
	"hvlog=2M@0x1FE00000 " \
	"memmap=0x200000$0x1fe00000 "
#else
#endif

/**
 * @}
 */

#endif /* MISC_CFG_H */
