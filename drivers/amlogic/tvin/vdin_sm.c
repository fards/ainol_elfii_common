/*
 * TVIN Signal State Machine
 *
 * Author: Lin Xu <lin.xu@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Standard Linux Headers */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

/* Amlogic Headers */
#include <linux/tvin/tvin.h>

/* Local Headers */
#include "tvin_frontend.h"
#include "vdin_sm.h"
#include "tvin_format_table.h"
#include "vdin_ctl.h"

/* Stay in TVIN_SIG_STATE_NOSIG for some cycles => be sure TVIN_SIG_STATE_NOSIG */
#define NOSIG_MAX_CNT               8
/* Stay in TVIN_SIG_STATE_UNSTABLE for some cycles => be sure TVIN_SIG_STATE_UNSTABLE */
#define UNSTABLE_MAX_CNT            2// 4
#define UNSTABLE_ATV_MAX_CNT       48
/* Have signal for some cycles  => exit TVIN_SIG_STATE_NOSIG */
#define EXIT_NOSIG_MAX_CNT          1
/* No signal for some cycles  => back to TVAFE_STATE_NOSIG */
#define BACK_NOSIG_MAX_CNT          24 //8
/* Signal unstable for some cycles => exit TVAFE_STATE_STABLE */
#define EXIT_STABLE_MAX_CNT         1
/* Signal stable for some cycles  => back to TVAFE_STATE_STABLE */
#define BACK_STABLE_MAX_CNT         50  //must >=500ms,for new api function

#define EXIT_PRESTABLE_MAX_CNT      50


static enum tvin_sm_status_e state = TVIN_SM_STATUS_NULL;    //TVIN_SIG_STATUS_NOSIG;

static unsigned int state_counter          = 0; // STATE_NOSIG, STATE_UNSTABLE
static unsigned int exit_nosig_counter     = 0; // STATE_NOSIG
static unsigned int back_nosig_counter     = 0; // STATE_UNSTABLE
static unsigned int back_stable_counter    = 0; // STATE_UNSTABLE
static unsigned int exit_prestable_counter = 0; // STATE_PRESTABLE

static int sm_debug_enable = 1;

static int sm_print_nosig  = 0;
static int sm_print_notsup = 0;
static int sm_print_unstable = 0;

module_param(sm_debug_enable, bool, 0664);
MODULE_PARM_DESC(sm_debug_enable, "enable/disable state machine debug message");

void tvin_smr_init_counter(void)
{
    state_counter          = 0;
    exit_nosig_counter     = 0;
    back_nosig_counter     = 0;
    back_stable_counter    = 0;
	exit_prestable_counter = 0;
}

/*
 * tvin state machine routine
 *
 */
