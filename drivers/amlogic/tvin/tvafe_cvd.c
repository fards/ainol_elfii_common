/*
 * TVAFE cvd2 device driver.
 *
 * Copyright (c) 2010 Frank zhao <frank.zhao@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/******************************Includes************************************/
#include <linux/kernel.h>
#include <linux/delay.h>
#include <mach/am_regs.h>
#include <asm/uaccess.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <linux/module.h>

#include "tvin_global.h"
#include "tvafe.h"
#include "tvafe_general.h"
#include "tvafe_regs.h"
#include "tvafe_cvd.h"
#include "tvafe_adc.h"


/***************************Local defines**********************************/

#define CVD_3D_COMB_CHECK_MAX_CNT           100  //cnt*vs_interval,3D comb error check delay

#define DECODER_MOTION_BUFFER_ADDR_OFFSET   0x70000
#define DECODER_MOTION_BUFFER_4F_LENGTH     0x15a60
#define DECODER_VBI_ADDR_OFFSET             0x86000
#define DECODER_VBI_VBI_SIZE                0x1000
#define DECODER_VBI_START_ADDR              0x0

#define TVAFE_CVD2_CORDIC_IN_MAX            0xe0  //CVD cordic range of right fmt
#define TVAFE_CVD2_CORDIC_IN_MIN            0x55  //0x20

#define TVAFE_CVD2_CORDIC_OUT_MAX           0xc0  //CVD cordic range of error fmt
#define TVAFE_CVD2_CORDIC_OUT_MIN           0x60  //0x40

#define TVAFE_CVD2_STATE_CNT                15  //cnt*10ms,delay between fmt check funtion
#define TVAFE_CVD2_SHIFT_CNT                5   //cnt*10ms,delay for fmt shift counter

#define TVAFE_CVD2_NONSTD_DGAIN_MAX         0x500  //CVD max digital gain for non-std signal
#define TVAFE_CVD2_NONSTD_CNT_MAX           0x0A  //CVD max cnt non-std signal

#define TVAFE_CVD2_CDTO_ADJ_TH              0x4cedb3  //CVD cdto adjust threshold
#define TVAFE_CVD2_CDTO_ADJ_STEP            3     //CVD cdto adjust step cdto/2^n

#define TVAFE_CVD2_PGA_MIN                  0    //CVD max cnt non-std signal
#define TVAFE_CVD2_PGA_MAX                  255   //CVD max cnt non-std signal

#define TVAFE_CVD2_NOSIG_REG_CHECK_CNT      50   //n*10ms
#define TVAFE_CVD2_NORMAL_REG_CHECK_CNT     100  //n*10ms

#define TVAFE_CVD2_FMT_LOOP_CNT             1   //n*loop, max fmt check loop counter

#define TVAFE_CVD2_NOT_TRUST_NOSIG  // Do not trust Reg no signal flag

#define PAL_BURST_MAG_UPPER_LIMIT 0x1c80
#define PAL_BURST_MAG_LOWER_LIMIT 0x1980

#define NTSC_BURST_MAG_UPPER_LIMIT 0x1580
#define NTSC_BURST_MAG_LOWER_LIMIT 0x1280

#define SYNC_HEIGHT_LOWER_LIMIT 0x60
#define SYNC_HEIGHT_UPPER_LIMIT 0xff

#define SYNC_HEIGHT_ADJ_CNT 0x2
//#define SYNC_HEIGHT_AUTO_TUNING

/********************************Local variables*********************************/
/* debug value */
static int cdto_adj_th = TVAFE_CVD2_CDTO_ADJ_TH;
module_param(cdto_adj_th, int, 0664);
MODULE_PARM_DESC(cdto_adj_th, "cvd2_adj_diff_threshold");

static int cdto_adj_step = TVAFE_CVD2_CDTO_ADJ_STEP;
module_param(cdto_adj_step, int, 0664);
MODULE_PARM_DESC(cdto_adj_step, "cvd2_adj_step");

static int cvd_dbg_en = 0;
module_param(cvd_dbg_en, bool, 0664);
MODULE_PARM_DESC(cvd_dbg_en, "cvd2 debug enable");

static unsigned int nosig_cnt = 0, normal_cnt = 0;
static bool cvd_pr_flag = false;

