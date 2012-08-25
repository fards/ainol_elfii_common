/*******************************************************************
*  Copyright C 2010 by Amlogic, Inc. All Rights Reserved.
*  File name: TVAFE_CVD.h
*  Description: IO function, structure, enum, used in TVIN AFE sub-module processing
*******************************************************************/
#ifndef _TVAFE_CVD_H
#define _TVAFE_CVD_H

// ***************************************************************************
// *** enum definitions *********************************************
// ***************************************************************************

typedef enum tvafe_cvd2_state_e {
    TVAFE_CVD2_STATE_IDLE = 0,
	TVAFE_CVD2_STATE_INIT,
    TVAFE_CVD2_STATE_FIND,
} tvafe_cvd2_state_t;

// ***************************************************************************
// *** structure definitions *********************************************
// ***************************************************************************

typedef struct tvafe_cvd2_hw_data_s {
    bool no_sig;
    bool h_lock;
    bool v_lock;
    bool h_nonstd;
    bool v_nonstd;
    bool no_color_burst;
    bool comb3d_off;
    bool chroma_lock;
    bool pal;
    bool secam;
    bool line625;
    bool noisy;
    bool vcr;
    bool vcrtrick;
    bool vcrff;
    bool vcrrew;
    unsigned char cordic;
} tvafe_cvd2_hw_data_t;

//CVD2 status list
typedef struct tvafe_cvd2_info_s {
    struct tvafe_cvd2_hw_data_s hw_data[3];
    struct tvafe_cvd2_hw_data_s data;
    unsigned char               hw_data_cur;
    bool                        s_video;
    bool                        tuner;
    enum tvin_sig_fmt_e         config_fmt;
    enum tvin_sig_fmt_e         manual_fmt;
    enum tvafe_cvd2_state_e     state;
    unsigned int                state_cnt;
    unsigned int                mem_addr;
    unsigned int                mem_size;
#ifdef TVAFE_SET_CVBS_CDTO_EN
    unsigned int                hcnt64[4];
    unsigned int                hcnt64_cnt;
#endif
#ifdef TVAFE_SET_CVBS_PGA_EN
    unsigned short              dgain[4];
    unsigned short              dgain_cnt;
#endif
    unsigned int                comb_check_cnt;
    unsigned int                fmt_shift_cnt;
    unsigned int                fmt_loop_cnt;
    bool                        non_std_enable;
    bool                        non_std_config;
    bool                        non_std_worst;
    bool                        adc_reload_en;
} tvafe_cvd2_info_t;

// *****************************************************************************
// ******** GLOBAL FUNCTION CLAIM ********
// *****************************************************************************
extern void tvafe_cvd2_try_format(struct tvafe_cvd2_info_s *cvd2_info, enum tvin_sig_fmt_e fmt);
extern void tvafe_cvd2_set_default(struct tvafe_cvd2_info_s *cvd2_info);
extern bool tvafe_cvd2_no_sig(struct tvafe_cvd2_info_s *cvd2_info);
extern bool tvafe_cvd2_fmt_chg(struct tvafe_cvd2_info_s *cvd2_info);
extern enum tvin_sig_fmt_e tvafe_cvd2_get_format(struct tvafe_cvd2_info_s *cvd2_info);
#ifdef TVAFE_SET_CVBS_PGA_EN
extern void tvafe_set_cvbs_pga(struct tvafe_cvd2_info_s *cvd2_info);
#endif
#ifdef TVAFE_SET_CVBS_CDTO_EN
extern void tvafe_set_cvbs_cdto(struct tvafe_cvd2_info_s *cvd2_info, unsigned int hcnt64);
#endif
void tvafe_check_cvbs_3d_comb(struct tvafe_cvd2_info_s *cvd2_info);
void tvafe_cvd2_reset_pga(struct tvafe_cvd2_info_s *cvd2_info);
#ifdef TVAFE_SET_CVBS_MANUAL_FMT_POS
void tvafe_cvd2_set_video_positon(struct tvafe_cvd2_info_s *cvd2_info, enum tvin_sig_fmt_e fmt);
#endif
#endif // _TVAFE_CVD_H
