/*
 * VDIN driver
 *
 * Author: Lin Xu <lin.xu@amlogic.com>
 *         Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


/* Standard Linux Headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <asm/fiq.h>
#include <asm/div64.h>

/* Amlogic Headers */
#include <linux/amports/canvas.h>
#include <mach/am_regs.h>
#include <linux/amports/vframe.h>
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>
#include <linux/amports/timestamp.h>
#include <linux/tvin/tvin.h>
#include <linux/aml_common.h>
#include <mach/irqs.h>

/* Local Headers */
#include "tvin_global.h"
#include "tvin_format_table.h"
#include "tvin_frontend.h"
#include "vdin_regs.h"
#include "vdin.h"
#include "vdin_ctl.h"
#include "vdin_sm.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"

#define VDIN_NAME               "vdin"
#define VDIN_DRIVER_NAME        "vdin"
#define VDIN_MODULE_NAME        "vdin"
#define VDIN_DEVICE_NAME        "vdin"
#define VDIN_CLASS_NAME         "vdin"
#define PROVIDER_NAME           "vdin"


#if defined(CONFIG_ARCH_MESON)
#define VDIN_COUNT              1
#elif defined(CONFIG_ARCH_MESON2)
#define VDIN_COUNT              2
#endif

#define VDIN_PUT_INTERVAL       (HZ/100)   //10ms, #define HZ 100

#define INVALID_VDIN_INPUT      0xffffffff

static dev_t vdin_devno;
static struct class *vdin_clsp;

#if defined(CONFIG_ARCH_MESON)
unsigned int vdin_addr_offset[VDIN_COUNT] = {0x00};
#elif defined(CONFIG_ARCH_MESON2)
unsigned int vdin_addr_offset[VDIN_COUNT] = {0x00, 0x70};
#endif

static vdin_dev_t *vdin_devp[VDIN_COUNT];

/*
 * canvas_config_mode
 * 0: canvas_config in driver probe
 * 1: start cofig
 * 2: auto config
 */
static int canvas_config_mode = 1;
module_param(canvas_config_mode, int, 0664);
MODULE_PARM_DESC(canvas_config_mode, "canvas configure mode");

static int work_mode_simple = 0;
module_param(work_mode_simple, bool, 0664);
MODULE_PARM_DESC(work_mode_simple, "enable/disable simple work mode");

static char *first_field_type = NULL;
module_param(first_field_type, charp, 0664);
MODULE_PARM_DESC(first_field_type, "first field type in simple work mode");

static int max_ignore_frames = 0;
module_param(max_ignore_frames, int, 0664);
MODULE_PARM_DESC(max_ignore_frames, "ignore first <n> frames");

static int ignore_frames = 0;
module_param(ignore_frames, int, 0664);
MODULE_PARM_DESC(ignore_frames, "ignore first <n> frames");

static int start_provider_delay = 100;
module_param(start_provider_delay, int, 0664);
MODULE_PARM_DESC(start_provider_delay, "ignore first <n> frames");


static int irq_max_count = 0;

static irqreturn_t vdin_isr_simple(int irq, void *dev_id);
static irqreturn_t vdin_isr(int irq, void *dev_id);

static u32 vdin_get_curr_field_type(struct vdin_dev_s *devp)
{
    enum tvin_scan_mode_e scan_mode = 0;
    u32 field_status;
    u32 type = VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD ;
    struct tvin_parm_s *parm = &devp->parm;

    scan_mode = tvin_fmt_tbl[parm->info.fmt].scan_mode;
    if (scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) {
        type |= VIDTYPE_PROGRESSIVE;
    }
    else
    {
        field_status = vdin_get_field_type(devp->addr_offset);
        type |= field_status ? VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;
    }
    if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444) ||
        (devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444))
        type |= VIDTYPE_VIU_444;
    else if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
        (devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422))
        type |= VIDTYPE_VIU_422;

    return type;
}

void vdin_timer_func(unsigned long arg)
{
    struct vdin_dev_s *devp = (struct vdin_dev_s *)arg;

    /* state machine routine */
    tvin_smr(devp);

    /* add timer */
    devp->timer.expires = jiffies + VDIN_PUT_INTERVAL;
    add_timer(&devp->timer);
}

static const struct vframe_operations_s vdin_vf_ops =
{
    .peek = vdin_vf_peek,
    .get  = vdin_vf_get,
    .put  = vdin_vf_put,
};

static struct vframe_provider_s vdin_vf_prov[VDIN_COUNT];

/*
 * 1. find the corresponding frontend according to the port & save it.
 * 2. set default register, including:
 *      a. set default write canvas address.
 *      b. mux null input.
 *      c. set clock auto.
 *      a&b will enable hw work.
 * 3. call the callback function of the frontend to open.
 * 4. regiseter provider.
 * 5. create timer for state machine.
 *
 * port: the port suported by frontend
 * index: the index of frontend
 * 0 success, otherwise failed
 */
static int vdin_open_fe(enum tvin_port_e port, int index,  struct vdin_dev_s *devp)
{
    struct tvin_frontend_s *fe = tvin_get_frontend(port, index);
    if (!fe) {
        pr_err("%s(%d): not supported port 0x%x \n", __func__, devp->index, port);
        return -1;
    }

    devp->frontend = fe;
    devp->parm.port     = port;
    devp->parm.info.fmt  = TVIN_SIG_FMT_NULL;
    devp->parm.info.status = TVIN_SIG_STATUS_NULL;
#ifdef TVAFE_SET_CVBS_MANUAL_FMT_POS
    devp->cvbs_pos_chg = TVIN_CVBS_POS_NULL;  //init position value
#endif
    devp->dec_enable = 1;  //enable decoder

    vdin_set_default_regmap(devp->addr_offset);

    if (devp->frontend->dec_ops && devp->frontend->dec_ops->open)
        devp->frontend->dec_ops->open(devp->frontend, port);

    /* init vdin state machine */
    tvin_smr_init();
    init_timer(&devp->timer);
    devp->timer.data = (ulong) devp;
    devp->timer.function = vdin_timer_func;
    devp->timer.expires = jiffies + VDIN_PUT_INTERVAL;
    add_timer(&devp->timer);

    pr_info("%s port:0x%x ok\n", __func__, port);
    return 0;
}

/*
 *
 * 1. disable hw work, including:
 *      a. mux null input.
 *      b. set clock off.
 * 2. delete timer for state machine.
 * 3. unregiseter provider & notify receiver.
 * 4. call the callback function of the frontend to close.
 *
 */
static void vdin_close_fe(struct vdin_dev_s *devp)
{
    /* avoid null pointer oops */
    if (!devp || !devp->frontend)
        return;
    devp->dec_enable = 0;  //disable decoder

    vdin_hw_disable(devp->addr_offset);
    del_timer_sync(&devp->timer);
    if (devp->frontend && devp->frontend->dec_ops->close) {
        devp->frontend->dec_ops->close(devp->frontend);
        devp->frontend = NULL;
    }
    devp->parm.port = TVIN_PORT_NULL;
    devp->parm.info.fmt  = TVIN_SIG_FMT_NULL;
    devp->parm.info.status = TVIN_SIG_STATUS_NULL;

    pr_info("%s ok\n", __func__);
}

