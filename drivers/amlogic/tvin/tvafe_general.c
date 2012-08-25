/******************************Includes************************************/
#include <linux/errno.h>
#include <mach/am_regs.h>

#include "tvin_global.h"
#include "tvafe.h"
#include "tvafe_regs.h"
#include "tvafe_adc.h"
#include "tvafe_cvd.h"
#include "tvafe_general.h"

struct tvafe_adc_cal_s cal_std_value_component={
		.a_analog_clamp	   = 73,
		.a_analog_gain 	   = 112,
		.a_digital_gain	   = 975, // 0dB
		.a_digital_offset1 = 1,
		.a_digital_offset2 = 28,
		.b_analog_clamp	   = 65,
		.b_analog_gain 	   = 122,
		.b_digital_gain	   = 899, // 0dB
		.b_digital_offset1 = 3,
		.b_digital_offset2 = 64,
		.c_analog_clamp	   = 62,
		.c_analog_gain 	   = 121,
		.c_digital_gain	   = 899, // 0dB
		.c_digital_offset1 = 2,
		.c_digital_offset2 = 64,
		};

struct tvafe_adc_cal_s cal_std_value_vga={
		.a_analog_clamp    = 66,
	    .a_analog_gain     = 153,
	    .a_digital_gain    = 1024, // 0dB
	    .a_digital_offset1 = 0,
	    .a_digital_offset2 = 0,
	    .b_analog_clamp    = 71,
	    .b_analog_gain     = 162,
	    .b_digital_gain    = 1024, // 0dB
	    .b_digital_offset1 = 0,
	    .b_digital_offset2 = 0,
	    .c_analog_clamp    = 65,
	    .c_analog_gain     = 162,
	    .c_digital_gain    = 1024,// 0dB
	    .c_digital_offset1 = 0,
	    .c_digital_offset2 = 0,
		};
static int threshold_value = 32;


/***************************Global Variables**********************************/
enum tvafe_adc_pin_e            tvafe_default_cvbs_out;

const signed short tvafe_comp_hs_patch[TVIN_SIG_FMT_COMP_MAX - TVIN_SIG_FMT_VGA_MAX - 1] =
{
    //VGA
    #if 0
      0,  // TVIN_SIG_FMT_VGA_560X384P_60D147,
      0,  // TVIN_SIG_FMT_VGA_640X200P_59D924,
      0,  // TVIN_SIG_FMT_VGA_640X350P_85D080,
      0,  // TVIN_SIG_FMT_VGA_640X400P_59D940,
      0,  // TVIN_SIG_FMT_VGA_640X400P_85D080,
      0,  // TVIN_SIG_FMT_VGA_640X400P_59D638,
      0,  // TVIN_SIG_FMT_VGA_640X400P_56D416,
      0,  // TVIN_SIG_FMT_VGA_640X480P_66D619,
      0,  // TVIN_SIG_FMT_VGA_640X480P_66D667,   // 10
      0,  // TVIN_SIG_FMT_VGA_640X480P_59D940,
      0,  // TVIN_SIG_FMT_VGA_640X480P_60D000,
      0,  // TVIN_SIG_FMT_VGA_640X480P_72D809,
      0,  // TVIN_SIG_FMT_VGA_640X480P_75D000_A,
      0,  // TVIN_SIG_FMT_VGA_640X480P_85D008,
      0,  // TVIN_SIG_FMT_VGA_640X480P_59D638,
      0,  // TVIN_SIG_FMT_VGA_640X480P_75D000_B,
      0,  // TVIN_SIG_FMT_VGA_640X870P_75D000,
      0,  // TVIN_SIG_FMT_VGA_720X350P_70D086,
      0,  // TVIN_SIG_FMT_VGA_720X400P_85D039,   // 20
      0,  // TVIN_SIG_FMT_VGA_720X400P_70D086,
      0,  // TVIN_SIG_FMT_VGA_720X400P_87D849,
      0,  // TVIN_SIG_FMT_VGA_720X400P_59D940,
      0,  // TVIN_SIG_FMT_VGA_720X480P_59D940,
      0,  // TVIN_SIG_FMT_VGA_768X480P_59D896,
      0,  // TVIN_SIG_FMT_VGA_800X600P_56D250,
      0,  // TVIN_SIG_FMT_VGA_800X600P_60D317,
      0,  // TVIN_SIG_FMT_VGA_800X600P_72D188,
      0,  // TVIN_SIG_FMT_VGA_800X600P_75D000,
      0,  // TVIN_SIG_FMT_VGA_800X600P_85D061,   // 30
      0,  // TVIN_SIG_FMT_VGA_832X624P_75D087,
      0,  // TVIN_SIG_FMT_VGA_848X480P_84D751,
      0,  // TVIN_SIG_FMT_VGA_960X600P_59D635,
      0,  // TVIN_SIG_FMT_VGA_1024X768P_59D278,
      0,  // TVIN_SIG_FMT_VGA_1024X768P_74D927,
      0,  // TVIN_SIG_FMT_VGA_1024X768P_60D004,
      0,  // TVIN_SIG_FMT_VGA_1024X768P_70D069,
      0,  // TVIN_SIG_FMT_VGA_1024X768P_75D029,
      0,  // TVIN_SIG_FMT_VGA_1024X768P_84D997,
      0,  // TVIN_SIG_FMT_VGA_1024X768P_60D000,   // 40
      0,  // TVIN_SIG_FMT_VGA_1024X768P_74D925,
      0,  // TVIN_SIG_FMT_VGA_1024X768P_75D020,
      0,  // TVIN_SIG_FMT_VGA_1024X768P_70D008,
      0,  // TVIN_SIG_FMT_VGA_1024X768P_75D782,
      0,  // TVIN_SIG_FMT_VGA_1024X768P_77D069,
      0,  // TVIN_SIG_FMT_VGA_1024X768P_71D799,
      0,  // TVIN_SIG_FMT_VGA_1024X1024P_60D000,
      0,  // TVIN_SIG_FMT_VGA_1152X864P_70D012,
      0,  // TVIN_SIG_FMT_VGA_1152X864P_75D000,
      0,  // TVIN_SIG_FMT_VGA_1152X864P_84D999,   // 50
      0,  // TVIN_SIG_FMT_VGA_1152X870P_75D062,
      0,  // TVIN_SIG_FMT_VGA_1152X900P_65D950,
      0,  // TVIN_SIG_FMT_VGA_1152X900P_66D004,
      0,  // TVIN_SIG_FMT_VGA_1152X900P_76D047,
      0,  // TVIN_SIG_FMT_VGA_1152X900P_76D149,
      0,  // TVIN_SIG_FMT_VGA_1280X720P_59D855,
      0,  // TVIN_SIG_FMT_VGA_1280X768P_59D870,
      0,  // TVIN_SIG_FMT_VGA_1280X768P_59D995,
      0,  // TVIN_SIG_FMT_VGA_1280X768P_60D100,
      0,  // TVIN_SIG_FMT_VGA_1280X768P_74D893,   // 60
      0,  // TVIN_SIG_FMT_VGA_1280X768P_84D837,
      0,  // TVIN_SIG_FMT_VGA_1280X800P_59D810,
      0,  // TVIN_SIG_FMT_VGA_1280X960P_60D000,
      0,  // TVIN_SIG_FMT_VGA_1280X960P_75D000,
      0,  // TVIN_SIG_FMT_VGA_1280X960P_85D002,
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_60D020,
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_75D025,
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_85D024,
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_59D979,
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_72D005,   // 70
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_60D002,
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_67D003,
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_74D112,
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_76D179,
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_66D718,
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_66D677,
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_76D107,
      0,  // TVIN_SIG_FMT_VGA_1280X1024P_59D996,
      0,  // TVIN_SIG_FMT_VGA_1360X768P_59D799,
      0,  // TVIN_SIG_FMT_VGA_1440X1080P_60D000,   // 80
      0,  // TVIN_SIG_FMT_VGA_1600X1200P_60D000,
      0,  // TVIN_SIG_FMT_VGA_1600X1200P_65D000,
      0,  // TVIN_SIG_FMT_VGA_1600X1200P_70D000,
      0,  // TVIN_SIG_FMT_VGA_1680X1080P_60D000,
      0,  // TVIN_SIG_FMT_VGA_1920X1080P_59D963,
      0,  // TVIN_SIG_FMT_VGA_1920X1080P_60D000,
      0,  // TVIN_SIG_FMT_VGA_1920X1200P_59D950,
      0,  // TVIN_SIG_FMT_VGA_MAX,
  #endif
    //Component format
    -47,   // TVIN_SIG_FMT_COMPONENT_480P_60D000,
      6,   // TVIN_SIG_FMT_COMPONENT_480I_59D940,
    -47,   // TVIN_SIG_FMT_COMPONENT_576P_50D000,
      3,   // TVIN_SIG_FMT_COMPONENT_576I_50D000,
    -29,   // TVIN_SIG_FMT_COMPONENT_720P_59D940,
    -29,   // TVIN_SIG_FMT_COMPONENT_720P_50D000,
      0,   // TVIN_SIG_FMT_COMPONENT_1080P_23D976,
      0,   // TVIN_SIG_FMT_COMPONENT_1080P_24D000,
     14,   // TVIN_SIG_FMT_COMPONENT_1080P_25D000,    // 90
     14,   // TVIN_SIG_FMT_COMPONENT_1080P_30D000,
    -29,   // TVIN_SIG_FMT_COMPONENT_1080P_50D000,
    -41,   // TVIN_SIG_FMT_COMPONENT_1080P_60D000,
      0,   // TVIN_SIG_FMT_COMPONENT_1080I_47D952,
      0,   // TVIN_SIG_FMT_COMPONENT_1080I_48D000,
    -29,   // TVIN_SIG_FMT_COMPONENT_1080I_50D000_A,
      0,   // TVIN_SIG_FMT_COMPONENT_1080I_50D000_B,
      0,   // TVIN_SIG_FMT_COMPONENT_1080I_50D000_C,
    -28,   // TVIN_SIG_FMT_COMPONENT_1080I_60D000,    //99
};