void tvin_smr(struct vdin_dev_s *devp)
{
    struct tvin_state_machine_ops_s *sm_ops;
    struct tvin_info_s *info;
    enum tvin_port_e port = TVIN_PORT_NULL;
	int unstable_cnt;

    if (!devp || !devp->frontend)
    {
        state = TVIN_SM_STATUS_NULL;
        return;
    }

    sm_ops = devp->frontend->sm_ops;
    info = &devp->parm.info;
    port = devp->parm.port;

    switch (state)
    {
        case TVIN_SM_STATUS_NOSIG:
            ++state_counter;
            if (devp->parm.flag & TVIN_PARM_FLAG_CAL)
            {
               if ((((port >= TVIN_PORT_COMP0) && (port <= TVIN_PORT_COMP7)) ||
                    ((port >= TVIN_PORT_VGA0 ) && (port <= TVIN_PORT_VGA7 ))
                   ) &&
                   (sm_ops->adc_cal)
                  )
               {
                   if (!sm_ops->adc_cal(devp->frontend))
                       devp->parm.flag &= ~TVIN_PARM_FLAG_CAL;
               }
               else
                   devp->parm.flag &= ~TVIN_PARM_FLAG_CAL;
            }
            else if (sm_ops->nosig(devp->frontend))
            {
                exit_nosig_counter = 0;
                if (state_counter >= NOSIG_MAX_CNT)
                {
                    state_counter       = NOSIG_MAX_CNT;
                    info->status        = TVIN_SIG_STATUS_NOSIG;
                    info->fmt           = TVIN_SIG_FMT_NULL;
                    if (sm_debug_enable && !sm_print_nosig) {
                        pr_info("[smr] no signal\n");
                        sm_print_nosig = 1;
                    }
                    sm_print_unstable = 0;
                }
            }
            else
            {
                ++exit_nosig_counter;
                if (exit_nosig_counter >= EXIT_NOSIG_MAX_CNT)
                {
                    tvin_smr_init_counter();
                    state = TVIN_SM_STATUS_UNSTABLE;
                    if (sm_debug_enable)
                        pr_info("[smr] no signal --> unstable\n");
                    sm_print_nosig  = 0;
                    sm_print_unstable = 0;
                }
            }
            break;

        case TVIN_SM_STATUS_UNSTABLE:
            ++state_counter;
            if (devp->parm.flag & TVIN_PARM_FLAG_CAL)
                devp->parm.flag &= ~TVIN_PARM_FLAG_CAL;
            if (sm_ops->nosig(devp->frontend))
            {
                back_stable_counter = 0;
                ++back_nosig_counter;
                if (back_nosig_counter >= BACK_NOSIG_MAX_CNT)
                {
                    tvin_smr_init_counter();
                    state        = TVIN_SM_STATUS_NOSIG;
                    info->status = TVIN_SIG_STATUS_NOSIG;
                    info->fmt    = TVIN_SIG_FMT_NULL;
                    if (sm_debug_enable)
                        pr_info("[smr] unstable --> no signal\n");
                    sm_print_nosig  = 0;
                    sm_print_unstable = 0;
                }
            }
            else
            {
                back_nosig_counter = 0;
                if (sm_ops->fmt_changed(devp->frontend) )

                {
                    back_stable_counter = 0;
                    if((port == TVIN_PORT_CVBS0)&&devp->unstable_flag)
                        unstable_cnt = UNSTABLE_ATV_MAX_CNT;
                    else
                        unstable_cnt = UNSTABLE_MAX_CNT;
                    if (state_counter >= unstable_cnt)
                    {
                        state_counter  = unstable_cnt;
                        info->status   = TVIN_SIG_STATUS_UNSTABLE;
                        info->fmt      = TVIN_SIG_FMT_NULL;
                        if (sm_debug_enable && !sm_print_unstable) {
                            pr_info("[smr] unstable\n");
                            sm_print_unstable = 1;
                        }
                        sm_print_nosig  = 0;
                    }
                }
                else
                {
                    ++back_stable_counter;
                    if (back_stable_counter >= BACK_STABLE_MAX_CNT)
                    {   //must wait enough time for cvd signal lock
                        back_stable_counter         = 0;
                        state_counter               = 0;
                        if (sm_ops->get_fmt) {
                            info->fmt   = sm_ops->get_fmt(devp->frontend);
                            if (sm_ops->get_sig_propery)
                            {
                                sm_ops->get_sig_propery(devp->frontend, &devp->prop);
                                devp->parm.info.trans_fmt = devp->prop.trans_fmt;
                            }
                        }
                        else
                            info->fmt   = TVIN_SIG_FMT_NULL;

                        /* set signal status */
                        if(info->fmt == TVIN_SIG_FMT_NULL)
                        {
                            info->status = TVIN_SIG_STATUS_NOTSUP;
                            if (sm_debug_enable && !sm_print_notsup) {
                                pr_info("[smr] unstable --> not support\n");
                                sm_print_notsup = 1;
                            }
                        }
                        else
                        {
                            if (sm_ops->fmt_config)
                                sm_ops->fmt_config(devp->frontend);
                            tvin_smr_init_counter();
                            state = TVIN_SM_STATUS_PRESTABLE;
                            if (sm_debug_enable)
                                pr_info("[smr] unstable --> prestable, and format is %d(%s)\n",
                                info->fmt, tvin_sig_fmt_str(info->fmt));
                            sm_print_nosig  = 0;
                            sm_print_unstable = 0;
                        }
                    }
                }
            }
            break;

        case TVIN_SM_STATUS_PRESTABLE:
        {
            bool nosig = false, fmt_changed = false;//, pll_lock = false;
            devp->unstable_flag = true;

            if (devp->parm.flag & TVIN_PARM_FLAG_CAL)
                devp->parm.flag &= ~TVIN_PARM_FLAG_CAL;
            if (sm_ops->nosig(devp->frontend)) {
                nosig = true;
                if (sm_debug_enable)
                    pr_info("[smr] warning: no signal\n");
            }

            if (sm_ops->fmt_changed(devp->frontend)) {
                fmt_changed = true;
                if (sm_debug_enable)
                    pr_info("[smr] warning: format changed\n");
            }

            if (nosig || fmt_changed)
            {
                ++state_counter;
                if (state_counter >= EXIT_STABLE_MAX_CNT)
                {
                    tvin_smr_init_counter();
                    state = TVIN_SM_STATUS_UNSTABLE;
                    if (sm_debug_enable)
                        pr_info("[smr] prestable --> unstable\n");
                    sm_print_nosig  = 0;
                    sm_print_notsup = 0;
                    sm_print_unstable = 0;

                    break;
                }
            }
            else
            {
                state_counter = 0;
            }

            /* wait comp stable */
            if ((port >= TVIN_PORT_COMP0) &&
                (port <= TVIN_PORT_COMP7))
            {
                ++exit_prestable_counter;
                if (exit_prestable_counter >= EXIT_PRESTABLE_MAX_CNT)
                {
                    tvin_smr_init_counter();
                    state               = TVIN_SM_STATUS_STABLE;
                    info->status        = TVIN_SIG_STATUS_STABLE;
                    if (sm_debug_enable)
                        pr_info("[smr] prestable --> stable\n");
                    sm_print_nosig  = 0;
                    sm_print_notsup = 0;
                }
            }
            else
            {
                    state               = TVIN_SM_STATUS_STABLE;
                    info->status        = TVIN_SIG_STATUS_STABLE;
                    if (sm_debug_enable)
                        pr_info("[smr] prestable --> stable\n");
                    sm_print_nosig  = 0;
                    sm_print_notsup = 0;
            }
            break;
        }
        case TVIN_SM_STATUS_STABLE:
        {
            bool nosig = false, fmt_changed = false;//, pll_lock = false;
        #ifdef TVAFE_SET_CVBS_MANUAL_FMT_POS
            enum tvin_cvbs_pos_ctl_e pos_ctl = TVIN_CVBS_POS_NULL;
        #endif

            devp->unstable_flag = true;

            if (devp->parm.flag & TVIN_PARM_FLAG_CAL)
                devp->parm.flag &= ~TVIN_PARM_FLAG_CAL;
        #ifdef TVAFE_SET_CVBS_MANUAL_FMT_POS
            /* cvbs manual fmt video size checking */
            if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_SVIDEO7) && (sm_ops->set_cvbs_fmt_pos))
            {
                pos_ctl = sm_ops->set_cvbs_fmt_pos(devp->frontend);
                if (devp->cvbs_pos_chg != pos_ctl)
                {
                    if (pos_ctl != TVIN_CVBS_POS_NULL)
                    {
                        /* avoid black screen for auto<-->fmt switch */
                        if (devp->cvbs_pos_chg != TVIN_CVBS_POS_NULL)
                        {
                            fmt_changed = true;
                            info->status = TVIN_SIG_STATUS_UNSTABLE;
                            if (sm_debug_enable)
                                pr_info("[smr] warning: cvbs manual fmt change:%d \n", devp->cvbs_pos_chg);
                        }
                    }
                    devp->cvbs_pos_chg = pos_ctl;
                }
            }
        #endif
            if (sm_ops->nosig(devp->frontend)) {
                nosig = true;
                if (sm_debug_enable)
                    pr_info("[smr] warning: no signal\n");
            }

            if (sm_ops->fmt_changed(devp->frontend)) {
                fmt_changed = true;
                if (sm_debug_enable)
                    pr_info("[smr] warning: format changed\n");
            }

            #if 0
            if (sm_ops->pll_lock(devp->frontend)) {
                pll_lock = true;
            }
            else {
                pll_lock = false;
                if (sm_debug_enable)
                    pr_info("[smr] warning: pll lock failed\n");
            }
            #endif

            if (nosig || fmt_changed /* || !pll_lock */)
            {
                ++state_counter;
                if (state_counter >= EXIT_STABLE_MAX_CNT)
                {
                    tvin_smr_init_counter();
                    state = TVIN_SM_STATUS_UNSTABLE;
                    if (sm_debug_enable)
                        pr_info("[smr] stable --> unstable\n");
                    sm_print_nosig  = 0;
                    sm_print_notsup = 0;
                    sm_print_unstable = 0;
                }
            }
            else
            {
                state_counter = 0;
            }
            break;
        }
        case TVIN_SM_STATUS_NULL:
        default:
            if (devp->parm.flag & TVIN_PARM_FLAG_CAL)
                devp->parm.flag &= ~TVIN_PARM_FLAG_CAL;
            state = TVIN_SM_STATUS_NOSIG;
            break;
    }
}

/*
 * tvin state machine routine init
 *
 */

void tvin_smr_init(void)
{
    state = TVIN_SM_STATUS_NULL;
    tvin_smr_init_counter();
}

enum tvin_sm_status_e tvin_get_sm_status()
{
    return state;
}


