/*******************************************************************
*  Copyright C 2010 by Amlogic, Inc. All Rights Reserved.
*  File name: tvafe.h
*  Description: IO function, structure, enum, used in TVIN AFE sub-module processing
*******************************************************************/

#ifndef _TVAFE_H
#define _TVAFE_H


#include <linux/tvin/tvin.h>
#include "tvin_global.h"
#include "tvin_format_table.h"
#include "tvafe_cvd.h"

/******************************Definitions************************************/

#define ABS(x) ( (x)<0 ? -(x) : (x))

// ***************************************************************************
// *** enum definitions *********************************************
// ***************************************************************************

typedef enum tvafe_src_type_e {
    TVAFE_SRC_TYPE_NULL = 0,
    TVAFE_SRC_TYPE_CVBS,
	TVAFE_SRC_TYPE_SVIDEO,
    TVAFE_SRC_TYPE_VGA,
    TVAFE_SRC_TYPE_COMP,
    TVAFE_SRC_TYPE_SCART,
} tvafe_src_type_t;

// ***************************************************************************
// *** structure definitions *********************************************
// ***************************************************************************

typedef struct tvafe_operand_s {
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int step;
    unsigned int bpg_h;
    unsigned int bpg_v;
    unsigned int clk_ctl;
    unsigned int clk_ext  :1;
    unsigned int bpg_m    :2;
    unsigned int lpf_a    :1;
    unsigned int lpf_b    :1;
    unsigned int lpf_c    :1;
    unsigned int clamp_inv:1;
    unsigned int clamp_ext:1;
    unsigned int adj      :1;
    unsigned int cnt      :3;
    unsigned int dir      :1;
    unsigned int dir0     :1;
    unsigned int dir1     :1;
    unsigned int dir2     :1;
    unsigned int adc0;
    unsigned int adc1;
    unsigned int adc2;
    unsigned int data0;
    unsigned int data1;
    unsigned int data2;
} tvafe_operand_t;

typedef struct tvafe_info_s {
    struct tvafe_cvd2_info_s                cvd2_info;
    struct tvafe_pin_mux_s                  *pinmux;
    //signal parameters
    struct tvin_parm_s                      param;
    enum tvafe_cmd_status_e                 cmd_status;
    //WSS data
    struct tvafe_comp_wss_s                 comp_wss;
    //adc calibration data
    struct tvafe_adc_cal_s                  adc_cal_val;
    struct tvafe_operand_s                  operand;
    unsigned char                           watchdog_cnt;
    unsigned char                           vga_auto_flag: 1;

    /* tvafe last fmt */
    enum tvin_sig_fmt_e                     last_fmt;
    struct tvafe_vga_parm_s                 vga_parm;
} tvafe_info_t;

#endif  // _TVAFE_H

