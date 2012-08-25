/*
 * TVAFE char device driver.
 *
 * Copyright (c) 2010 Bo Yang <bo.yang@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/* Standard Linux headers */
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
#include <linux/mutex.h>

/* Amlogic headers */
#include <linux/amports/canvas.h>
#include <mach/am_regs.h>
#include <linux/amports/vframe.h>

/* Local include */
#include "tvin_global.h"
#include "tvin_frontend.h"
#include "tvafe_regs.h"
#include "tvafe.h"           /* For user used */
#include "tvafe_general.h"   /* For Kernel used only */
#include "tvafe_adc.h"       /* For Kernel used only */
#include "tvafe_cvd.h"       /* For Kernel used only */

#define TVAFE_NAME               "tvafe"
#define TVAFE_DRIVER_NAME        "tvafe"
#define TVAFE_MODULE_NAME        "tvafe"
#define TVAFE_DEVICE_NAME        "tvafe"
#define TVAFE_CLASS_NAME         "tvafe"

#define TVAFE_COUNT              1

static dev_t                     tvafe_devno;
static struct class              *tvafe_clsp;

/* used to set the flag of tvafe_dev_s */
#define TVAFE_FLAG_DEV_OPENED       0x00000010
#define TVAFE_FLAG_DEV_STARTED      0x00000020

typedef struct tvafe_dev_s {
    int                         index;
    struct cdev                 cdev;
    struct timer_list           timer;

    struct tvafe_info_s         tvafe_info;
    tvin_frontend_t             frontend;
    unsigned int                flag;
    struct mutex                afe_mutex;
} tvafe_dev_t;

#define TVAFE_TIMER_INTERVAL    (HZ/100)   //10ms, #define HZ 100


#define TVAFE_ADC_CONFIGURE_INIT     1
#define TVAFE_ADC_CONFIGURE_NORMAL   1|(1<<POWERDOWNZ_BIT)|(1<<RSTDIGZ_BIT) // 7
#define TVAFE_ADC_CONFIGURE_RESET_ON 1|(1<<POWERDOWNZ_BIT)


static struct tvafe_dev_s *tvafe_devp[TVAFE_COUNT];

static void tvafe_vga_pinmux_enable(void)
{
    // diable TCON
    //WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_7, 0,  1, 1);
    // diable DVIN
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 27, 1);
    // HS0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 15, 1);
    // VS0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 14, 1);
    // DDC_SDA0
    //WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 13, 1);
    // DDC_SCL0
    //WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 12, 1);
}

static void tvafe_vga_pinmux_disable(void)
{
    // HS0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 15, 1);
    // VS0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 14, 1);
    // DDC_SDA0
    //WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 13, 1);
    // DDC_SCL0
    //WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 12, 1);
}




void tvafe_timer_handler(unsigned long arg)
{
    struct tvafe_dev_s *devp = (struct tvafe_dev_s *)arg;
    //int tmp_data;
    enum tvin_port_e port = devp->tvafe_info.param.port;

    if (((port >= TVIN_PORT_VGA0) && (port <= TVIN_PORT_VGA7)) &&
        (devp->tvafe_info.vga_auto_flag == 1))
    {
        tvafe_vga_auto_adjust_handler(&devp->tvafe_info);
        if ((devp->tvafe_info.cmd_status == TVAFE_CMD_STATUS_FAILED) ||
            (devp->tvafe_info.cmd_status == TVAFE_CMD_STATUS_SUCCESSFUL)
           )
            devp->tvafe_info.vga_auto_flag = 0;
    }
    devp->timer.expires = jiffies + TVAFE_TIMER_INTERVAL;
    add_timer(&devp->timer);
}

int tvafe_dec_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
    if ((port < TVIN_PORT_VGA0) || (port > TVIN_PORT_SVIDEO7))
        return -1;
    return 0;
}

void tvafe_dec_open(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->tvafe_info.param;

    mutex_lock(&devp->afe_mutex);
    if (devp->flag & TVAFE_FLAG_DEV_OPENED)
    {
        pr_err("%s(%d), %s opened already\n", __func__,
            devp->index, tvin_port_str(port));
        mutex_unlock(&devp->afe_mutex);
        return ;
    }

    devp->tvafe_info.param.port = port;

    /* init variable */
    parm->info.fmt = TVIN_SIG_FMT_NULL;
    parm->info.status = TVIN_SIG_STATUS_NULL;
    devp->tvafe_info.last_fmt = TVIN_SIG_FMT_NULL; //reset last fmt after source switch
    devp->tvafe_info.adc_cal_val.reserved &= ~TVAFE_ADC_CAL_VALID;
    if (((parm->port >= TVIN_PORT_VGA0)  &&
         (parm->port <= TVIN_PORT_VGA7)) ||
        ((parm->port >= TVIN_PORT_COMP0) &&
         (parm->port <= TVIN_PORT_COMP7)))
    {
        memset(&devp->tvafe_info.operand, 0, sizeof(struct tvafe_operand_s));
    }
    else if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS7))
    {
        devp->tvafe_info.cvd2_info.s_video = false;
        if (port != TVIN_PORT_CVBS0)
            devp->tvafe_info.cvd2_info.tuner = false;
        else
            devp->tvafe_info.cvd2_info.tuner = true;
    }
    else // S_VIDEO0
    {
        devp->tvafe_info.cvd2_info.s_video = true;
        devp->tvafe_info.cvd2_info.tuner = false;
    }

    devp->tvafe_info.vga_auto_flag = 0;

    /**enable tvafe clock**/
    tvafe_enable_module(true);

    switch(devp->tvafe_info.param.port)
    {
        case TVIN_PORT_VGA0:
            tvafe_vga_pinmux_enable();
            break;

        default:
            break;
    }

    if ((port >= TVIN_PORT_VGA0) && (port <= TVIN_PORT_VGA7))
        tvafe_set_vga_default(TVIN_SIG_FMT_VGA_1024X768P_60D004);
    else if ((port >= TVIN_PORT_COMP0) && (port <= TVIN_PORT_COMP7))
        tvafe_set_comp_default(TVIN_SIG_FMT_COMP_720P_59D940);
    else if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_SVIDEO7))
    {
        tvafe_cvd2_set_default(&devp->tvafe_info.cvd2_info);
    }

    tvafe_source_muxing(&devp->tvafe_info);

    /* timer */
    init_timer(&devp->timer);
    devp->timer.data = (ulong)devp;
    devp->timer.function = tvafe_timer_handler;
    devp->timer.expires = jiffies + (TVAFE_TIMER_INTERVAL);
    add_timer(&devp->timer);

    /* set the flag to enabble ioctl access */
    devp->flag |= TVAFE_FLAG_DEV_OPENED;
    pr_info("%s ok.\n", __func__);

    mutex_unlock(&devp->afe_mutex);
}