static inline void vdin_set_source_type(struct vdin_dev_s *devp, struct vframe_s *vf)
{
    switch (devp->parm.port)
    {
        case TVIN_PORT_CVBS0:
            vf->source_type= VFRAME_SOURCE_TYPE_TUNER;
            break;
        case TVIN_PORT_CVBS1:
        case TVIN_PORT_CVBS2:
        case TVIN_PORT_CVBS3:
        case TVIN_PORT_CVBS4:
        case TVIN_PORT_CVBS5:
        case TVIN_PORT_CVBS6:
        case TVIN_PORT_CVBS7:
            vf->source_type = VFRAME_SOURCE_TYPE_CVBS;
            break;
        default:
            vf->source_type = VFRAME_SOURCE_TYPE_OTHERS;
            break;
    }
}



static inline void vdin_set_source_mode(struct vdin_dev_s *devp, struct vframe_s *vf)
{
    switch (devp->parm.info.fmt)
    {
        case TVIN_SIG_FMT_CVBS_NTSC_M:
        case TVIN_SIG_FMT_CVBS_NTSC_443:
            vf->source_mode = VFRAME_SOURCE_MODE_NTSC;
            break;
        case TVIN_SIG_FMT_CVBS_PAL_I:
        case TVIN_SIG_FMT_CVBS_PAL_M:
        case TVIN_SIG_FMT_CVBS_PAL_60:
        case TVIN_SIG_FMT_CVBS_PAL_CN:
            vf->source_mode = VFRAME_SOURCE_MODE_PAL;
            break;
        case TVIN_SIG_FMT_CVBS_SECAM:
            vf->source_mode = VFRAME_SOURCE_MODE_SECAM;
            break;
        default:
            vf->source_mode = VFRAME_SOURCE_MODE_OTHERS;
            break;
    }
}

/*
    based on the bellow parameters:
        1.h_active
        2.v_active
 */
static void vdin_vf_init(struct vdin_dev_s *devp)
{
    int i = 0;
    struct vf_entry *master, *slave;
    struct vframe_s *vf;
    struct vf_pool *p = devp->vfp;

    pr_info("vdin%d vframe initial infomation table: (%d of %d)\n",
        devp->index, p->size, p->max_size);
    for (i = 0; i < p->size; ++i)
    {
        master = vf_get_master(p, i);
        master->flag |= VF_FLAG_NORMAL_FRAME;
        vf = &master->vf;
        memset(vf, 0, sizeof(struct vframe_s));
        vf->index = i;
        vf->width = devp->h_active;
        vf->height = devp->v_active;
        if ((tvin_fmt_tbl[devp->parm.info.fmt].scan_mode == TVIN_SCAN_MODE_INTERLACED) &&
            (!(devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) &&
            (devp->parm.info.fmt != TVIN_SIG_FMT_NULL))
           )
            vf->height <<= 1;
        //vf->duration = tvin_fmt_tbl[devp->parm.info.fmt].duration;
        vf->canvas0Addr = vf->canvas1Addr = vdin_canvas_ids[devp->index][vf->index];

        /* set source type & mode */
        vdin_set_source_type(devp, vf);
        vdin_set_source_mode(devp, vf);


        /* init slave vframe */
        slave  = vf_get_slave(p, i);
        slave->flag = master->flag;
        memset(&slave->vf, 0, sizeof(struct vframe_s));
        slave->vf.index       = vf->index;
        slave->vf.width       = vf->width;
        slave->vf.height      = vf->height;
        slave->vf.duration    = vf->duration;
        slave->vf.canvas0Addr = vf->canvas0Addr;
        slave->vf.canvas1Addr = vf->canvas1Addr;
        /* set slave vf source type & mode */
        slave->vf.source_type = vf->source_type;
        slave->vf.source_mode = vf->source_mode;

        pr_info("   %2d: %3u %ux%u, duration = %u\n", vf->index,
                vf->canvas0Addr, vf->width, vf->height, vf->duration);
    }
}

/*
 * 1. config canvas for video frame.
 * 2. enable hw work, including:
 *      a. mux null input.
 *      b. set clock auto.
 * 3. set all registeres including:
 *      a. mux input.
 * 4. call the callback function of the frontend to start.
 * 5. enable irq .
 *
 */
static void vdin_start_dec(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;
    enum tvin_sig_fmt_e fmt = 0;
    /* avoid null pointer oops */
    if (!devp || !devp->frontend)
        return;
    sm_ops = devp->frontend->sm_ops;
    fmt = devp->parm.info.fmt;
	sm_ops->get_sig_propery(devp->frontend, &devp->prop);

    vdin_get_format_convert(devp);
    devp->curr_wr_vfe = NULL;

    /* h_active/v_active will be recalculated by bellow calling */
    vdin_set_decimation(devp);
    vdin_set_cutwin(devp);

    pr_info("h_active = %d, v_active = %d\n", devp->h_active, devp->v_active);
    pr_info("signal format  = %s(%d)\n"
            "trans_fmt      = %s(%d)\n"
            "color_format   = %s(%d)\n"
            "format_convert = %s(%d)\n"
            "aspect_ratio   = %s(%d)\n"
            "pixel_repeat   = %u\n",
            tvin_sig_fmt_str(devp->parm.info.fmt), devp->parm.info.fmt,
            tvin_trans_fmt_str(devp->prop.trans_fmt), devp->prop.trans_fmt,
            tvin_color_fmt_str(devp->prop.color_format), devp->prop.color_format,
            vdin_fmt_convert_str(devp->format_convert), devp->format_convert,
            tvin_aspect_ratio_str(devp->prop.aspect_ratio), devp->prop.aspect_ratio,
            devp->prop.pixel_repeat);

    /* h_active/v_active will be used by bellow calling */
    if (canvas_config_mode == 1) {
        vdin_canvas_start_config(devp);
    }
    else if (canvas_config_mode == 2){
        vdin_canvas_auto_config(devp);
    }
    vdin_set_canvas_id(devp->addr_offset, vdin_canvas_ids[devp->index][0]);

    vf_pool_init(devp->vfp, devp->canvas_max_num);
    vdin_vf_init(devp);

    devp->abnormal_cnt = 0;

    irq_max_count = 0;
    devp->v.isr_count = 0;
    devp->v.tval.tv_sec = 0;
    devp->v.tval.tv_usec = 0;
    devp->v.min_isr_time = 0;
    devp->v.max_isr_time = 0;
    devp->v.avg_isr_time = 0;
    devp->v.less_5ms_cnt = 0;
    devp->v.isr_interval = 0;


    devp->vga_clr_cnt = devp->canvas_max_num;

    devp->vs_cnt_valid = 0;
    devp->vs_cnt_ignore = 0;

    //pr_info("start clean_counter is %d\n",clean_counter);
    /* configure regs and enable hw */
    vdin_hw_enable(devp->addr_offset);
    vdin_set_all_regs(devp);

    if ((fmt  < TVIN_SIG_FMT_VGA_MAX          ) &&
        (fmt != TVIN_SIG_FMT_NULL             )
       )
       vdin_set_matrix_blank(devp);

    if (!(devp->parm.flag & TVIN_PARM_FLAG_CAP) &&
        devp->frontend->dec_ops && devp->frontend->dec_ops->start)
        devp->frontend->dec_ops->start(devp->frontend, devp->parm.info.fmt);

    /* register provider, so the receiver can get the valid vframe */
    udelay(start_provider_delay);
    vf_reg_provider(&vdin_vf_prov[devp->index]);


    /* enable system_time */
    timestamp_pcrscr_enable(1);

    pr_info("%s port:0x%x ok\n", __func__, devp->parm.port);
}