// *****************************************************************************
// Function:
//
//   Params:
//
//   Return:
//
// *****************************************************************************
enum tvafe_adc_pin_e tvafe_get_free_pga_pin(struct tvafe_pin_mux_s *pinmux)
{
    unsigned int i = 0;
    unsigned int flag = 0;
    enum tvafe_adc_pin_e ret = TVAFE_ADC_PIN_NULL;

    for (i=0; i<TVAFE_SRC_SIG_MAX_NUM; i++)
    {
        if (pinmux->pin[i] == TVAFE_ADC_PIN_A_PGA_0)
            flag |= 0x00000001;
        if (pinmux->pin[i] == TVAFE_ADC_PIN_A_PGA_1)
            flag |= 0x00000002;
        if (pinmux->pin[i] == TVAFE_ADC_PIN_A_PGA_2)
            flag |= 0x00000004;
        if (pinmux->pin[i] == TVAFE_ADC_PIN_A_PGA_3)
            flag |= 0x00000008;
    }
    if (!(flag&0x00000001))
    {
        ret = TVAFE_ADC_PIN_A_PGA_0;
    }
    else if (!(flag&0x00000002))
    {
        ret = TVAFE_ADC_PIN_A_PGA_1;
    }
    else if (!(flag&0x00000004))
    {
        ret = TVAFE_ADC_PIN_A_PGA_2;
    }
    else if (!(flag&0x00000008))
    {
        ret = TVAFE_ADC_PIN_A_PGA_3;
    }
    else // In the worst case, CVBS_OUT links to TV
    {
        ret = pinmux->pin[CVBS0_Y];
    }
    return ret;
}
#include <linux/kernel.h>
static inline enum tvafe_adc_ch_e tvafe_pin_adc_muxing(enum tvafe_adc_pin_e pin)
{
    enum tvafe_adc_ch_e ret = TVAFE_ADC_CH_NULL;

    if ((pin >= TVAFE_ADC_PIN_A_PGA_0) && (pin <= TVAFE_ADC_PIN_A_PGA_3))
    {
        WRITE_APB_REG_BITS(ADC_REG_06, 1, ENPGA_BIT, ENPGA_WID);
        WRITE_APB_REG_BITS(ADC_REG_17, pin-TVAFE_ADC_PIN_A_PGA_0, INMUXA_BIT, INMUXA_WID);
        ret = TVAFE_ADC_CH_PGA;
    }
    else if ((pin >= TVAFE_ADC_PIN_A_0) && (pin <= TVAFE_ADC_PIN_A_3))
    {
        WRITE_APB_REG_BITS(ADC_REG_06, 0, ENPGA_BIT, ENPGA_WID);
        WRITE_APB_REG_BITS(ADC_REG_17, pin-TVAFE_ADC_PIN_A_0, INMUXA_BIT, INMUXA_WID);
        ret = TVAFE_ADC_CH_A;
    }
    else if ((pin >= TVAFE_ADC_PIN_B_0) && (pin <= TVAFE_ADC_PIN_B_4))
    {
        WRITE_APB_REG_BITS(ADC_REG_17, pin-TVAFE_ADC_PIN_B_0, INMUXB_BIT, INMUXB_WID);
        ret = TVAFE_ADC_CH_B;
    }
    else if ((pin >= TVAFE_ADC_PIN_C_0) && (pin <= TVAFE_ADC_PIN_C_4))
    {
        WRITE_APB_REG_BITS(ADC_REG_18, pin-TVAFE_ADC_PIN_C_0, INMUXC_BIT, INMUXC_WID);
        ret = TVAFE_ADC_CH_C;
    }
    return ret;
}

/*
000: abc
001: acb
010: bac
011: bca
100: cab
101: cba
*/
static inline int tvafe_adc_top_muxing(enum tvafe_adc_ch_e gy,
                                        enum tvafe_adc_ch_e bpb,
                                        enum tvafe_adc_ch_e rpr,
                                        unsigned int s_video_flag)
{
    int ret = 0;

    switch (gy)
    {
        case TVAFE_ADC_CH_PGA:
        case TVAFE_ADC_CH_A:
            switch (bpb)
            {
                case TVAFE_ADC_CH_B:
                    // abc => abc
                    if (s_video_flag || (rpr == TVAFE_ADC_CH_C))
                    {
                        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, SWT_GY_BCB_RCR_IN_BIT,
                            SWT_GY_BCB_RCR_IN_WID);
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                    break;
                case TVAFE_ADC_CH_C:
                    // acb => abc
                    if (s_video_flag || (rpr == TVAFE_ADC_CH_B))
                    {
                        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, SWT_GY_BCB_RCR_IN_BIT,
                            SWT_GY_BCB_RCR_IN_WID);
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                    break;
                default:
                    ret = -EFAULT;
                    break;
            }
            break;
        case TVAFE_ADC_CH_B:
            switch (bpb)
            {
                case TVAFE_ADC_CH_PGA:
                case TVAFE_ADC_CH_A:
                    // bac => abc
                    if (s_video_flag || (rpr == TVAFE_ADC_CH_C))
                    {
                        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 2, SWT_GY_BCB_RCR_IN_BIT,
                            SWT_GY_BCB_RCR_IN_WID);
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                    break;
                case TVAFE_ADC_CH_C:
                    // bca => abc
                    if (s_video_flag || (rpr == TVAFE_ADC_CH_PGA)
                        || (rpr == TVAFE_ADC_CH_A))
                    {
                        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 3, SWT_GY_BCB_RCR_IN_BIT,
                            SWT_GY_BCB_RCR_IN_WID);
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                    break;
                default:
                    ret = -EFAULT;
                    break;
            }
            break;
        case TVAFE_ADC_CH_C:
            switch (bpb)
            {
                case TVAFE_ADC_CH_PGA:
                case TVAFE_ADC_CH_A:
                    // cab => abc
                    if (s_video_flag || (rpr == TVAFE_ADC_CH_B))
                    {
                        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 4, SWT_GY_BCB_RCR_IN_BIT,
                            SWT_GY_BCB_RCR_IN_WID);
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                    break;
                case TVAFE_ADC_CH_B:
                    // cba => abc
                    if (s_video_flag || (rpr == TVAFE_ADC_CH_PGA)
                        || (rpr == TVAFE_ADC_CH_A))
                    {
                        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 5, SWT_GY_BCB_RCR_IN_BIT,
                            SWT_GY_BCB_RCR_IN_WID);
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                    break;
                default:
                    ret = -EFAULT;
                    break;
            }
            break;
        default:
            ret = -EFAULT;
            break;
    }
    return ret;
}