void tvafe_dec_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt)
{
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->tvafe_info.param;

    mutex_lock(&devp->afe_mutex);
    if(!(devp->flag & TVAFE_FLAG_DEV_OPENED))
    {
        pr_err("tvafe_dec_start(%d) decode havn't opened\n", devp->index);
        mutex_unlock(&devp->afe_mutex);
        return;
    }

    if (devp->flag & TVAFE_FLAG_DEV_STARTED)
    {
        pr_err("%s(%d), %s started already\n", __func__,
            devp->index, tvin_port_str(parm->port));
        mutex_unlock(&devp->afe_mutex);
        return;
    }

    parm->info.fmt = fmt;
    parm->info.status = TVIN_SIG_STATUS_STABLE;

    if ((parm->port >= TVIN_PORT_CVBS0) &&
        (parm->port <= TVIN_PORT_CVBS7))
    {
#ifdef TVAFE_SET_CVBS_PGA_EN
        devp->tvafe_info.cvd2_info.dgain_cnt = 0;
#endif
#ifdef TVAFE_SET_CVBS_CDTO_EN
        devp->tvafe_info.cvd2_info.hcnt64_cnt = 0;
#endif
    }

    if (((parm->port >= TVIN_PORT_VGA0)  &&
         (parm->port <= TVIN_PORT_VGA7)) ||
        ((parm->port >= TVIN_PORT_COMP0) &&
         (parm->port <= TVIN_PORT_COMP7)))
    {
        memset(&devp->tvafe_info.operand, 0, sizeof(struct tvafe_operand_s));
    }

    tvafe_source_muxing(&devp->tvafe_info); // why again?
    devp->flag |= TVAFE_FLAG_DEV_STARTED;
    pr_info("%s ok.\n", __func__);

    mutex_unlock(&devp->afe_mutex);
}

void tvafe_dec_stop(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->tvafe_info.param;

    mutex_lock(&devp->afe_mutex);
    if(!(devp->flag & TVAFE_FLAG_DEV_STARTED))
    {
        pr_err("tvafe_dec_stop(%d) decode havn't started\n", devp->index);
        mutex_unlock(&devp->afe_mutex);
        return;
    }

    #if 0
    if ((parm->port >= TVIN_PORT_CVBS0) &&
        (parm->port <= TVIN_PORT_SVIDEO7))
        tvafe_cvd2_try_format(&devp->tvafe_info.cvd2_info, TVIN_SIG_FMT_CVBS_PAL_I);
    #endif
    parm->info.fmt = TVIN_SIG_FMT_NULL;
    parm->info.status = TVIN_SIG_STATUS_NULL;
    devp->tvafe_info.last_fmt = TVIN_SIG_FMT_NULL; //reset last fmt after source switch
    devp->tvafe_info.adc_cal_val.reserved &= ~TVAFE_ADC_CAL_VALID;
    if (((parm->port >= TVIN_PORT_VGA0)  &&
         (parm->port <= TVIN_PORT_VGA7)) ||
        ((parm->port >= TVIN_PORT_COMP0) &&
         (parm->port <= TVIN_PORT_COMP7)))
    {
        memset(&devp->tvafe_info.operand, 0, sizeof(struct tvafe_operand_s));
    }

    // tvafe_reset_module();
    tvafe_adc_digital_reset();
    // need to do ...
#ifdef TVAFE_ADC_CVBS_CLAMP_SEQUENCE_EN
    /** write 7740 register for cvbs clamp **/
    if ((parm->port >= TVIN_PORT_CVBS0) &&
        (parm->port <= TVIN_PORT_SVIDEO7))
    {
        tvafe_cvd2_reset_pga(&devp->tvafe_info.cvd2_info);
        tvafe_adc_cvbs_clamp_sequence();
    }
#endif
    devp->flag &= (~TVAFE_FLAG_DEV_STARTED);
    pr_info("%s ok.\n", __func__);

    mutex_unlock(&devp->afe_mutex);
}