/* TOP */
const static unsigned int cvbs_top_reg_default[][2] = {
    {TVFE_DVSS_MUXCTRL                      ,0x07000008/*0x00000000*/,}, // TVFE_DVSS_MUXCTRL
    {TVFE_DVSS_MUXVS_REF                    ,0x00000000,}, // TVFE_DVSS_MUXVS_REF
    {TVFE_DVSS_MUXCOAST_V                   ,0x00000000,}, // TVFE_DVSS_MUXCOAST_V
    {TVFE_DVSS_SEP_HVWIDTH                  ,0x00000000,}, // TVFE_DVSS_SEP_HVWIDTH
    {TVFE_DVSS_SEP_HPARA                    ,0x00000000,}, // TVFE_DVSS_SEP_HPARA
    {TVFE_DVSS_SEP_VINTEG                   ,0x00000000,}, // TVFE_DVSS_SEP_VINTEG
    {TVFE_DVSS_SEP_H_THR                    ,0x00000000,}, // TVFE_DVSS_SEP_H_THR
    {TVFE_DVSS_SEP_CTRL                     ,0x00000000,}, // TVFE_DVSS_SEP_CTRL
    {TVFE_DVSS_GEN_WIDTH                    ,0x00000000,}, // TVFE_DVSS_GEN_WIDTH
    {TVFE_DVSS_GEN_PRD                      ,0x00000000,}, // TVFE_DVSS_GEN_PRD
    {TVFE_DVSS_GEN_COAST                    ,0x00000000,}, // TVFE_DVSS_GEN_COAST
    {TVFE_DVSS_NOSIG_PARA                   ,0x00000000,}, // TVFE_DVSS_NOSIG_PARA
    {TVFE_DVSS_NOSIG_PLS_TH                 ,0x00000000,}, // TVFE_DVSS_NOSIG_PLS_TH
    {TVFE_DVSS_GATE_H                       ,0x00000000,}, // TVFE_DVSS_GATE_H
    {TVFE_DVSS_GATE_V                       ,0x00000000,}, // TVFE_DVSS_GATE_V
    {TVFE_DVSS_INDICATOR1                   ,0x00000000,}, // TVFE_DVSS_INDICATOR1
    {TVFE_DVSS_INDICATOR2                   ,0x00000000,}, // TVFE_DVSS_INDICATOR2
    {TVFE_DVSS_MVDET_CTRL1                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL1
    {TVFE_DVSS_MVDET_CTRL2                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL2
    {TVFE_DVSS_MVDET_CTRL3                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL3
    {TVFE_DVSS_MVDET_CTRL4                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL4
    {TVFE_DVSS_MVDET_CTRL5                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL5
    {TVFE_DVSS_MVDET_CTRL6                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL6
    {TVFE_DVSS_MVDET_CTRL7                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL7
    {TVFE_SYNCTOP_SPOL_MUXCTRL              ,0x00000009,}, // TVFE_SYNCTOP_SPOL_MUXCTRL
    {TVFE_SYNCTOP_INDICATOR1_HCNT           ,0x00000000,}, // TVFE_SYNCTOP_INDICATOR1_HCNT
    {TVFE_SYNCTOP_INDICATOR2_VCNT           ,0x00000000,}, // TVFE_SYNCTOP_INDICATOR2_VCNT
    {TVFE_SYNCTOP_INDICATOR3                ,0x00000000,}, // TVFE_SYNCTOP_INDICATOR3
    {TVFE_SYNCTOP_SFG_MUXCTRL1              ,0x00000000,}, // TVFE_SYNCTOP_SFG_MUXCTRL1
    {TVFE_SYNCTOP_SFG_MUXCTRL2              ,0x00330000,}, // TVFE_SYNCTOP_SFG_MUXCTRL2
    {TVFE_SYNCTOP_INDICATOR4                ,0x00000000,}, // TVFE_SYNCTOP_INDICATOR4
    {TVFE_SYNCTOP_SAM_MUXCTRL               ,0x00082001,}, // TVFE_SYNCTOP_SAM_MUXCTRL
    {TVFE_MISC_WSS1_MUXCTRL1                ,0x00000000,}, // TVFE_MISC_WSS1_MUXCTRL1
    {TVFE_MISC_WSS1_MUXCTRL2                ,0x00000000,}, // TVFE_MISC_WSS1_MUXCTRL2
    {TVFE_MISC_WSS2_MUXCTRL1                ,0x00000000,}, // TVFE_MISC_WSS2_MUXCTRL1
    {TVFE_MISC_WSS2_MUXCTRL2                ,0x00000000,}, // TVFE_MISC_WSS2_MUXCTRL2
    {TVFE_MISC_WSS1_INDICATOR1              ,0x00000000,}, // TVFE_MISC_WSS1_INDICATOR1
    {TVFE_MISC_WSS1_INDICATOR2              ,0x00000000,}, // TVFE_MISC_WSS1_INDICATOR2
    {TVFE_MISC_WSS1_INDICATOR3              ,0x00000000,}, // TVFE_MISC_WSS1_INDICATOR3
    {TVFE_MISC_WSS1_INDICATOR4              ,0x00000000,}, // TVFE_MISC_WSS1_INDICATOR4
    {TVFE_MISC_WSS1_INDICATOR5              ,0x00000000,}, // TVFE_MISC_WSS1_INDICATOR5
    {TVFE_MISC_WSS2_INDICATOR1              ,0x00000000,}, // TVFE_MISC_WSS2_INDICATOR1
    {TVFE_MISC_WSS2_INDICATOR2              ,0x00000000,}, // TVFE_MISC_WSS2_INDICATOR2
    {TVFE_MISC_WSS2_INDICATOR3              ,0x00000000,}, // TVFE_MISC_WSS2_INDICATOR3
    {TVFE_MISC_WSS2_INDICATOR4              ,0x00000000,}, // TVFE_MISC_WSS2_INDICATOR4
    {TVFE_MISC_WSS2_INDICATOR5              ,0x00000000,}, // TVFE_MISC_WSS2_INDICATOR5
    {TVFE_AP_MUXCTRL1                       ,0x00000000,}, // TVFE_AP_MUXCTRL1
    {TVFE_AP_MUXCTRL2                       ,0x00000000,}, // TVFE_AP_MUXCTRL2
    {TVFE_AP_MUXCTRL3                       ,0x00000000,}, // TVFE_AP_MUXCTRL3
    {TVFE_AP_MUXCTRL4                       ,0x00000000,}, // TVFE_AP_MUXCTRL4
    {TVFE_AP_MUXCTRL5                       ,0x00000000,}, // TVFE_AP_MUXCTRL5
    {TVFE_AP_INDICATOR1                     ,0x00000000,}, // TVFE_AP_INDICATOR1
    {TVFE_AP_INDICATOR2                     ,0x00000000,}, // TVFE_AP_INDICATOR2
    {TVFE_AP_INDICATOR3                     ,0x00000000,}, // TVFE_AP_INDICATOR3
    {TVFE_AP_INDICATOR4                     ,0x00000000,}, // TVFE_AP_INDICATOR4
    {TVFE_AP_INDICATOR5                     ,0x00000000,}, // TVFE_AP_INDICATOR5
    {TVFE_AP_INDICATOR6                     ,0x00000000,}, // TVFE_AP_INDICATOR6
    {TVFE_AP_INDICATOR7                     ,0x00000000,}, // TVFE_AP_INDICATOR7
    {TVFE_AP_INDICATOR8                     ,0x00000000,}, // TVFE_AP_INDICATOR8
    {TVFE_AP_INDICATOR9                     ,0x00000000,}, // TVFE_AP_INDICATOR9
    {TVFE_AP_INDICATOR10                    ,0x00000000,}, // TVFE_AP_INDICATOR10
    {TVFE_AP_INDICATOR11                    ,0x00000000,}, // TVFE_AP_INDICATOR11
    {TVFE_AP_INDICATOR12                    ,0x00000000,}, // TVFE_AP_INDICATOR12
    {TVFE_AP_INDICATOR13                    ,0x00000000,}, // TVFE_AP_INDICATOR13
    {TVFE_AP_INDICATOR14                    ,0x00000000,}, // TVFE_AP_INDICATOR14
    {TVFE_AP_INDICATOR15                    ,0x00000000,}, // TVFE_AP_INDICATOR15
    {TVFE_AP_INDICATOR16                    ,0x00000000,}, // TVFE_AP_INDICATOR16
    {TVFE_AP_INDICATOR17                    ,0x00000000,}, // TVFE_AP_INDICATOR17
    {TVFE_AP_INDICATOR18                    ,0x00000000,}, // TVFE_AP_INDICATOR18
    {TVFE_AP_INDICATOR19                    ,0x00000000,}, // TVFE_AP_INDICATOR19
    {TVFE_BD_MUXCTRL1                       ,0x00000000,}, // TVFE_BD_MUXCTRL1
    {TVFE_BD_MUXCTRL2                       ,0x00000000,}, // TVFE_BD_MUXCTRL2
    {TVFE_BD_MUXCTRL3                       ,0x00000000,}, // TVFE_BD_MUXCTRL3
    {TVFE_BD_MUXCTRL4                       ,0x00000000,}, // TVFE_BD_MUXCTRL4
    {TVFE_CLP_MUXCTRL1                      ,0x00000000,}, // TVFE_CLP_MUXCTRL1
    {TVFE_CLP_MUXCTRL2                      ,0x00000000,}, // TVFE_CLP_MUXCTRL2
    {TVFE_CLP_MUXCTRL3                      ,0x00000000,}, // TVFE_CLP_MUXCTRL3
    {TVFE_CLP_MUXCTRL4                      ,0x00000000,}, // TVFE_CLP_MUXCTRL4
    {TVFE_CLP_INDICATOR1                    ,0x00000000,}, // TVFE_CLP_INDICATOR1
    {TVFE_BPG_BACKP_H                       ,0x00000000,}, // TVFE_BPG_BACKP_H
    {TVFE_BPG_BACKP_V                       ,0x00000000,}, // TVFE_BPG_BACKP_V
    {TVFE_DEG_H                             ,0x00000000,}, // TVFE_DEG_H
    {TVFE_DEG_VODD                          ,0x00000000,}, // TVFE_DEG_VODD
    {TVFE_DEG_VEVEN                         ,0x00000000,}, // TVFE_DEG_VEVEN
    {TVFE_OGO_OFFSET1                       ,0x00000000,}, // TVFE_OGO_OFFSET1
    {TVFE_OGO_GAIN1                         ,0x00000000,}, // TVFE_OGO_GAIN1
    {TVFE_OGO_GAIN2                         ,0x00000000,}, // TVFE_OGO_GAIN2
    {TVFE_OGO_OFFSET2                       ,0x00000000,}, // TVFE_OGO_OFFSET2
    {TVFE_OGO_OFFSET3                       ,0x00000000,}, // TVFE_OGO_OFFSET3
    {TVFE_VAFE_CTRL                         ,0x00000000,}, // TVFE_VAFE_CTRL
    {TVFE_VAFE_STATUS                       ,0x00000000,}, // TVFE_VAFE_STATUS
    {TVFE_TOP_CTRL                          ,0x000C4F60 /*0x00004B60*/,}, // TVFE_TOP_CTRL
    {TVFE_CLAMP_INTF                        ,0x00008666,}, // TVFE_CLAMP_INTF
    {TVFE_RST_CTRL                          ,0x00000000,}, // TVFE_RST_CTRL
    {TVFE_EXT_VIDEO_AFE_CTRL_MUX1           ,0x00000000,}, // TVFE_EXT_VIDEO_AFE_CTRL_MUX1
    {TVFE_AAFILTER_CTRL1                    ,0x00082222,}, // TVFE_AAFILTER_CTRL1
    {TVFE_AAFILTER_CTRL2                    ,0x252b39c6,}, // TVFE_AAFILTER_CTRL2
    {TVFE_EDID_CONFIG                       ,TVAFE_EDID_CONFIG,}, // TVFE_EDID_CONFIG
    {TVFE_EDID_RAM_ADDR                     ,0x00000000,}, // TVFE_EDID_RAM_ADDR
    {TVFE_EDID_RAM_WDATA                    ,0x00000000,}, // TVFE_EDID_RAM_WDATA
    {TVFE_EDID_RAM_RDATA                    ,0x00000000,}, // TVFE_EDID_RAM_RDATA
    {TVFE_APB_ERR_CTRL_MUX1                 ,0x8fff8fff,}, // TVFE_APB_ERR_CTRL_MUX1
    {TVFE_APB_ERR_CTRL_MUX2                 ,0x00008fff,}, // TVFE_APB_ERR_CTRL_MUX2
    {TVFE_APB_INDICATOR1                    ,0x00000000,}, // TVFE_APB_INDICATOR1
    {TVFE_APB_INDICATOR2                    ,0x00000000,}, // TVFE_APB_INDICATOR2
    {TVFE_ADC_READBACK_CTRL                 ,0x80140003,}, // TVFE_ADC_READBACK_CTRL
    {TVFE_ADC_READBACK_INDICATOR            ,0x00000000,}, // TVFE_ADC_READBACK_INDICATOR
    {TVFE_INT_CLR                           ,0x00000000,}, // TVFE_INT_CLR
    {TVFE_INT_MSKN                          ,0x00000000,}, // TVFE_INT_MASKN
    {TVFE_INT_INDICATOR1                    ,0x00000000,}, // TVFE_INT_INDICATOR1
    {TVFE_INT_SET                           ,0x00000000,}, // TVFE_INT_SET
    //{TVFE_CHIP_VERSION                      ,0x00000000,}, // TVFE_CHIP_VERSION
    {0xFFFFFFFF                             ,0x00000000,}
};

const static unsigned int cvd_mem_4f_length[TVIN_SIG_FMT_CVBS_SECAM-TVIN_SIG_FMT_CVBS_NTSC_M+1] =
{
    0x0000e946, // TVIN_SIG_FMT_CVBS_NTSC_M,
    0x0000e946, // TVIN_SIG_FMT_CVBS_NTSC_443,
    0x00015a60, // TVIN_SIG_FMT_CVBS_PAL_I,
    0x0000e905, // TVIN_SIG_FMT_CVBS_PAL_M,
    0x00015a60, // TVIN_SIG_FMT_CVBS_PAL_60,
    0x000117d9, // TVIN_SIG_FMT_CVBS_PAL_CN,
    0x00015a60, // TVIN_SIG_FMT_CVBS_SECAM,
};

static void tvafe_cvd2_memory_init(struct tvafe_cvd2_info_s *cvd2_info)
{
    unsigned int cvd2_addr = cvd2_info->mem_addr / 8;

    if ((cvd2_info->config_fmt < TVIN_SIG_FMT_CVBS_NTSC_M) ||
        (cvd2_info->config_fmt > TVIN_SIG_FMT_CVBS_SECAM) ||
        (cvd2_info->mem_addr == 0) ||
        (cvd2_info->mem_size== 0)
       )
    {
        if (cvd_dbg_en)
            pr_err(" cvd2 fmt or memory error.\n");
        return;
    }

    /* CVD2 mem addr is based on 64bit, system mem is based on 8bit*/
    WRITE_APB_REG(CVD2_REG_96, cvd2_addr);
    WRITE_APB_REG(ACD_REG_30, (cvd2_addr + DECODER_MOTION_BUFFER_ADDR_OFFSET));
    WRITE_APB_REG_BITS(ACD_REG_2A, cvd_mem_4f_length[cvd2_info->config_fmt - TVIN_SIG_FMT_CVBS_NTSC_M],
        REG_4F_MOTION_LENGTH_BIT, REG_4F_MOTION_LENGTH_WID);
    WRITE_APB_REG(ACD_REG_2F, (cvd2_addr + DECODER_VBI_ADDR_OFFSET));
    WRITE_APB_REG_BITS(ACD_REG_21, DECODER_VBI_VBI_SIZE, AML_VBI_SIZE_BIT, AML_VBI_SIZE_WID);
    WRITE_APB_REG_BITS(ACD_REG_21, DECODER_VBI_START_ADDR, AML_VBI_START_ADDR_BIT, AML_VBI_START_ADDR_WID);

    return;
}

#if 0
// *****************************************************************************
// Function: cvd2 3d comb control
//
//   Params: none
//
//   Return: none
//
// *****************************************************************************
static void tvafe_cvd2_enable_3d_comb(struct tvafe_cvd2_info_s *cvd2_info)
{
    //if (cvd2_info->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I)
    //{
    //    WRITE_APB_REG_BITS(ACD_REG_29, 1, REG_4FRAME_MODE_BIT, REG_4FRAME_MODE_WID);
    //    WRITE_APB_REG_BITS(ACD_REG_2A, 2, REG_4F_DISAGENT_BIT, REG_4F_DISAGENT_WID);
    //}

    WRITE_APB_REG_BITS(CVD2_REG_B2, 0, COMB2D_ONLY_BIT, COMB2D_ONLY_WID);
}
#endif

static void tvafe_cvd2_write_mode_reg(struct tvafe_cvd2_info_s *cvd2_info)
{
    unsigned int i = 0;

    //check format validation
    if ((cvd2_info->config_fmt < TVIN_SIG_FMT_CVBS_NTSC_M) ||
        (cvd2_info->config_fmt > TVIN_SIG_FMT_CVBS_SECAM))
    {
        if (cvd_dbg_en)
            pr_err("%s: fmt = %d \n",__FUNCTION__, cvd2_info->config_fmt);
        return;
    }

    //reset CVD2
    WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 1, SOFT_RST_BIT, SOFT_RST_WID);

    /* for rf&cvbs source acd table */
    if (cvd2_info->tuner)
    {
        for (i=0; i<(ACD_REG_MAX+1); i++)
        {
            if ((i == 0x1E) || (i == 0x31))
                continue;
            WRITE_APB_REG((ACD_BASE_ADD+i)<<2,
                (rf_acd_table[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][i]));
        }
    }
    else
    {
        for (i=0; i<(ACD_REG_MAX+1); i++)
        {
            if ((i == 0x1E) || (i == 0x31))
                continue;
            WRITE_APB_REG((ACD_BASE_ADD+i)<<2,
                (cvbs_acd_table[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][i]));
        }
    }

    // load CVD2 reg 0x00~3f (char)
    for (i=0; i<CVD_PART1_REG_NUM; i++)
    {
        WRITE_APB_REG((CVD_BASE_ADD+CVD_PART1_REG_MIN+i)<<2, (cvd_part1_table[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][i]));
    }

    // load CVD2 reg 0x70~ff (char)
    for (i=0; i<CVD_PART2_REG_NUM; i++)
    {
        WRITE_APB_REG((CVD_BASE_ADD+CVD_PART2_REG_MIN+i)<<2, (cvd_part2_table[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][i]));
    }

    // reload CVD2 reg 0x87, 0x93, 0x94, 0x95, 0x96, 0xe6, 0xfa (int)
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_0)<<2, cvd_part3_table[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][0]);
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_1)<<2, cvd_part3_table[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][1]);
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_2)<<2, cvd_part3_table[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][2]);
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_3)<<2, cvd_part3_table[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][3]);
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_4)<<2, cvd_part3_table[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][4]);
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_5)<<2, cvd_part3_table[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][5]);
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_6)<<2, cvd_part3_table[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][6]);

    //s-video setting => reload reg 0x00~03 & 0x18~1f
    if (cvd2_info->s_video)
    {
        for (i=0; i<4; i++)
        {
            WRITE_APB_REG((CVD_BASE_ADD+i)<<2, (cvd_yc_reg_0x00_0x03[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][i]));
        }
        for (i=0; i<8; i++)
        {
            WRITE_APB_REG((CVD_BASE_ADD+i+0x18)<<2, (cvd_yc_reg_0x18_0x1f[cvd2_info->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][i]));
        }
    }

    /* for tuner picture quality */
    if (cvd2_info->tuner)
    {
        WRITE_APB_REG(CVD2_REG_B0, 0xf0);
        WRITE_APB_REG_BITS(CVD2_REG_B2, 0, ADAPTIVE_CHROMA_MODE_BIT, ADAPTIVE_CHROMA_MODE_WID);
        WRITE_APB_REG_BITS(CVD2_CONTROL1, 0, CHROMA_BW_LO_BIT, CHROMA_BW_LO_WID);
    }

    // 3D comb filter buffer assignment
    tvafe_cvd2_memory_init(cvd2_info);

    // enable CVD2
    WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 0, SOFT_RST_BIT, SOFT_RST_WID);

    return;
}