int tvafe_source_muxing(struct tvafe_info_s *info)
{
    int ret = 0;

    switch (info->param.port)
    {
        case TVIN_PORT_CVBS0:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS0_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS0_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS0_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS0_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
			}
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_CVBS1:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS1_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS1_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS1_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS1_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_CVBS2:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS2_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS2_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS2_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS2_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_CVBS3:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS3_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS3_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS3_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS3_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_CVBS4:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS4_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS4_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS4_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS5_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
         case TVIN_PORT_CVBS5:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS5_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS5_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS5_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS5_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_CVBS6:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS6_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS6_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS6_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS6_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_CVBS7:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS7_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS7_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS7_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS7_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO0:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO0_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO0_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO0_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO0_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO1:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO1_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO1_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO1_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO1_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO2:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO2_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO2_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO2_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO2_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO3:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO3_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO3_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO3_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO3_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO4:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO4_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO4_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO4_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO4_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO5:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO5_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO5_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO5_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO5_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO6:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO6_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO6_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO6_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO6_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO7:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO7_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO7_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO7_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO7_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA0:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA0_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA0_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA0_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA0_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA0_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);

                WRITE_APB_REG_BITS(ADC_REG_39, 0, INSYNCMUXCTRL_BIT, INSYNCMUXCTRL_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA1:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA1_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA1_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA1_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA1_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA1_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                WRITE_APB_REG_BITS(ADC_REG_39, 1, INSYNCMUXCTRL_BIT, INSYNCMUXCTRL_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA2:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA2_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA2_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA2_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA2_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA2_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA3:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA3_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA3_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA3_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA3_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA3_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA4:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA4_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA4_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA4_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA4_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA4_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA5:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA5_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA5_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA5_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA5_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA5_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA6:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA6_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA6_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA6_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA6_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA6_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA7:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA7_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA7_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA7_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA7_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA7_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP0:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP0_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP0_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP0_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP0_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP0_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP1:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP1_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP1_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP1_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP1_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP1_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP2:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP2_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP2_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP2_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP2_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP2_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP3:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP3_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP3_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP3_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP3_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP3_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP4:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP4_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP4_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP4_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP4_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP4_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP5:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP5_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP5_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP5_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP5_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP5_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP6:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP6_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP6_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP6_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP6_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP6_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP7:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP7_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP7_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP7_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP7_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP7_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        default:
            ret = -EFAULT;
            break;
    }

    return ret;
}

void tvafe_vga_set_edid(struct tvafe_vga_edid_s *edid)
{
    unsigned int i = 0;

    // diable TCON
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_7, 0,  1, 1);
    // diable DVIN
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 27, 1);
    // DDC_SDA0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 13, 1);
    // DDC_SCL0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 12, 1);

    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, EDID_CLK_EN_BIT,EDID_CLK_EN_WID); // VGA_CLK_EN
    // APB Bus accessing mode
    WRITE_APB_REG(TVFE_EDID_CONFIG, 0x00000000);
    WRITE_APB_REG(TVFE_EDID_RAM_ADDR, 0x00000000);
    for (i=0; i<256; i++)
        WRITE_APB_REG(TVFE_EDID_RAM_WDATA, (unsigned int)edid->value[i]);
    // Slave IIC acessing mode, 8-bit standard IIC protocol
    WRITE_APB_REG(TVFE_EDID_CONFIG, TVAFE_EDID_CONFIG);
}

void tvafe_vga_get_edid(struct tvafe_vga_edid_s *edid)
{
    unsigned int i = 0;

    // diable TCON
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_7, 0,  1, 1);
    // diable DVIN
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 27, 1);
    // DDC_SDA0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 13, 1);
    // DDC_SCL0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 12, 1);

    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, EDID_CLK_EN_BIT,EDID_CLK_EN_WID); // VGA_CLK_EN
    // APB Bus accessing mode
    WRITE_APB_REG(TVFE_EDID_CONFIG, 0x00000000);
    WRITE_APB_REG(TVFE_EDID_RAM_ADDR, 0x00000100);
    for (i=0; i<256; i++)
        edid->value[i] = (unsigned char)(READ_APB_REG_BITS(TVFE_EDID_RAM_RDATA, EDID_RAM_RDATA_BIT, EDID_RAM_RDATA_WID));
    // Slave IIC acessing mode, 8-bit standard IIC protocol
    WRITE_APB_REG(TVFE_EDID_CONFIG, TVAFE_EDID_CONFIG);

    return;
}

///////////////////TVFE top control////////////////////
const static unsigned int aafilter_ctl[][2] = {

    //TVIN_SIG_FMT_NULL = 0,
	{0,0},
    //VDIN_SIG_FORMAT_VGA_512X384P_60D147,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_560X384P_60D147,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X200P_59D924,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X350P_85D080,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X400P_59D940,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X400P_85D080,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X400P_59D638,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X400P_56D416,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480I_29D970,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_66D619,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_66D667,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_59D940,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_72D809,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_75D000_A,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_85D008,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_59D638,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_75D000_B,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X870P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_720X350P_70D086,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_720X400P_85D039,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_720X400P_70D086,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_720X400P_87D849,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_720X400P_59D940,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_720X480P_59D940,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_752X484I_29D970,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_768X574I_25D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_800X600P_56D250,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_800X600P_60D317,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_800X600P_72D188,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_800X600P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_800X600P_85D061,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_832X624P_75D087,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_848X480P_84D751,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_59D278,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_74D927,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768I_43D479,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_60D004,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_70D069,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_75D029,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_84D997,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_74D925,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_75D020,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_70D008,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_75D782,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_77D069,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_71D799,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X1024P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1053X754I_43D453,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1056X768I_43D470,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1120X750I_40D021,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X864P_70D012,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X864P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X864P_84D999,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X870P_75D062,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X900P_65D950,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X900P_66D004,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X900P_76D047,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X900P_76D149,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1244X842I_30D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X768P_59D995,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X768P_74D893,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X768P_84D837,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X960P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X960P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X960P_85D002,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024I_43D436,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_60D020,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_75D025,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_85D024,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_59D979,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_72D005,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_60D002,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_67D003,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_74D112,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_76D179,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_66D718,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_66D677,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_76D107,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_59D996,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1360X768P_59D799,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1360X1024I_51D476,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1440X1080P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200I_48D040,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200P_65D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200P_70D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200P_80D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200P_85D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1280P_66D931,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1680X1080P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1792X1344P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1792X1344P_74D997,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1856X1392P_59D995,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1856X1392P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1868X1200P_75D000,
    //VDIN_SIG_FORMAT_VGA_1920X1080P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1080P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1080P_85D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1200P_84D932,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1200P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1200P_85D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1234P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1234P_85D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1440P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1440P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_2048X1536P_60D000_A,
	{
	        0x00082222, //
	        0x252b39c6, //
    },
    //VDIN_SIG_FORMAT_VGA_2048X1536P_75D000,
    {0,0},
    //VDIN_SIG_FORMAT_VGA_2048X1536P_60D000_B,
    {0,0},
    //VDIN_SIG_FORMAT_VGA_2048X2048P_60D008,
    {0,0},
    //TVIN_SIG_FMT_VGA_MAX,
    {0,0},

///////////////////////////////////////////////////////////////
    //VDIN_SIG_FORMAT_COMPONENT_480P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_480I_59D940,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_576P_50D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_576I_50D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_720P_59D940,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_720P_50D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080P_23D976,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080P_24D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080P_25D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080P_30D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080P_50D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_29D970,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_47D952,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_48D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_50D000_A,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_50D000_B,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_50D000_C,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //TVIN_SIG_FMT_COMP_MAX,
    {0,0},

    //VDIN_SIG_FORMAT_CVBS_NTSC_M,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_CVBS_NTSC_443,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_CVBS_PAL_I,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_CVBS_PAL_M,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_CVBS_PAL_60,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_CVBS_PAL_CN,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_CVBS_SECAM,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },

    //VDIN_SIG_FORMAT_MAX,
	{0,0 }
};

// *****************************************************************************
// Function:set aafilter control
//
//   Params: format index
//
//   Return: none
//
// *****************************************************************************
void tvafe_top_set_aafilter_control(enum tvin_sig_fmt_e fmt)
{

    WRITE_APB_REG(TVFE_AAFILTER_CTRL1, aafilter_ctl[fmt][0]);
    WRITE_APB_REG(TVFE_AAFILTER_CTRL2, aafilter_ctl[fmt][1]);


    return;
}

// *****************************************************************************
// Function:set bp gate of tvfe top module
//
//   Params: format index
//
//   Return: none
//
// *****************************************************************************
void tvafe_top_set_bp_gate(enum tvin_sig_fmt_e fmt)
{
    unsigned int h_bp_end,h_bp_start;
    unsigned int v_bp_end,v_bp_start;

    h_bp_start = tvin_fmt_tbl[fmt].hs_width + 1;
    WRITE_APB_REG_BITS(TVFE_BPG_BACKP_H, h_bp_start, BACKP_H_ST_BIT, BACKP_H_ST_WID);

    h_bp_end = tvin_fmt_tbl[fmt].h_total
                - tvin_fmt_tbl[fmt].hs_front + 1;
    WRITE_APB_REG_BITS(TVFE_BPG_BACKP_H, h_bp_end, BACKP_H_ED_BIT, BACKP_H_ED_WID);

    v_bp_start = tvin_fmt_tbl[fmt].vs_width + 1;
    WRITE_APB_REG_BITS(TVFE_BPG_BACKP_V, v_bp_start, BACKP_V_ST_BIT, BACKP_V_ST_WID);

    v_bp_end = tvin_fmt_tbl[fmt].v_total
                - tvin_fmt_tbl[fmt].vs_front + 1;
    WRITE_APB_REG_BITS(TVFE_BPG_BACKP_V, v_bp_end, BACKP_V_ED_BIT, BACKP_V_ED_WID);

    return;
}