// #define TVAFE_POWERDOWN_IN_IDLE
void tvafe_dec_close(struct tvin_frontend_s *fe)
{
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->tvafe_info.param;

    mutex_lock(&devp->afe_mutex);
    if(!(devp->flag & TVAFE_FLAG_DEV_OPENED))
    {
        pr_err("tvafe_dec_close(%d) decode havn't opened\n", devp->index);
        mutex_unlock(&devp->afe_mutex);
        return;
    }

    del_timer_sync(&devp->timer);
    switch(parm->port)
    {
        case TVIN_PORT_VGA0:
            tvafe_vga_pinmux_disable();
            break;

        default:
            break;
    }

#ifdef TVAFE_POWERDOWN_IN_IDLE
    /**disable tvafe clock**/
    tvafe_enable_module(false);
#endif

    parm->info.fmt = TVIN_SIG_FMT_NULL;
    parm->info.status = TVIN_SIG_STATUS_NULL;
    devp->tvafe_info.last_fmt = TVIN_SIG_FMT_NULL; //reset last fmt after source switch
    devp->tvafe_info.adc_cal_val.reserved &= ~TVAFE_ADC_CAL_VALID;
    if (((parm->port >= TVIN_PORT_VGA0)  &&
         (parm->port <= TVIN_PORT_VGA7)) ||
        ((parm->port >= TVIN_PORT_COMP0) &&
         (parm->port <= TVIN_PORT_COMP7)))
    {
        memset(&devp->tvafe_info.operand, 0, sizeof(struct tvafe_operand_s));
    }

    devp->flag &= (~TVAFE_FLAG_DEV_OPENED);
    pr_info("%s ok.\n", __func__);

    mutex_unlock(&devp->afe_mutex);
}

int tvafe_dec_isr(struct tvin_frontend_s *fe, unsigned int hcnt64)
{
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->tvafe_info.param;

    /* if there is any error or overflow, do some reset, then rerurn -1;*/
    if ((parm->info.status != TVIN_SIG_STATUS_STABLE) ||
        (parm->info.fmt == TVIN_SIG_FMT_NULL)) {
        return -1;
    }

    /* TVAFE CVD2 3D works abnormally => reset cvd2 */
    if ((parm->port >= TVIN_PORT_CVBS0) &&
        (parm->port <= TVIN_PORT_CVBS7))
    {
        tvafe_check_cvbs_3d_comb(&devp->tvafe_info.cvd2_info);
    }

#ifdef TVAFE_SET_CVBS_PGA_EN
    if ((parm->port >= TVIN_PORT_CVBS0) &&
        (parm->port <= TVIN_PORT_SVIDEO7))
    {
        tvafe_set_cvbs_pga(&devp->tvafe_info.cvd2_info);
    }
#endif

#ifdef TVAFE_SET_CVBS_CDTO_EN
    if (parm->info.fmt == TVIN_SIG_FMT_CVBS_PAL_I)
    {
        tvafe_set_cvbs_cdto(&devp->tvafe_info.cvd2_info,hcnt64);
    }
#endif

    //tvafe_adc_clamp_adjust(&devp->tvafe_info);
    tvafe_vga_vs_cnt();

    // fetch WSS data must get them during VBI
    tvafe_get_wss_data(&devp->tvafe_info.comp_wss);

    return 0;
}

static struct tvin_decoder_ops_s tvafe_dec_ops = {
    .support    = tvafe_dec_support,
    .open       = tvafe_dec_open,
    .start      = tvafe_dec_start,
    .stop       = tvafe_dec_stop,
    .close      = tvafe_dec_close,
    .decode_isr = tvafe_dec_isr,
};


bool tvafe_is_nosig(struct tvin_frontend_s *fe)
{
    bool ret = 0;
    /* Get the per-device structure that contains this frontend */
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->tvafe_info.param;

    #ifdef LOG_ADC_CAL
    struct tvafe_operand_s *operand = &devp->tvafe_info.operand;
    struct tvafe_adc_cal_s *adc_cal = &devp->tvafe_info.adc_cal_val;
    #endif

    if ((parm->port >= TVIN_PORT_VGA0) &&
        (parm->port <= TVIN_PORT_COMP7))
        ret = tvafe_adc_no_sig();
    else if ((parm->port >= TVIN_PORT_CVBS0) &&
        (parm->port <= TVIN_PORT_SVIDEO7))
    {
        ret = tvafe_cvd2_no_sig(&devp->tvafe_info.cvd2_info);

        /* normal sigal & adc reg error, reload source mux */
        if (devp->tvafe_info.cvd2_info.adc_reload_en && !ret)
        {
            devp->tvafe_info.cvd2_info.adc_reload_en = false;
            tvafe_source_muxing(&devp->tvafe_info);
        }
    }
    #ifdef LOG_ADC_CAL
    if (operand->adj)
    {
        operand->adj = 0;
        pr_info("\nadc_clamp_adjust\n");
        pr_info("Data = %4d %4d %4d\n",        operand->data0,
                                               operand->data1,
                                               operand->data2);
        pr_info("A_cl = %4d %4d %4d\n",  (int)(adc_cal->a_analog_clamp),
                                         (int)(adc_cal->b_analog_clamp),
                                         (int)(adc_cal->c_analog_clamp));
        pr_info("A_gn = %4d %4d %4d\n",  (int)(adc_cal->a_analog_gain),
                                         (int)(adc_cal->b_analog_gain),
                                         (int)(adc_cal->c_analog_gain));
        pr_info("D_gn = %4d %4d %4d\n",  (int)(adc_cal->a_digital_gain),
                                         (int)(adc_cal->b_digital_gain),
                                         (int)(adc_cal->c_digital_gain));
        pr_info("D_o1 = %4d %4d %4d\n", ((int)(adc_cal->a_digital_offset1) << 21) >> 21,
                                        ((int)(adc_cal->b_digital_offset1) << 21) >> 21,
                                        ((int)(adc_cal->c_digital_offset1) << 21) >> 21);
        pr_info("D_o2 = %4d %4d %4d\n", ((int)(adc_cal->a_digital_offset2) << 21) >> 21,
                                        ((int)(adc_cal->b_digital_offset2) << 21) >> 21,
                                        ((int)(adc_cal->c_digital_offset2) << 21) >> 21);
        pr_info("\n");
    }
    #endif
    return ret;
}