static void tvafe_cvd2_non_std_config(struct tvafe_cvd2_info_s *cvd2_info)
{
    if (cvd2_info->non_std_config == cvd2_info->non_std_enable)
        return;

    if (cvd_dbg_en)
        pr_info("%s: config non-std signal reg. \n",__func__);

    cvd2_info->non_std_config = cvd2_info->non_std_enable;
    if (cvd2_info->non_std_config)
    {
#ifdef CONFIG_TVIN_TUNER_SI2176
        if (cvd2_info->tuner)        
        {                           
            WRITE_APB_REG_BITS(CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 3, HNON_STD_TH_BIT,HNON_STD_TH_WID);             
            WRITE_APB_REG_BITS(CVD2_VSYNC_SIGNAL_THRESHOLD, 1, VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);            
            WRITE_APB_REG(CVD2_NOISE_THRESHOLD, 0x04);            
            WRITE_APB_REG(CVD2_VSYNC_CNTL, 0x02);        
        }        
        else        
        {            
            WRITE_APB_REG(CVD2_HSYNC_RISING_EDGE_START, 0x25);     
            WRITE_APB_REG(TVFE_CLAMP_INTF, 0x8000);
            WRITE_APB_REG_BITS(CVD2_H_LOOP_MAXSTATE, 4, HSTATE_MAX_BIT, HSTATE_MAX_WID);            
            WRITE_APB_REG(CVD2_VSYNC_CNTL, 0x02);            
            WRITE_APB_REG_BITS(CVD2_H_LOOP_MAXSTATE, 1, DISABLE_HFINE_BIT, DISABLE_HFINE_WID);            
            WRITE_APB_REG(CVD2_NOISE_THRESHOLD, 0x08);   
        }
#else
        WRITE_APB_REG(CVD2_HSYNC_RISING_EDGE_START, 0x25);
        WRITE_APB_REG(TVFE_CLAMP_INTF, 0x8000);
        WRITE_APB_REG_BITS(CVD2_H_LOOP_MAXSTATE, 4, HSTATE_MAX_BIT, HSTATE_MAX_WID);
        if (cvd2_info->tuner)
        {
            WRITE_APB_REG_BITS(CVD2_VSYNC_SIGNAL_THRESHOLD, 1, VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
            WRITE_APB_REG(CVD2_NOISE_THRESHOLD, 0x00);
        }
        else
        {
            WRITE_APB_REG(CVD2_VSYNC_CNTL, 0x02);
            WRITE_APB_REG_BITS(CVD2_H_LOOP_MAXSTATE, 1, DISABLE_HFINE_BIT, DISABLE_HFINE_WID);
            WRITE_APB_REG(CVD2_NOISE_THRESHOLD, 0x08);
        }
#endif
    }
    else
    {
        WRITE_APB_REG(CVD2_HSYNC_RISING_EDGE_START, 0x6d);
        WRITE_APB_REG(TVFE_CLAMP_INTF, 0x8666);
        WRITE_APB_REG_BITS(CVD2_H_LOOP_MAXSTATE, 5, HSTATE_MAX_BIT, HSTATE_MAX_WID);
        if (cvd2_info->tuner)
        {
            WRITE_APB_REG_BITS(CVD2_VSYNC_SIGNAL_THRESHOLD, 0,
                VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
            WRITE_APB_REG(CVD2_NOISE_THRESHOLD, 0x32);
        }
        else
        {
            WRITE_APB_REG(CVD2_VSYNC_CNTL, 0x01);
            WRITE_APB_REG_BITS(CVD2_H_LOOP_MAXSTATE, 0, DISABLE_HFINE_BIT, DISABLE_HFINE_WID);
            WRITE_APB_REG(CVD2_NOISE_THRESHOLD, 0x32);
        }
    }

}

void tvafe_cvd2_reset_pga(struct tvafe_cvd2_info_s *cvd2_info)
{
    /* reset pga value */
    if ((READ_APB_REG_BITS(ADC_REG_05, PGAGAIN_BIT, PGAGAIN_WID) != 0x30) ||
        (READ_APB_REG_BITS(ADC_REG_06, PGAMODE_BIT, PGAMODE_WID) != 0))
    {
        WRITE_APB_REG_BITS(ADC_REG_05, 0x30, PGAGAIN_BIT, PGAGAIN_WID);
        WRITE_APB_REG_BITS(ADC_REG_06, 0, PGAMODE_BIT, PGAMODE_WID);
        cvd2_info->dgain[0] = cvd2_info->dgain[1] = cvd2_info->dgain[2] = cvd2_info->dgain[3] = 0x0200;
        if (cvd_dbg_en)
            pr_info("%s: reset pga value \n",__func__);
    }
}

#ifdef TVAFE_SET_CVBS_CDTO_EN
static unsigned int tvafe_cvd2_get_cdto(void)
{
    unsigned int cdto = 0;

    cdto = (READ_APB_REG_BITS(CVD2_CHROMA_DTO_INCREMENT_29_24,
                 CDTO_INC_29_24_BIT, CDTO_INC_29_24_WID) & 0x0000003f)<<24;
    cdto += (READ_APB_REG_BITS(CVD2_CHROMA_DTO_INCREMENT_23_16,
                 CDTO_INC_23_16_BIT, CDTO_INC_23_16_WID) & 0x000000ff)<<16;
    cdto += (READ_APB_REG_BITS(CVD2_CHROMA_DTO_INCREMENT_15_8,
                 CDTO_INC_15_8_BIT, CDTO_INC_15_8_WID) & 0x000000ff)<<8;
    cdto += (READ_APB_REG_BITS(CVD2_CHROMA_DTO_INCREMENT_7_0,
                 CDTO_INC_7_0_BIT, CDTO_INC_7_0_WID) & 0x000000ff);
    return cdto;
}

static void tvafe_cvd2_set_cdto(unsigned int cdto)
{
    WRITE_APB_REG(CVD2_CHROMA_DTO_INCREMENT_29_24, (cdto >> 24) & 0x0000003f);
    WRITE_APB_REG(CVD2_CHROMA_DTO_INCREMENT_23_16, (cdto >> 16) & 0x000000ff);
    WRITE_APB_REG(CVD2_CHROMA_DTO_INCREMENT_15_8,  (cdto >>  8) & 0x000000ff);
    WRITE_APB_REG(CVD2_CHROMA_DTO_INCREMENT_7_0,   (cdto >>  0) & 0x000000ff);
}

#endif
void tvafe_cvd2_try_format(struct tvafe_cvd2_info_s *cvd2_info, enum tvin_sig_fmt_e fmt)
{
    if (cvd_dbg_en)
        pr_info("%s: try format:%s \n",__func__, tvin_sig_fmt_str(fmt));

    cvd2_info->config_fmt = fmt;
    tvafe_cvd2_write_mode_reg(cvd2_info);

    /* reset pga value */
    tvafe_cvd2_reset_pga(cvd2_info);

    cvd2_info->state = TVAFE_CVD2_STATE_INIT;
    cvd2_info->state_cnt = 0;
    cvd2_info->fmt_shift_cnt = 0;
    cvd2_info->non_std_enable = 0;  //reset non-standard signal flag
    cvd2_info->non_std_config = 0;  //reset non-standard signal flag

    cvd2_info->comb_check_cnt = 0;  //reset 3d comb check cnt


    return;
}

void tvafe_cvd2_set_default(struct tvafe_cvd2_info_s *cvd2_info)
{
    unsigned int i = 0;

    /**disable auto mode clock**/
    WRITE_CBUS_REG(HHI_TVFE_AUTOMODE_CLK_CNTL, 0);

    /** write 7740 register **/
    tvafe_adc_configure(TVIN_SIG_FMT_CVBS_PAL_I);

    /**enable ADC B channel for chroma of s-video**/
    if (cvd2_info->s_video)
    {
        WRITE_APB_REG_BITS(ADC_REG_14, 1, ENADCB_BIT, ENADCB_WID);
    }

    WRITE_APB_REG(((ADC_BASE_ADD+0x21)<<2), 1);
    WRITE_APB_REG(((ADC_BASE_ADD+0x21)<<2), 5);
    WRITE_APB_REG(((ADC_BASE_ADD+0x21)<<2), 7);

#ifdef TVAFE_ADC_CVBS_CLAMP_SEQUENCE_EN
    /** write 7740 register for cvbs clamp **/
    tvafe_adc_cvbs_clamp_sequence();
#endif
    /** write top register **/
    i = 0;
    while (cvbs_top_reg_default[i][0] != 0xFFFFFFFF) {
        WRITE_APB_REG(cvbs_top_reg_default[i][0], cvbs_top_reg_default[i][1]);
        i++;
    }

    tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_PAL_I);


    return;
}

static void tvafe_cvd2_get_signal_status(struct tvafe_cvd2_info_s *cvd2_info)
{
    int data = 0;

    data = READ_APB_REG(CVD2_STATUS_REGISTER1);
    cvd2_info->hw_data[cvd2_info->hw_data_cur].no_sig =      (bool)((data & 0x01) >> NO_SIGNAL_BIT );
    cvd2_info->hw_data[cvd2_info->hw_data_cur].h_lock =      (bool)((data & 0x02) >> HLOCK_BIT     );
    cvd2_info->hw_data[cvd2_info->hw_data_cur].v_lock =      (bool)((data & 0x04) >> VLOCK_BIT     );
    cvd2_info->hw_data[cvd2_info->hw_data_cur].chroma_lock = (bool)((data & 0x08) >> CHROMALOCK_BIT);

    data = READ_APB_REG(CVD2_STATUS_REGISTER2);
    cvd2_info->hw_data[cvd2_info->hw_data_cur].h_nonstd =       (bool)((data & 0x02) >> HNON_STD_BIT         );
    cvd2_info->hw_data[cvd2_info->hw_data_cur].v_nonstd =       (bool)((data & 0x04) >> VNON_STD_BIT         );
    cvd2_info->hw_data[cvd2_info->hw_data_cur].no_color_burst = (bool)((data & 0x08) >> BKNWT_DETECTED_BIT   );
    cvd2_info->hw_data[cvd2_info->hw_data_cur].comb3d_off =     (bool)((data & 0x10) >> STATUS_COMB3D_OFF_BIT);

    data = READ_APB_REG(CVD2_STATUS_REGISTER3);
    cvd2_info->hw_data[cvd2_info->hw_data_cur].pal =      (bool)((data & 0x01) >> PAL_DETECTED_BIT     );
    cvd2_info->hw_data[cvd2_info->hw_data_cur].secam =    (bool)((data & 0x02) >> SECAM_DETECTED_BIT   );
    cvd2_info->hw_data[cvd2_info->hw_data_cur].line625 =  (bool)((data & 0x04) >> LINES625_DETECTED_BIT);
    cvd2_info->hw_data[cvd2_info->hw_data_cur].noisy =    (bool)((data & 0x08) >> NOISY_BIT            );
    cvd2_info->hw_data[cvd2_info->hw_data_cur].vcr =      (bool)((data & 0x10) >> VCR_BIT              );
    cvd2_info->hw_data[cvd2_info->hw_data_cur].vcrtrick = (bool)((data & 0x20) >> VCR_TRICK_BIT        );
    cvd2_info->hw_data[cvd2_info->hw_data_cur].vcrff =    (bool)((data & 0x40) >> VCR_FF_BIT           );
    cvd2_info->hw_data[cvd2_info->hw_data_cur].vcrrew =   (bool)((data & 0x80) >> VCR_REW_BIT          );

    cvd2_info->hw_data[cvd2_info->hw_data_cur].cordic = READ_APB_REG_BITS(CVD2_CORDIC_FREQUENCY_STATUS, STATUS_CORDIQ_FRERQ_BIT, STATUS_CORDIQ_FRERQ_WID);

    if (++ cvd2_info->hw_data_cur >= 3)
        cvd2_info->hw_data_cur = 0;

#ifdef TVAFE_CVD2_NOT_TRUST_NOSIG
#else
    if (cvd2_info->hw_data[0].no_sig && cvd2_info->hw_data[1].no_sig && cvd2_info->hw_data[2].no_sig)
        cvd2_info->data.no_sig = true;
    if (!cvd2_info->hw_data[0].no_sig && !cvd2_info->hw_data[1].no_sig && !cvd2_info->hw_data[2].no_sig)
        cvd2_info->data.no_sig = false;
#endif
    if (cvd2_info->hw_data[0].h_lock && cvd2_info->hw_data[1].h_lock && cvd2_info->hw_data[2].h_lock)
        cvd2_info->data.h_lock = true;
    if (!cvd2_info->hw_data[0].h_lock && !cvd2_info->hw_data[1].h_lock && !cvd2_info->hw_data[2].h_lock)
        cvd2_info->data.h_lock = false;

    if (cvd2_info->hw_data[0].v_lock && cvd2_info->hw_data[1].v_lock && cvd2_info->hw_data[2].v_lock)
        cvd2_info->data.v_lock = true;
    if (!cvd2_info->hw_data[0].v_lock && !cvd2_info->hw_data[1].v_lock && !cvd2_info->hw_data[2].v_lock)
        cvd2_info->data.v_lock = false;

    if (cvd2_info->hw_data[0].h_nonstd && cvd2_info->hw_data[1].h_nonstd && cvd2_info->hw_data[2].h_nonstd)
        cvd2_info->data.h_nonstd = true;
    if (!cvd2_info->hw_data[0].h_nonstd && !cvd2_info->hw_data[1].h_nonstd && !cvd2_info->hw_data[2].h_nonstd)
        cvd2_info->data.h_nonstd = false;

    if (cvd2_info->hw_data[0].v_nonstd && cvd2_info->hw_data[1].v_nonstd && cvd2_info->hw_data[2].v_nonstd)
        cvd2_info->data.v_nonstd = true;
    if (!cvd2_info->hw_data[0].v_nonstd && !cvd2_info->hw_data[1].v_nonstd && !cvd2_info->hw_data[2].v_nonstd)
        cvd2_info->data.v_nonstd = false;

    if (cvd2_info->hw_data[0].no_color_burst && cvd2_info->hw_data[1].no_color_burst && cvd2_info->hw_data[2].no_color_burst)
        cvd2_info->data.no_color_burst = true;
    if (!cvd2_info->hw_data[0].no_color_burst && !cvd2_info->hw_data[1].no_color_burst && !cvd2_info->hw_data[2].no_color_burst)
        cvd2_info->data.no_color_burst = false;

    if (cvd2_info->hw_data[0].comb3d_off && cvd2_info->hw_data[1].comb3d_off && cvd2_info->hw_data[2].comb3d_off)
        cvd2_info->data.comb3d_off = true;
    if (!cvd2_info->hw_data[0].comb3d_off && !cvd2_info->hw_data[1].comb3d_off && !cvd2_info->hw_data[2].comb3d_off)
        cvd2_info->data.comb3d_off = false;

    if (cvd2_info->hw_data[0].chroma_lock && cvd2_info->hw_data[1].chroma_lock && cvd2_info->hw_data[2].chroma_lock)
        cvd2_info->data.chroma_lock = true;
    if (!cvd2_info->hw_data[0].chroma_lock && !cvd2_info->hw_data[1].chroma_lock && !cvd2_info->hw_data[2].chroma_lock)
        cvd2_info->data.chroma_lock = false;

    if (cvd2_info->hw_data[0].pal && cvd2_info->hw_data[1].pal && cvd2_info->hw_data[2].pal)
        cvd2_info->data.pal = true;
    if (!cvd2_info->hw_data[0].pal && !cvd2_info->hw_data[1].pal && !cvd2_info->hw_data[2].pal)
        cvd2_info->data.pal = false;

    if (cvd2_info->hw_data[0].secam && cvd2_info->hw_data[1].secam && cvd2_info->hw_data[2].secam)
        cvd2_info->data.secam = true;
    if (!cvd2_info->hw_data[0].secam && !cvd2_info->hw_data[1].secam && !cvd2_info->hw_data[2].secam)
        cvd2_info->data.secam = false;

    if (cvd2_info->hw_data[0].line625 && cvd2_info->hw_data[1].line625 && cvd2_info->hw_data[2].line625)
        cvd2_info->data.line625 = true;
    if (!cvd2_info->hw_data[0].line625 && !cvd2_info->hw_data[1].line625 && !cvd2_info->hw_data[2].line625)
        cvd2_info->data.line625 = false;

    if (cvd2_info->hw_data[0].noisy && cvd2_info->hw_data[1].noisy && cvd2_info->hw_data[2].noisy)
        cvd2_info->data.noisy = true;
    if (!cvd2_info->hw_data[0].noisy && !cvd2_info->hw_data[1].noisy && !cvd2_info->hw_data[2].noisy)
        cvd2_info->data.noisy = false;

    if (cvd2_info->hw_data[0].vcr && cvd2_info->hw_data[1].vcr && cvd2_info->hw_data[2].vcr)
        cvd2_info->data.vcr = true;
    if (!cvd2_info->hw_data[0].vcr && !cvd2_info->hw_data[1].vcr && !cvd2_info->hw_data[2].vcr)
        cvd2_info->data.vcr = false;

    if (cvd2_info->hw_data[0].vcrtrick && cvd2_info->hw_data[1].vcrtrick && cvd2_info->hw_data[2].vcrtrick)
        cvd2_info->data.vcrtrick = true;
    if (!cvd2_info->hw_data[0].vcrtrick && !cvd2_info->hw_data[1].vcrtrick && !cvd2_info->hw_data[2].vcrtrick)
        cvd2_info->data.vcrtrick = false;

    if (cvd2_info->hw_data[0].vcrff && cvd2_info->hw_data[1].vcrff && cvd2_info->hw_data[2].vcrff)
        cvd2_info->data.vcrff = true;
    if (!cvd2_info->hw_data[0].vcrff && !cvd2_info->hw_data[1].vcrff && !cvd2_info->hw_data[2].vcrff)
        cvd2_info->data.vcrff = false;

    if (cvd2_info->hw_data[0].vcrrew && cvd2_info->hw_data[1].vcrrew && cvd2_info->hw_data[2].vcrrew)
        cvd2_info->data.vcrrew = true;
    if (!cvd2_info->hw_data[0].vcrrew && !cvd2_info->hw_data[1].vcrrew && !cvd2_info->hw_data[2].vcrrew)
        cvd2_info->data.vcrrew = false;
#ifdef TVAFE_CVD2_NOT_TRUST_NOSIG //while tv channel switch, avoid black screen
    if (!cvd2_info->data.h_lock)
        cvd2_info->data.no_sig = true;
    if (cvd2_info->data.h_lock)
        cvd2_info->data.no_sig = false;
#endif

    data  = 0;
    data += (int)cvd2_info->hw_data[0].cordic;
    data += (int)cvd2_info->hw_data[1].cordic;
    data += (int)cvd2_info->hw_data[2].cordic;
    if (cvd2_info->hw_data[0].cordic & 0x80)
    {
        data -= 256;
    }
    if (cvd2_info->hw_data[1].cordic & 0x80)
    {
        data -= 256;
    }
    if (cvd2_info->hw_data[2].cordic & 0x80)
    {
        data -= 256;
    }
    data /= 3;
    cvd2_info->data.cordic  = (unsigned char)(data & 0xff);

    return;
}

static bool tvafe_cvd2_cordic_match(struct tvafe_cvd2_info_s *cvd2_info)
{
    if ((cvd2_info->data.cordic <= TVAFE_CVD2_CORDIC_IN_MIN) || (cvd2_info->data.cordic >= TVAFE_CVD2_CORDIC_IN_MAX))
    {
        if (cvd_dbg_en)
            pr_info("%s: codic:0x%x in range:<0x%x or >0x%x \n",__func__,cvd2_info->data.cordic,
                    TVAFE_CVD2_CORDIC_IN_MIN, TVAFE_CVD2_CORDIC_IN_MAX);
        return true;
    }
    else
    {
        if (cvd_dbg_en)
            pr_info("%s: codic:0x%x out range:0x%x~0x%x \n",__func__,cvd2_info->data.cordic,
                    TVAFE_CVD2_CORDIC_IN_MIN, TVAFE_CVD2_CORDIC_IN_MAX);
        return false;
    }
}

static bool tvafe_cvd2_condition_shift(struct tvafe_cvd2_info_s *cvd2_info)
{
    bool ret = false;
    unsigned short dgain = 0;

    if (cvd2_info->data.no_sig || !cvd2_info->data.h_lock || !cvd2_info->data.v_lock)
    {
        if (!cvd2_info->tuner)
        {
            if (cvd_dbg_en)
                pr_info("%s: CVBS shift, nosig:%d,h-lock:%d,v-lock:%d\n",__func__,
                            cvd2_info->data.no_sig,cvd2_info->data.h_lock,cvd2_info->data.v_lock);
            return true;
        }
        if (cvd2_info->data.no_sig || !cvd2_info->data.h_lock)
        {
            if (cvd_dbg_en)
                pr_info("%s: RF shift, nosig:%d,h-lock:%d,v-lock:%d\n",__func__,
                            cvd2_info->data.no_sig,cvd2_info->data.h_lock,cvd2_info->data.v_lock);
            return true;
        }
    }
    if (cvd2_info->manual_fmt)
        return false;

    /* check line flag */
    switch (cvd2_info->config_fmt)
    {
        case TVIN_SIG_FMT_CVBS_PAL_I:
        case TVIN_SIG_FMT_CVBS_PAL_CN:
        case TVIN_SIG_FMT_CVBS_SECAM:
            if (!cvd2_info->data.line625)
                ret = true;
            break;
        case TVIN_SIG_FMT_CVBS_PAL_M:
        case TVIN_SIG_FMT_CVBS_NTSC_443:
        case TVIN_SIG_FMT_CVBS_PAL_60:
        case TVIN_SIG_FMT_CVBS_NTSC_M:
            if (cvd2_info->data.line625)
                ret = true;
            break;
        default:
            break;
    }
    if (ret)
    {
        if (cvd_dbg_en)
            pr_info("%s: line625 error!!!! \n",__func__);
        return true;
    }

	/* check non standard signal, ignore SECAM/525 mode */
    if (cvd2_info->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I)
    {
        dgain = READ_APB_REG_BITS(CVD2_AGC_GAIN_STATUS_7_0, AGC_GAIN_7_0_BIT, AGC_GAIN_7_0_WID);
        dgain |= READ_APB_REG_BITS(CVD2_AGC_GAIN_STATUS_11_8, AGC_GAIN_11_8_BIT, AGC_GAIN_11_8_WID)<<8;
        if ((dgain >= TVAFE_CVD2_NONSTD_DGAIN_MAX) ||
        	cvd2_info->data.h_nonstd ||
        	(READ_APB_REG_BITS(ADC_REG_06, PGAMODE_BIT, PGAMODE_WID)))
        {
            cvd2_info->non_std_enable = 1;
        }
        else
        {
        	cvd2_info->non_std_enable = 0;
        }
    }

    if (cvd2_info->data.no_color_burst)
    {
		/* for SECAM format, set PAL_I */
        if (cvd2_info->config_fmt != TVIN_SIG_FMT_CVBS_SECAM)  //set default fmt
        {
            if (!cvd_pr_flag && cvd_dbg_en)
            {
                cvd_pr_flag = true;
                pr_info("%s: no-color-burst, do not change mode. \n",__func__);
            }
            return false;
        }
    }
	/* ignore pal flag because of cdto adjustment */
    if ((cvd2_info->non_std_worst || cvd2_info->data.h_nonstd) &&
        (cvd2_info->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I))
    {
        if (!cvd_pr_flag && cvd_dbg_en)
        {
            cvd_pr_flag = true;
            pr_info("%s: if adj cdto or h-nonstd, ignore mode change. \n",__func__);
        }
        return false;
    }

    /* add for skyworth cst02 board*/
    if ((cvd2_info->tuner) && (cvd2_info->fmt_loop_cnt >= TVAFE_CVD2_FMT_LOOP_CNT))
    {
        if (!cvd_pr_flag && cvd_dbg_en)
        {
            cvd_pr_flag = true;
            pr_info("%s: RF signal, polling %d times, do not check cordic&pal flag. \n",__func__,TVAFE_CVD2_FMT_LOOP_CNT);
        }
        return false;
    }
    /* codic check. if non-std signal detected, need ignore codic offset*/
    if ((cvd2_info->data.cordic >= TVAFE_CVD2_CORDIC_OUT_MIN) && (cvd2_info->data.cordic <= TVAFE_CVD2_CORDIC_OUT_MAX))
    {
        if (!cvd2_info->non_std_enable)
        {
            if (cvd_dbg_en)
                pr_info("%s: cordic:0x%x dismatch, shift for normal. \n",__func__,cvd2_info->data.cordic);
            return true;
        }
    }

    /* check pal/secam flag */
    switch (cvd2_info->config_fmt)
    {
        case TVIN_SIG_FMT_CVBS_PAL_I:
        case TVIN_SIG_FMT_CVBS_PAL_CN:
        case TVIN_SIG_FMT_CVBS_PAL_60:
        case TVIN_SIG_FMT_CVBS_PAL_M:
            if (!cvd2_info->data.pal)
                ret = true;
            break;
        case TVIN_SIG_FMT_CVBS_SECAM:
            if (!cvd2_info->data.secam)
                ret = true;
            break;
        case TVIN_SIG_FMT_CVBS_NTSC_443:
        case TVIN_SIG_FMT_CVBS_NTSC_M:
            if (cvd2_info->data.pal)
                ret = true;
            break;
        default:
            break;
    }

    if (ret)
    {
        if (cvd_dbg_en)
            pr_info("%s: pal/secam flag change. \n",__func__);
        return true;
    }
    else
        return false;
}

static void tvafe_cvd2_search_video_mode(struct tvafe_cvd2_info_s *cvd2_info)
{
    unsigned int shift_cnt = 0;
    // get signal status from HW
    tvafe_cvd2_get_signal_status(cvd2_info);

      // execute manual mode
    if ((cvd2_info->manual_fmt) &&
		(cvd2_info->config_fmt != cvd2_info->manual_fmt) &&
		(cvd2_info->config_fmt != TVIN_SIG_FMT_NULL))
    {
        tvafe_cvd2_try_format(cvd2_info, cvd2_info->manual_fmt);
    }
    // state-machine
    switch (cvd2_info->state)
    {
        case TVAFE_CVD2_STATE_IDLE:

            break;
        case TVAFE_CVD2_STATE_INIT:
            // wait for signal setup
            if (((cvd2_info->data.no_sig || !cvd2_info->data.h_lock) && cvd2_info->tuner)    ||
                ((cvd2_info->data.no_sig || !cvd2_info->data.h_lock || !cvd2_info->data.v_lock) && !cvd2_info->tuner)
               )
            {
                cvd2_info->state_cnt = 0;
                if (!cvd_pr_flag && cvd_dbg_en)
                {
                    cvd_pr_flag = true;
                    pr_info("%s: search, nosig:%d,h-lock:%d,v-lock:%d\n",__func__,
                        cvd2_info->data.no_sig,cvd2_info->data.h_lock,cvd2_info->data.v_lock);
                }
            }
            // wait for signal stable
            else if (++cvd2_info->state_cnt > TVAFE_CVD2_STATE_CNT)
            {
                cvd_pr_flag = false;
                cvd2_info->state_cnt = 0;
                // manual mode => go directly to the manual format
                if (cvd2_info->manual_fmt)
                {
                    cvd2_info->state = TVAFE_CVD2_STATE_FIND;
                    if (cvd_dbg_en)
                        pr_info("%s: manual fmt is:%s, do not need try other format!!!\n",__func__,
                            tvin_sig_fmt_str(cvd2_info->manual_fmt));
                }
                // auto mode
                else
                {
                    if (cvd_dbg_en)
                        pr_info("%s: switch to fmt:%s,hnon:%d,vnon:%d,c-lk:%d,pal:%d,secam:%d,h-lk:%d,v-lk:%d,\n",__func__,
                            tvin_sig_fmt_str(cvd2_info->config_fmt), cvd2_info->data.h_nonstd, cvd2_info->data.v_nonstd,
                            cvd2_info->data.chroma_lock,cvd2_info->data.pal,cvd2_info->data.secam,cvd2_info->data.h_lock
                            ,cvd2_info->data.v_lock);
                    switch (cvd2_info->config_fmt)
                    {
                        case TVIN_SIG_FMT_CVBS_PAL_I:
                            if (cvd2_info->data.line625)
                            {
                                if (cvd2_info->tuner)
                                {
                                    cvd2_info->state = TVAFE_CVD2_STATE_FIND;  //set defalut pal-i for RF
                                }
                                else
                                {
                                    if (tvafe_cvd2_cordic_match(cvd2_info))
                                    {
                                        // 625 + cordic_match => confirm PAL_I
                                        cvd2_info->state = TVAFE_CVD2_STATE_FIND;
                                    }
                                    else
                                    {
                                        // 625 + cordic_diamatch => try PAL_CN
                                        tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_PAL_CN);
                                    }
                                }
                            }
                            else
                            {
                                // 525 => try PAL_M
                                tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_PAL_M);
                            }
                            break;
                        case TVIN_SIG_FMT_CVBS_PAL_CN:
                            if (cvd2_info->data.line625)
                            {
                                if (tvafe_cvd2_cordic_match(cvd2_info))
                                {
                                    // 625 + cordic_match => confirm PAL_CN
                                    cvd2_info->state = TVAFE_CVD2_STATE_FIND;
                                }
                                else
                                {
                                    // 625 + cordic_diamatch => set default to SECAM
                                    tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_SECAM);
                                }
                            }
                            else
                            {
                                // 525 => try PAL_M
                                tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_PAL_M);
                            }
                            break;
                    #if 0
                        case TVIN_SIG_FMT_CVBS_SECAM:
                            cvd2_info->state = TVAFE_CVD2_STATE_FIND;
                            break;
                    #else
                        case TVIN_SIG_FMT_CVBS_SECAM:
                            if (cvd2_info->data.line625)
                            {
                                if (tvafe_cvd2_cordic_match(cvd2_info))
                                {
                                    // 625 + cordic_match => confirm PAL_CN
                                    cvd2_info->state = TVAFE_CVD2_STATE_FIND;
                                }
                                else
                                {
                                    // 625 + cordic_diamatch => set default to SECAM
                                    tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_PAL_I);
                                    cvd2_info->state = TVAFE_CVD2_STATE_FIND;
                                }
                            }
                            else
                            {
                                // 525 => try PAL_M
                                tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_PAL_M);
                            }
                            break;
                    #endif
                        case TVIN_SIG_FMT_CVBS_PAL_M:
                            if (!cvd2_info->data.line625)
                            {
                                if (tvafe_cvd2_cordic_match(cvd2_info))
                                {
                                    if (cvd2_info->data.pal)
                                    {
                                        // 525 + cordic_match + pal => confirm PAL_M
                                        cvd2_info->state = TVAFE_CVD2_STATE_FIND;
                                    }
                                    else
                                    {
                                        // 525 + cordic_match + ntsc => confirm NTSC_M
                                        tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_NTSC_M);
                                    }
                                }
                                else
                                {
                                    // 525 + cordic_dismatch => try PAL_60
                                    tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_PAL_60);
                                }
                            }
                            else
                            {
                                // 625 => ret PAL_I
                                tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_PAL_I);
                            }
                            break;
                        case TVIN_SIG_FMT_CVBS_NTSC_M:
                            cvd2_info->state = TVAFE_CVD2_STATE_FIND;
                            break;
                        case TVIN_SIG_FMT_CVBS_PAL_60:
                            if (!cvd2_info->data.line625)
                            {
                                if (tvafe_cvd2_cordic_match(cvd2_info))
                                {
                                    if (cvd2_info->data.pal)
                                    {
                                        // 525 + cordic_match + pal => confirm PAL_60
                                        cvd2_info->state = TVAFE_CVD2_STATE_FIND;
                                    }
                                    else
                                    {
                                        // 525 + cordic_match + ntsc => confirm NTSC_443
                                        tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_NTSC_443);
                                    }
                                }
                                else
                                {
                                    // 525 + cordic_dismatch => set default to NTSC_M
                                    tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_NTSC_M);
                                }
                            }
                            else
                            {
                                // 625 => ret PAL_I
                                tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_PAL_I);
                            }
                            break;
                       case TVIN_SIG_FMT_CVBS_NTSC_443:
                            cvd2_info->state = TVAFE_CVD2_STATE_FIND;
                            break;
                        default:
                            break;
                    }
                    cvd2_info->fmt_loop_cnt++; //add loop cnt
                }
                if (cvd_dbg_en)
                    pr_info("%s: next fmt is:%s\n",__func__, tvin_sig_fmt_str(cvd2_info->config_fmt));
            }
            break;
        case TVAFE_CVD2_STATE_FIND:
            // manual mode => go directly to the manual format
            if (cvd2_info->manual_fmt)
            {
                break;
            }
            if (tvafe_cvd2_condition_shift(cvd2_info))
            {
                shift_cnt = TVAFE_CVD2_SHIFT_CNT;
                if (cvd2_info->non_std_enable)
                    shift_cnt = TVAFE_CVD2_SHIFT_CNT*10;

                /* if no color burst, pal flag can not be trusted */
                if (cvd2_info->fmt_shift_cnt++ > shift_cnt)
                {
                    tvafe_cvd2_try_format(cvd2_info, TVIN_SIG_FMT_CVBS_PAL_I);
                    cvd2_info->state = TVAFE_CVD2_STATE_INIT;  //TVAFE_CVD2_STATE_IDLE;
                    cvd_pr_flag = false;
                    if (cvd2_info->fmt_loop_cnt >= TVAFE_CVD2_FMT_LOOP_CNT)
                    {
                        if (cvd_dbg_en)
                            pr_info("%s: reset fmt loop cnt. \n",__func__);
                        cvd2_info->fmt_loop_cnt = 0;
                    }
                }
            }

            /* non-standard signal config */
            tvafe_cvd2_non_std_config(cvd2_info);

            break;
        default:
            break;
    }

    return;
}