// *****************************************************************************
// Function:set mvdet control of tvfe module
//
//   Params: format index
//
//   Return: none
//
// *****************************************************************************
void tvafe_top_set_mvdet_control(enum tvin_sig_fmt_e fmt)
{
    unsigned int sd_mvd_reg_15_1b[] = {0, 0, 0, 0, 0, 0, 0,};

    if ((fmt > TVIN_SIG_FMT_COMP_480P_60D000)
        && (fmt < TVIN_SIG_FMT_COMP_576I_50D000)) {
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL1, sd_mvd_reg_15_1b[0]);
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL2, sd_mvd_reg_15_1b[1]);
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL3, sd_mvd_reg_15_1b[2]);
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL4, sd_mvd_reg_15_1b[3]);
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL5, sd_mvd_reg_15_1b[4]);
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL6, sd_mvd_reg_15_1b[5]);
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL7, sd_mvd_reg_15_1b[6]);

    }

    return;
}

// *****************************************************************************
// Function:set wss of tvfe top module
//
//   Params: format index
//
//   Return: none
//
// *****************************************************************************
void tvafe_top_set_wss_control(enum tvin_sig_fmt_e fmt)
{
    unsigned int hd_mvd_reg_2a_2d[] = {0, 0, 0, 0};

    if (fmt > TVIN_SIG_FMT_COMP_720P_59D940
        && fmt < TVIN_SIG_FMT_COMP_1080I_60D000) {
        WRITE_APB_REG(TVFE_MISC_WSS1_MUXCTRL1, hd_mvd_reg_2a_2d[0]);
        WRITE_APB_REG(TVFE_MISC_WSS1_MUXCTRL2, hd_mvd_reg_2a_2d[1]);
        WRITE_APB_REG(TVFE_MISC_WSS2_MUXCTRL1, hd_mvd_reg_2a_2d[2]);
        WRITE_APB_REG(TVFE_MISC_WSS2_MUXCTRL2, hd_mvd_reg_2a_2d[3]);
    }

    return;
}

// *****************************************************************************
// Function:set sfg control of tvfe top module
//
//   Params: format index
//
//   Return: none
//
// *****************************************************************************
void tvafe_top_set_sfg_mux_control(enum tvin_sig_fmt_e fmt)
{
    unsigned int h_total = tvin_fmt_tbl[fmt].h_total, pixel_clk = tvin_fmt_tbl[fmt].pixel_clk, tmp = 0;

    if (pixel_clk)
    {
        //tmp = (h_total*219)/pixel_clk; // ((h_total*5M/(pixel_clk*10K))/2)*7/8 = h_total*219/pixel_clk
        //if (tmp > 255)
        //    tmp = 255;
        //WRITE_APB_REG(ADC_REG_31, tmp);
        tmp = (h_total+2)>>2;
    WRITE_APB_REG_BITS(TVFE_SYNCTOP_SFG_MUXCTRL1, tmp, SFG_DET_HSTART_BIT, SFG_DET_HSTART_WID);
        tmp = (h_total+h_total+h_total+2)>>2;
    WRITE_APB_REG_BITS(TVFE_SYNCTOP_SFG_MUXCTRL1, tmp, SFG_DET_HEND_BIT, SFG_DET_HEND_WID);
    }

    return;
}

enum tvin_scan_mode_e tvafe_top_get_scan_mode(void)
{
    unsigned int scan_mode;

    scan_mode = READ_APB_REG_BITS(TVFE_SYNCTOP_INDICATOR3,
                        SFG_PROGRESSIVE_BIT, SFG_PROGRESSIVE_WID);

    if (scan_mode == 0)
        return TVIN_SCAN_MODE_INTERLACED;
    else
        return TVIN_SCAN_MODE_PROGRESSIVE;
}
//
//function:the parameter stdvalue  replace the parameter original
//
void tvafe_reset_cal_value(struct tvafe_adc_cal_s *original,struct tvafe_adc_cal_s *stdvalue)
{
	#ifdef LOG_ADC_CAL
	printk("++++++reset cal value\n");
	#endif
	//a
	original->a_analog_clamp    = stdvalue->a_analog_clamp;
	original->a_analog_gain     = stdvalue->a_analog_gain;
	original->a_digital_offset1 = stdvalue->a_digital_offset1;
	//b
	original->b_analog_clamp    = stdvalue->b_analog_clamp;
	original->b_analog_gain     = stdvalue->b_analog_gain;
	original->b_digital_offset1 = stdvalue->b_digital_offset1;
	//c
	original->c_analog_clamp    = stdvalue->c_analog_clamp;
	original->c_analog_gain     = stdvalue->c_analog_gain;
	original->c_digital_offset1 = stdvalue->c_digital_offset1;
}
//
//function:compare calibration value (calvalue) with standard value (stdvalue)
//
unsigned char tvafe_compare_cal_value(unsigned short calvalue,unsigned short stdvalue)
{
	unsigned char ret=0;
	if(calvalue > stdvalue)
		{
			if((calvalue - stdvalue) > threshold_value)
				{
					ret=1;
					return ret;
				}
		}
	else if((stdvalue - calvalue) > threshold_value)
		{
			ret=1;
			return ret;
		}
	return ret;

}

//
//function:if adc_cal value is not valid,then adc_cal value will been replaced
//
void tvafe_adjust_cal_value(struct tvafe_adc_cal_s *para,bool iscomponent)
{
	unsigned char flag = 0;
	struct tvafe_adc_cal_s *stdvaluep = iscomponent ? (&cal_std_value_component) : (&cal_std_value_vga);

	flag |= tvafe_compare_cal_value(para->a_analog_clamp,   stdvaluep->a_analog_clamp   );
	flag |= tvafe_compare_cal_value(para->a_analog_gain,    stdvaluep->a_analog_gain    );

	flag |= tvafe_compare_cal_value(para->b_analog_clamp,   stdvaluep->b_analog_clamp   );
	flag |= tvafe_compare_cal_value(para->b_analog_gain,    stdvaluep->b_analog_gain    );

	flag |= tvafe_compare_cal_value(para->c_analog_clamp,   stdvaluep->c_analog_clamp   );
	flag |= tvafe_compare_cal_value(para->c_analog_gain,    stdvaluep->c_analog_gain    );

	if(flag)
		tvafe_reset_cal_value(para,stdvaluep);

}


// *****************************************************************************
// Function: get & set cal result & internal cal
//
//   Params: system info
//
//   Return: none
//
// *****************************************************************************
void tvafe_set_cal_value(struct tvafe_adc_cal_s *para)
{
    unsigned int clamp_h = 0, clamp_l = 0;

    if (!(para->reserved & TVAFE_ADC_CAL_VALID))
        return;

    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET1, 1, OGO_EN_BIT, OGO_EN_WID);

    clamp_h = (para->a_analog_clamp)>>2;
    clamp_l = (para->a_analog_clamp)&3;
    WRITE_APB_REG_BITS(ADC_REG_0B, clamp_h, CTRCLREFA_0B_BIT, CTRCLREFA_0B_WID);
    WRITE_APB_REG_BITS(ADC_REG_0F, clamp_l, CTRCLREFA_0F_BIT, CTRCLREFA_0F_WID);
    clamp_h = (para->b_analog_clamp)>>2;
    clamp_l = (para->b_analog_clamp)&3;
    WRITE_APB_REG_BITS(ADC_REG_0C, clamp_h, CTRCLREFB_0C_BIT, CTRCLREFB_0C_WID);
    WRITE_APB_REG_BITS(ADC_REG_10, clamp_l, CTRCLREFB_10_BIT, CTRCLREFB_10_WID);
    clamp_h = (para->c_analog_clamp)>>2;
    clamp_l = (para->c_analog_clamp)&3;
    WRITE_APB_REG_BITS(ADC_REG_0D, clamp_h, CTRCLREFC_0D_BIT, CTRCLREFC_0D_WID);
    WRITE_APB_REG_BITS(ADC_REG_11, clamp_l, CTRCLREFC_11_BIT, CTRCLREFC_11_WID);

    WRITE_APB_REG_BITS(ADC_REG_07, para->a_analog_gain, ADCGAINA_BIT, ADCGAINA_WID);
    WRITE_APB_REG_BITS(ADC_REG_08, para->b_analog_gain, ADCGAINB_BIT, ADCGAINB_WID);
    WRITE_APB_REG_BITS(ADC_REG_09, para->c_analog_gain, ADCGAINC_BIT, ADCGAINC_WID);

    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET1, para->a_digital_offset1,
        OGO_YG_OFFSET1_BIT, OGO_YG_OFFSET1_WID);
    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET1, para->b_digital_offset1,
        OGO_UB_OFFSET1_BIT, OGO_UB_OFFSET1_WID);
    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET3, para->c_digital_offset1,
        OGO_VR_OFFSET1_BIT, OGO_VR_OFFSET1_WID);

    WRITE_APB_REG_BITS(TVFE_OGO_GAIN1, para->a_digital_gain,
        OGO_YG_GAIN_BIT, OGO_YG_GAIN_WID);
    WRITE_APB_REG_BITS(TVFE_OGO_GAIN1, para->b_digital_gain,
        OGO_UB_GAIN_BIT, OGO_UB_GAIN_WID);
    WRITE_APB_REG_BITS(TVFE_OGO_GAIN2, para->c_digital_gain,
        OGO_VR_GAIN_BIT, OGO_VR_GAIN_WID);

    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET2, para->a_digital_offset2,
        OGO_YG_OFFSET2_BIT, OGO_YG_OFFSET2_WID);
    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET2, para->b_digital_offset2,
        OGO_UB_OFFSET2_BIT, OGO_UB_OFFSET2_WID);
    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET3, para->c_digital_offset2,
        OGO_VR_OFFSET2_BIT, OGO_VR_OFFSET2_WID);
}