bool tvafe_fmt_chg(struct tvin_frontend_s *fe)
{
    bool ret = false;
    /* Get the per-device structure that contains this frontend */
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->tvafe_info.param;

    if ((parm->port >= TVIN_PORT_VGA0) &&
        (parm->port <= TVIN_PORT_COMP7))
        ret = tvafe_adc_fmt_chg(&devp->tvafe_info);
    else  if ((parm->port >= TVIN_PORT_CVBS0) &&
        (parm->port <= TVIN_PORT_SVIDEO7))
        ret = tvafe_cvd2_fmt_chg(&devp->tvafe_info.cvd2_info);
    return ret;
}

bool tvafe_pll_lock(struct tvin_frontend_s *fe)
{
    bool ret = true;

#if 0  //can not trust pll lock status
    /* Get the per-device structure that contains this frontend */
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->tvafe_info.param;

    if ((parm->port >= TVIN_PORT_VGA0) &&
        (parm->port <= TVIN_PORT_COMP7))
        ret = tvafe_adc_get_pll_status();
#endif
    return (ret);
}

enum tvin_sig_fmt_e tvafe_get_fmt(struct tvin_frontend_s *fe)
{
    enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
    /* Get the per-device structure that contains this frontend */
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->tvafe_info.param;

    if ((parm->port >= TVIN_PORT_VGA0) &&
        (parm->port <= TVIN_PORT_COMP7))
        fmt = tvafe_adc_search_mode(parm->port);
    else  if ((parm->port >= TVIN_PORT_CVBS0) &&
        (parm->port <= TVIN_PORT_SVIDEO7))
        fmt = tvafe_cvd2_get_format(&devp->tvafe_info.cvd2_info);

    parm->info.fmt = fmt;
    return fmt;
}

#ifdef TVAFE_SET_CVBS_MANUAL_FMT_POS
enum tvin_cvbs_pos_ctl_e tvafe_set_cvbs_fmt_pos(struct tvin_frontend_s *fe)
{
    /* Get the per-device structure that contains this frontend */
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvafe_cvd2_info_s *cvd2_info = &devp->tvafe_info.cvd2_info;

    enum tvin_cvbs_pos_ctl_e cvbs_pos_ctl = TVIN_CVBS_POS_NULL;

    if (cvd2_info->manual_fmt == TVIN_SIG_FMT_CVBS_PAL_I)
    {
        if (!cvd2_info->data.line625)  //wrong format, change video size
        {
            cvbs_pos_ctl = TVIN_CVBS_POS_P_TO_N;
            tvafe_cvd2_set_video_positon(cvd2_info, TVIN_SIG_FMT_CVBS_NTSC_M);
        }
        else  //right format, reload video size
        {
            cvbs_pos_ctl = TVIN_CVBS_POS_P_TO_P;
            tvafe_cvd2_set_video_positon(cvd2_info, TVIN_SIG_FMT_CVBS_PAL_I);
        }
    }
    else if ((cvd2_info->manual_fmt == TVIN_SIG_FMT_CVBS_NTSC_M) )
    {
        if (cvd2_info->data.line625)  //wrong format, change video size
        {
            cvbs_pos_ctl = TVIN_CVBS_POS_N_TO_P;
            tvafe_cvd2_set_video_positon(cvd2_info, TVIN_SIG_FMT_CVBS_PAL_I);
        }
        else  //right format, reload video size
        {
            cvbs_pos_ctl = TVIN_CVBS_POS_N_TO_N;
            tvafe_cvd2_set_video_positon(cvd2_info, TVIN_SIG_FMT_CVBS_NTSC_M);
        }
    }
    else  // reset default postion
    {
        tvafe_cvd2_set_video_positon(cvd2_info, cvd2_info->config_fmt);
    }

    return (cvbs_pos_ctl);
}
#endif

void tvafe_get_sig_propery(struct tvin_frontend_s *fe, struct tvin_sig_property_s *prop)
{
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->tvafe_info.param;

    prop->trans_fmt = TVIN_TFMT_2D;
    if ((parm->port >= TVIN_PORT_VGA0) &&
        (parm->port <= TVIN_PORT_VGA7))
        prop->color_format = TVIN_RGB444;
    else
        prop->color_format = TVIN_YUV444;
    prop->aspect_ratio = TVIN_ASPECT_NULL;
    prop->pixel_repeat= 0;
}

void tvafe_vga_set_parm(struct tvafe_vga_parm_s *vga_parm, struct tvin_frontend_s *fe)
{
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvafe_info_s *info = &devp->tvafe_info;

    tvafe_adc_set_param(vga_parm, info);
}