// call by 10ms_timer at frontend side
bool tvafe_cvd2_no_sig(struct tvafe_cvd2_info_s *cvd2_info)
{
    unsigned char tmp = 0, i = 0;

    tvafe_cvd2_search_video_mode(cvd2_info);

    if (normal_cnt++ == TVAFE_CVD2_NORMAL_REG_CHECK_CNT)
    {
    	normal_cnt = 0;
    	cvd2_info->adc_reload_en = false;
        for (i=0x01; i<ADC_REG_NUM; i++)
        {
            //0x20 has output buffer register, I have not list it here, should I?
            //0x21 has reset & sleep mode, should I watch it here?
#if   defined(CONFIG_MESON2_CHIP_B)  
             if ((i==0x05) || (i==0x1d) || (i==0x20) || (i==0x24) || (i==0x2b) ||
                 (i==0x35) || (i==0x3a) || (i==0x53) || (i==0x54) || (i==0x6a)
               )
#elif defined(CONFIG_MESON2_CHIP_C)
            if ((i==0x05) || (i==0x1d) || (i==0x20) || (i==0x24) || (i==0x2b)  ||                
                (i==0x35) || (i==0x3a) || (i==0x53) || (i==0x54) || (i==0x6a)  ||            
                (i==0x34) || (i==0x46) || (i>=0x4b && i<=0x55)   || (i==0x57)  ||                
                (i>=0x6a && i<ADC_REG_NUM) || (i==0x67)               )
#endif           
               continue;

    	    tmp = READ_APB_REG((ADC_BASE_ADD + i) << 2);
            if (i == 0x06)
            {
            	if((tmp&0x10) != 0x10)
            	    cvd2_info->adc_reload_en = true;
            }
            else if (i == 0x17)
            {
                if ((( cvd2_info->tuner) && (tmp != 0x00)) ||
                    ((!cvd2_info->tuner) && (tmp != 0x10))
                   )
                   cvd2_info->adc_reload_en = true;
            }
            else if (i == 0x3d)            
            {               
                if((tmp&0x80) != 0x80)                    
                    cvd2_info->adc_reload_en = true;            
            }
            else
            {
                if(tmp != adc_cvbs_table[i])
        	        cvd2_info->adc_reload_en = true;
            }

            if (cvd2_info->adc_reload_en && cvd_dbg_en)
            {
                pr_info("%s: zhuangwei, wrong reg adc[%x] = %x, table[%x] = %x \n",__func__, i,tmp,i,adc_cvbs_table[i]);
                break;
            }
            #ifdef CONFIG_MESON2_CHIP_C
            cvd2_info->adc_reload_en = false;
            #endif
        }

        /* if adc reg readback error reload adc reg table */
        if (cvd2_info->adc_reload_en)
        {
            /** write 7740 register **/
            tvafe_adc_configure(TVIN_SIG_FMT_CVBS_PAL_I);

            /**enable ADC B channel for chroma of s-video**/
            if (cvd2_info->s_video)
            {
                cvd2_info->manual_fmt = TVIN_SIG_FMT_NULL;
                WRITE_APB_REG_BITS(ADC_REG_14, 1, ENADCB_BIT, ENADCB_WID);
            }
            tvafe_adc_digital_reset();
        }
    }

	/* add for reset variable */
    if (cvd2_info->data.no_sig)
    {

#ifdef TVAFE_ADC_CVBS_CLAMP_SEQUENCE_EN
        /** write 7740 register for cvbs clamp **/
        if (nosig_cnt++ == TVAFE_CVD2_NOSIG_REG_CHECK_CNT)
        {
    	    nosig_cnt = 0;
    	    tvafe_adc_cvbs_clamp_sequence();
        }

#endif

        if ((CVD2_CHROMA_DTO_PAL_I != tvafe_cvd2_get_cdto()) &&
            (cvd2_info->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I))
        {
            tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_PAL_I);
            if (cvd_dbg_en)
                pr_info("%s: set default cdto. \n",__func__);

        }
        /* reset pga value */
        tvafe_cvd2_reset_pga(cvd2_info);

        /* init variable */
        cvd2_info->state = TVAFE_CVD2_STATE_INIT;
        cvd2_info->state_cnt = 0;
        cvd2_info->fmt_shift_cnt = 0;
        cvd2_info->non_std_enable = 0;
        cvd2_info->non_std_config = 0;
        cvd2_info->comb_check_cnt = 0;
    }
    return (cvd2_info->data.no_sig);
}