/*
 * 1. disable irq.
 * 2. disable hw work, including:
 *      a. mux null input.
 *      b. set clock off.
 * 3. call the callback function of the frontend to stop.
 *
 */
static void vdin_stop_dec(struct vdin_dev_s *devp)
{
    /* avoid null pointer oops */
    if (!devp || !devp->frontend)
        return;

    vf_unreg_provider(&vdin_vf_prov[devp->index]);
    if (!(devp->parm.flag & TVIN_PARM_FLAG_CAP) &&
        devp->frontend->dec_ops && devp->frontend->dec_ops->stop)
        devp->frontend->dec_ops->stop(devp->frontend, devp->parm.port);
    vdin_set_default_regmap(devp->addr_offset);
    vdin_hw_disable(devp->addr_offset);
    //disable_irq_nosync(devp->irq);
    /* reset default canvas  */
    vdin_set_def_wr_canvas(devp);

    memset(&devp->prop, 0, sizeof(struct tvin_sig_property_s));
    ignore_frames = 0;
    pr_info("%s ok\n", __func__);
}

static void vdin_pause_dec(struct vdin_dev_s *devp)
{
    vdin_hw_disable(devp->addr_offset);
}

static void vdin_resume_dec(struct vdin_dev_s *devp)
{
    vdin_hw_enable(devp->addr_offset);
}

static void vdin_vf_reg(struct vdin_dev_s *devp)
{
    vf_reg_provider(&vdin_vf_prov[devp->index]);
}

static void vdin_vf_unreg(struct vdin_dev_s *devp)
{
    vf_unreg_provider(&vdin_vf_prov[devp->index]);
}


static inline void vdin_set_view(struct vdin_dev_s *devp, struct vframe_s *vf)
{
    struct vframe_view_s *left_eye, *right_eye;

    left_eye  = &vf->left_eye;
    right_eye = &vf->right_eye;

    switch (devp->parm.info.trans_fmt)
    {
        case TVIN_TFMT_3D_LRH_OLER:
            left_eye->start_x    = 0;
            left_eye->start_y    = 0;
            left_eye->width      = devp->h_active >> 1;
            left_eye->height     = devp->v_active;
            right_eye->start_x   = devp->h_active >> 1;
            right_eye->start_y   = 0;
            right_eye->width     = devp->h_active >> 1;
            right_eye->height    = devp->v_active;
            break;
        case TVIN_TFMT_3D_TB:
            left_eye->start_x    = 0;
            left_eye->start_y    = 0;
            left_eye->width      = devp->h_active;
            left_eye->height     = devp->v_active >> 1;
            right_eye->start_x   = 0;
            right_eye->start_y   = devp->v_active >> 1;
            right_eye->width     = devp->h_active;
            right_eye->height    = devp->v_active >> 1;
            break;
        case TVIN_TFMT_3D_FP:
        {
            unsigned int vactive = 0;
            unsigned int vspace = 0;
            struct vf_entry *slave = NULL;

            vspace  = tvin_fmt_tbl[devp->parm.info.fmt].vs_front +
                      tvin_fmt_tbl[devp->parm.info.fmt].vs_width +
                      tvin_fmt_tbl[devp->parm.info.fmt].vs_bp;

            if ((devp->parm.info.fmt == TVIN_SIG_FMT_HDMI_1920x1080I_50Hz_FRAME_PACKING) ||
                (devp->parm.info.fmt == TVIN_SIG_FMT_HDMI_1920x1080I_60Hz_FRAME_PACKING))
            {
                vactive = (tvin_fmt_tbl[devp->parm.info.fmt].v_active - vspace - vspace - vspace + 1) >> 2;
                slave = vf_get_slave(devp->vfp, vf->index);

                slave->vf.left_eye.start_x  = 0;
                slave->vf.left_eye.start_y  = vactive + vspace + vactive + vspace - 1;
                slave->vf.left_eye.width    = devp->h_active;
                slave->vf.left_eye.height   = vactive;
                slave->vf.right_eye.start_x = 0;
                slave->vf.right_eye.start_y = vactive + vspace + vactive + vspace + vactive + vspace - 1;
                slave->vf.right_eye.width   = devp->h_active;
                slave->vf.right_eye.height  = vactive;
            }
            else
                vactive = (tvin_fmt_tbl[devp->parm.info.fmt].v_active - vspace) >> 1;


            left_eye->start_x    = 0;
            left_eye->start_y    = 0;
            left_eye->width      = devp->h_active;
            left_eye->height     = vactive;
            right_eye->start_x   = 0;
            right_eye->start_y   = vactive + vspace;
            right_eye->width     = devp->h_active;
            right_eye->height    = vactive;
            break;
        }
        default:
            left_eye->start_x    = 0;
            left_eye->start_y    = 0;
            left_eye->width      = 0;
            left_eye->height     = 0;
            right_eye->start_x   = 0;
            right_eye->start_y   = 0;
            right_eye->width     = 0;
            right_eye->height    = 0;
            break;
    }
}

static irqreturn_t vdin_isr_simple(int irq, void *dev_id)
{
    struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;
    struct tvin_decoder_ops_s *decops;
    unsigned int last_field_type;
    unsigned int hcnt64;

    if (irq_max_count >= devp->canvas_max_num) {
        vdin_hw_disable(devp->addr_offset);
        return IRQ_HANDLED;
    }

    last_field_type = devp->curr_field_type;
    devp->curr_field_type = vdin_get_curr_field_type(devp);

    decops =devp->frontend->dec_ops;
    hcnt64 = vdin_get_meas_hcnt64(devp);
    if (decops->decode_isr(devp->frontend, hcnt64) == -1)
        return IRQ_HANDLED;
    /* set canvas address */

    vdin_set_canvas_id(devp->addr_offset, vdin_canvas_ids[devp->index][irq_max_count]);
    pr_info("%2d: canvas id %d, field type %s\n", irq_max_count,
        vdin_canvas_ids[devp->index][irq_max_count],
        ((last_field_type & VIDTYPE_TYPEMASK)== VIDTYPE_INTERLACE_TOP ? "top":"buttom"));

    irq_max_count++;
    return IRQ_HANDLED;
}