void tvafe_vga_get_parm(struct tvafe_vga_parm_s *vga_parm, struct tvin_frontend_s *fe)
{
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvafe_vga_parm_s *parm = &devp->tvafe_info.vga_parm;

    vga_parm->clk_step     = parm->clk_step;
    vga_parm->phase        = parm->phase;
    vga_parm->hpos_step    = parm->hpos_step;
    vga_parm->vpos_step    = parm->vpos_step;
    vga_parm->vga_in_clean = parm->vga_in_clean;
}

void tvafe_adc_monitor(enum tvin_sig_fmt_e fmt)
{
    unsigned int x = tvin_fmt_tbl[fmt].hs_width + tvin_fmt_tbl[fmt].hs_bp + ((tvin_fmt_tbl[fmt].h_active + 1) >> 1);
    unsigned int y = tvin_fmt_tbl[fmt].vs_width + tvin_fmt_tbl[fmt].vs_bp - 10;

    WRITE_APB_REG(TVFE_ADC_READBACK_CTRL, (1 << 31) |
                                          (1 << 29) |
                                          (x << 16) |
                                          (1 << 13) |
                                          (y <<  0));
}

void fmt_config(struct tvin_frontend_s *fe)
{
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->tvafe_info.param;

    /* do not need to configure format again if format remains unchanged */
    if (devp->tvafe_info.last_fmt == parm->info.fmt)
        return;

    if ((parm->port >= TVIN_PORT_COMP0) &&
        (parm->port <= TVIN_PORT_COMP7)) {
        tvafe_set_comp_default(parm->info.fmt);
        tvafe_source_muxing(&devp->tvafe_info);
        tvafe_set_cal_value(&devp->tvafe_info.adc_cal_val);
        tvafe_adc_monitor(parm->info.fmt);
    }
    if ((parm->port >= TVIN_PORT_VGA0) &&
        (parm->port <= TVIN_PORT_VGA7)) {
        tvafe_set_vga_default(parm->info.fmt);
        tvafe_source_muxing(&devp->tvafe_info);
        tvafe_set_cal_value(&devp->tvafe_info.adc_cal_val);
        tvafe_adc_monitor(parm->info.fmt);
    }
    devp->tvafe_info.last_fmt = parm->info.fmt;

}

bool adc_cal(struct tvin_frontend_s *fe)
{
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);

    return tvafe_adc_cal(&devp->tvafe_info, &devp->tvafe_info.operand);
}

bool tvafe_check_frame_skip(struct tvin_frontend_s *fe)
{
    bool ret = false;
    struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->tvafe_info.param;

    if (((parm->port >= TVIN_PORT_COMP0) && (parm->port <= TVIN_PORT_COMP7)) ||
        ((parm->port >= TVIN_PORT_VGA0) && (parm->port <= TVIN_PORT_VGA7))) {
        ret = tvafe_adc_check_frame_skip();
    }

    return ret;
}

static struct tvin_state_machine_ops_s tvafe_sm_ops = {
    .nosig            = tvafe_is_nosig,
    .fmt_changed      = tvafe_fmt_chg,
    .get_fmt          = tvafe_get_fmt,
    .fmt_config       = fmt_config,
    .adc_cal          = adc_cal,
    .pll_lock         = tvafe_pll_lock,
    .get_sig_propery  = tvafe_get_sig_propery,
#ifdef TVAFE_SET_CVBS_MANUAL_FMT_POS
    .set_cvbs_fmt_pos = tvafe_set_cvbs_fmt_pos,
#endif
    .vga_set_param    = tvafe_vga_set_parm,
	.vga_get_param    = tvafe_vga_get_parm,
	.check_frame_skip = tvafe_check_frame_skip,
};

static int tvafe_open(struct inode *inode, struct file *file)
{
    tvafe_dev_t *devp;

    /* Get the per-device structure that contains this cdev */
    devp = container_of(inode->i_cdev, tvafe_dev_t, cdev);
    file->private_data = devp;

    /* ... */

    pr_info("%s: open device \n", __FUNCTION__);

    return 0;
}

static int tvafe_release(struct inode *inode, struct file *file)
{
    tvafe_dev_t *devp = file->private_data;

    file->private_data = NULL;

    /* Release some other fields */
    /* ... */

    pr_info("tvafe: device %d release ok.\n", devp->index);

    return 0;
}