void tvafe_get_cal_value(struct tvafe_adc_cal_s *para)
{
    unsigned int clamp_h = 0, clamp_l = 0;

    clamp_h = READ_APB_REG_BITS(ADC_REG_0B, CTRCLREFA_0B_BIT, CTRCLREFA_0B_WID);
    clamp_l = READ_APB_REG_BITS(ADC_REG_0F, CTRCLREFA_0F_BIT, CTRCLREFA_0F_WID);
    para->a_analog_clamp = (clamp_h<<2)|clamp_l;
    clamp_h = READ_APB_REG_BITS(ADC_REG_0C, CTRCLREFB_0C_BIT, CTRCLREFB_0C_WID);
    clamp_l = READ_APB_REG_BITS(ADC_REG_10, CTRCLREFB_10_BIT, CTRCLREFB_10_WID);
    para->b_analog_clamp = (clamp_h<<2)|clamp_l;
    clamp_h = READ_APB_REG_BITS(ADC_REG_0D, CTRCLREFC_0D_BIT, CTRCLREFC_0D_WID);
    clamp_l = READ_APB_REG_BITS(ADC_REG_11, CTRCLREFC_11_BIT, CTRCLREFC_11_WID);
    para->c_analog_clamp = (clamp_h<<2)|clamp_l;

    para->a_analog_gain = READ_APB_REG_BITS(ADC_REG_07, ADCGAINA_BIT, ADCGAINA_WID);
    para->b_analog_gain = READ_APB_REG_BITS(ADC_REG_08, ADCGAINB_BIT, ADCGAINB_WID);
    para->c_analog_gain = READ_APB_REG_BITS(ADC_REG_09, ADCGAINC_BIT, ADCGAINC_WID);

    para->a_digital_offset1 = READ_APB_REG_BITS(TVFE_OGO_OFFSET1, OGO_YG_OFFSET1_BIT, OGO_YG_OFFSET1_WID);
    para->b_digital_offset1 = READ_APB_REG_BITS(TVFE_OGO_OFFSET1, OGO_UB_OFFSET1_BIT, OGO_UB_OFFSET1_WID);
    para->c_digital_offset1 = READ_APB_REG_BITS(TVFE_OGO_OFFSET3, OGO_VR_OFFSET1_BIT, OGO_VR_OFFSET1_WID);

    para->a_digital_gain = READ_APB_REG_BITS(TVFE_OGO_GAIN1, OGO_YG_GAIN_BIT, OGO_YG_GAIN_WID);
    para->b_digital_gain = READ_APB_REG_BITS(TVFE_OGO_GAIN1, OGO_UB_GAIN_BIT, OGO_UB_GAIN_WID);
    para->c_digital_gain = READ_APB_REG_BITS(TVFE_OGO_GAIN2, OGO_VR_GAIN_BIT, OGO_VR_GAIN_WID);

    para->a_digital_offset2 = READ_APB_REG_BITS(TVFE_OGO_OFFSET2, OGO_YG_OFFSET2_BIT, OGO_YG_OFFSET2_WID);
    para->b_digital_offset2 = READ_APB_REG_BITS(TVFE_OGO_OFFSET2, OGO_UB_OFFSET2_BIT, OGO_UB_OFFSET2_WID);
    para->c_digital_offset2 = READ_APB_REG_BITS(TVFE_OGO_OFFSET3, OGO_VR_OFFSET2_BIT, OGO_VR_OFFSET2_WID);
}

void tvafe_adc_cal_read(unsigned char *ch, unsigned int *a, unsigned int *b, unsigned int *c)
{
    unsigned int i = 0, j = 0, data = 0, da[16], db[16], dc[16];

    for (i = 0; i < 16; i++)
    {
        data = READ_APB_REG(TVFE_ADC_READBACK_INDICATOR);
        da[i] = (data >> ADC_READBACK_DA_BIT) & ((1 << ADC_READBACK_DA_WID) - 1);
        db[i] = (data >> ADC_READBACK_DB_BIT) & ((1 << ADC_READBACK_DB_WID) - 1);
        dc[i] = (data >> ADC_READBACK_DC_BIT) & ((1 << ADC_READBACK_DC_WID) - 1);
    }
    for (i = 0; i < 15; i++)
    {
        for (j = i + 1; j < 16; j++)
        {
            if (da[i] > da[j])
            {
                data  = da[i];
                da[i] = da[j];
                da[j] = data;
            }
            if (db[i] > db[j])
            {
                data  = db[i];
                db[i] = db[j];
                db[j] = data;
            }
            if (dc[i] > dc[j])
            {
                data  = dc[i];
                dc[i] = dc[j];
                dc[j] = data;
            }
        }
    }
    *a = *b = *c = 4;
    for (i = 4; i < 12; i++)
    {
        *a += da[i];
        *b += db[i];
        *c += dc[i];
    }
    *a >>= 3;
    *b >>= 3;
    *c >>= 3;
    #ifdef LOG_ADC_CAL
        pr_info("%s = %4d %4d %4d\n", ch, (int)*a, (int)*b, (int)*c);
    #endif
}

// for Y:
// diff*0.777V/(479/1023)V <= 1023, diff <=616
// standard is 0.7V, to support -4% ~ +107% supper black/white, increase the range to 111% of 0.7V
// for Cb/Cr/R/G/B:
// diff*0.7V/(479/1023)V <= 1023, diff <= 684
// standard is 0.7V
bool tvafe_gain_overflow(unsigned int a1, unsigned int a2, bool is_component)
{
    unsigned int win = is_component ? 616 : 684;

    if ((a2 - a1) > win)
        return (true);
    else
        return (false);
}

// #define TVAFE_ADC_CAL_VALIDATION
#define TVAFE_ADC_CAL_STEP_GAIN  1
#define TVAFE_ADC_CAL_STEP_CLAMP 1
#define TVAFE_ADC_CAL_STEP_SHIFT 1
#define TVAFE_ADC_CAL_STEP_STAGE (TVAFE_ADC_CAL_STEP_GAIN*16 + TVAFE_ADC_CAL_STEP_SHIFT)