static void vdin_backup_histgram(struct vframe_s *vf, struct vdin_dev_s *devp)
{
    unsigned int i = 0;

    for (i = 0; i < 64; i++)
        devp->parm.histgram[i] = vf->prop.hist.gamma[i];
}
/*as use the spin_lock,
 *1--there is no sleep,
 *2--it is better to shorter the time,
 *3--it is better to shorter the time,
*/
static irqreturn_t vdin_isr(int irq, void *dev_id)
{
    ulong flags;
    struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;
    enum tvin_sm_status_e state;

    struct vf_entry *next_wr_vfe = NULL;
    struct vf_entry *curr_wr_vfe = NULL;
    struct vframe_s *curr_wr_vf = NULL;
    unsigned int last_field_type;
    signed short step = 0;
    struct tvin_decoder_ops_s *decops;
    unsigned int hcnt64;
    unsigned long long vs_cnt;
    struct timeval tval;
    struct tvin_state_machine_ops_s *sm_ops;
    unsigned long long total_time;
    unsigned long long total_tmp;
    struct tvafe_vga_parm_s vga_parm = {0};
    bool is_vga = false, is_abnormal = false;

    if (!devp) return IRQ_HANDLED;

    /* debug interrupt interval time
     *
     * this code about system time must be outside of spinlock.
     * because the spinlock may affect the system time.
     */
    devp->v.isr_count++;
#if 0    
    do_gettimeofday(&tval);
    if (devp->v.tval.tv_sec) {
        devp->v.isr_interval = abs(1000000*(tval.tv_sec - devp->v.tval.tv_sec) + tval.tv_usec - devp->v.tval.tv_usec);
        if (devp->v.min_isr_time == 0) {
            devp->v.min_isr_time = devp->v.isr_interval;
            devp->v.max_isr_time = devp->v.isr_interval;
            devp->v.avg_isr_time = devp->v.isr_interval;
        }
        else {
            if (devp->v.isr_interval < devp->v.min_isr_time)
                devp->v.min_isr_time = devp->v.isr_interval;
            if (devp->v.isr_interval > devp->v.max_isr_time)
                devp->v.max_isr_time = devp->v.isr_interval;
            total_time = ((devp->v.avg_isr_time * (devp->v.isr_count - 2)) + devp->v.isr_interval);
            total_tmp = total_time;
            do_div(total_tmp, devp->v.isr_count - 1);
            devp->v.avg_isr_time = total_tmp;
            if (devp->v.isr_interval <= 5000)
                devp->v.less_5ms_cnt++;
        }
        devp->v.tval.tv_sec  = tval.tv_sec;
        devp->v.tval.tv_usec = tval.tv_usec;
        /* work-around to ignore unexpected fake vsync temporarily */
        if (devp->v.isr_interval <= 1000)
            return IRQ_HANDLED;
    }
    else {
        devp->v.tval.tv_sec  = tval.tv_sec;
        devp->v.tval.tv_usec = tval.tv_usec;
    }
#endif

    spin_lock_irqsave(&devp->isr_lock, flags);

    /* avoid null pointer oops */
    if (!devp || !devp->frontend) {
        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }

   if (READ_CBUS_REG_BITS(VDIN_COM_STATUS0, DIRECT_DONE_STATUS_BIT, DIRECT_DONE_STATUS_WID))
    {
        WRITE_CBUS_REG_BITS(VDIN_WR_CTRL, 1, DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
        WRITE_CBUS_REG_BITS(VDIN_WR_CTRL, 0, DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
    }
    else if ((vdin_get_active_v(devp->addr_offset) >= devp->v_active) && 
             (vdin_get_active_h(devp->addr_offset) >= devp->h_active) 
            )
        is_abnormal = true;
    if (is_abnormal)
        devp->abnormal_cnt++;
    else
        devp->abnormal_cnt = 0;
    if (devp->abnormal_cnt > 60)
    {
        devp->abnormal_cnt = 0;
        devp->flags |= VDIN_FLAG_FORCE_UNSTABLE;
    }

    if (get_foreign_affairs(FOREIGN_AFFAIRS_02))
    {
        rst_foreign_affairs(FOREIGN_AFFAIRS_02);
        devp->flags |= VDIN_FLAG_FORCE_UNSTABLE;
    }

    if (devp->flags & VDIN_FLAG_DEC_STOP_ISR)
    {
        vdin_hw_disable(devp->addr_offset);
        devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }
    sm_ops = devp->frontend->sm_ops;

    if ((devp->parm.port >= TVIN_PORT_VGA0) &&
        (devp->parm.port <= TVIN_PORT_VGA7) &&
        (devp->dec_enable))
    {
        is_vga = true;
    }
    else{
        is_vga = false;
    }
    last_field_type = devp->curr_field_type;
    devp->curr_field_type = vdin_get_curr_field_type(devp);

    /* ignore the unstable signal */
    state = tvin_get_sm_status();
    if (devp->parm.info.status != TVIN_SIG_STATUS_STABLE ||
        state != TVIN_SM_STATUS_STABLE) {
        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }

    /* ignore invalid vs to void screen flicker  */
    if (vdin_check_vs(devp))
    {
        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }

    /* first vframe */
    if (!devp->curr_wr_vfe) {
        devp->curr_wr_vfe = provider_vf_get(devp->vfp);
        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }
    /* for 2D->3D mode & interlaced format, give up bottom field */
    if ((devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) &&
        ((last_field_type & VIDTYPE_INTERLACE_BOTTOM )== VIDTYPE_INTERLACE_BOTTOM)
       )
    {
        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }
    curr_wr_vfe = devp->curr_wr_vfe;
    curr_wr_vf  = &curr_wr_vfe->vf;

    if (is_vga)
        sm_ops->vga_get_param(&vga_parm, devp->frontend);
    /* 4.csc=default wr=zoomed */
    if ((curr_wr_vf->vga_parm.vga_in_clean == 1) && is_vga)
    {
        curr_wr_vf->vga_parm.vga_in_clean--;
        vdin_set_matrix(devp);
        sm_ops->vga_set_param(&curr_wr_vf->vga_parm, devp->frontend);
        step = curr_wr_vf->vga_parm.vpos_step;
        if (step > 0)
            vdin_delay_line(1, devp->addr_offset);
        else
            vdin_delay_line(0, devp->addr_offset);

        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }
    /* 3.csc=blank wr=zoomed */
    if ((curr_wr_vf->vga_parm.vga_in_clean == 2) && is_vga)
    {
        curr_wr_vf->vga_parm.vga_in_clean--;
        curr_wr_vf->vga_parm.clk_step = vga_parm.clk_step;
        curr_wr_vf->vga_parm.phase = vga_parm.phase;
        curr_wr_vf->vga_parm.hpos_step = vga_parm.hpos_step;
        curr_wr_vf->vga_parm.vpos_step = vga_parm.vpos_step;
        set_wr_ctrl(curr_wr_vf->vga_parm.hpos_step, curr_wr_vf->vga_parm.vpos_step,devp);

        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }
    /* 2.csc=blank wr=default */
    if ((curr_wr_vf->vga_parm.vga_in_clean == 3) && is_vga)
    {
        curr_wr_vf->vga_parm.vga_in_clean--;

        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }

    decops = devp->frontend->dec_ops;
    hcnt64 = vdin_get_meas_hcnt64(devp);
    if (decops->decode_isr(devp->frontend, hcnt64) == -1) {
        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }

    if (ignore_frames < max_ignore_frames ) {
        ignore_frames++;
        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }

    if (sm_ops->check_frame_skip && sm_ops->check_frame_skip(devp->frontend)) {
        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }

    next_wr_vfe = provider_vf_peek(devp->vfp);
    if (!next_wr_vfe) {
        spin_unlock_irqrestore(&devp->isr_lock, flags);
        return IRQ_HANDLED;
    }

    curr_wr_vf->type = last_field_type;
    /* for 2D->3D mode & interlaced format, fill-in as progressive format */
    if ((devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) &&
        (last_field_type & VIDTYPE_INTERLACE)
       )
    {
        curr_wr_vf->type &= ~VIDTYPE_INTERLACE_TOP;
        curr_wr_vf->type |=  VIDTYPE_PROGRESSIVE;
        curr_wr_vf->type |=  VIDTYPE_PRE_INTERLACE;
    }
    vdin_set_vframe_prop_info(curr_wr_vf, devp->addr_offset);
    vdin_backup_histgram(curr_wr_vf, devp);
    curr_wr_vf->trans_fmt = devp->parm.info.trans_fmt;


    vdin_set_view(devp, curr_wr_vf);

    vs_cnt = curr_wr_vf->prop.meas.vs_cnt + 125;
    do_div(vs_cnt, 250);
    curr_wr_vf->duration = (unsigned int)vs_cnt;
    /* for 2D->3D mode & interlaced format, double top field duration to match software frame lock */
    if ((devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) &&
        (last_field_type & VIDTYPE_INTERLACE))
        curr_wr_vf->duration <<= 1;

    /* put for receiver */
    if ((devp->parm.info.fmt == TVIN_SIG_FMT_HDMI_1920x1080I_50Hz_FRAME_PACKING) ||
        (devp->parm.info.fmt == TVIN_SIG_FMT_HDMI_1920x1080I_60Hz_FRAME_PACKING))
    {
        struct vf_entry *slave = vf_get_slave(devp->vfp, curr_wr_vf->index);
        slave->vf.type = curr_wr_vf->type;
        slave->vf.trans_fmt = curr_wr_vf->trans_fmt;
        memcpy(&slave->vf.prop, &curr_wr_vf->prop, sizeof(struct vframe_prop_s));
        slave->flag &= (~VF_FLAG_NORMAL_FRAME);
        curr_wr_vf->duration = (curr_wr_vf->duration + 1) >> 1;
        slave->vf.duration = curr_wr_vf->duration;
        curr_wr_vfe->flag &= (~VF_FLAG_NORMAL_FRAME);
        provider_vf_put(curr_wr_vfe, devp->vfp);
        provider_vf_put(slave, devp->vfp);
    }
    else {
        curr_wr_vfe->flag |= VF_FLAG_NORMAL_FRAME;
        provider_vf_put(curr_wr_vfe, devp->vfp);
    }


    /* prepare for next input data */
    next_wr_vfe = provider_vf_get(devp->vfp);
    vdin_set_canvas_id(devp->addr_offset, vdin_canvas_ids[devp->index][next_wr_vfe->vf.index]);
    devp->curr_wr_vfe = next_wr_vfe;
    vf_notify_receiver(devp->name,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);

    /* 1.csc=blank wr=default*/
    if (is_vga &&
        ((vga_parm.hpos_step != next_wr_vfe->vf.vga_parm.hpos_step) ||
         (vga_parm.vpos_step != next_wr_vfe->vf.vga_parm.vpos_step) ||
         (vga_parm.clk_step  != next_wr_vfe->vf.vga_parm.clk_step ) ||
         (vga_parm.phase     != next_wr_vfe->vf.vga_parm.phase    ) ||
         (devp->vga_clr_cnt > 0                                   )
        )
       )
    {
        set_wr_ctrl(0, 0, devp);
        vdin_set_matrix_blank(devp);
        vga_parm.hpos_step = 0;
        vga_parm.vpos_step = 0;
        vga_parm.phase     = next_wr_vfe->vf.vga_parm.phase;
        sm_ops->vga_set_param(&vga_parm, devp->frontend);
        if (next_wr_vfe->vf.vga_parm.vga_in_clean == 0)
            next_wr_vfe->vf.vga_parm.vga_in_clean = 3;
        else
            next_wr_vfe->vf.vga_parm.vga_in_clean--;
        if (devp->vga_clr_cnt > 0)
            devp->vga_clr_cnt--;
    }

    spin_unlock_irqrestore(&devp->isr_lock, flags);
    return IRQ_HANDLED;
}



static int vdin_open(struct inode *inode, struct file *file)
{
    vdin_dev_t *devp;

    /* Get the per-device structure that contains this cdev */
    devp = container_of(inode->i_cdev, vdin_dev_t, cdev);
    file->private_data = devp;

    if (devp->index >= VDIN_COUNT)
        return -ENXIO;

    if (devp->flags &= VDIN_FLAG_FS_OPENED) {
        pr_info("%s, device %s opened already\n", __func__, dev_name(devp->dev));
        return 0;
    }

    devp->flags |= VDIN_FLAG_FS_OPENED;

    /* request irq */
    snprintf(devp->irq_name, sizeof(devp->irq_name),  "vdin%d-irq", devp->index);
    if (work_mode_simple) {
        pr_info("vdin work in simple mode\n");
        request_irq(devp->irq, vdin_isr_simple, IRQF_SHARED, devp->irq_name, (void *)devp);
    }
    else {
        pr_info("vdin work in normal mode\n");
        request_irq(devp->irq, vdin_isr, IRQF_SHARED, devp->irq_name, (void *)devp);
    }


    /* remove the hardware limit to vertical [0-max]*/
    WRITE_CBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000fff);
    pr_info("open device %s ok\n", dev_name(devp->dev));
    return 0;
}

static int vdin_release(struct inode *inode, struct file *file)
{
    vdin_dev_t *devp = file->private_data;

    if (!(devp->flags &= VDIN_FLAG_FS_OPENED)) {
        pr_info("%s, device %s not opened\n", __func__, dev_name(devp->dev));
        return 0;
    }

    devp->flags &= (~VDIN_FLAG_FS_OPENED);

    /* free irq */
    free_irq(devp->irq,(void *)devp);

    file->private_data = NULL;

    /* reset the hardware limit to vertical [0-1079]  */
    WRITE_CBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000437);
    pr_info("close device %s ok\n", dev_name(devp->dev));
    return 0;
}