static int tvafe_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct tvafe_vga_edid_s edid;
    enum tvafe_cvbs_video_e cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_UNLOCKED;
    void __user *argp = (void __user *)arg;
    struct tvafe_dev_s *devp;
	struct tvin_parm_s *parm;

    devp = container_of(inode->i_cdev, struct tvafe_dev_s, cdev);
	parm = &devp->tvafe_info.param;

    mutex_lock(&devp->afe_mutex);

    /* EDID command !!! */
    if (((cmd != TVIN_IOC_S_AFE_VGA_EDID) && (cmd != TVIN_IOC_G_AFE_VGA_EDID)) &&
        (!(devp->flag & TVAFE_FLAG_DEV_OPENED))
        )
    {
        pr_info("%s, tvafe device is disable, ignore the command %d\n", __func__, cmd);
        mutex_unlock(&devp->afe_mutex);
        return -EPERM;
    }

    switch (cmd)
    {
        case TVIN_IOC_S_AFE_ADC_CAL:
            if (copy_from_user(&devp->tvafe_info.adc_cal_val, argp, sizeof(struct tvafe_adc_cal_s)))
            {
                ret = -EFAULT;
                break;
            }

            devp->tvafe_info.adc_cal_val.reserved |= TVAFE_ADC_CAL_VALID;
            tvafe_vga_auto_adjust_disable(&devp->tvafe_info);
			if ((parm->port >= TVIN_PORT_COMP0) &&(parm->port <= TVIN_PORT_COMP7))
				{
				if((parm->info.fmt>=TVIN_SIG_FMT_COMP_1080P_23D976)&&(parm->info.fmt<=TVIN_SIG_FMT_COMP_1080P_60D000))
				    devp->tvafe_info.adc_cal_val.a_analog_clamp += 7;
				else if((parm->info.fmt>=TVIN_SIG_FMT_COMP_1080I_47D952)&&(parm->info.fmt<=TVIN_SIG_FMT_COMP_1080I_60D000))
					devp->tvafe_info.adc_cal_val.a_analog_clamp += 6;
				else
				    devp->tvafe_info.adc_cal_val.a_analog_clamp += 5;
				devp->tvafe_info.adc_cal_val.b_analog_clamp += 1;
				devp->tvafe_info.adc_cal_val.c_analog_clamp += 2;
				}
			else if((parm->port >= TVIN_PORT_VGA0) &&(parm->port <= TVIN_PORT_VGA7))
				{
				devp->tvafe_info.adc_cal_val.a_analog_clamp += 5;
				devp->tvafe_info.adc_cal_val.b_analog_clamp += 5;
				devp->tvafe_info.adc_cal_val.c_analog_clamp += 5;
				}
            //pr_info("\nNot allow to use TVIN_IOC_S_AFE_ADC_CAL command!!!\n\n");
            tvafe_set_cal_value(&devp->tvafe_info.adc_cal_val);
            #ifdef LOG_ADC_CAL
                pr_info("\nset_adc_cal\n");
                pr_info("A_cl = %4d %4d %4d\n",  (int)(devp->tvafe_info.adc_cal_val.a_analog_clamp),
                                                 (int)(devp->tvafe_info.adc_cal_val.b_analog_clamp),
                                                 (int)(devp->tvafe_info.adc_cal_val.c_analog_clamp));
                pr_info("A_gn = %4d %4d %4d\n",  (int)(devp->tvafe_info.adc_cal_val.a_analog_gain),
                                                 (int)(devp->tvafe_info.adc_cal_val.b_analog_gain),
                                                 (int)(devp->tvafe_info.adc_cal_val.c_analog_gain));
                pr_info("D_gn = %4d %4d %4d\n",  (int)(devp->tvafe_info.adc_cal_val.a_digital_gain),
                                                 (int)(devp->tvafe_info.adc_cal_val.b_digital_gain),
                                                 (int)(devp->tvafe_info.adc_cal_val.c_digital_gain));
                pr_info("D_o1 = %4d %4d %4d\n", ((int)(devp->tvafe_info.adc_cal_val.a_digital_offset1) << 21) >> 21,
                                                ((int)(devp->tvafe_info.adc_cal_val.b_digital_offset1) << 21) >> 21,
                                                ((int)(devp->tvafe_info.adc_cal_val.c_digital_offset1) << 21) >> 21);
                pr_info("D_o2 = %4d %4d %4d\n", ((int)(devp->tvafe_info.adc_cal_val.a_digital_offset2) << 21) >> 21,
                                                ((int)(devp->tvafe_info.adc_cal_val.b_digital_offset2) << 21) >> 21,
                                                ((int)(devp->tvafe_info.adc_cal_val.c_digital_offset2) << 21) >> 21);
                pr_info("\n");
            #endif

            break;

        case TVIN_IOC_G_AFE_ADC_CAL:
            tvafe_get_cal_value(&devp->tvafe_info.adc_cal_val);
            #ifdef LOG_ADC_CAL
                pr_info("\nget_adc_cal\n");
                pr_info("A_cl = %4d %4d %4d\n",  (int)(devp->tvafe_info.adc_cal_val.a_analog_clamp),
                                                 (int)(devp->tvafe_info.adc_cal_val.b_analog_clamp),
                                                 (int)(devp->tvafe_info.adc_cal_val.c_analog_clamp));
                pr_info("A_gn = %4d %4d %4d\n",  (int)(devp->tvafe_info.adc_cal_val.a_analog_gain),
                                                 (int)(devp->tvafe_info.adc_cal_val.b_analog_gain),
                                                 (int)(devp->tvafe_info.adc_cal_val.c_analog_gain));
                pr_info("D_gn = %4d %4d %4d\n",  (int)(devp->tvafe_info.adc_cal_val.a_digital_gain),
                                                 (int)(devp->tvafe_info.adc_cal_val.b_digital_gain),
                                                 (int)(devp->tvafe_info.adc_cal_val.c_digital_gain));
                pr_info("D_o1 = %4d %4d %4d\n", ((int)(devp->tvafe_info.adc_cal_val.a_digital_offset1) << 21) >> 21,
                                                ((int)(devp->tvafe_info.adc_cal_val.b_digital_offset1) << 21) >> 21,
                                                ((int)(devp->tvafe_info.adc_cal_val.c_digital_offset1) << 21) >> 21);
                pr_info("D_o2 = %4d %4d %4d\n", ((int)(devp->tvafe_info.adc_cal_val.a_digital_offset2) << 21) >> 21,
                                                ((int)(devp->tvafe_info.adc_cal_val.b_digital_offset2) << 21) >> 21,
                                                ((int)(devp->tvafe_info.adc_cal_val.c_digital_offset2) << 21) >> 21);
                pr_info("\n");
            #endif
            if (copy_to_user(argp, &devp->tvafe_info.adc_cal_val, sizeof(struct tvafe_adc_cal_s)))
            {
                ret = -EFAULT;
                break;
            }

            break;
        case TVIN_IOC_G_AFE_COMP_WSS:
            if (copy_to_user(argp, &devp->tvafe_info.comp_wss, sizeof(struct tvafe_comp_wss_s)))
            {
                ret = -EFAULT;
                break;
            }
            break;
        case TVIN_IOC_S_AFE_VGA_EDID:
            if (copy_from_user(&edid, argp, sizeof(struct tvafe_vga_edid_s)))
            {
                ret = -EFAULT;
                break;
            }
            tvafe_vga_set_edid(&edid);

            break;
        case TVIN_IOC_G_AFE_VGA_EDID:
            tvafe_vga_get_edid(&edid);
            if (copy_to_user(argp, &edid, sizeof(struct tvafe_vga_edid_s)))
            {
                ret = -EFAULT;
                break;
            }
            break;
        case TVIN_IOC_S_AFE_VGA_PARM:
        {
            if (copy_from_user(&devp->tvafe_info.vga_parm, argp, sizeof(struct tvafe_vga_parm_s)))
            {
                ret = -EFAULT;
                break;
            }
            tvafe_vga_auto_adjust_disable(&devp->tvafe_info);
            //tvafe_adc_set_param(&devp->tvafe_info.vga_parm, &devp->tvafe_info);

            break;
        }
        case TVIN_IOC_G_AFE_VGA_PARM:
        {
            struct tvafe_vga_parm_s vga_parm = {0, 0, 0, 0, 0};
            tvafe_adc_get_param(&vga_parm, &devp->tvafe_info);
            if (copy_to_user(argp, &vga_parm, sizeof(struct tvafe_vga_parm_s)))
            {
                ret = -EFAULT;
                break;
            }
            break;
        }
        case TVIN_IOC_S_AFE_VGA_AUTO:
            ret = tvafe_vga_auto_adjust_enable(&devp->tvafe_info);
            break;
        case TVIN_IOC_G_AFE_CMD_STATUS:
            if (copy_to_user(argp, &devp->tvafe_info.cmd_status, sizeof(enum tvafe_cmd_status_e)))
            {
                ret = -EFAULT;
                break;
            }
            if ((devp->tvafe_info.cmd_status == TVAFE_CMD_STATUS_SUCCESSFUL)
                || (devp->tvafe_info.cmd_status == TVAFE_CMD_STATUS_FAILED)
                || (devp->tvafe_info.cmd_status == TVAFE_CMD_STATUS_TERMINATED))
            {
                devp->tvafe_info.cmd_status = TVAFE_CMD_STATUS_IDLE;
            }

            break;
        case TVIN_IOC_G_AFE_CVBS_LOCK:
            if (!devp->tvafe_info.cvd2_info.data.h_lock && !devp->tvafe_info.cvd2_info.data.v_lock)
                cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_UNLOCKED;
            else if (devp->tvafe_info.cvd2_info.data.h_lock)
                cvbs_lock_status = TVAFE_CVBS_VIDEO_H_LOCKED;
            else if (devp->tvafe_info.cvd2_info.data.v_lock)
                cvbs_lock_status = TVAFE_CVBS_VIDEO_V_LOCKED;
            else if (devp->tvafe_info.cvd2_info.data.h_lock && devp->tvafe_info.cvd2_info.data.v_lock)
                cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_LOCKED;
            if (copy_to_user(argp, &cvbs_lock_status, sizeof(int)))
            {
                ret = -EFAULT;
                break;
            }
            break;
        case TVIN_IOC_S_AFE_CVBS_STD:
        {
            enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;

            if (copy_from_user(&fmt, argp, sizeof(enum tvin_sig_fmt_e))) {
                ret = -EFAULT;
                break;
            }
            devp->tvafe_info.cvd2_info.manual_fmt = fmt;
            pr_info("%s: ioctl set cvd2 manual fmt:%s. \n", __func__, tvin_sig_fmt_str(fmt));
            break;
        }
        default:
            ret = -ENOIOCTLCMD;
            break;
    }

    mutex_unlock(&devp->afe_mutex);
    return ret;
}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations tvafe_fops = {
    .owner   = THIS_MODULE,         /* Owner */
    .open    = tvafe_open,          /* Open method */
    .release = tvafe_release,       /* Release method */
    .ioctl   = tvafe_ioctl,         /* Ioctl method */
    /* ... */
};