// call by 10ms_timer at frontend side
bool tvafe_cvd2_fmt_chg(struct tvafe_cvd2_info_s *cvd2_info)
{
    if (cvd2_info->state == TVAFE_CVD2_STATE_FIND)
        return false;
    else
        return true;
}

enum tvin_sig_fmt_e tvafe_cvd2_get_format(struct tvafe_cvd2_info_s *cvd2_info)
{
    if (cvd2_info->state == TVAFE_CVD2_STATE_FIND)
        return cvd2_info->config_fmt;
    else
        return TVIN_SIG_FMT_NULL;
}

#ifdef TVAFE_SET_CVBS_PGA_EN
void tvafe_set_cvbs_pga(struct tvafe_cvd2_info_s *cvd2_info)
{
    unsigned short dg_max = 0, dg_min = 0xffff, dg_ave = 0, i = 0, pga = 0;
    unsigned int tmp = 0;
    unsigned char step = 0;

    cvd2_info->dgain[0] = cvd2_info->dgain[1];
    cvd2_info->dgain[1] = cvd2_info->dgain[2];
    cvd2_info->dgain[2] = cvd2_info->dgain[3];
    cvd2_info->dgain[3] = READ_APB_REG_BITS(CVD2_AGC_GAIN_STATUS_7_0, AGC_GAIN_7_0_BIT, AGC_GAIN_7_0_WID);
    cvd2_info->dgain[3] |= READ_APB_REG_BITS(CVD2_AGC_GAIN_STATUS_11_8, AGC_GAIN_11_8_BIT, AGC_GAIN_11_8_WID)<<8;
    for (i = 0; i < 4; i++)
    {
        if (dg_max < cvd2_info->dgain[i])
            dg_max = cvd2_info->dgain[i];
        if (dg_min > cvd2_info->dgain[i])
            dg_min = cvd2_info->dgain[i];
        dg_ave += cvd2_info->dgain[i];
    }
    if (++cvd2_info->dgain_cnt >= TVAFE_SET_CVBS_PGA_START + TVAFE_SET_CVBS_PGA_STEP)
    {
        cvd2_info->dgain_cnt = TVAFE_SET_CVBS_PGA_START;
    }
    if (cvd2_info->dgain_cnt == TVAFE_SET_CVBS_PGA_START)
    {
        cvd2_info->dgain_cnt = 0;
        dg_ave = (dg_ave - dg_max - dg_min + 1) >> 1;
        pga = READ_APB_REG_BITS(ADC_REG_05, PGAGAIN_BIT, PGAGAIN_WID);

        if (READ_APB_REG_BITS(ADC_REG_06, PGAMODE_BIT, PGAMODE_WID))
        {
            pga += 64;
        }
        if (((dg_ave >= CVD2_DGAIN_LIMITL) && (dg_ave <= CVD2_DGAIN_LIMITH)) ||
            (pga >= (255+64)) ||
            (pga == 0))
        {
            return;
        }
        tmp = ABS((signed short)dg_ave - (signed short)CVD2_DGAIN_MIDDLE);

        if (tmp > CVD2_DGAIN_MIDDLE)
            step = 16;
        else if (tmp > (CVD2_DGAIN_MIDDLE >> 1))
            step = 5;
        else if (tmp > (CVD2_DGAIN_MIDDLE >> 2))
            step = 2;
        else
            step = 1;
        if (dg_ave > CVD2_DGAIN_LIMITH)
        {
            pga += step;
            if (pga >= (255+64))  //set max value
                pga = (255+64);
        }
        else
        {
            if (pga < step)  //set min value
                pga = 0;
            else
                pga -= step;
        }
        if (pga > 255)
        {
            pga -= 64;
            WRITE_APB_REG_BITS(ADC_REG_06, 1, PGAMODE_BIT, PGAMODE_WID);
        }
        else
        {
            WRITE_APB_REG_BITS(ADC_REG_06, 0, PGAMODE_BIT, PGAMODE_WID);
        }
        //if (cvd_dbg_en)
        //    pr_info("%s: set pag:0x%x \n",__func__, pga);
        WRITE_APB_REG_BITS(ADC_REG_05, pga, PGAGAIN_BIT, PGAGAIN_WID);
    }

    return;
}
#endif