static int vdin_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    unsigned int delay_cnt = 0;
    vdin_dev_t *devp = NULL;
    void __user *argp = (void __user *)arg;

    if (_IOC_TYPE(cmd) != TVIN_IOC_MAGIC) {
        pr_err("%s invalid command: %u\n", __func__, cmd);
        return -ENOSYS;
    }


    /* Get the per-device structure that contains this cdev */
    devp = container_of(inode->i_cdev, vdin_dev_t, cdev);

    switch (cmd)
    {
        case TVIN_IOC_OPEN:
        {
            struct tvin_parm_s parm = {0};

            mutex_lock(&devp->fe_lock);

            if (copy_from_user(&parm, argp, sizeof(struct tvin_parm_s)))
            {
                pr_err("TVIN_IOC_OPEN(%d) invalid parameter\n", devp->index);
                ret = -EFAULT;
                mutex_unlock(&devp->fe_lock);
                break;
            }

            if (devp->flags & VDIN_FLAG_DEC_OPENED) {
                pr_err("TVIN_IOC_OPEN(%d) port %s opend already\n",
                    parm.index, tvin_port_str(parm.port));
                ret = -EBUSY;
                mutex_unlock(&devp->fe_lock);
                break;

            }

            devp->parm.index = parm.index;
            devp->parm.port  = parm.port;
			devp->unstable_flag = false;
            ret = vdin_open_fe(devp->parm.port, devp->parm.index, devp);
            if (ret) {
                pr_err("TVIN_IOC_OPEN(%d) failed to open port 0x%x\n",
                    devp->index, parm.port);
                ret = -EFAULT;
                mutex_unlock(&devp->fe_lock);
                break;
            }

            devp->flags |= VDIN_FLAG_DEC_OPENED;
            pr_info("TVIN_IOC_OPEN(%d) port %s opened ok\n\n", parm.index,
                tvin_port_str(devp->parm.port));
            mutex_unlock(&devp->fe_lock);
            break;
        }
        case TVIN_IOC_START_DEC:
        {
            struct tvin_parm_s parm = {0};

            mutex_lock(&devp->fe_lock);
            if (devp->flags & VDIN_FLAG_DEC_STARTED) {
                pr_err("TVIN_IOC_START_DEC(%d) port 0x%x, decode started already\n", parm.index, parm.port);
                ret = -EBUSY;
                mutex_unlock(&devp->fe_lock);
            }

            if ((devp->parm.info.status != TVIN_SIG_STATUS_STABLE) ||
                (devp->parm.info.fmt == TVIN_SIG_FMT_NULL))
            {
                pr_err("TVIN_IOC_START_DEC: port %s start invalid\n",
                        tvin_port_str(devp->parm.port));
                pr_err("    status: %s, fmt: %s\n",
                    tvin_sig_status_str(devp->parm.info.status),
                    tvin_sig_fmt_str(devp->parm.info.fmt));
                ret = -EPERM;
                mutex_unlock(&devp->fe_lock);
                break;
            }

            if (copy_from_user(&parm, argp, sizeof(struct tvin_parm_s)))
            {
                pr_err("TVIN_IOC_START_DEC(%d) invalid parameter\n", devp->index);
                ret = -EFAULT;
                mutex_unlock(&devp->fe_lock);
                break;
            }
            devp->parm.cutwin.hs = parm.cutwin.hs;
            // odd number in line width => decrease to even number in line width
            if (!((parm.cutwin.hs + parm.cutwin.he) & 1))
                devp->parm.cutwin.hs++;
            //devp->parm.info.fmt = parm.info.fmt;
            devp->parm.cutwin.he = parm.cutwin.he;
            devp->parm.cutwin.vs = parm.cutwin.vs;
            devp->parm.cutwin.ve = parm.cutwin.ve;
            vdin_start_dec(devp);

            devp->flags |= VDIN_FLAG_DEC_STARTED;
            pr_info("TVIN_IOC_START_DEC port %s, decode started ok\n\n",
                tvin_port_str(devp->parm.port));
            mutex_unlock(&devp->fe_lock);
            break;
        }
        case TVIN_IOC_STOP_DEC:
        {
            struct tvin_parm_s *parm = &devp->parm;

            mutex_lock(&devp->fe_lock);
            if(!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
                pr_err("TVIN_IOC_STOP_DEC(%d) decode havn't started\n", devp->index);
                ret = -EPERM;
                mutex_unlock(&devp->fe_lock);
                break;
            }

            devp->flags |= VDIN_FLAG_DEC_STOP_ISR;
            delay_cnt = 7;
            while ((devp->flags & VDIN_FLAG_DEC_STOP_ISR) && delay_cnt)
            {
                mdelay(10);
                delay_cnt--;
            }

            vdin_stop_dec(devp);

            if (devp->flags & VDIN_FLAG_FORCE_UNSTABLE)
            {
                set_foreign_affairs(FOREIGN_AFFAIRS_00);
                pr_info("video reset vpp.\n");
            }
            delay_cnt = 7;
            while (get_foreign_affairs(FOREIGN_AFFAIRS_00) && delay_cnt)
            {
                mdelay(10);
                delay_cnt--;
            }

            /* init flag */
            devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
            devp->flags &= ~VDIN_FLAG_FORCE_UNSTABLE;

            /* clear the flag of decode started */
            devp->flags &= (~VDIN_FLAG_DEC_STARTED);
            pr_info("TVIN_IOC_STOP_DEC(%d) port %s, decode stop ok\n\n",
                parm->index, tvin_port_str(parm->port));
            mutex_unlock(&devp->fe_lock);
            break;
        }
        case TVIN_IOC_VF_REG:
            if ((devp->flags & VDIN_FLAG_DEC_REGED) == VDIN_FLAG_DEC_REGED) {
                pr_err("TVIN_IOC_VF_REG(%d) decoder is registered already\n", devp->index);
                ret = -EINVAL;
                break;
            }
            devp->flags |= VDIN_FLAG_DEC_REGED;
            vdin_vf_reg(devp);
            pr_info("TVIN_IOC_VF_REG(%d) ok\n\n", devp->index);
            break;
        case TVIN_IOC_VF_UNREG:
            if ((devp->flags & VDIN_FLAG_DEC_REGED) != VDIN_FLAG_DEC_REGED) {
                pr_err("TVIN_IOC_VF_UNREG(%d) decoder isn't registered\n", devp->index);
                ret = -EINVAL;
                break;
            }
            devp->flags &= (~VDIN_FLAG_DEC_REGED);
            vdin_vf_unreg(devp);
            pr_info("TVIN_IOC_VF_REG(%d) ok\n\n", devp->index);
            break;
        case TVIN_IOC_CLOSE:
        {
            struct tvin_parm_s *parm = &devp->parm;
            enum tvin_port_e port = parm->port;

            mutex_lock(&devp->fe_lock);
            if (!(devp->flags & VDIN_FLAG_DEC_OPENED)) {
                pr_err("TVIN_IOC_CLOSE(%d) you have not opened port\n", devp->index);
                ret = -EPERM;
                mutex_unlock(&devp->fe_lock);
                break;
            }
            vdin_close_fe(devp);

            devp->flags &= (~VDIN_FLAG_DEC_OPENED);
            pr_info("TVIN_IOC_CLOSE(%d) port %s closed ok\n\n", parm->index,
                tvin_port_str(port));
            mutex_unlock(&devp->fe_lock);
            break;
        }
        case TVIN_IOC_S_PARM:
        {
            struct tvin_parm_s parm = {0};
            if (copy_from_user(&parm, argp, sizeof(struct tvin_parm_s))) {
                ret = -EFAULT;
                break;
            }
            devp->parm.flag = parm.flag;
            devp->parm.reserved = parm.reserved;
            break;
        }
        case TVIN_IOC_G_PARM:
        {
            struct tvin_parm_s parm;
            memcpy(&parm, &devp->parm, sizeof(tvin_parm_t));
            if (devp->flags & VDIN_FLAG_FORCE_UNSTABLE)
                parm.info.status = TVIN_SIG_STATUS_UNSTABLE;
            if (copy_to_user(argp, &parm, sizeof(tvin_parm_t)))
               ret = -EFAULT;
            break;
        }
        case TVIN_IOC_G_SIG_INFO:
        {
            struct tvin_info_s info;
            memset(&info, 0, sizeof(tvin_info_t));
            mutex_lock(&devp->fe_lock);
            /* if port is not opened, ignore this command */
            if (!(devp->flags & VDIN_FLAG_DEC_OPENED)) {
                ret = -EPERM;
                mutex_unlock(&devp->fe_lock);
                break;
            }
            memcpy(&info, &devp->parm.info, sizeof(tvin_info_t));
            if (devp->flags & VDIN_FLAG_FORCE_UNSTABLE)
                info.status = TVIN_SIG_STATUS_UNSTABLE;
            if (copy_to_user(argp, &info, sizeof(tvin_info_t))) {
               ret = -EFAULT;
               mutex_unlock(&devp->fe_lock);
            }
            mutex_unlock(&devp->fe_lock);
            break;
        }
        case TVIN_IOC_G_BUF_INFO:
        {
            struct tvin_buf_info_s buf_info;
            buf_info.buf_count  = devp->canvas_max_num;
            buf_info.buf_width  = devp->canvas_w;
            buf_info.buf_height = devp->canvas_h;
            buf_info.buf_size   = devp->canvas_max_size;
            buf_info.wr_list_size = devp->vfp->wr_list_size;
            if (copy_to_user(argp, &buf_info, sizeof(struct tvin_buf_info_s)))
               ret = -EFAULT;
            break;
        }
        case TVIN_IOC_START_GET_BUF:
            devp->vfp->wr_next = devp->vfp->wr_list.next;
            break;
        case TVIN_IOC_GET_BUF:
        {
            struct tvin_video_buf_s tvbuf;
            struct vf_entry *vfe;
            vfe = list_entry(devp->vfp->wr_next, struct vf_entry, list);
            devp->vfp->wr_next = devp->vfp->wr_next->next;
            if (devp->vfp->wr_next != &devp->vfp->wr_list) {
                tvbuf.index = vfe->vf.index;
            }
            else {
                tvbuf.index = -1;
            }
            if (copy_to_user(argp, &tvbuf, sizeof(struct tvin_video_buf_s)))
               ret = -EFAULT;
            break;
        }
        case TVIN_IOC_PAUSE_DEC:
            vdin_pause_dec(devp);
            break;
        case TVIN_IOC_RESUME_DEC:
            vdin_resume_dec(devp);
            break;
        default:
            ret = -ENOIOCTLCMD;
            pr_info("%s %d is not supported command\n", __func__, cmd);
            break;
    }
    return ret;
}