static int tvafe_probe(struct platform_device *pdev)
{
    int ret;
    int i;
    struct device *devp;
    struct resource *res;
    //struct tvin_frontend_s * frontend;

    ret = alloc_chrdev_region(&tvafe_devno, 0, TVAFE_COUNT, TVAFE_NAME);
    if (ret < 0) {
        pr_err("tvafe: failed to allocate major number\n");
        return 0;
    }

    tvafe_clsp = class_create(THIS_MODULE, TVAFE_NAME);
    if (IS_ERR(tvafe_clsp))
    {
        pr_err("tvafe: can't get tvafe_clsp\n");
        unregister_chrdev_region(tvafe_devno, TVAFE_COUNT);
        return PTR_ERR(tvafe_clsp);
    }

    for (i = 0; i < TVAFE_COUNT; ++i)
    {
        /* allocate memory for the per-device structure */
        tvafe_devp[i] = kmalloc(sizeof(struct tvafe_dev_s), GFP_KERNEL);
        if (!tvafe_devp[i])
        {
            pr_err("tvafe: failed to allocate memory for tvafe device\n");
            return -ENOMEM;
        }
        memset(tvafe_devp[i], 0, sizeof(struct tvafe_dev_s));

        tvafe_devp[i]->index = i;

        /* connect the file operations with cdev */
        cdev_init(&tvafe_devp[i]->cdev, &tvafe_fops);
        tvafe_devp[i]->cdev.owner = THIS_MODULE;
        /* connect the major/minor number to the cdev */
        ret = cdev_add(&tvafe_devp[i]->cdev, (tvafe_devno + i), 1);
        if (ret) {
            pr_err("tvafe: failed to add device\n");
            /* @todo do with error */
            return ret;
        }
        /* create /dev nodes */
        devp = device_create(tvafe_clsp, NULL, MKDEV(MAJOR(tvafe_devno), i),
                            NULL, "tvafe%d", i);
        if (IS_ERR(devp)) {
            pr_err("tvafe: failed to create device node\n");
            class_destroy(tvafe_clsp);
            /* @todo do with error */
            return PTR_ERR(devp);
        }
        /* get device memory */
        res = platform_get_resource(pdev, IORESOURCE_MEM, i);
        if (!res) {
            pr_err("tvafe: can't get memory resource\n");
            return -EFAULT;
        }
        tvafe_devp[i]->tvafe_info.cvd2_info.mem_addr = res->start;
        tvafe_devp[i]->tvafe_info.cvd2_info.mem_size = res->end - res->start + 1;
        pr_info(" tvafe[%d] cvd memory addr is:0x%x, cvd mem_size is:0x%x . \n",i,
            tvafe_devp[i]->tvafe_info.cvd2_info.mem_addr,
            tvafe_devp[i]->tvafe_info.cvd2_info.mem_size);
	    tvafe_devp[i]->tvafe_info.pinmux = pdev->dev.platform_data;
	    if (!tvafe_devp[i]->tvafe_info.pinmux) {
		    pr_err("tvafe: no platform data!\n");
		    ret = -ENODEV;
	    }

        /* frontend */
        tvin_frontend_init(&tvafe_devp[i]->frontend, &tvafe_dec_ops, &tvafe_sm_ops, i);
        sprintf(tvafe_devp[i]->frontend.name, "%s%d", TVAFE_NAME, i);
        tvin_reg_frontend(&tvafe_devp[i]->frontend);

        mutex_init(&tvafe_devp[i]->afe_mutex);

        /* set APB bus register accessing error exception */
        WRITE_APB_REG(TVFE_APB_ERR_CTRL_MUX1, 0x8fff8fff);
        WRITE_APB_REG(TVFE_APB_ERR_CTRL_MUX2, 0x00008fff);

    }
    pr_info("tvafe: driver probe ok\n");
    return 0;
}

