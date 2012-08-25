#ifndef _TVAFE_GENERAL_H
#define _TVAFE_GENERAL_H

// ***************************************************************************
// *** enum definitions *********************************************
// ***************************************************************************
typedef enum tvafe_adc_ch_e {
    TVAFE_ADC_CH_NULL = 0,
    TVAFE_ADC_CH_PGA,
    TVAFE_ADC_CH_A,
    TVAFE_ADC_CH_B,
    TVAFE_ADC_CH_C,
} tvafe_adc_ch_t;

// ***************************************************************************
// *** macro definitions *********************************************
// ***************************************************************************

#define TVAFE_EDID_CONFIG   0x03800050
//#define LOG_ADC_CAL

// ***************************************************************************
// *** global parameters **********
// ***************************************************************************
extern enum tvafe_adc_pin_e         tvafe_default_cvbs_out;
extern const signed short tvafe_comp_hs_patch[TVIN_SIG_FMT_COMP_MAX - TVIN_SIG_FMT_VGA_MAX - 1];
// *****************************************************************************
// ******** function claims ********
// *****************************************************************************
enum tvafe_adc_pin_e tvafe_get_free_pga_pin(struct tvafe_pin_mux_s *pinmux);
int tvafe_source_muxing(struct tvafe_info_s *info);
void tvafe_vga_set_edid(struct tvafe_vga_edid_s *edid);
void tvafe_vga_get_edid(struct tvafe_vga_edid_s *edid);
void tvafe_set_cal_value(struct tvafe_adc_cal_s *para);
void tvafe_get_cal_value(struct tvafe_adc_cal_s *para);
bool tvafe_adc_cal(struct tvafe_info_s *info, struct tvafe_operand_s *operand);
void tvafe_adc_clamp_adjust(struct tvafe_info_s *info);
void tvafe_get_wss_data(struct tvafe_comp_wss_s *para);
enum tvin_scan_mode_e tvafe_top_get_scan_mode(void);
//enum tvin_field_mode_e tvafe_get_field_mode(void);
void tvafe_reset_module(void);
void tvafe_enable_module(bool enable);
void tvafe_top_config(enum tvin_sig_fmt_e fmt);


#endif  // _TVAFE_GENERAL_H