static int vdin_mmap(struct file *file, struct vm_area_struct * vma)
{
    vdin_dev_t *devp = file->private_data;
    unsigned long start, len, off;
    unsigned long pfn, size;

    if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT)) {
        return -EINVAL;
    }

	start = devp->mem_start & PAGE_MASK;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + devp->mem_size);

	off = vma->vm_pgoff << PAGE_SHIFT;

	if ((vma->vm_end - vma->vm_start + off) > len) {
		return -EINVAL;
	}

	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    vma->vm_flags |= VM_IO | VM_RESERVED;

    size = vma->vm_end - vma->vm_start;
    pfn  = off >> PAGE_SHIFT;

    if (io_remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
        return -EAGAIN;

    return 0;
}

static struct file_operations vdin_fops = {
    .owner   = THIS_MODULE,
    .open    = vdin_open,
    .release = vdin_release,
    .ioctl   = vdin_ioctl,
    .mmap    = vdin_mmap,
};

#ifdef VF_LOG_EN
static ssize_t vdin_vf_log_show(struct device * dev,
    struct device_attribute *attr, char * buf)
{
    int len = 0;
    struct vdin_dev_s *devp = dev_get_drvdata(dev);
    struct vf_log_s *log = &devp->vfp->log;

    len += sprintf(buf + len, "%d of %d\n", log->log_cur, VF_LOG_LEN);
    return len;
}