static int tvafe_remove(struct platform_device *pdev)
{
    int i = 0;

    for (i = 0; i < TVAFE_COUNT; ++i)
    {
        mutex_destroy(&tvafe_devp[i]->afe_mutex);
        tvin_unreg_frontend(&tvafe_devp[i]->frontend);
        device_destroy(tvafe_clsp, MKDEV(MAJOR(tvafe_devno), i));
        cdev_del(&tvafe_devp[i]->cdev);
        kfree(tvafe_devp[i]);
    }
    class_destroy(tvafe_clsp);
    unregister_chrdev_region(tvafe_devno, TVAFE_COUNT);
    pr_info("tvafe: driver removed ok.\n");
    return 0;
}

#ifdef CONFIG_PM
static int tvafe_suspend(struct platform_device *pdev,pm_message_t state)
{
    int i = 0;

    for (i = 0; i < TVAFE_COUNT; ++i)
        tvafe_devp[i]->flag &= (~TVAFE_FLAG_DEV_OPENED);
    tvafe_enable_module(false);
    pr_info("tvafe: suspend module\n");
    return 0;
}

static int tvafe_resume(struct platform_device *pdev)
{
    pr_info("tvafe: resume module\n");
    return 0;
}
#endif

static struct platform_driver tvafe_driver = {
    .probe      = tvafe_probe,
    .remove     = tvafe_remove,
#ifdef CONFIG_PM
    .suspend    = tvafe_suspend,
    .resume     = tvafe_resume,
#endif
    .driver     = {
        .name   = TVAFE_DRIVER_NAME,
    }
};

static int __init tvafe_init(void)
{
    int ret = 0;

    ret = platform_driver_register(&tvafe_driver);
    if (ret != 0) {
        pr_err("failed to register tvafe module, error %d\n", ret);
        return -ENODEV;
    }

    return ret;
}

static void __exit tvafe_exit(void)
{
    platform_driver_unregister(&tvafe_driver);
}

module_init(tvafe_init);
module_exit(tvafe_exit);

MODULE_DESCRIPTION("AMLOGIC TVAFE driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xu Lin <lin.xu@amlogic.com>");