// standard is 16~235, supper black/white is 7~250
bool tvafe_adc_cal(struct tvafe_info_s *info, struct tvafe_operand_s *operand)
{
    struct tvafe_adc_cal_s *adc_cal = &info->adc_cal_val;
    bool is_component = (info->param.port >= TVIN_PORT_COMP0) && (info->param.port <= TVIN_PORT_COMP7);
    unsigned int a = 0, b = 0, c = 0, mutex = 0;
    unsigned int z_lu = is_component ?  37 :    0; //  37 = 1023*4%/111%
    unsigned int z_ch = is_component ? 512 :    0;
    unsigned int l_dg = is_component ? 975 : 1024; // 976 = 219*111%*4+3
    unsigned int l_do = is_component ?  28 :    0; //  28 = 7*4
    unsigned int c_dg = is_component ? 899 : 1024; // 899 = 224*4+3
    unsigned int c_do = is_component ?  64 :    0; //  64 = 16*4
    unsigned short step_up = 0, step_dn = 0;

    switch (operand->step++)
    {
        case TVAFE_ADC_CAL_STEP_GAIN*0:
            // init
            adc_cal->a_analog_clamp    =   64;
            adc_cal->a_analog_gain     =  128;
            adc_cal->a_digital_gain    = l_dg; // 0dB
            adc_cal->a_digital_offset1 =    0;
            adc_cal->a_digital_offset2 = l_do;
            adc_cal->b_analog_clamp    =   64;
            adc_cal->b_analog_gain     =  128;
            adc_cal->b_digital_gain    = c_dg; // 0dB
            adc_cal->b_digital_offset1 =    0;
            adc_cal->b_digital_offset2 = c_do;
            adc_cal->c_analog_clamp    =   64;
            adc_cal->c_analog_gain     =  128;
            adc_cal->c_digital_gain    = c_dg; // 0dB
            adc_cal->c_digital_offset1 =    0;
            adc_cal->c_digital_offset2 = c_do;
            // config readback
            WRITE_APB_REG_BITS(TVFE_ADC_READBACK_CTRL, 0, ADC_READBACK_MODE_BIT , ADC_READBACK_MODE_WID );
            // record bpg_h, bpg_v, bpg_m, clamp_inv, clamp_ext, clk_ext, clk_ctl, lpf_a, lpf_b, lpf_c
            operand->bpg_h     = READ_APB_REG(TVFE_BPG_BACKP_H);
            operand->bpg_v     = READ_APB_REG(TVFE_BPG_BACKP_V);
            operand->bpg_m     = READ_APB_REG_BITS(TVFE_TOP_CTRL,     TVFE_BACKP_GATE_MUX_BIT, TVFE_BACKP_GATE_MUX_WID);
            operand->clamp_inv = READ_APB_REG_BITS(TVFE_DVSS_MUXCTRL, DVSS_CLAMP_INV_BIT,      DVSS_CLAMP_INV_WID     );
            operand->clamp_ext = READ_APB_REG_BITS(ADC_REG_2F,        CLAMPEXT_BIT,            CLAMPEXT_WID           );
          //  operand->lpf_a     = READ_APB_REG_BITS(ADC_REG_19, ENLPFA_BIT,    ENLPFA_WID   );
          //  operand->lpf_b     = READ_APB_REG_BITS(ADC_REG_1A, ENLPFB_BIT,    ENLPFB_WID   );
          //  operand->lpf_c     = READ_APB_REG_BITS(ADC_REG_1B, ENLPFC_BIT,    ENLPFC_WID   );
            operand->clk_ext   = READ_APB_REG_BITS(ADC_REG_58, EXTCLKSEL_BIT, EXTCLKSEL_WID);
            operand->clk_ctl   = READ_CBUS_REG(HHI_VAFE_CLKIN_CNTL);
            // disable lpf_a, lpf_b, lpf_c
          //  WRITE_APB_REG_BITS(ADC_REG_19, 0, ENLPFA_BIT, ENLPFA_WID);
          //  WRITE_APB_REG_BITS(ADC_REG_1A, 0, ENLPFB_BIT, ENLPFB_WID);
          //  WRITE_APB_REG_BITS(ADC_REG_1B, 0, ENLPFC_BIT, ENLPFC_WID);
            // set clk_ext & clk_ctl
            WRITE_APB_REG_BITS(ADC_REG_58, 1, EXTCLKSEL_BIT, EXTCLKSEL_WID);
            WRITE_CBUS_REG(HHI_VAFE_CLKIN_CNTL, 0x00000100);
            // load adc cal values
            adc_cal->reserved |= TVAFE_ADC_CAL_VALID;
            tvafe_set_cal_value(adc_cal);
            adc_cal->reserved &= ~TVAFE_ADC_CAL_VALID;
            // disable offset calibration
            WRITE_APB_REG_BITS(TVFE_VAFE_CTRL, 0, VAFE_ENOFFSETCAL_BIT, VAFE_ENOFFSETCAL_WID);
            // enable gain calibration
            WRITE_APB_REG_BITS(TVFE_VAFE_CTRL, 1, VAFE_ENGAINCAL_BIT, VAFE_ENGAINCAL_WID);
            // select level 1
            WRITE_APB_REG_BITS(TVFE_VAFE_CTRL, 1, VAFE_SELGAINCALLVL_BIT, VAFE_SELGAINCALLVL_WID);
            break;
        case TVAFE_ADC_CAL_STEP_GAIN*1:
        case TVAFE_ADC_CAL_STEP_GAIN*3:
        case TVAFE_ADC_CAL_STEP_GAIN*5:
        case TVAFE_ADC_CAL_STEP_GAIN*7:
        case TVAFE_ADC_CAL_STEP_GAIN*9:
        case TVAFE_ADC_CAL_STEP_GAIN*11:
        case TVAFE_ADC_CAL_STEP_GAIN*13:
        case TVAFE_ADC_CAL_STEP_GAIN*15:
            #ifdef LOG_ADC_CAL
                pr_info("\nA_gn = %4d %4d %4d\n", (int)adc_cal->a_analog_gain, (int)adc_cal->b_analog_gain, (int)adc_cal->c_analog_gain);
            #endif
            // get data of level 1
            tvafe_adc_cal_read("Lvl1", &operand->a, &operand->b, &operand->c);
            // select level 2
            WRITE_APB_REG_BITS(TVFE_VAFE_CTRL, 2, VAFE_SELGAINCALLVL_BIT, VAFE_SELGAINCALLVL_WID);
            break;
        case TVAFE_ADC_CAL_STEP_GAIN*2:
            if (!mutex)
            {
                mutex   =  1;
                step_up = 64;
                step_dn = 64;
            }
        case TVAFE_ADC_CAL_STEP_GAIN*4:
            if (!mutex)
            {
                mutex   =  1;
                step_up = 32;
                step_dn = 32;
            }
        case TVAFE_ADC_CAL_STEP_GAIN*6:
            if (!mutex)
            {
                mutex   =  1;
                step_up = 16;
                step_dn = 16;
            }
        case TVAFE_ADC_CAL_STEP_GAIN*8:
            if (!mutex)
            {
                mutex   = 1;
                step_up = 8;
                step_dn = 8;
            }
        case TVAFE_ADC_CAL_STEP_GAIN*10:
            if (!mutex)
            {
                mutex   = 1;
                step_up = 4;
                step_dn = 4;
            }
        case TVAFE_ADC_CAL_STEP_GAIN*12:
            if (!mutex)
            {
                mutex   = 1;
                step_up = 2;
                step_dn = 2;
            }
        case TVAFE_ADC_CAL_STEP_GAIN*14:
            if (!mutex)
            {
                mutex   = 1;
                step_up = 1;
                step_dn = 1;
            }
        case TVAFE_ADC_CAL_STEP_GAIN*16:
            if (!mutex)
            {
                mutex   = 1;
                step_up = 0;
                step_dn = 1;
            }
            // get data of level 2
            tvafe_adc_cal_read("Lvl2", &a, &b, &c);
            #ifdef LOG_ADC_CAL
                pr_info("Diff = %4d %4d %4d\n", (int)(a - operand->a), (int)(b - operand->b), (int)(c - operand->c));
            #endif
            // tune analog gain
            if (tvafe_gain_overflow(operand->a, a, is_component))
                adc_cal->a_analog_gain -= step_dn;
            else
                adc_cal->a_analog_gain += step_up;
            if (tvafe_gain_overflow(operand->b, b, false))
                adc_cal->b_analog_gain -= step_dn;
            else
                adc_cal->b_analog_gain += step_up;
            if (tvafe_gain_overflow(operand->c, c, false))
                adc_cal->c_analog_gain -= step_dn;
            else
                adc_cal->c_analog_gain += step_up;
            // load adc cal values
            adc_cal->reserved |= TVAFE_ADC_CAL_VALID;
            tvafe_set_cal_value(adc_cal);
            adc_cal->reserved &= ~TVAFE_ADC_CAL_VALID;
            // select level 1
            WRITE_APB_REG_BITS(TVFE_VAFE_CTRL, 1, VAFE_SELGAINCALLVL_BIT, VAFE_SELGAINCALLVL_WID);
            if (operand->step == TVAFE_ADC_CAL_STEP_GAIN*16 + 1)
            {
                #ifdef LOG_ADC_CAL
                    pr_info("\nfresh_adc_cal after gain\n");
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
                #endif
                // disable gain calibration
                WRITE_APB_REG_BITS(TVFE_VAFE_CTRL, 0, VAFE_ENGAINCAL_BIT, VAFE_ENGAINCAL_WID);
                // set bpg_h, bpg_v, bpg_m, clamp_inv, clamp_ext
                WRITE_APB_REG(TVFE_BPG_BACKP_H, 1);
                WRITE_APB_REG(TVFE_BPG_BACKP_V, 1);
            //    WRITE_APB_REG_BITS(TVFE_TOP_CTRL,     1, TVFE_BACKP_GATE_MUX_BIT, TVFE_BACKP_GATE_MUX_WID);
        //        WRITE_APB_REG_BITS(TVFE_DVSS_MUXCTRL, 1, DVSS_CLAMP_INV_BIT,      DVSS_CLAMP_INV_WID     );
                WRITE_APB_REG_BITS(ADC_REG_2F,        1, CLAMPEXT_BIT,            CLAMPEXT_WID           );
                WRITE_APB_REG_BITS(ADC_REG_58,        1, EXTCLKSEL_BIT,            EXTCLKSEL_WID           );			
            }
            break;
        case TVAFE_ADC_CAL_STEP_STAGE + TVAFE_ADC_CAL_STEP_CLAMP*1:
            if (!mutex)
            {
                mutex   =  1;
                step_up = 32;
                step_dn = 32;
            }
        case TVAFE_ADC_CAL_STEP_STAGE + TVAFE_ADC_CAL_STEP_CLAMP*2:
            if (!mutex)
            {
                mutex   =  1;
                step_up = 16;
                step_dn = 16;
            }
        case TVAFE_ADC_CAL_STEP_STAGE + TVAFE_ADC_CAL_STEP_CLAMP*3:
            if (!mutex)
            {
                mutex   = 1;
                step_up = 8;
                step_dn = 8;
            }
        case TVAFE_ADC_CAL_STEP_STAGE + TVAFE_ADC_CAL_STEP_CLAMP*4:
            if (!mutex)
            {
                mutex   = 1;
                step_up = 4;
                step_dn = 4;
            }
        case TVAFE_ADC_CAL_STEP_STAGE + TVAFE_ADC_CAL_STEP_CLAMP*5:
            if (!mutex)
            {
                mutex   = 1;
                step_up = 2;
                step_dn = 2;
            }
        case TVAFE_ADC_CAL_STEP_STAGE + TVAFE_ADC_CAL_STEP_CLAMP*6:
            if (!mutex)
            {
                mutex   = 1;
                step_up = 1;
                step_dn = 1;
            }
        case TVAFE_ADC_CAL_STEP_STAGE + TVAFE_ADC_CAL_STEP_CLAMP*7:
            if (!mutex)
            {
                mutex   = 1;
                step_up = 0;
                step_dn = 1;
            }
            #ifdef LOG_ADC_CAL
                pr_info("\nA_cl = %4d %4d %4d\n", (int)adc_cal->a_analog_clamp, (int)adc_cal->b_analog_clamp, (int)adc_cal->c_analog_clamp);
            #endif
            // get data of blank
            tvafe_adc_cal_read("Dark", &a, &b, &c);
            // tune analog clamp
            if (a > z_lu)
                adc_cal->a_analog_clamp -= step_dn;
            else
                adc_cal->a_analog_clamp += step_up;
            if (b > z_ch)
                adc_cal->b_analog_clamp -= step_dn;
            else
                adc_cal->b_analog_clamp += step_up;
            if (c > z_ch)
                adc_cal->c_analog_clamp -= step_dn;
            else
                adc_cal->c_analog_clamp += step_up;
            // load adc cal values
            adc_cal->reserved |= TVAFE_ADC_CAL_VALID;
            tvafe_set_cal_value(adc_cal);
            adc_cal->reserved &= ~TVAFE_ADC_CAL_VALID;
            if (operand->step == TVAFE_ADC_CAL_STEP_STAGE + TVAFE_ADC_CAL_STEP_CLAMP*7 + 1)
            {
            	#ifdef LOG_ADC_CAL
                    pr_info("\nfresh_adc_cal after clamp\n");
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
                #endif
                #ifdef TVAFE_ADC_CAL_VALIDATION
                // adjust adc cal values
                    tvafe_adjust_cal_value(adc_cal,is_component);
                // load adc cal values
                    adc_cal->reserved |= TVAFE_ADC_CAL_VALID;
                    tvafe_set_cal_value(adc_cal);
                    adc_cal->reserved &= ~TVAFE_ADC_CAL_VALID;
                #endif
                #ifdef LOG_ADC_CAL
                    pr_info("\nfinal_adc_cal after validation\n");
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
                #endif
                // restore bpg_h, bpg_v, bpg_m, clamp_inv, clamp_ext, clk_ext, clk_ctl, lpf_a, lpf_b, lpf_c
                WRITE_APB_REG(TVFE_BPG_BACKP_H, operand->bpg_h);
                WRITE_APB_REG(TVFE_BPG_BACKP_V, operand->bpg_v);
                WRITE_APB_REG_BITS(TVFE_TOP_CTRL, operand->bpg_m, TVFE_BACKP_GATE_MUX_BIT, TVFE_BACKP_GATE_MUX_WID);
   //             WRITE_APB_REG_BITS(TVFE_DVSS_MUXCTRL, operand->clamp_inv, DVSS_CLAMP_INV_BIT,      DVSS_CLAMP_INV_WID     );
                WRITE_APB_REG_BITS(ADC_REG_2F, operand->clamp_ext, CLAMPEXT_BIT,            CLAMPEXT_WID           );
               // WRITE_APB_REG_BITS(ADC_REG_19, operand->lpf_a, ENLPFA_BIT,    ENLPFA_WID   );
               // WRITE_APB_REG_BITS(ADC_REG_1A, operand->lpf_b, ENLPFB_BIT,    ENLPFB_WID   );
               // WRITE_APB_REG_BITS(ADC_REG_1B, operand->lpf_c, ENLPFC_BIT,    ENLPFC_WID   );
                WRITE_APB_REG_BITS(ADC_REG_58, operand->clk_ext, EXTCLKSEL_BIT, EXTCLKSEL_WID);
                WRITE_CBUS_REG(HHI_VAFE_CLKIN_CNTL, operand->clk_ctl);
                // validate result
                // adc_cal->reserved |= TVAFE_ADC_CAL_VALID;
                // end of cal
                operand->step = 0;
            }
            break;
        default:
            break;
    }

    if (operand->step)
        return (true);
    else
        return (false);
}