static ssize_t vdin_vf_log_store(struct device * dev,
    struct device_attribute *attr, const char * buf, size_t count)
{
    struct vdin_dev_s *devp = dev_get_drvdata(dev);

    if(!strncmp(buf, "start", 5)){
        vf_log_init(devp->vfp);
    }
    else if (!strncmp(buf, "print", 5)) {
        vf_log_print(devp->vfp);
    }
    else
    {
        pr_info("unknow command: %s\n"
                "Usage:\n"
                "a. show log messsage:\n"
                "echo print > /sys/class/vdin/vdin0/vf_log\n"
                "b. restart log message:\n"
                "echo start > /sys/class/vdin/vdin0/vf_log\n"
                "c. show log records\n"
                "cat > /sys/class/vdin/vdin0/vf_log\n" , buf);
    }
    return count;
}

/*
    1. show log length.
    cat /sys/class/vdin/vdin0/vf_log
    cat /sys/class/vdin/vdin1/vf_log
    2. clear log buffer and start log.
    echo start > /sys/class/vdin/vdin0/vf_log
    echo start > /sys/class/vdin/vdin1/vf_log
    3. print log
    echo print > /sys/class/vdin/vdin0/vf_log
    echo print > /sys/class/vdin/vdin1/vf_log
*/
static DEVICE_ATTR(vf_log, S_IWUGO | S_IRUGO, vdin_vf_log_show, vdin_vf_log_store);


static ssize_t vdin_isr_time_show(struct device * dev,
    struct device_attribute *attr, char * buf)
{
    int len = 0;
    struct vdin_dev_s *devp = dev_get_drvdata(dev);

    len += sprintf(buf + len, "interval:%lu, min:%lu, max:%lu, average:%lu less5ms:%lu of %llu\n", devp->v.isr_interval,
        devp->v.min_isr_time, devp->v.max_isr_time, devp->v.avg_isr_time,
        devp->v.less_5ms_cnt, devp->v.isr_count);
    return len;
}

