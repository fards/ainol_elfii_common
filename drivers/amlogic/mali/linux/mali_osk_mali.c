/*
 * Copyright (C) 2010-2012 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_osk_mali.c
 * Implementation of the OS abstraction layer which is specific for the Mali kernel device driver
 */
#include <linux/kernel.h>
#include <asm/uaccess.h>

#include "mali_kernel_common.h" /* MALI_xxx macros */
#include "mali_osk.h"           /* kernel side OS functions */
#include "mali_uk_types.h"
#include "arch/config.h"        /* contains the configuration of the arch we are compiling for */
#ifdef CONFIG_ARCH_MESON6
#include "mali_platform.h"      /* mali_meson_is_revb() */
#endif

_mali_osk_errcode_t _mali_osk_resources_init( _mali_osk_resource_t **arch_config, u32 *num_resources )
{
#ifdef CONFIG_ARCH_MESON6
    if (mali_meson_is_revb()) {
        *num_resources = sizeof(arch_configuration_revb) / sizeof(arch_configuration_revb[0]);
        *arch_config = arch_configuration_revb;
    } else {
        *num_resources = sizeof(arch_configuration) / sizeof(arch_configuration[0]);
        *arch_config = arch_configuration;
    }
#else
    *num_resources = sizeof(arch_configuration) / sizeof(arch_configuration[0]);
    *arch_config = arch_configuration;
#endif
    return _MALI_OSK_ERR_OK;
}

void _mali_osk_resources_term( _mali_osk_resource_t **arch_config, u32 num_resources )
{
    /* Nothing to do */
}