#ifdef TVAFE_SET_CVBS_CDTO_EN

static void tvafe_cvd2_adj_cdto(unsigned int cur, unsigned int dest)
{
    unsigned int diff = 0, step = 0;

    diff = (unsigned int)ABS((signed int)cur - (signed int)dest);

    if (diff == 0)
    {
        return;
    }

    if ((diff > (diff>>1)) && (diff > (0x1<<cdto_adj_step)))
        step = diff >> cdto_adj_step;
    else
        step = 1;

    if (cur > dest)
        cur -= step;
    else
        cur += step;

    WRITE_APB_REG(CVD2_CHROMA_DTO_INCREMENT_29_24, (cur >> 24) & 0x0000003f);
    WRITE_APB_REG(CVD2_CHROMA_DTO_INCREMENT_23_16, (cur >> 16) & 0x000000ff);
    WRITE_APB_REG(CVD2_CHROMA_DTO_INCREMENT_15_8,  (cur >>  8) & 0x000000ff);
    WRITE_APB_REG(CVD2_CHROMA_DTO_INCREMENT_7_0,   (cur >>  0) & 0x000000ff);

}

void tvafe_set_cvbs_cdto(struct tvafe_cvd2_info_s *cvd2_info, unsigned int hcnt64)
{
    unsigned int hcnt64_max = 0, hcnt64_min = 0xffffffff, hcnt64_ave = 0, i = 0;
    unsigned int cur_cdto = 0, diff = 0;
    u64 cal_cdto = 0;

    /* only for pal-i adjusment */
    if (!cvd2_info->data.line625)
        return;

    cvd2_info->hcnt64[0] = cvd2_info->hcnt64[1];
    cvd2_info->hcnt64[1] = cvd2_info->hcnt64[2];
    cvd2_info->hcnt64[2] = cvd2_info->hcnt64[3];
    cvd2_info->hcnt64[3] = hcnt64;
    for (i = 0; i < 4; i++)
    {
        if (hcnt64_max < cvd2_info->hcnt64[i])
            hcnt64_max = cvd2_info->hcnt64[i];
        if (hcnt64_min > cvd2_info->hcnt64[i])
            hcnt64_min = cvd2_info->hcnt64[i];
        hcnt64_ave += cvd2_info->hcnt64[i];
    }
    if (++cvd2_info->hcnt64_cnt >= TVAFE_SET_CVBS_CDTO_START + TVAFE_SET_CVBS_CDTO_STEP)
    {
        cvd2_info->hcnt64_cnt = TVAFE_SET_CVBS_CDTO_START;
    }
    if (cvd2_info->hcnt64_cnt == TVAFE_SET_CVBS_CDTO_START)
    {
        hcnt64_ave = (hcnt64_ave - hcnt64_max - hcnt64_min + 1) >> 1;
        if (hcnt64_ave == 0)  // to avoid kernel crash
            return;
        cal_cdto = CVD2_CHROMA_DTO_PAL_I;
        cal_cdto *= HS_CNT_STANDARD;
        do_div(cal_cdto, hcnt64_ave);

        cur_cdto = tvafe_cvd2_get_cdto();
        diff = (unsigned int)ABS((signed int)cal_cdto - (signed int)CVD2_CHROMA_DTO_PAL_I);

        if (diff < cdto_adj_th)
        {
            /* reset cdto to default value */
            if (cur_cdto != CVD2_CHROMA_DTO_PAL_I)
                tvafe_cvd2_adj_cdto(cur_cdto, (unsigned int)CVD2_CHROMA_DTO_PAL_I);
            cvd2_info->non_std_worst = 0;
            return;
        }
        else
            cvd2_info->non_std_worst = 1;

        if (cvd_dbg_en)
            pr_info("%s: adj cdto from:0x%x to:0x%x\n",__func__,(u32)cur_cdto, (u32)cal_cdto);
        tvafe_cvd2_adj_cdto(cur_cdto, (unsigned int)cal_cdto);
    }

    return;
}
#endif