static ssize_t vdin_isr_time_store(struct device * dev,
    struct device_attribute *attr, const char * buf, size_t count)
{
    return count;
}

static DEVICE_ATTR(isr_time, S_IWUGO | S_IRUGO, vdin_isr_time_show, vdin_isr_time_store);

#endif //VF_LOG_EN

static int vdin_probe(struct platform_device *pdev)
{
    int ret;
    int i /*, canvas_cnt*/;
    struct resource *res;

    ret = alloc_chrdev_region(&vdin_devno, 0, VDIN_COUNT, VDIN_NAME);
    if (ret < 0) {
        printk(KERN_ERR "vdin: failed to allocate major number\n");
        return 0;
    }

    vdin_clsp = class_create(THIS_MODULE, VDIN_NAME);
    if (IS_ERR(vdin_clsp))
    {
        unregister_chrdev_region(vdin_devno, VDIN_COUNT);
        return PTR_ERR(vdin_clsp);
    }

    for (i = 0; i < VDIN_COUNT; ++i)
    {
        /* allocate memory for the per-device structure */
        vdin_devp[i] = kmalloc(sizeof(struct vdin_dev_s), GFP_KERNEL);
        if (!vdin_devp[i])
        {
            printk(KERN_ERR "vdin: failed to allocate memory for vdin device\n");
            return -ENOMEM;
        }
        memset(vdin_devp[i], 0, (sizeof(struct vdin_dev_s) ));

        vdin_devp[i]->index = i;

        /* connect the file operations with cdev */
        cdev_init(&vdin_devp[i]->cdev, &vdin_fops);
        vdin_devp[i]->cdev.owner = THIS_MODULE;
        /* connect the major/minor number to the cdev */
        ret = cdev_add(&vdin_devp[i]->cdev, (vdin_devno + i), 1);
        if (ret) {
            printk(KERN_ERR "vdin: failed to add device\n");
            /* @todo do with error */
            return ret;
        }
        /* create /dev nodes */
        vdin_devp[i]->dev = device_create(vdin_clsp, NULL, MKDEV(MAJOR(vdin_devno), i),
                            NULL, "%s%d", VDIN_DEVICE_NAME, i);
        if (IS_ERR(vdin_devp[i]->dev)) {
            printk(KERN_ERR "vdin: failed to create device node\n");
            class_destroy(vdin_clsp);
            /* @todo do with error */
            return PTR_ERR(vdin_devp[i]->dev);
        }
        device_create_file(vdin_devp[i]->dev, &dev_attr_vf_log);
        device_create_file(vdin_devp[i]->dev, &dev_attr_isr_time);

        /* get device memory */
        res = platform_get_resource(pdev, IORESOURCE_MEM, i);
        if (!res) {
            printk(KERN_ERR "vdin: can't get memory resource\n");
            return -EFAULT;
        }
        vdin_devp[i]->mem_start = res->start;
        vdin_devp[i]->mem_size  = res->end - res->start + 1;
        pr_info("vdin[%d] mem_start = 0x%x, mem_size = 0x%x\n",i,
            vdin_devp[i]->mem_start,vdin_devp[i]->mem_size);

        /* get device irq */
        res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
        if (!res) {
            printk(KERN_ERR "vdin: can't get memory resource\n");
            return -EFAULT;
        }
        vdin_devp[i]->irq = res->start;
        vdin_devp[i]->flags = VDIN_FLAG_NULL;

        vdin_devp[i]->addr_offset = vdin_addr_offset[i];
        vdin_devp[i]->flags = 0;
        vdin_devp[i]->vfp = vf_pool_alloc(VDIN_CANVAS_MAX_CNT);
        if (!vdin_devp[i]->vfp) {
            return -ENOMEM;
        }
        /* init vframe provider */
        vf_provider_init(&vdin_vf_prov[i], vdin_devp[i]->name, &vdin_vf_ops, vdin_devp[i]->vfp);

        /* init isr */
        vdin_devp[i]->isr_lock = SPIN_LOCK_UNLOCKED;


        mutex_init(&vdin_devp[i]->mm_lock);
        mutex_init(&vdin_devp[i]->fe_lock);

        if (canvas_config_mode == 0 || canvas_config_mode == 1) {
            vdin_canvas_init(vdin_devp[i]);
        }

        vdin_devp[i]->flags &= (~VDIN_FLAG_FS_OPENED);

        /* set provider name */
        sprintf(vdin_devp[i]->name, "%s%d", PROVIDER_NAME, vdin_devp[i]->index);

        vdin_devp[i]->frontend = NULL;
        dev_set_drvdata(vdin_devp[i]->dev ,vdin_devp[i]);
    }

    printk(KERN_INFO "vdin: driver initialized ok\n");
    return 0;
}

static int vdin_remove(struct platform_device *pdev)
{
    int i = 0;

    for (i = 0; i < VDIN_COUNT; ++i)
    {
        mutex_destroy(&vdin_devp[i]->mm_lock);
        mutex_destroy(&vdin_devp[i]->fe_lock);
        device_remove_file(vdin_devp[i]->dev, &dev_attr_vf_log);
        device_remove_file(vdin_devp[i]->dev, &dev_attr_isr_time);
        device_destroy(vdin_clsp, MKDEV(MAJOR(vdin_devno), i));
        cdev_del(&vdin_devp[i]->cdev);
        vf_pool_free(vdin_devp[i]->vfp);
        kfree(vdin_devp[i]);
    }
    class_destroy(vdin_clsp);
    unregister_chrdev_region(vdin_devno, VDIN_COUNT);

    printk(KERN_ERR "vdin: driver removed ok.\n");
    return 0;
}

#ifdef CONFIG_PM
static int vdin_suspend(struct platform_device *pdev, pm_message_t state)
{
    int i = 0;

    for (i = 0; i < VDIN_COUNT; ++i)
    {
        vdin_enable_module(vdin_devp[i]->addr_offset, false);
    }

    return 0;
}

static int vdin_resume(struct platform_device *pdev)
{
    int i = 0;

    for (i = 0; i < VDIN_COUNT; ++i)
    {
        vdin_enable_module(vdin_devp[i]->addr_offset, true);
    }

    return 0;
}
#endif

static struct platform_driver vdin_driver = {
    .probe      = vdin_probe,
    .remove     = vdin_remove,
#ifdef CONFIG_PM
    .suspend    = vdin_suspend,
    .resume     = vdin_resume,
#endif
    .driver     = {
        .name   = VDIN_DRIVER_NAME,
    }
};

static int __init vdin_init(void)
{
    int ret = 0;
    ret = platform_driver_register(&vdin_driver);
    if (ret != 0) {
        printk(KERN_ERR "failed to register vdin module, error %d\n", ret);
        return -ENODEV;
    }
    return ret;
}

static void __exit vdin_exit(void)
{
    platform_driver_unregister(&vdin_driver);
}

module_init(vdin_init);
module_exit(vdin_exit);

MODULE_DESCRIPTION("AMLOGIC VDIN driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xu Lin <lin.xu@amlogic.com>");