void tvafe_adc_clamp_adjust(struct tvafe_info_s *info)
{
    struct tvafe_operand_s *operand = &info->operand;
    struct tvafe_adc_cal_s *adc_cal = &info->adc_cal_val;
    unsigned int data = READ_APB_REG(TVFE_ADC_READBACK_INDICATOR);
    unsigned int tgt_y = 0, tgt_c = 0;
    bool cond = true;

    // do adjust after TVIN_IOC_S_AFE_ADC_CAL is executed
    if (!(adc_cal->reserved & TVAFE_ADC_CAL_VALID))
        return;

    // skip over the 1st vsync after TVIN_IOC_S_AFE_ADC_CAL is executed
    operand->cnt++;
    if (operand->cnt <= 1)
        return;

    if ((!adc_cal->a_analog_clamp) ||
        (!adc_cal->b_analog_clamp) ||
        (!adc_cal->c_analog_clamp) ||
        (adc_cal->a_analog_clamp == 127) ||
        (adc_cal->b_analog_clamp == 127) ||
        (adc_cal->c_analog_clamp == 127)
       )
        return;

    if ((info->param.port >= TVIN_PORT_COMP0) &&
        (info->param.port <= TVIN_PORT_COMP7)
       )
    {
        tgt_y = 37;
        tgt_c = 512;
    }
    else if ((info->param.port >= TVIN_PORT_VGA0) &&
             (info->param.port <= TVIN_PORT_VGA7)
            )
    {
        tgt_y = 0;
        tgt_c = 0;
    }
    else
        return;

    operand->adc0 += (data >> 20) & 0x000003ff;
    operand->adc1 += (data >> 10) & 0x000003ff;
    operand->adc2 += (data >>  0) & 0x000003ff;
    if (operand->cnt == 5)
    {
        operand->adj  = 0;
        operand->adc0  = (operand->adc0 + 2) >> 2;
        operand->adc1  = (operand->adc1 + 2) >> 2;
        operand->adc2  = (operand->adc2 + 2) >> 2;
        if (operand->adc0 > tgt_y)
        {
            cond = (!operand->dir) || (operand->dir0);
            if ((adc_cal->a_analog_clamp) && cond)
            {
                adc_cal->a_analog_clamp--;
                operand->dir   = 1;
                operand->dir0  = 1;
                operand->adj  = 1;
            }
        }
        else
        {
            cond = (!operand->dir) || (!operand->dir0);
            if ((adc_cal->a_analog_clamp < 127) && cond)
            {
                adc_cal->a_analog_clamp++;
                operand->dir   = 1;
                operand->adj = 1;
            }
        }
        if (operand->adc1 > tgt_c)
        {
            cond = (!operand->dir) || (operand->dir1);
            if ((adc_cal->b_analog_clamp) && cond)
            {
                adc_cal->b_analog_clamp--;
                operand->dir   = 1;
                operand->dir1  = 1;
                operand->adj  = 1;
            }
        }
        else
        {
            cond = (!operand->dir) || (!operand->dir1);
            if ((adc_cal->b_analog_clamp < 127) && cond)
            {
                adc_cal->b_analog_clamp++;
                operand->dir   = 1;
                operand->adj = 1;
            }
        }
        if (operand->adc2 > tgt_c)
        {
            cond = (!operand->dir) || (operand->dir2);
            if ((adc_cal->c_analog_clamp) && cond)
            {
                adc_cal->c_analog_clamp--;
                operand->dir   = 1;
                operand->dir0  = 1;
                operand->adj  = 1;
            }
        }
        else
        {
            cond = (!operand->dir) || (!operand->dir2);
            if ((adc_cal->c_analog_clamp < 127) && cond)
            {
                adc_cal->c_analog_clamp++;
                operand->dir   = 1;
                operand->adj = 1;
            }
        }
        if (operand->adj)
        {
            tvafe_set_cal_value(adc_cal);
            operand->data0 = operand->adc0;
            operand->data1 = operand->adc1;
            operand->data2 = operand->adc2;
        }
        operand->cnt  = 1;
        operand->adc0 = 0;
        operand->adc1 = 0;
        operand->adc2 = 0;
    }
}