// *****************************************************************************
// Function: set cvd2 3d comb error status
//
//   Params: none
//
//   Return: none
//
// *****************************************************************************
void tvafe_check_cvbs_3d_comb(struct tvafe_cvd2_info_s *cvd2_info)
{
    unsigned int cvd2_3d_status = READ_APB_REG(CVD2_REG_95);

#ifdef SYNC_HEIGHT_AUTO_TUNING
    int burst_mag = 0;
    int burst_mag_16msb = 0, burst_mag_16lsb = 0;
    unsigned int reg_sync_height = 0;
    int burst_mag_upper_limitation = 0;
    int burst_mag_lower_limitation = 0;
    unsigned int std_sync_height = 0xdd;
    unsigned int cur_div_result = 0;
    unsigned int mult_result = 0;
    unsigned int final_contrast = 0;
    unsigned int reg_contrast_default = 0;

        if (cvd2_info->non_std_config) {
    	}
        else if (cvd2_info->tuner) {
        }
        else if ((cvd2_info->config_fmt == TVIN_SIG_FMT_CVBS_NTSC_M)|(cvd2_info->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I)) { //try to detect AVin NTSCM/PALI
        	if (cvd2_info->config_fmt == TVIN_SIG_FMT_CVBS_NTSC_M) {
			burst_mag_upper_limitation = NTSC_BURST_MAG_UPPER_LIMIT & 0xffff;
			burst_mag_lower_limitation = NTSC_BURST_MAG_LOWER_LIMIT & 0xffff;
			reg_contrast_default = 0x7b;
		}
		else if (cvd2_info->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I) {
			burst_mag_upper_limitation = PAL_BURST_MAG_UPPER_LIMIT & 0xffff;
			burst_mag_lower_limitation = PAL_BURST_MAG_LOWER_LIMIT & 0xffff;
			reg_contrast_default = 0x7d ;
		}

	     	burst_mag_16msb = READ_APB_REG(CVD2_STATUS_BURST_MAGNITUDE_LSB);
	     	burst_mag_16lsb = READ_APB_REG(CVD2_STATUS_BURST_MAGNITUDE_MSB);
	     	burst_mag = (((burst_mag_16msb&0xff) << 8)|(burst_mag_16lsb&0xff));
	     	if (burst_mag > burst_mag_upper_limitation) {
			reg_sync_height = READ_APB_REG(CVD2_LUMA_AGC_VALUE);
	     		if (reg_sync_height > SYNC_HEIGHT_LOWER_LIMIT ) {
	     			reg_sync_height = reg_sync_height - 1;
				WRITE_APB_REG(CVD2_LUMA_AGC_VALUE,reg_sync_height&0xff);

				cur_div_result = std_sync_height << 16;
				do_div(cur_div_result,reg_sync_height);	
				mult_result = cur_div_result * (reg_contrast_default&0xff);
				final_contrast = (mult_result + 0x8000) >> 16;	
				if (final_contrast > 0xff) {
					WRITE_APB_REG(CVD2_LUMA_CONTRAST_ADJUSTMENT,0xff);
				}
				else if (final_contrast > 0x50) {
					WRITE_APB_REG(CVD2_LUMA_CONTRAST_ADJUSTMENT,final_contrast&0xff);
				}
			}
		}
	     	else if (burst_mag < burst_mag_lower_limitation) {
			reg_sync_height = READ_APB_REG(CVD2_LUMA_AGC_VALUE);
	     		if (reg_sync_height < SYNC_HEIGHT_UPPER_LIMIT) {
	     		 	reg_sync_height = reg_sync_height + 1;
				WRITE_APB_REG(CVD2_LUMA_AGC_VALUE,reg_sync_height&0xff);

                                cur_div_result = std_sync_height << 16;
                                do_div(cur_div_result,reg_sync_height);
				mult_result = cur_div_result * (reg_contrast_default&0xff);
                                final_contrast = (mult_result + 0x8000) >> 16;  
                                if (final_contrast > 0xff) {
                                        WRITE_APB_REG(CVD2_LUMA_CONTRAST_ADJUSTMENT,0xff);
                                }
                                else if (final_contrast > 0x50) {
                                        WRITE_APB_REG(CVD2_LUMA_CONTRAST_ADJUSTMENT,final_contrast&0xff);
                                }
	     		}
		}	
	}
#endif

    if (cvd2_info->comb_check_cnt++ > CVD_3D_COMB_CHECK_MAX_CNT)
    {
    #ifdef TVAFE_ADC_CVBS_CLAMP_SEQUENCE_EN
        /** write 7740 register for cvbs clamp **/
        tvafe_adc_cvbs_clamp_sequence();
    #endif
        cvd2_info->comb_check_cnt = 0;
    }
    if (cvd2_3d_status & 0x1ffff)
    {
        WRITE_APB_REG_BITS(CVD2_REG_B2, 1, COMB2D_ONLY_BIT, COMB2D_ONLY_WID);
        WRITE_APB_REG_BITS(CVD2_REG_B2, 0, COMB2D_ONLY_BIT, COMB2D_ONLY_WID);
        //if (cvd_dbg_en)
        //    pr_info("%s: reset 3d comb  sts:0x%x \n",__func__, cvd2_3d_status);
    }
}

