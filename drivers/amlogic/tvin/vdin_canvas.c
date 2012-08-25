/*
 * TVIN Canvas
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#if (defined CONFIG_ARCH_MESON6)
#include <mach/io.h>
#else
#include <mach/dmc.h>
#endif
#include <linux/amports/canvas.h>
#include <linux/amports/vframe.h>
#include <linux/mm.h>

#include "tvin_format_table.h"
#include "vdin.h"
#include "vdin_canvas.h"

const unsigned int vdin_canvas_ids[2][48] = {
    {
        12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29,
        30, 31, 32, 33, 34, 35,
        36, 37, 38, 39, 40, 41,
        42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53,
        54, 55, 56, 57, 58, 59,
    },
    {
        132, 133, 134, 135, 136, 137,
        138, 139, 140, 141, 142, 143,
        144, 145, 146, 147, 148, 149,
        150, 151, 152, 153, 154, 155,
        156, 157, 158, 159, 160, 161,
        162, 163, 164, 165, 166, 167,
        168, 169, 170, 171,	172, 173,
        174, 175, 176, 177,	178, 179,
    },
};

inline void vdin_canvas_init(struct vdin_dev_s *devp)
{
    int i, canvas_id;
    unsigned int canvas_addr;
    int canvas_max_w = VDIN_CANVAS_MAX_WIDTH << 1;
    int canvas_max_h = VDIN_CANVAS_MAX_HEIGH;
    devp->canvas_max_size = PAGE_ALIGN(canvas_max_w*canvas_max_h);
    devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size;
    if (devp->canvas_max_num > VDIN_CANVAS_MAX_CNT)
        devp->canvas_max_num = VDIN_CANVAS_MAX_CNT;

    pr_info("vdin%d cnavas initial table:\n", devp->index);
    for ( i = 0; i < devp->canvas_max_num; i++)
    {
        canvas_id = vdin_canvas_ids[devp->index][i];
        canvas_addr = devp->mem_start + devp->canvas_max_size * i;
        canvas_config(canvas_id, canvas_addr, canvas_max_w, canvas_max_h,
            CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
        pr_info("   %d: 0x%x-0x%x  %dx%d (%d KB)\n",
            canvas_id, canvas_addr, (canvas_addr + devp->canvas_max_size),
            canvas_max_w, canvas_max_h, (devp->canvas_max_size >> 10));
    }
}

inline void vdin_canvas_start_config(struct vdin_dev_s *devp)
{
    int i, canvas_id;
    unsigned long canvas_addr;

    if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444) ||
        (devp->format_convert == VDIN_FORMAT_CONVERT_YUV_RGB   ) ||
        (devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444) ||
        (devp->format_convert == VDIN_FORMAT_CONVERT_RGB_RGB   ))
        devp->canvas_w = devp->h_active * 3;
    else
        devp->canvas_w = devp->h_active * 2;

#if 0
    if(tvin_fmt_tbl[devp->parm.info.fmt].scan_mode == TVIN_SCAN_MODE_INTERLACED)
        devp->canvas_h = devp->v_active * 2;
    else
        devp->canvas_h = devp->v_active;
#else
    devp->canvas_h = devp->v_active;
#endif

    pr_info("vdin%d cnavas configuration table:\n", devp->index);
    for (i = 0; i < devp->canvas_max_num; i++)
    {
        canvas_id = vdin_canvas_ids[devp->index][i];
        canvas_addr = canvas_get_addr(canvas_id);
        canvas_config(canvas_id, canvas_addr, devp->canvas_w, devp->canvas_h,
            CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
        pr_info("   %d: 0x%lx-0x%lx %ux%u\n",
            canvas_id, canvas_addr, canvas_addr + devp->canvas_max_size, devp->canvas_w, devp->canvas_h);
    }
}

void vdin_canvas_auto_config(struct vdin_dev_s *devp)
{
    int i = 0;
    int canvas_id;
    unsigned long canvas_addr;

    if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444) ||
        (devp->format_convert == VDIN_FORMAT_CONVERT_YUV_RGB   ) ||
        (devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444) ||
        (devp->format_convert == VDIN_FORMAT_CONVERT_RGB_RGB   ))
        devp->canvas_w = devp->h_active * 3;
    else
        devp->canvas_w = devp->h_active * 2;
    devp->canvas_h = devp->v_active;
    devp->canvas_max_size = PAGE_ALIGN(devp->canvas_w*devp->canvas_h);
    devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size;
    if (devp->canvas_max_num > VDIN_CANVAS_MAX_CNT)
        devp->canvas_max_num = VDIN_CANVAS_MAX_CNT;

    pr_info("vdin%d cnavas auto configuration table:\n", devp->index);
    for (i = 0; i < devp->canvas_max_num; i++)
    {
        canvas_id = vdin_canvas_ids[devp->index][i];
        canvas_addr = devp->mem_start + devp->canvas_max_size * i;
        canvas_config(canvas_id, canvas_addr, devp->canvas_w, devp->canvas_h,
            CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
        pr_info("   %3d: 0x%lx-0x%lx %ux%u\n",
            canvas_id, canvas_addr, canvas_addr + devp->canvas_max_size,
            devp->canvas_w, devp->canvas_h);
    }
}