// *****************************************************************************
// Function: fetch WSS data
//
//   Params: system info
//
//   Return: none
//
// *****************************************************************************
void tvafe_get_wss_data(struct tvafe_comp_wss_s *para)
{
    para->wss1[0] = READ_APB_REG(TVFE_MISC_WSS1_INDICATOR1);
    para->wss1[1] = READ_APB_REG(TVFE_MISC_WSS1_INDICATOR2);
    para->wss1[2] = READ_APB_REG(TVFE_MISC_WSS1_INDICATOR3);
    para->wss1[3] = READ_APB_REG(TVFE_MISC_WSS1_INDICATOR4);
    para->wss1[4] = READ_APB_REG_BITS(TVFE_MISC_WSS1_INDICATOR5, WSS1_DATA_143_128_BIT, WSS1_DATA_143_128_WID);
    para->wss2[0] = READ_APB_REG(TVFE_MISC_WSS2_INDICATOR1);
    para->wss2[1] = READ_APB_REG(TVFE_MISC_WSS2_INDICATOR2);
    para->wss2[2] = READ_APB_REG(TVFE_MISC_WSS2_INDICATOR3);
    para->wss2[3] = READ_APB_REG(TVFE_MISC_WSS2_INDICATOR4);
    para->wss2[4] = READ_APB_REG_BITS(TVFE_MISC_WSS2_INDICATOR5, WSS2_DATA_143_128_BIT, WSS2_DATA_143_128_WID);
}

void tvafe_top_set_de(enum tvin_sig_fmt_e fmt)
{
    unsigned int hs = 0, he = 0, vs = 0, ve = 0;

    hs = tvin_fmt_tbl[fmt].hs_width + tvin_fmt_tbl[fmt].hs_bp;

    /* fix Comp de bug*/
    if (fmt < TVIN_SIG_FMT_COMP_MAX && fmt > TVIN_SIG_FMT_VGA_MAX)
    hs = (unsigned int)((signed int)hs + (signed int)tvafe_comp_hs_patch[fmt- TVIN_SIG_FMT_VGA_MAX - 1]);

    vs = tvin_fmt_tbl[fmt].vs_width + tvin_fmt_tbl[fmt].vs_bp;
    he = hs + tvin_fmt_tbl[fmt].h_active - 1;
    ve = vs + tvin_fmt_tbl[fmt].v_active - 1;


    WRITE_APB_REG_BITS(TVFE_DEG_H, hs,
                       DEG_HSTART_BIT, DEG_HSTART_WID);
    WRITE_APB_REG_BITS(TVFE_DEG_H, he,
                       DEG_HEND_BIT, DEG_HEND_WID);
    WRITE_APB_REG_BITS(TVFE_DEG_VODD, vs,
                       DEG_VSTART_ODD_BIT, DEG_VSTART_ODD_WID);
    WRITE_APB_REG_BITS(TVFE_DEG_VODD, ve,
                       DEG_VEND_ODD_BIT, DEG_VEND_ODD_WID);
    WRITE_APB_REG_BITS(TVFE_DEG_VEVEN, vs,
                       DEG_VSTART_EVEN_BIT, DEG_VSTART_EVEN_WID);
    WRITE_APB_REG_BITS(TVFE_DEG_VEVEN, ve,
                       DEG_VEND_EVEN_BIT, DEG_VEND_EVEN_WID);

    //fix 1080i60 field gen bug
    if (fmt == TVIN_SIG_FMT_COMP_1080I_60D000)
        WRITE_APB_REG_BITS(TVFE_SYNCTOP_SFG_MUXCTRL1, 0,
                       SFG_FLD_MANUAL_INV_BIT, SFG_FLD_MANUAL_INV_WID);
    else
        WRITE_APB_REG_BITS(TVFE_SYNCTOP_SFG_MUXCTRL1, 1,
                       SFG_FLD_MANUAL_INV_BIT, SFG_FLD_MANUAL_INV_WID);
}

void tvafe_top_config(enum tvin_sig_fmt_e fmt)
{
    //tvafe_top_set_aafilter_control(fmt);
    //tvafe_top_set_bp_gate(fmt);
    //tvafe_top_set_mvdet_control(fmt);
    tvafe_top_set_sfg_mux_control(fmt);
    tvafe_top_set_wss_control(fmt);
    tvafe_top_set_de(fmt);
}

void tvafe_reset_module(void)
{
    WRITE_APB_REG_BITS(TVFE_RST_CTRL, 1, ALL_CLK_RST_BIT, ALL_CLK_RST_WID);
    WRITE_APB_REG_BITS(TVFE_RST_CTRL, 0, ALL_CLK_RST_BIT, ALL_CLK_RST_WID);
}

void tvafe_enable_module(bool enable)
{
    // enable

    //main clk up
    WRITE_CBUS_REG(HHI_VAFE_CLKXTALIN_CNTL, 0x100);
    WRITE_CBUS_REG(HHI_VAFE_CLKOSCIN_CNTL, 0x100);
    WRITE_CBUS_REG(HHI_VAFE_CLKIN_CNTL, 0x100);
    WRITE_CBUS_REG(HHI_VAFE_CLKPI_CNTL, 0x100);
    WRITE_CBUS_REG(HHI_TVFE_AUTOMODE_CLK_CNTL, 0x100);

    //tvfe power up
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, COMP_CLK_ENABLE_BIT, COMP_CLK_ENABLE_WID);
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, EDID_CLK_EN_BIT, EDID_CLK_EN_WID);
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, DCLK_ENABLE_BIT, DCLK_ENABLE_WID);
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, VAFE_MCLK_EN_BIT, VAFE_MCLK_EN_WID);
	WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 3, TVFE_ADC_CLK_DIV_BIT, TVFE_ADC_CLK_DIV_WID);
    //adc power up
    WRITE_APB_REG_BITS(ADC_REG_21, 1, FULLPDZ_BIT, FULLPDZ_WID);

    /*reset module*/
    tvafe_reset_module();

    // disable
    if (!enable)
    {
        //adc power down
        WRITE_APB_REG_BITS(ADC_REG_21, 0, FULLPDZ_BIT, FULLPDZ_WID);

        //tvfe power down
        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, COMP_CLK_ENABLE_BIT, COMP_CLK_ENABLE_WID);
        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, EDID_CLK_EN_BIT, EDID_CLK_EN_WID);
        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, DCLK_ENABLE_BIT, DCLK_ENABLE_WID);
        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, VAFE_MCLK_EN_BIT, VAFE_MCLK_EN_WID);
        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, TVFE_ADC_CLK_DIV_BIT, TVFE_ADC_CLK_DIV_WID);

        //main clk down
        WRITE_CBUS_REG(HHI_VAFE_CLKXTALIN_CNTL, 0);
        WRITE_CBUS_REG(HHI_VAFE_CLKOSCIN_CNTL, 0);
        WRITE_CBUS_REG(HHI_VAFE_CLKIN_CNTL, 0);
        WRITE_CBUS_REG(HHI_VAFE_CLKPI_CNTL, 0);
        WRITE_CBUS_REG(HHI_TVFE_AUTOMODE_CLK_CNTL, 0);
    }
}