#ifdef TVAFE_SET_CVBS_MANUAL_FMT_POS
void tvafe_cvd2_set_video_positon(struct tvafe_cvd2_info_s *cvd2_info, enum tvin_sig_fmt_e fmt)
{
    if (fmt == TVIN_SIG_FMT_CVBS_PAL_I)
    {
        if (READ_APB_REG(ACD_REG_2E) != 0x00170137)
        {
            /* set positon with ntsc fmt */
            WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VSTART, 0x2a);
            WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VHEIGHT, 0xc0);
            WRITE_APB_REG_BITS(CVD2_CONTROL0, 1, VLINE_625_BIT, VLINE_625_WID);
            WRITE_APB_REG(ACD_REG_2E, 0x00170137);
            if (cvd_dbg_en)
                pr_info("%s: set:%s position. \n",__func__, tvin_sig_fmt_str(fmt));
        }
    }
    else
    {
        if (READ_APB_REG(ACD_REG_2E) != 0x00110101)
        {
            /* set positon with ntsc fmt */
            WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VSTART, 0x22);
            WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VHEIGHT, 0x61);
            WRITE_APB_REG_BITS(CVD2_CONTROL0, 0, VLINE_625_BIT, VLINE_625_WID);
            WRITE_APB_REG(ACD_REG_2E, 0x00110101);
            if (cvd_dbg_en)
                pr_info("%s: set:%s position. \n",__func__, tvin_sig_fmt_str(fmt));
        }
    }
}
#endif

