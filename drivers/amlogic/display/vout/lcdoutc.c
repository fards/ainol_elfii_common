/*
 * AMLOGIC TCON controller driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:  Tim Yao <timyao@amlogic.com>
 *
 */
#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/vout/lcdoutc.h>
#include <linux/vout/vinfo.h>
#include <linux/vout/vout_notify.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/logo/logo.h>
#include <mach/am_regs.h>
#include <mach/mlvds_regs.h>
#include <mach/clock.h>
#include <asm/fiq.h>
#include <linux/delay.h>
#include <plat/regops.h>
#include <mach/reg_addr.h>
extern unsigned int clk_util_clk_msr(unsigned int clk_mux);

//M6 PLL control value 
#define M6_PLL_CNTL_CST2 (0x814d3928)
#define M6_PLL_CNTL_CST3 (0x6b425012)
#define M6_PLL_CNTL_CST4 (0x110)

//VID PLL
#define M6_VID_PLL_CNTL_2 (M6_PLL_CNTL_CST2)
#define M6_VID_PLL_CNTL_3 (M6_PLL_CNTL_CST3)
#define M6_VID_PLL_CNTL_4 (M6_PLL_CNTL_CST4)

#define FIQ_VSYNC

#define BL_MAX_LEVEL 0x100
#define PANEL_NAME    "panel"

#define PRINT_DEBUG_INFO
#ifdef PRINT_DEBUG_INFO
#define PRINT_INFO(...)        printk(__VA_ARGS__)
#else
#define PRINT_INFO(...)    
#endif

#ifdef CONFIG_AM_TV_OUTPUT2
static unsigned int vpp2_sel = 0; /*0,vpp; 1, vpp2 */
#endif

typedef struct {
    Lcd_Config_t conf;
    vinfo_t lcd_info;
} lcd_dev_t;

static unsigned long ddr_pll_clk = 0;

static lcd_dev_t *pDev = NULL;

static void _lcd_init(Lcd_Config_t *pConf) ;

static void set_lcd_gamma_table_ttl(u16 *data, u32 rgb_mask)
{
    int i;

    while (!(aml_read_reg32(P_GAMMA_CNTL_PORT) & (0x1 << LCD_ADR_RDY)));
    aml_write_reg32(P_GAMMA_ADDR_PORT, (0x1 << LCD_H_AUTO_INC) |
                                    (0x1 << rgb_mask)   |
                                    (0x0 << LCD_HADR));
    for (i=0;i<256;i++) {
        while (!( aml_read_reg32(P_GAMMA_CNTL_PORT) & (0x1 << LCD_WR_RDY) )) ;
        aml_write_reg32(P_GAMMA_DATA_PORT, data[i]);
    }
    while (!(aml_read_reg32(P_GAMMA_CNTL_PORT) & (0x1 << LCD_ADR_RDY)));
    aml_write_reg32(P_GAMMA_ADDR_PORT, (0x1 << LCD_H_AUTO_INC) |
                                    (0x1 << rgb_mask)   |
                                    (0x23 << LCD_HADR));
}

static void set_lcd_gamma_table_lvds(u16 *data, u32 rgb_mask)
{
    int i;

    while (!(aml_read_reg32(P_L_GAMMA_CNTL_PORT) & (0x1 << LCD_ADR_RDY)));
    aml_write_reg32(P_L_GAMMA_ADDR_PORT, (0x1 << LCD_H_AUTO_INC) |
                                    (0x1 << rgb_mask)   |
                                    (0x0 << LCD_HADR));
    for (i=0;i<256;i++) {
        while (!( aml_read_reg32(P_L_GAMMA_CNTL_PORT) & (0x1 << LCD_WR_RDY) )) ;
        aml_write_reg32(P_L_GAMMA_DATA_PORT, data[i]);
    }
    while (!(aml_read_reg32(P_L_GAMMA_CNTL_PORT) & (0x1 << LCD_ADR_RDY)));
    aml_write_reg32(P_L_GAMMA_ADDR_PORT, (0x1 << LCD_H_AUTO_INC) |
                                    (0x1 << rgb_mask)   |
                                    (0x23 << LCD_HADR));
}

static void write_tcon_double(Mlvds_Tcon_Config_t *mlvds_tcon)
{
    unsigned int tmp;
    int channel_num = mlvds_tcon->channel_num;
    int hv_sel = mlvds_tcon->hv_sel;
    int hstart_1 = mlvds_tcon->tcon_1st_hs_addr;
    int hend_1 = mlvds_tcon->tcon_1st_he_addr;
    int vstart_1 = mlvds_tcon->tcon_1st_vs_addr;
    int vend_1 = mlvds_tcon->tcon_1st_ve_addr;
    int hstart_2 = mlvds_tcon->tcon_2nd_hs_addr;
    int hend_2 = mlvds_tcon->tcon_2nd_he_addr;
    int vstart_2 = mlvds_tcon->tcon_2nd_vs_addr;
    int vend_2 = mlvds_tcon->tcon_2nd_ve_addr;

    tmp = aml_read_reg32(P_L_TCON_MISC_SEL_ADDR);
    switch(channel_num)
    {
        case 0 :
            aml_write_reg32(P_MTCON0_1ST_HS_ADDR, hstart_1);
            aml_write_reg32(P_MTCON0_1ST_HE_ADDR, hend_1);
            aml_write_reg32(P_MTCON0_1ST_VS_ADDR, vstart_1);
            aml_write_reg32(P_MTCON0_1ST_VE_ADDR, vend_1);
            aml_write_reg32(P_MTCON0_2ND_HS_ADDR, hstart_2);
            aml_write_reg32(P_MTCON0_2ND_HE_ADDR, hend_2);
            aml_write_reg32(P_MTCON0_2ND_VS_ADDR, vstart_2);
            aml_write_reg32(P_MTCON0_2ND_VE_ADDR, vend_2);
            tmp &= (~(1 << LCD_STH1_SEL)) | (hv_sel << LCD_STH1_SEL);
            aml_write_reg32(P_L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 1 :
            aml_write_reg32(P_MTCON1_1ST_HS_ADDR, hstart_1);
            aml_write_reg32(P_MTCON1_1ST_HE_ADDR, hend_1);
            aml_write_reg32(P_MTCON1_1ST_VS_ADDR, vstart_1);
            aml_write_reg32(P_MTCON1_1ST_VE_ADDR, vend_1);
            aml_write_reg32(P_MTCON1_2ND_HS_ADDR, hstart_2);
            aml_write_reg32(P_MTCON1_2ND_HE_ADDR, hend_2);
            aml_write_reg32(P_MTCON1_2ND_VS_ADDR, vstart_2);
            aml_write_reg32(P_MTCON1_2ND_VE_ADDR, vend_2);
            tmp &= (~(1 << LCD_CPV1_SEL)) | (hv_sel << LCD_CPV1_SEL);
            aml_write_reg32(P_L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 2 :
            aml_write_reg32(P_MTCON2_1ST_HS_ADDR, hstart_1);
            aml_write_reg32(P_MTCON2_1ST_HE_ADDR, hend_1);
            aml_write_reg32(P_MTCON2_1ST_VS_ADDR, vstart_1);
            aml_write_reg32(P_MTCON2_1ST_VE_ADDR, vend_1);
            aml_write_reg32(P_MTCON2_2ND_HS_ADDR, hstart_2);
            aml_write_reg32(P_MTCON2_2ND_HE_ADDR, hend_2);
            aml_write_reg32(P_MTCON2_2ND_VS_ADDR, vstart_2);
            aml_write_reg32(P_MTCON2_2ND_VE_ADDR, vend_2);
            tmp &= (~(1 << LCD_STV1_SEL)) | (hv_sel << LCD_STV1_SEL);
            aml_write_reg32(P_L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 3 :
            aml_write_reg32(P_MTCON3_1ST_HS_ADDR, hstart_1);
            aml_write_reg32(P_MTCON3_1ST_HE_ADDR, hend_1);
            aml_write_reg32(P_MTCON3_1ST_VS_ADDR, vstart_1);
            aml_write_reg32(P_MTCON3_1ST_VE_ADDR, vend_1);
            aml_write_reg32(P_MTCON3_2ND_HS_ADDR, hstart_2);
            aml_write_reg32(P_MTCON3_2ND_HE_ADDR, hend_2);
            aml_write_reg32(P_MTCON3_2ND_VS_ADDR, vstart_2);
            aml_write_reg32(P_MTCON3_2ND_VE_ADDR, vend_2);
            tmp &= (~(1 << LCD_OEV1_SEL)) | (hv_sel << LCD_OEV1_SEL);
            aml_write_reg32(P_L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 4 :
            aml_write_reg32(P_MTCON4_1ST_HS_ADDR, hstart_1);
            aml_write_reg32(P_MTCON4_1ST_HE_ADDR, hend_1);
            aml_write_reg32(P_MTCON4_1ST_VS_ADDR, vstart_1);
            aml_write_reg32(P_MTCON4_1ST_VE_ADDR, vend_1);
            tmp &= (~(1 << LCD_STH2_SEL)) | (hv_sel << LCD_STH2_SEL);
            aml_write_reg32(P_L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 5 :
            aml_write_reg32(P_MTCON5_1ST_HS_ADDR, hstart_1);
            aml_write_reg32(P_MTCON5_1ST_HE_ADDR, hend_1);
            aml_write_reg32(P_MTCON5_1ST_VS_ADDR, vstart_1);
            aml_write_reg32(P_MTCON5_1ST_VE_ADDR, vend_1);
            tmp &= (~(1 << LCD_CPV2_SEL)) | (hv_sel << LCD_CPV2_SEL);
            aml_write_reg32(P_L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 6 :
            aml_write_reg32(P_MTCON6_1ST_HS_ADDR, hstart_1);
            aml_write_reg32(P_MTCON6_1ST_HE_ADDR, hend_1);
            aml_write_reg32(P_MTCON6_1ST_VS_ADDR, vstart_1);
            aml_write_reg32(P_MTCON6_1ST_VE_ADDR, vend_1);
            tmp &= (~(1 << LCD_OEH_SEL)) | (hv_sel << LCD_OEH_SEL);
            aml_write_reg32(P_L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 7 :
            aml_write_reg32(P_MTCON7_1ST_HS_ADDR, hstart_1);
            aml_write_reg32(P_MTCON7_1ST_HE_ADDR, hend_1);
            aml_write_reg32(P_MTCON7_1ST_VS_ADDR, vstart_1);
            aml_write_reg32(P_MTCON7_1ST_VE_ADDR, vend_1);
            tmp &= (~(1 << LCD_OEV3_SEL)) | (hv_sel << LCD_OEV3_SEL);
            aml_write_reg32(P_L_TCON_MISC_SEL_ADDR, tmp);
            break;
        default:
            break;
    }
}

static void set_tcon_ttl(Lcd_Config_t *pConf)
{
    Lcd_Timing_t *tcon_adr = &(pConf->lcd_timing);
    
    set_lcd_gamma_table_ttl(pConf->lcd_effect.GammaTableR, LCD_H_SEL_R);
    set_lcd_gamma_table_ttl(pConf->lcd_effect.GammaTableG, LCD_H_SEL_G);
    set_lcd_gamma_table_ttl(pConf->lcd_effect.GammaTableB, LCD_H_SEL_B);

    aml_write_reg32(P_GAMMA_CNTL_PORT, pConf->lcd_effect.gamma_cntl_port);
    aml_write_reg32(P_GAMMA_VCOM_HSWITCH_ADDR, pConf->lcd_effect.gamma_vcom_hswitch_addr);

    aml_write_reg32(P_RGB_BASE_ADDR,   pConf->lcd_effect.rgb_base_addr);
    aml_write_reg32(P_RGB_COEFF_ADDR,  pConf->lcd_effect.rgb_coeff_addr);
    aml_write_reg32(P_POL_CNTL_ADDR,   pConf->lcd_timing.pol_cntl_addr);
    if(pConf->lcd_basic.lcd_bits == 8)
		aml_write_reg32(P_DITH_CNTL_ADDR,  0x400);
	else
		aml_write_reg32(P_DITH_CNTL_ADDR,  0x600);

    aml_write_reg32(P_STH1_HS_ADDR,    tcon_adr->sth1_hs_addr);
    aml_write_reg32(P_STH1_HE_ADDR,    tcon_adr->sth1_he_addr);
    aml_write_reg32(P_STH1_VS_ADDR,    tcon_adr->sth1_vs_addr);
    aml_write_reg32(P_STH1_VE_ADDR,    tcon_adr->sth1_ve_addr);

    aml_write_reg32(P_OEH_HS_ADDR,     tcon_adr->oeh_hs_addr);
    aml_write_reg32(P_OEH_HE_ADDR,     tcon_adr->oeh_he_addr);
    aml_write_reg32(P_OEH_VS_ADDR,     tcon_adr->oeh_vs_addr);
    aml_write_reg32(P_OEH_VE_ADDR,     tcon_adr->oeh_ve_addr);

    aml_write_reg32(P_VCOM_HSWITCH_ADDR, tcon_adr->vcom_hswitch_addr);
    aml_write_reg32(P_VCOM_VS_ADDR,    tcon_adr->vcom_vs_addr);
    aml_write_reg32(P_VCOM_VE_ADDR,    tcon_adr->vcom_ve_addr);

    aml_write_reg32(P_CPV1_HS_ADDR,    tcon_adr->cpv1_hs_addr);
    aml_write_reg32(P_CPV1_HE_ADDR,    tcon_adr->cpv1_he_addr);
    aml_write_reg32(P_CPV1_VS_ADDR,    tcon_adr->cpv1_vs_addr);
    aml_write_reg32(P_CPV1_VE_ADDR,    tcon_adr->cpv1_ve_addr);

    aml_write_reg32(P_STV1_HS_ADDR,    tcon_adr->stv1_hs_addr);
    aml_write_reg32(P_STV1_HE_ADDR,    tcon_adr->stv1_he_addr);
    aml_write_reg32(P_STV1_VS_ADDR,    tcon_adr->stv1_vs_addr);
    aml_write_reg32(P_STV1_VE_ADDR,    tcon_adr->stv1_ve_addr);

    aml_write_reg32(P_OEV1_HS_ADDR,    tcon_adr->oev1_hs_addr);
    aml_write_reg32(P_OEV1_HE_ADDR,    tcon_adr->oev1_he_addr);
    aml_write_reg32(P_OEV1_VS_ADDR,    tcon_adr->oev1_vs_addr);
    aml_write_reg32(P_OEV1_VE_ADDR,    tcon_adr->oev1_ve_addr);

    aml_write_reg32(P_INV_CNT_ADDR,    tcon_adr->inv_cnt_addr);
    aml_write_reg32(P_TCON_MISC_SEL_ADDR,     tcon_adr->tcon_misc_sel_addr);
    aml_write_reg32(P_DUAL_PORT_CNTL_ADDR, tcon_adr->dual_port_cntl_addr);

#ifdef CONFIG_AM_TV_OUTPUT2
    if(vpp2_sel){
        aml_write_reg32(P_VPP2_MISC, aml_read_reg32(P_VPP2_MISC) & ~(VPP_OUT_SATURATE));
    }
    else{
        aml_write_reg32(P_VPP_MISC, aml_read_reg32(P_VPP_MISC) & ~(VPP_OUT_SATURATE));
    }
#else
    aml_write_reg32(P_VPP_MISC, aml_read_reg32(P_VPP_MISC) & ~(VPP_OUT_SATURATE));
#endif    
}

static void set_tcon_lvds(Lcd_Config_t *pConf)
{
    Lcd_Timing_t *tcon_adr = &(pConf->lcd_timing);
    
    set_lcd_gamma_table_lvds(pConf->lcd_effect.GammaTableR, LCD_H_SEL_R);
    set_lcd_gamma_table_lvds(pConf->lcd_effect.GammaTableG, LCD_H_SEL_G);
    set_lcd_gamma_table_lvds(pConf->lcd_effect.GammaTableB, LCD_H_SEL_B);

    aml_write_reg32(P_L_GAMMA_CNTL_PORT, pConf->lcd_effect.gamma_cntl_port);
    aml_write_reg32(P_L_GAMMA_VCOM_HSWITCH_ADDR, pConf->lcd_effect.gamma_vcom_hswitch_addr);

    aml_write_reg32(P_L_RGB_BASE_ADDR,   pConf->lcd_effect.rgb_base_addr);
    aml_write_reg32(P_L_RGB_COEFF_ADDR,  pConf->lcd_effect.rgb_coeff_addr);
    aml_write_reg32(P_L_POL_CNTL_ADDR,   pConf->lcd_timing.pol_cntl_addr);    
	if(pConf->lcd_basic.lcd_bits == 8)
		aml_write_reg32(P_L_DITH_CNTL_ADDR,  0x400);
	else
		aml_write_reg32(P_L_DITH_CNTL_ADDR,  0x600);

    aml_write_reg32(P_L_INV_CNT_ADDR,    tcon_adr->inv_cnt_addr);
    aml_write_reg32(P_L_TCON_MISC_SEL_ADDR,     tcon_adr->tcon_misc_sel_addr);
    aml_write_reg32(P_L_DUAL_PORT_CNTL_ADDR, tcon_adr->dual_port_cntl_addr);

#ifdef CONFIG_AM_TV_OUTPUT2
    if(vpp2_sel){
        aml_write_reg32(P_VPP2_MISC, aml_read_reg32(P_VPP2_MISC) & ~(VPP_OUT_SATURATE));
    }
    else{
    aml_write_reg32(P_VPP_MISC, aml_read_reg32(P_VPP_MISC) & ~(VPP_OUT_SATURATE));
}
#else
    aml_write_reg32(P_VPP_MISC, aml_read_reg32(P_VPP_MISC) & ~(VPP_OUT_SATURATE));
#endif
}

// Set the mlvds TCON
// this function should support dual gate or singal gate TCON setting.
// singal gate TCON, Scan Function TO DO.
// scan_function   // 0 - Z1, 1 - Z2, 2- Gong
static void set_tcon_mlvds(Lcd_Config_t *pConf)
{
    Mlvds_Tcon_Config_t *mlvds_tconfig_l = pConf->lvds_mlvds_config.mlvds_tcon_config;
    int dual_gate = pConf->lvds_mlvds_config.mlvds_config->test_dual_gate;
    int bit_num = pConf->lcd_basic.lcd_bits;
    int pair_num = pConf->lvds_mlvds_config.mlvds_config->test_pair_num;

    unsigned int data32;

    int pclk_div;
    int ext_pixel = dual_gate ? pConf->lvds_mlvds_config.mlvds_config->total_line_clk : 0;
    int dual_wr_rd_start;
    int i = 0;

//    PRINT_INFO(" Notice: Setting VENC_DVI_SETTING[0x%4x] and GAMMA_CNTL_PORT[0x%4x].LCD_GAMMA_EN as 0 temporary\n", VENC_DVI_SETTING, GAMMA_CNTL_PORT);
//    PRINT_INFO(" Otherwise, the panel will display color abnormal.\n");
//    aml_write_reg32(P_VENC_DVI_SETTING, 0);

    set_lcd_gamma_table_lvds(pConf->lcd_effect.GammaTableR, LCD_H_SEL_R);
    set_lcd_gamma_table_lvds(pConf->lcd_effect.GammaTableG, LCD_H_SEL_G);
    set_lcd_gamma_table_lvds(pConf->lcd_effect.GammaTableB, LCD_H_SEL_B);

    aml_write_reg32(P_L_GAMMA_CNTL_PORT, pConf->lcd_effect.gamma_cntl_port);
    aml_write_reg32(P_L_GAMMA_VCOM_HSWITCH_ADDR, pConf->lcd_effect.gamma_vcom_hswitch_addr);

    aml_write_reg32(P_L_RGB_BASE_ADDR, pConf->lcd_effect.rgb_base_addr);
    aml_write_reg32(P_L_RGB_COEFF_ADDR, pConf->lcd_effect.rgb_coeff_addr);
    //aml_write_reg32(P_L_POL_CNTL_ADDR, pConf->pol_cntl_addr);
	if(pConf->lcd_basic.lcd_bits == 8)
		aml_write_reg32(P_L_DITH_CNTL_ADDR,  0x400);
	else
		aml_write_reg32(P_L_DITH_CNTL_ADDR,  0x600);

//    aml_write_reg32(P_L_INV_CNT_ADDR, pConf->inv_cnt_addr);
//    aml_write_reg32(P_L_TCON_MISC_SEL_ADDR, pConf->tcon_misc_sel_addr);
//    aml_write_reg32(P_L_DUAL_PORT_CNTL_ADDR, pConf->dual_port_cntl_addr);
//
/*
#ifdef CONFIG_AM_TV_OUTPUT2
    if(vpp2_sel){
        CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_OUT_SATURATE);
    }
    else{
        CLEAR_MPEG_REG_MASK(VPP_MISC, VPP_OUT_SATURATE);
    }
#else
    CLEAR_MPEG_REG_MASK(VPP_MISC, VPP_OUT_SATURATE);
#endif
*/
    data32 = (0x9867 << tcon_pattern_loop_data) |
             (1 << tcon_pattern_loop_start) |
             (4 << tcon_pattern_loop_end) |
             (1 << ((mlvds_tconfig_l[6].channel_num)+tcon_pattern_enable)); // POL_CHANNEL use pattern generate

    aml_write_reg32(P_L_TCON_PATTERN_HI,  (data32 >> 16));
    aml_write_reg32(P_L_TCON_PATTERN_LO, (data32 & 0xffff));

    pclk_div = (bit_num == 8) ? 3 : // phy_clk / 8
                                2 ; // phy_clk / 6
   data32 = (1 << ((mlvds_tconfig_l[7].channel_num)-2+tcon_pclk_enable)) |  // enable PCLK_CHANNEL
            (pclk_div << tcon_pclk_div) |
            (
              (pair_num == 6) ?
              (
              ((bit_num == 8) & dual_gate) ?
              (
                (0 << (tcon_delay + 0*3)) |
                (0 << (tcon_delay + 1*3)) |
                (0 << (tcon_delay + 2*3)) |
                (0 << (tcon_delay + 3*3)) |
                (0 << (tcon_delay + 4*3)) |
                (0 << (tcon_delay + 5*3)) |
                (0 << (tcon_delay + 6*3)) |
                (0 << (tcon_delay + 7*3))
              ) :
              (
                (0 << (tcon_delay + 0*3)) |
                (0 << (tcon_delay + 1*3)) |
                (0 << (tcon_delay + 2*3)) |
                (0 << (tcon_delay + 3*3)) |
                (0 << (tcon_delay + 4*3)) |
                (0 << (tcon_delay + 5*3)) |
                (0 << (tcon_delay + 6*3)) |
                (0 << (tcon_delay + 7*3))
              )
              ) :
              (
              ((bit_num == 8) & dual_gate) ?
              (
                (0 << (tcon_delay + 0*3)) |
                (0 << (tcon_delay + 1*3)) |
                (0 << (tcon_delay + 2*3)) |
                (0 << (tcon_delay + 3*3)) |
                (0 << (tcon_delay + 4*3)) |
                (0 << (tcon_delay + 5*3)) |
                (0 << (tcon_delay + 6*3)) |
                (0 << (tcon_delay + 7*3))
              ) :
              (bit_num == 8) ?
              (
                (0 << (tcon_delay + 0*3)) |
                (0 << (tcon_delay + 1*3)) |
                (0 << (tcon_delay + 2*3)) |
                (0 << (tcon_delay + 3*3)) |
                (0 << (tcon_delay + 4*3)) |
                (0 << (tcon_delay + 5*3)) |
                (0 << (tcon_delay + 6*3)) |
                (0 << (tcon_delay + 7*3))
              ) :
              (
                (0 << (tcon_delay + 0*3)) |
                (0 << (tcon_delay + 1*3)) |
                (0 << (tcon_delay + 2*3)) |
                (0 << (tcon_delay + 3*3)) |
                (0 << (tcon_delay + 4*3)) |
                (0 << (tcon_delay + 5*3)) |
                (0 << (tcon_delay + 6*3)) |
                (0 << (tcon_delay + 7*3))
              )
              )
            );

    aml_write_reg32(P_TCON_CONTROL_HI,  (data32 >> 16));
    aml_write_reg32(P_TCON_CONTROL_LO, (data32 & 0xffff));


    aml_write_reg32(P_L_TCON_DOUBLE_CTL,
                   (1<<(mlvds_tconfig_l[3].channel_num))   // invert CPV
                  );

    // for channel 4-7, set second setting same as first
    aml_write_reg32(P_L_DE_HS_ADDR, (0xf << 12) | ext_pixel);   // 0xf -- enable double_tcon fir channel7:4
    aml_write_reg32(P_L_DE_HE_ADDR, (0xf << 12) | ext_pixel);   // 0xf -- enable double_tcon fir channel3:0
    aml_write_reg32(P_L_DE_VS_ADDR, 0);
    aml_write_reg32(P_L_DE_VE_ADDR, 0);

    dual_wr_rd_start = 0x5d;
    aml_write_reg32(P_MLVDS_DUAL_GATE_WR_START, dual_wr_rd_start);
    aml_write_reg32(P_MLVDS_DUAL_GATE_WR_END, dual_wr_rd_start + 1280);
    aml_write_reg32(P_MLVDS_DUAL_GATE_RD_START, dual_wr_rd_start + ext_pixel - 2);
    aml_write_reg32(P_MLVDS_DUAL_GATE_RD_END, dual_wr_rd_start + 1280 + ext_pixel - 2);

    aml_write_reg32(P_MLVDS_SECOND_RESET_CTL, (pConf->lvds_mlvds_config.mlvds_config->mlvds_insert_start + ext_pixel));

    data32 = (0 << ((mlvds_tconfig_l[5].channel_num)+mlvds_tcon_field_en)) |  // enable EVEN_F on TCON channel 6
             ( (0x0 << mlvds_scan_mode_odd) | (0x0 << mlvds_scan_mode_even)
             ) | (0 << mlvds_scan_mode_start_line);

    aml_write_reg32(P_MLVDS_DUAL_GATE_CTL_HI,  (data32 >> 16));
    aml_write_reg32(P_MLVDS_DUAL_GATE_CTL_LO, (data32 & 0xffff));

    PRINT_INFO("write minilvds tcon 0~7.\n");
    for(i = 0; i < 8; i++)
    {
		write_tcon_double(&mlvds_tconfig_l[i]);
    }
}

static void set_video_spread_spectrum(int video_ss_level)
{ 
    switch (video_ss_level)
	{
		case 0:  // disable ss
			aml_write_reg32(P_HHI_VID_PLL_CNTL2, 0x814d3928 );
			aml_write_reg32(P_HHI_VID_PLL_CNTL3, 0x6b425012 );
			aml_write_reg32(P_HHI_VID_PLL_CNTL4, 0x110 );
			break;
		case 1:  //about 1%
			aml_write_reg32(P_HHI_VIID_PLL_CNTL2, 0x16110696);
			aml_write_reg32(P_HHI_VIID_PLL_CNTL3, 0x4d625012);
			aml_write_reg32(P_HHI_VIID_PLL_CNTL4, 0x130);
			break;
		case 2:  //about 2%
			aml_write_reg32(P_HHI_VIID_PLL_CNTL2, 0x16110696);
			aml_write_reg32(P_HHI_VIID_PLL_CNTL3, 0x2d425012);
			aml_write_reg32(P_HHI_VIID_PLL_CNTL4, 0x130);
			break;
		case 3:  //about 3%
			aml_write_reg32(P_HHI_VIID_PLL_CNTL2, 0x16110696);
			aml_write_reg32(P_HHI_VIID_PLL_CNTL3, 0x1d425012);
			aml_write_reg32(P_HHI_VIID_PLL_CNTL4, 0x130);
			break;		
		case 4:  //about 4%
			aml_write_reg32(P_HHI_VIID_PLL_CNTL2, 0x16110696);
			aml_write_reg32(P_HHI_VIID_PLL_CNTL3, 0x0d125012);
			aml_write_reg32(P_HHI_VIID_PLL_CNTL4, 0x130);
			break;
		case 5:  //about 5%
			aml_write_reg32(P_HHI_VIID_PLL_CNTL2, 0x16110696);
			aml_write_reg32(P_HHI_VIID_PLL_CNTL3, 0x0e425012);
			aml_write_reg32(P_HHI_VIID_PLL_CNTL4, 0x130);
			break;	
		default:  //disable ss
			aml_write_reg32(P_HHI_VID_PLL_CNTL2, 0x814d3928);
			aml_write_reg32(P_HHI_VID_PLL_CNTL3, 0x6b425012);
			aml_write_reg32(P_HHI_VID_PLL_CNTL4, 0x110);		
	}	
	//PRINT_INFO("set video spread spectrum %d%%.\n", video_ss_level);	
}

static void vclk_set_lcd(int lcd_type, int ss_level, int pll_sel, int pll_div_sel, int vclk_sel, unsigned long pll_reg, unsigned long vid_div_reg, unsigned int xd)
{
    PRINT_INFO("setup lcd clk.\n");
    vid_div_reg |= (1 << 16) ; // turn clock gate on
    vid_div_reg |= (pll_sel << 15); // vid_div_clk_sel
    
   
    if(vclk_sel) {
      aml_write_reg32(P_HHI_VIID_CLK_CNTL, aml_read_reg32(P_HHI_VIID_CLK_CNTL) & ~(1 << 19) );     //disable clk_div0 
    }
    else {
      aml_write_reg32(P_HHI_VID_CLK_CNTL, aml_read_reg32(P_HHI_VID_CLK_CNTL) & ~(1 << 19) );     //disable clk_div0 
      aml_write_reg32(P_HHI_VID_CLK_CNTL, aml_read_reg32(P_HHI_VID_CLK_CNTL) & ~(1 << 20) );     //disable clk_div1 
    } 

    // delay 2uS to allow the sync mux to switch over
    //aml_write_reg32(P_ISA_TIMERE, 0); while( aml_read_reg32(P_ISA_TIMERE) < 2 ) {}    
    udelay(2);
    

    if (!ddr_pll_clk){
//        ddr_pll_clk = clk_util_clk_msr(CTS_DDR_CLK);
        printk("(CTS_DDR_CLK) = %ldMHz\n", ddr_pll_clk);
    }    

    if ((ddr_pll_clk==516)||(ddr_pll_clk==508)||(ddr_pll_clk==474)||(ddr_pll_clk==486)) { //use ddr pll 
        aml_write_reg32(P_HHI_VID_PLL_CNTL, pll_reg|(1<<30) );
        //aml_write_reg32(P_HHI_VID_PLL_CNTL2, 0x65e31ff );
        //aml_write_reg32(P_HHI_VID_PLL_CNTL3, 0x9649a941 );
        aml_write_reg32(P_HHI_VIID_PLL_CNTL, pll_reg|(1<<30) );
    } else {
        if(pll_sel){
#ifdef CONFIG_ARCH_MESON6
            M6_PLL_RESET(P_HHI_VIID_PLL_CNTL);
            aml_write_reg32(P_HHI_VIID_PLL_CNTL, pll_reg|(1<<30) );
            set_video_spread_spectrum(ss_level);  //set pll_ctrl2, pll_ctrl3, pll_ctrl4 reg and spread_spectrum
            aml_write_reg32(P_HHI_VIID_PLL_CNTL, pll_reg );
            M6_PLL_WAIT_FOR_LOCK(P_HHI_VIID_PLL_CNTL);
#else
            aml_write_reg32(P_HHI_VIID_PLL_CNTL, pll_reg|(1<<30) );
            aml_write_reg32(P_HHI_VIID_PLL_CNTL2, 0x65e31ff );
            aml_write_reg32(P_HHI_VIID_PLL_CNTL3, 0x9649a941 );
            aml_write_reg32(P_HHI_VIID_PLL_CNTL, pll_reg );
#endif
        }    
        else{
#ifdef CONFIG_ARCH_MESON6
            M6_PLL_RESET(P_HHI_VID_PLL_CNTL);
            aml_write_reg32(P_HHI_VID_PLL_CNTL, pll_reg|(1<<30) );
            aml_write_reg32(P_HHI_VID_PLL_CNTL2, M6_VID_PLL_CNTL_2 );
            aml_write_reg32(P_HHI_VID_PLL_CNTL3, M6_VID_PLL_CNTL_3 );
            aml_write_reg32(P_HHI_VID_PLL_CNTL4, M6_VID_PLL_CNTL_4 );
            aml_write_reg32(P_HHI_VID_PLL_CNTL, pll_reg );
            M6_PLL_WAIT_FOR_LOCK(P_HHI_VID_PLL_CNTL);
#else
            aml_write_reg32(P_HHI_VID_PLL_CNTL, pll_reg|(1<<30) );
            aml_write_reg32(P_HHI_VID_PLL_CNTL2, 0x65e31ff );
            aml_write_reg32(P_HHI_VID_PLL_CNTL3, 0x9649a941 );
            aml_write_reg32(P_HHI_VID_PLL_CNTL, pll_reg );
#endif
        }
    }
    udelay(10);
	
    if(pll_div_sel ){
        aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL,   vid_div_reg);
		//reset HHI_VIID_DIVIDER_CNTL
		aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VIID_DIVIDER_CNTL)|(1<<7));    //0x104c[7]:SOFT_RESET_POST
        aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VIID_DIVIDER_CNTL)|(1<<3));    //0x104c[3]:SOFT_RESET_PRE
        aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VIID_DIVIDER_CNTL)&(~(1<<1)));    //0x104c[1]:RESET_N_POST
        aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VIID_DIVIDER_CNTL)&(~(1<<0)));    //0x104c[0]:RESET_N_PRE
        msleep(2);
        aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VIID_DIVIDER_CNTL)&(~((1<<7)|(1<<3))));
        aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VIID_DIVIDER_CNTL)|((1<<1)|(1<<0)));
    }
    else{
        aml_write_reg32(P_HHI_VID_DIVIDER_CNTL,   vid_div_reg);
        //reset HHI_VID_DIVIDER_CNTL
        aml_write_reg32(P_HHI_VID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VID_DIVIDER_CNTL)|(1<<7));    //0x1066[7]:SOFT_RESET_POST
        aml_write_reg32(P_HHI_VID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VID_DIVIDER_CNTL)|(1<<3));    //0x1066[3]:SOFT_RESET_PRE
        aml_write_reg32(P_HHI_VID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VID_DIVIDER_CNTL)&(~(1<<1)));    //0x1066[1]:RESET_N_POST
        aml_write_reg32(P_HHI_VID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VID_DIVIDER_CNTL)&(~(1<<0)));    //0x1066[0]:RESET_N_PRE
        msleep(2);
        aml_write_reg32(P_HHI_VID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VID_DIVIDER_CNTL)&(~((1<<7)|(1<<3))));
        aml_write_reg32(P_HHI_VID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VID_DIVIDER_CNTL)|((1<<1)|(1<<0)));
    }

    if(vclk_sel)
        aml_write_reg32(P_HHI_VIID_CLK_DIV, (aml_read_reg32(P_HHI_VIID_CLK_DIV) & ~(0xFF << 0)) | (xd-1) );   // setup the XD divider value
    else 
        aml_write_reg32(P_HHI_VID_CLK_DIV, (aml_read_reg32(P_HHI_VID_CLK_DIV) & ~(0xFF << 0)) | (xd-1) );   // setup the XD divider value

    // delay 5uS
    //aml_write_reg32(P_ISA_TIMERE, 0); while( aml_read_reg32(P_ISA_TIMERE) < 5 ) {}    
    udelay(5);
    
    if ((ddr_pll_clk==516)||(ddr_pll_clk==508)||(ddr_pll_clk==474)||(ddr_pll_clk==486)) { //use ddr pll 
        aml_set_reg32_bits (P_HHI_VID_CLK_CNTL, 3, 16, 3);
        aml_write_reg32(P_HHI_VID_CLK_CNTL, aml_read_reg32(P_HHI_VID_CLK_CNTL) |  (1 << 19) );     //enable clk_div0 
        aml_write_reg32(P_HHI_VID_CLK_CNTL, aml_read_reg32(P_HHI_VID_CLK_CNTL) |  (1 << 20) );    //enable clk_div1 
        aml_write_reg32(P_HHI_VID_CLK_DIV, (aml_read_reg32(P_HHI_VID_CLK_DIV) & ~(0xFF << 24)) | (0x99 << 24));  //turn off enci and encp
        aml_write_reg32(P_HHI_VIID_CLK_DIV, (aml_read_reg32(P_HHI_VIID_CLK_DIV) & ~(0xFF00F << 12)) | (0x99009 << 12) ); //turn off encl vdac_clk1 vdac_clk0
        aml_write_reg32(P_HHI_HDMI_CLK_CNTL, (aml_read_reg32(P_HHI_HDMI_CLK_CNTL) & ~(0xF << 16)) | (0x9 << 16) ); //turn off hdmi_tx_pixel_clk 
    } else {
        if(vclk_sel) {
            if(pll_div_sel) aml_set_reg32_bits (P_HHI_VIID_CLK_CNTL, 4, 16, 3);  // Bit[18:16] - v2_cntl_clk_in_sel
            else aml_set_reg32_bits (P_HHI_VIID_CLK_CNTL, 0, 16, 3);  // Bit[18:16] - cntl_clk_in_sel
            //aml_write_reg32(P_HHI_VIID_CLK_CNTL, aml_read_reg32(P_HHI_VIID_CLK_CNTL) |  (1 << 19) );     //enable clk_div0 
			aml_set_reg32_mask(P_HHI_VIID_CLK_CNTL, (1 << 19) );     //enable clk_div0
        }
        else {
            if(pll_div_sel) aml_set_reg32_bits (P_HHI_VID_CLK_CNTL, 4, 16, 3);  // Bit[18:16] - v2_cntl_clk_in_sel
            else aml_set_reg32_bits (P_HHI_VID_CLK_CNTL, 0, 16, 3);  // Bit[18:16] - cntl_clk_in_sel
            //aml_write_reg32(P_HHI_VID_CLK_CNTL, aml_read_reg32(P_HHI_VID_CLK_CNTL) |  (1 << 19) );     //enable clk_div0 
			aml_set_reg32_mask(P_HHI_VID_CLK_CNTL, (1 << 19) );     //enable clk_div0 
            //aml_write_reg32(P_HHI_VID_CLK_CNTL, aml_read_reg32(P_HHI_VID_CLK_CNTL) |  (1 << 20) );     //enable clk_div1 
			aml_set_reg32_mask(P_HHI_VID_CLK_CNTL, (1 << 20) );     //enable clk_div1 
        }
    }
    // delay 2uS
    //aml_write_reg32(P_ISA_TIMERE, 0); while( aml_read_reg32(P_ISA_TIMERE) < 2 ) {}    
    udelay(2);
    
    // set tcon_clko setting
    aml_set_reg32_bits (P_HHI_VID_CLK_CNTL, 
                    (
                    (0 << 11) |     //clk_div1_sel
                    (1 << 10) |     //clk_inv
                    (0 << 9)  |     //neg_edge_sel
                    (0 << 5)  |     //tcon high_thresh
                    (0 << 1)  |     //tcon low_thresh
                    (1 << 0)        //cntl_clk_en1
                    ), 
                    20, 12);

    if(lcd_type == LCD_DIGITAL_TTL){
		if(vclk_sel)
		{
			aml_set_reg32_bits (P_HHI_VID_CLK_DIV, 8, 20, 4); // [23:20] enct_clk_sel, select v2_clk_div1
		}
		else
		{
			aml_set_reg32_bits (P_HHI_VID_CLK_DIV, 0, 20, 4); // [23:20] enct_clk_sel, select v1_clk_div1
		}
		
	}
	else {
		if(vclk_sel)
		{
			aml_set_reg32_bits (P_HHI_VIID_CLK_DIV, 8, 12, 4); // [23:20] encl_clk_sel, select v2_clk_div1
		}
		else
		{
			aml_set_reg32_bits (P_HHI_VIID_CLK_DIV, 0, 12, 4); // [23:20] encl_clk_sel, select v1_clk_div1
		}		
	}
	
    if(vclk_sel) {
      aml_set_reg32_bits (P_HHI_VIID_CLK_CNTL, 
                   (1<<0),  // Enable cntl_div1_en
                   0, 1    // cntl_div1_en
                   );
      aml_set_reg32_bits (P_HHI_VIID_CLK_CNTL, 1, 15, 1);  //soft reset
      aml_set_reg32_bits (P_HHI_VIID_CLK_CNTL, 0, 15, 1);  //release soft reset
    }
    else {
      aml_set_reg32_bits (P_HHI_VID_CLK_CNTL, 
                   (1<<0),  // Enable cntl_div1_en
                   0, 1    // cntl_div1_en
                   );
      aml_set_reg32_bits (P_HHI_VID_CLK_CNTL, 1, 15, 1);  //soft reset
      aml_set_reg32_bits (P_HHI_VID_CLK_CNTL, 0, 15, 1);  //release soft reset
    }    

    //PRINT_INFO("video pl1 clk = %d\n", clk_util_clk_msr(VID_PLL_CLK));
    //PRINT_INFO("video pll2 clk = %d\n", clk_util_clk_msr(VID2_PLL_CLK));
    //PRINT_INFO("cts_enct clk = %d\n", clk_util_clk_msr(CTS_ENCT_CLK));
	//PRINT_INFO("cts_encl clk = %d\n", clk_util_clk_msr(CTS_ENCL_CLK));
}

static void set_pll_ttl(Lcd_Config_t *pConf)
{            
    unsigned pll_reg, div_reg, xd;    
    int pll_sel, pll_div_sel, vclk_sel;
	int lcd_type, ss_level;
    
    pll_reg = pConf->lcd_timing.pll_ctrl;
    div_reg = pConf->lcd_timing.div_ctrl | 0x3; 
	ss_level = ((pConf->lcd_timing.clk_ctrl) >>16) & 0xf;      
    pll_sel = ((pConf->lcd_timing.clk_ctrl) >>12) & 0x1;
    pll_div_sel = ((pConf->lcd_timing.clk_ctrl) >>8) & 0x1;
    vclk_sel = ((pConf->lcd_timing.clk_ctrl) >>4) & 0x1;        
	xd = pConf->lcd_timing.clk_ctrl & 0xf;
	
	lcd_type = pConf->lcd_basic.lcd_type;
    
    printk("ss_level=%d, pll_sel=%d, pll_div_sel=%d, vclk_sel=%d, pll_reg=0x%x, div_reg=0x%x, xd=%d.\n", ss_level, pll_sel, pll_div_sel, vclk_sel, pll_reg, div_reg, xd);
    vclk_set_lcd(lcd_type, ss_level, pll_sel, pll_div_sel, vclk_sel, pll_reg, div_reg, xd);  	
}

static void clk_util_lvds_set_clk_div(  unsigned long   divn_sel,
                                    unsigned long   divn_tcnt,
                                    unsigned long   div2_en  )
{
    // assign          lvds_div_phy_clk_en     = tst_lvds_tmode ? 1'b1         : phy_clk_cntl[10];
    // assign          lvds_div_div2_sel       = tst_lvds_tmode ? atest_i[5]   : phy_clk_cntl[9];
    // assign          lvds_div_sel            = tst_lvds_tmode ? atest_i[7:6] : phy_clk_cntl[8:7];
    // assign          lvds_div_tcnt           = tst_lvds_tmode ? 3'd6         : phy_clk_cntl[6:4];
    // If dividing by 1, just select the divide by 1 path
    if( divn_tcnt == 1 ) {
        divn_sel = 0;
    }
    aml_write_reg32(P_LVDS_PHY_CLK_CNTL, ((aml_read_reg32(P_LVDS_PHY_CLK_CNTL) & ~((0x3 << 7) | (1 << 9) | (0x7 << 4))) | ((1 << 10) | (divn_sel << 7) | (div2_en << 9) | (((divn_tcnt-1)&0x7) << 4))) );
}

static void set_pll_lvds(Lcd_Config_t *pConf)
{
    PRINT_INFO("%s\n", __FUNCTION__);

    int pll_div_post;
    int phy_clk_div2;
    int FIFO_CLK_SEL;
    unsigned long rd_data;

    unsigned pll_reg, div_reg, xd;
    int pll_sel, pll_div_sel, vclk_sel;
	int lcd_type, ss_level;
	
    pll_reg = pConf->lcd_timing.pll_ctrl;
    div_reg = pConf->lcd_timing.div_ctrl | 0x3;    
	ss_level = ((pConf->lcd_timing.clk_ctrl) >>16) & 0xf;
    pll_sel = ((pConf->lcd_timing.clk_ctrl) >>12) & 0x1;
    pll_div_sel = ((pConf->lcd_timing.clk_ctrl) >>8) & 0x1;
    vclk_sel = ((pConf->lcd_timing.clk_ctrl) >>4) & 0x1;
	xd = pConf->lcd_timing.clk_ctrl & 0xf;
	
	lcd_type = pConf->lcd_basic.lcd_type;
	
    pll_div_post = 7;

    phy_clk_div2 = 0;
	
	div_reg = (div_reg | (1 << 8) | (1 << 11) | ((pll_div_post-1) << 12) | (phy_clk_div2 << 10));
	printk("ss_level=%d, pll_sel=%d, pll_div_sel=%d, vclk_sel=%d, pll_reg=0x%x, div_reg=0x%x, xd=%d.\n", ss_level, pll_sel, pll_div_sel, vclk_sel, pll_reg, div_reg, xd);
    vclk_set_lcd(lcd_type, ss_level, pll_sel, pll_div_sel, vclk_sel, pll_reg, div_reg, xd);					 
	
	clk_util_lvds_set_clk_div(1, pll_div_post, phy_clk_div2);
	
    aml_write_reg32(P_LVDS_PHY_CNTL0, 0xffff );

    //    lvds_gen_cntl       <= {10'h0,      // [15:4] unused
    //                            2'h1,       // [5:4] divide by 7 in the PHY
    //                            1'b0,       // [3] fifo_en
    //                            1'b0,       // [2] wr_bist_gate
    //                            2'b00};     // [1:0] fifo_wr mode
    FIFO_CLK_SEL = 1; // div7
    rd_data = aml_read_reg32(P_LVDS_GEN_CNTL);
    rd_data &= 0xffcf | (FIFO_CLK_SEL<< 4);
    aml_write_reg32(P_LVDS_GEN_CNTL, rd_data);

    // Set Hard Reset lvds_phy_ser_top
    aml_write_reg32(P_LVDS_PHY_CLK_CNTL, aml_read_reg32(P_LVDS_PHY_CLK_CNTL) & 0x7fff);
    // Release Hard Reset lvds_phy_ser_top
    aml_write_reg32(P_LVDS_PHY_CLK_CNTL, aml_read_reg32(P_LVDS_PHY_CLK_CNTL) | 0x8000);	
}

static void set_pll_mlvds(Lcd_Config_t *pConf)
{
    PRINT_INFO("%s\n", __FUNCTION__);

    int test_bit_num = pConf->lcd_basic.lcd_bits;
    int test_dual_gate = pConf->lvds_mlvds_config.mlvds_config->test_dual_gate;
    int test_pair_num= pConf->lvds_mlvds_config.mlvds_config->test_pair_num;
    int pll_div_post;
    int phy_clk_div2;
    int FIFO_CLK_SEL;
    int MPCLK_DELAY;
    int MCLK_half;
    int MCLK_half_delay;
    unsigned int data32;
    unsigned long mclk_pattern_dual_6_6;
    int test_high_phase = (test_bit_num != 8) | test_dual_gate;
    unsigned long rd_data;

    unsigned pll_reg, div_reg, xd;
    int pll_sel, pll_div_sel, vclk_sel;
	int lcd_type, ss_level;
	
    pll_reg = pConf->lcd_timing.pll_ctrl;
    div_reg = pConf->lcd_timing.div_ctrl | 0x3;    
	ss_level = ((pConf->lcd_timing.clk_ctrl) >>16) & 0xf;
    pll_sel = ((pConf->lcd_timing.clk_ctrl) >>12) & 0x1;
    pll_div_sel = ((pConf->lcd_timing.clk_ctrl) >>8) & 0x1;
    vclk_sel = ((pConf->lcd_timing.clk_ctrl) >>4) & 0x1;
	xd = pConf->lcd_timing.clk_ctrl & 0xf;
	
	lcd_type = pConf->lcd_basic.lcd_type;

    switch(pConf->lvds_mlvds_config.mlvds_config->TL080_phase)
    {
      case 0 :
        mclk_pattern_dual_6_6 = 0xc3c3c3;
        MCLK_half = 1;
        break;
      case 1 :
        mclk_pattern_dual_6_6 = 0xc3c3c3;
        MCLK_half = 0;
        break;
      case 2 :
        mclk_pattern_dual_6_6 = 0x878787;
        MCLK_half = 1;
        break;
      case 3 :
        mclk_pattern_dual_6_6 = 0x878787;
        MCLK_half = 0;
        break;
      case 4 :
        mclk_pattern_dual_6_6 = 0x3c3c3c;
        MCLK_half = 1;
        break;
       case 5 :
        mclk_pattern_dual_6_6 = 0x3c3c3c;
        MCLK_half = 0;
        break;
       case 6 :
        mclk_pattern_dual_6_6 = 0x787878;
        MCLK_half = 1;
        break;
      default : // case 7
        mclk_pattern_dual_6_6 = 0x787878;
        MCLK_half = 0;
        break;
    }

    pll_div_post = (test_bit_num == 8) ?
                      (
                        test_dual_gate ? 4 :
                                         8
                      ) :
                      (
                        test_dual_gate ? 3 :
                                         6
                      ) ;

    phy_clk_div2 = (test_pair_num != 3);
	
	div_reg = (div_reg | (1 << 8) | (1 << 11) | ((pll_div_post-1) << 12) | (phy_clk_div2 << 10));
	printk("ss_level=%d, pll_sel=%d, pll_div_sel=%d, vclk_sel=%d, pll_reg=0x%x, div_reg=0x%x, xd=%d.\n", ss_level, pll_sel, pll_div_sel, vclk_sel, pll_reg, div_reg, xd);
    vclk_set_lcd(lcd_type, ss_level, pll_sel, pll_div_sel, vclk_sel, pll_reg, div_reg, xd);	  					 
	
	clk_util_lvds_set_clk_div(1, pll_div_post, phy_clk_div2);	
	
	//enable v2_clk div
    // aml_write_reg32(P_ HHI_VIID_CLK_CNTL, aml_read_reg32(P_HHI_VIID_CLK_CNTL) | (0xF << 0) );
    // aml_write_reg32(P_ HHI_VID_CLK_CNTL, aml_read_reg32(P_HHI_VID_CLK_CNTL) | (0xF << 0) );

    aml_write_reg32(P_LVDS_PHY_CNTL0, 0xffff );

    //    lvds_gen_cntl       <= {10'h0,      // [15:4] unused
    //                            2'h1,       // [5:4] divide by 7 in the PHY
    //                            1'b0,       // [3] fifo_en
    //                            1'b0,       // [2] wr_bist_gate
    //                            2'b00};     // [1:0] fifo_wr mode

    FIFO_CLK_SEL = (test_bit_num == 8) ? 2 : // div8
                                    0 ; // div6
    rd_data = aml_read_reg32(P_LVDS_GEN_CNTL);
    rd_data = (rd_data & 0xffcf) | (FIFO_CLK_SEL<< 4);
    aml_write_reg32(P_LVDS_GEN_CNTL, rd_data);

    MPCLK_DELAY = (test_pair_num == 6) ?
                  ((test_bit_num == 8) ? (test_dual_gate ? 5 : 3) : 2) :
                  ((test_bit_num == 8) ? 3 : 3) ;

    MCLK_half_delay = pConf->lvds_mlvds_config.mlvds_config->phase_select ? MCLK_half :
                      (
                      test_dual_gate &
                      (test_bit_num == 8) &
                      (test_pair_num != 6)
                      );

    if(test_high_phase)
    {
        if(test_dual_gate)
        data32 = (MPCLK_DELAY << mpclk_dly) |
                 (((test_bit_num == 8) ? 3 : 2) << mpclk_div) |
                 (1 << use_mpclk) |
                 (MCLK_half_delay << mlvds_clk_half_delay) |
                 (((test_bit_num == 8) ? (
                                           (test_pair_num == 6) ? 0x999999 : // DIV4
                                                                  0x555555   // DIV2
                                         ) :
                                         (
                                           (test_pair_num == 6) ? mclk_pattern_dual_6_6 : // DIV8
                                                                  0x999999   // DIV4
                                         )
                                         ) << mlvds_clk_pattern);      // DIV 8
        else if(test_bit_num == 8)
            data32 = (MPCLK_DELAY << mpclk_dly) |
                     (((test_bit_num == 8) ? 3 : 2) << mpclk_div) |
                     (1 << use_mpclk) |
                     (0 << mlvds_clk_half_delay) |
                     (0xc3c3c3 << mlvds_clk_pattern);      // DIV 8
        else
            data32 = (MPCLK_DELAY << mpclk_dly) |
                     (((test_bit_num == 8) ? 3 : 2) << mpclk_div) |
                     (1 << use_mpclk) |
                     (0 << mlvds_clk_half_delay) |
                     (
                       (
                         (test_pair_num == 6) ? 0xc3c3c3 : // DIV8
                                                0x999999   // DIV4
                       ) << mlvds_clk_pattern
                     );
    }
    else
    {
        if(test_pair_num == 6)
        {
            data32 = (MPCLK_DELAY << mpclk_dly) |
                     (((test_bit_num == 8) ? 3 : 2) << mpclk_div) |
                     (1 << use_mpclk) |
                     (0 << mlvds_clk_half_delay) |
                     (
                       (
                         (test_pair_num == 6) ? 0x999999 : // DIV4
                                                0x555555   // DIV2
                       ) << mlvds_clk_pattern
                     );
        }
        else
        {
            data32 = (1 << mlvds_clk_half_delay) |
                   (0x555555 << mlvds_clk_pattern);      // DIV 2
        }
    }

    aml_write_reg32(P_MLVDS_CLK_CTL_HI,  (data32 >> 16));
    aml_write_reg32(P_MLVDS_CLK_CTL_LO, (data32 & 0xffff));

	//pll_div_sel
    if(1){
		// Set Soft Reset vid_pll_div_pre
		aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VIID_DIVIDER_CNTL) | 0x00008);
		// Set Hard Reset vid_pll_div_post
		aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VIID_DIVIDER_CNTL) & 0x1fffd);
		// Set Hard Reset lvds_phy_ser_top
		aml_write_reg32(P_LVDS_PHY_CLK_CNTL, aml_read_reg32(P_LVDS_PHY_CLK_CNTL) & 0x7fff);
		// Release Hard Reset lvds_phy_ser_top
		aml_write_reg32(P_LVDS_PHY_CLK_CNTL, aml_read_reg32(P_LVDS_PHY_CLK_CNTL) | 0x8000);
		// Release Hard Reset vid_pll_div_post
		aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VIID_DIVIDER_CNTL) | 0x00002);
		// Release Soft Reset vid_pll_div_pre
		aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VIID_DIVIDER_CNTL) & 0x1fff7);
	}
	else{
		// Set Soft Reset vid_pll_div_pre
		aml_write_reg32(P_HHI_VID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VID_DIVIDER_CNTL) | 0x00008);
		// Set Hard Reset vid_pll_div_post
		aml_write_reg32(P_HHI_VID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VID_DIVIDER_CNTL) & 0x1fffd);
		// Set Hard Reset lvds_phy_ser_top
		aml_write_reg32(P_LVDS_PHY_CLK_CNTL, aml_read_reg32(P_LVDS_PHY_CLK_CNTL) & 0x7fff);
		// Release Hard Reset lvds_phy_ser_top
		aml_write_reg32(P_LVDS_PHY_CLK_CNTL, aml_read_reg32(P_LVDS_PHY_CLK_CNTL) | 0x8000);
		// Release Hard Reset vid_pll_div_post
		aml_write_reg32(P_HHI_VID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VID_DIVIDER_CNTL) | 0x00002);
		// Release Soft Reset vid_pll_div_pre
		aml_write_reg32(P_HHI_VID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VID_DIVIDER_CNTL) & 0x1fff7);
    }	
}

static void venc_set_ttl(Lcd_Config_t *pConf)
{
    PRINT_INFO("%s\n", __FUNCTION__);
#ifdef CONFIG_AM_TV_OUTPUT2
    if(vpp2_sel){
        aml_write_reg32(P_VPU_VIU_VENC_MUX_CTRL, (aml_read_reg32(P_VPU_VIU_VENC_MUX_CTRL)&(~(0x3<<2)))|(0x3<<2)); //viu2 select enct
    }
    else{
        aml_write_reg32(P_VPU_VIU_VENC_MUX_CTRL, (aml_read_reg32(P_VPU_VIU_VENC_MUX_CTRL)&(~0xff3))|0x443); //viu1 select enct,Select encT clock to VDIN, Enable VIU of ENC_T domain to VDIN
    }
#else
    aml_write_reg32(P_VPU_VIU_VENC_MUX_CTRL,
       (3<<0) |    // viu1 select enct
       (3<<2)      // viu2 select enct
    );
#endif    
    aml_write_reg32(P_ENCT_VIDEO_MODE,        0);
    aml_write_reg32(P_ENCT_VIDEO_MODE_ADV,    0x0418);  
	  
	// bypass filter
    aml_write_reg32(P_ENCT_VIDEO_FILT_CTRL,    0x1000);
    aml_write_reg32(P_VENC_DVI_SETTING,        0x11);
    aml_write_reg32(P_VENC_VIDEO_PROG_MODE,    0x100);

    aml_write_reg32(P_ENCT_VIDEO_MAX_PXCNT,    pConf->lcd_basic.h_period - 1);
    aml_write_reg32(P_ENCT_VIDEO_MAX_LNCNT,    pConf->lcd_basic.v_period - 1);

    aml_write_reg32(P_ENCT_VIDEO_HAVON_BEGIN,  pConf->lcd_timing.video_on_pixel);
    aml_write_reg32(P_ENCT_VIDEO_HAVON_END,    pConf->lcd_basic.h_active - 1 + pConf->lcd_timing.video_on_pixel);
    aml_write_reg32(P_ENCT_VIDEO_VAVON_BLINE,  pConf->lcd_timing.video_on_line);
    aml_write_reg32(P_ENCT_VIDEO_VAVON_ELINE,  pConf->lcd_basic.v_active + 3  + pConf->lcd_timing.video_on_line);

    aml_write_reg32(P_ENCT_VIDEO_HSO_BEGIN,    15);
    aml_write_reg32(P_ENCT_VIDEO_HSO_END,      31);
    aml_write_reg32(P_ENCT_VIDEO_VSO_BEGIN,    15);
    aml_write_reg32(P_ENCT_VIDEO_VSO_END,      31);
    aml_write_reg32(P_ENCT_VIDEO_VSO_BLINE,    0);
    aml_write_reg32(P_ENCT_VIDEO_VSO_ELINE,    2);    
    
    // enable enct
    aml_write_reg32(P_ENCT_VIDEO_EN,           1);
}

static void venc_set_lvds(Lcd_Config_t *pConf)
{
    PRINT_INFO("%s\n", __FUNCTION__);
    
	aml_write_reg32(P_ENCL_VIDEO_EN,           0);
	//int havon_begin = 80;
#ifdef CONFIG_AM_TV_OUTPUT2
    if(vpp2_sel){
        aml_write_reg32(P_VPU_VIU_VENC_MUX_CTRL, (aml_read_reg32(P_VPU_VIU_VENC_MUX_CTRL)&(~(0x3<<2)))|(0x0<<2)); //viu2 select encl
    }
    else{
        aml_write_reg32(P_VPU_VIU_VENC_MUX_CTRL, (aml_read_reg32(P_VPU_VIU_VENC_MUX_CTRL)&(~0xff3))|0x880); //viu1 select encl,Select encL clock to VDIN, Enable VIU of ENC_L domain to VDIN
    }
#else
    aml_write_reg32(P_VPU_VIU_VENC_MUX_CTRL,
       (0<<0) |    // viu1 select encl
       (0<<2)      // viu2 select encl
       );
#endif	
	aml_write_reg32(P_ENCL_VIDEO_MODE,         0x0040 | (1<<14)); // Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
	aml_write_reg32(P_ENCL_VIDEO_MODE_ADV,     0x0008); // Sampling rate: 1

	// bypass filter
 	aml_write_reg32(P_ENCL_VIDEO_FILT_CTRL	,0x1000);
	
	aml_write_reg32(P_ENCL_VIDEO_MAX_PXCNT,	pConf->lcd_basic.h_period - 1);
	aml_write_reg32(P_ENCL_VIDEO_MAX_LNCNT,	pConf->lcd_basic.v_period - 1);

	aml_write_reg32(P_ENCL_VIDEO_HAVON_BEGIN,	pConf->lcd_timing.video_on_pixel);
	aml_write_reg32(P_ENCL_VIDEO_HAVON_END,		pConf->lcd_basic.h_active - 1 + pConf->lcd_timing.video_on_pixel);
	aml_write_reg32(P_ENCL_VIDEO_VAVON_BLINE,	pConf->lcd_timing.video_on_line);
	aml_write_reg32(P_ENCL_VIDEO_VAVON_ELINE,	pConf->lcd_basic.v_active + 1  + pConf->lcd_timing.video_on_line);

	aml_write_reg32(P_ENCL_VIDEO_HSO_BEGIN,	pConf->lcd_timing.sth1_hs_addr);//10);
	aml_write_reg32(P_ENCL_VIDEO_HSO_END,	pConf->lcd_timing.sth1_he_addr);//20);
	aml_write_reg32(P_ENCL_VIDEO_VSO_BEGIN,	pConf->lcd_timing.stv1_hs_addr);//10);
	aml_write_reg32(P_ENCL_VIDEO_VSO_END,	pConf->lcd_timing.stv1_he_addr);//20);
	aml_write_reg32(P_ENCL_VIDEO_VSO_BLINE,	pConf->lcd_timing.stv1_vs_addr);//2);
	aml_write_reg32(P_ENCL_VIDEO_VSO_ELINE,	pConf->lcd_timing.stv1_ve_addr);//4);
		
	aml_write_reg32(P_ENCL_VIDEO_RGBIN_CTRL, 	0);
    	
	// enable encl
    aml_write_reg32(P_ENCL_VIDEO_EN,           1);
}

static void venc_set_mlvds(Lcd_Config_t *pConf)
{
    PRINT_INFO("%s\n", __FUNCTION__);

    aml_write_reg32(P_ENCL_VIDEO_EN,           0);

#ifdef CONFIG_AM_TV_OUTPUT2
    if(vpp2_sel){
        aml_write_reg32(P_VPU_VIU_VENC_MUX_CTRL, (aml_read_reg32(P_VPU_VIU_VENC_MUX_CTRL)&(~(0x3<<2)))|(0x0<<2)); //viu2 select encl
    }
    else{
        aml_write_reg32(P_VPU_VIU_VENC_MUX_CTRL, (aml_read_reg32(P_VPU_VIU_VENC_MUX_CTRL)&(~(0x3<<0)))|(0x0<<0)); //viu1 select encl
    }
#else
    aml_write_reg32(P_VPU_VIU_VENC_MUX_CTRL,
       (0<<0) |    // viu1 select encl
       (0<<2)      // viu2 select encl
       );
#endif
	int ext_pixel = pConf->lvds_mlvds_config.mlvds_config->test_dual_gate ? pConf->lvds_mlvds_config.mlvds_config->total_line_clk : 0;
	int active_h_start = pConf->lcd_timing.video_on_pixel;
	int active_v_start = pConf->lcd_timing.video_on_line;
	int width = pConf->lcd_basic.h_active;
	int height = pConf->lcd_basic.v_active;
	int max_height = pConf->lcd_basic.v_period;

	aml_write_reg32(P_ENCL_VIDEO_MODE,             0x0040 | (1<<14)); // Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
	aml_write_reg32(P_ENCL_VIDEO_MODE_ADV,         0x0008); // Sampling rate: 1
	
	// bypass filter
 	aml_write_reg32(P_ENCL_VIDEO_FILT_CTRL,	0x1000);
	
	aml_write_reg32(P_ENCL_VIDEO_YFP1_HTIME,       active_h_start);
	aml_write_reg32(P_ENCL_VIDEO_YFP2_HTIME,       active_h_start + width);

	aml_write_reg32(P_ENCL_VIDEO_MAX_PXCNT,        pConf->lvds_mlvds_config.mlvds_config->total_line_clk - 1 + ext_pixel);
	aml_write_reg32(P_ENCL_VIDEO_MAX_LNCNT,        max_height - 1);

	aml_write_reg32(P_ENCL_VIDEO_HAVON_BEGIN,      active_h_start);
	aml_write_reg32(P_ENCL_VIDEO_HAVON_END,        active_h_start + width - 1);  // for dual_gate mode still read 1408 pixel at first half of line
	aml_write_reg32(P_ENCL_VIDEO_VAVON_BLINE,      active_v_start);
	aml_write_reg32(P_ENCL_VIDEO_VAVON_ELINE,      active_v_start + height -1);  //15+768-1);

	aml_write_reg32(P_ENCL_VIDEO_HSO_BEGIN,        24);
	aml_write_reg32(P_ENCL_VIDEO_HSO_END,          1420 + ext_pixel);
	aml_write_reg32(P_ENCL_VIDEO_VSO_BEGIN,        1400 + ext_pixel);
	aml_write_reg32(P_ENCL_VIDEO_VSO_END,          1410 + ext_pixel);
	aml_write_reg32(P_ENCL_VIDEO_VSO_BLINE,        1);
	aml_write_reg32(P_ENCL_VIDEO_VSO_ELINE,        3);

	aml_write_reg32(P_ENCL_VIDEO_RGBIN_CTRL, 	0); 	

	// enable encl
    aml_write_reg32(P_ENCL_VIDEO_EN,           1);
}

static void set_control_lvds(Lcd_Config_t *pConf)
{
    PRINT_INFO("%s\n", __FUNCTION__);
	
	int lvds_repack, pn_swap, bit_num;
	unsigned long data32;    

    data32 = (0x00 << LVDS_blank_data_r) |
             (0x00 << LVDS_blank_data_g) |
             (0x00 << LVDS_blank_data_b) ; 
    aml_write_reg32(P_LVDS_BLANK_DATA_HI,  (data32 >> 16));
    aml_write_reg32(P_LVDS_BLANK_DATA_LO, (data32 & 0xffff));
	
	aml_write_reg32(P_LVDS_PHY_CNTL0, 0xffff );
	aml_write_reg32(P_LVDS_PHY_CNTL1, 0xff00 );
	
	//aml_write_reg32(P_ENCL_VIDEO_EN,           1);	
	lvds_repack = (pConf->lvds_mlvds_config.lvds_config->lvds_repack) & 0x1;
	pn_swap = (pConf->lvds_mlvds_config.lvds_config->pn_swap) & 0x1;
	switch(pConf->lcd_basic.lcd_bits)
	{
		case 10:
			bit_num=0;
			break;
		case 8:
			bit_num=1;
			break;
		case 6:
			bit_num=2;
			break;
		case 4:
			bit_num=3;
			break;
		default:
			bit_num=1;
			break;
	}
	
	aml_write_reg32(P_MLVDS_CONTROL,  (aml_read_reg32(P_MLVDS_CONTROL) & ~(1 << 0)));  //disable mlvds	
    
	aml_write_reg32(P_LVDS_PACK_CNTL_ADDR, 
					( lvds_repack<<0 ) | // repack
					( 0<<2 ) | // odd_even
					( 0<<3 ) | // reserve
					( 0<<4 ) | // lsb first
					( pn_swap<<5 ) | // pn swap
					( 0<<6 ) | // dual port
					( 0<<7 ) | // use tcon control
					( bit_num<<8 ) | // 0:10bits, 1:8bits, 2:6bits, 3:4bits.
					( 0<<10 ) | //r_select  //0:R, 1:G, 2:B, 3:0
					( 1<<12 ) | //g_select  //0:R, 1:G, 2:B, 3:0
					( 2<<14 ));  //b_select  //0:R, 1:G, 2:B, 3:0; 
				   
    aml_write_reg32(P_LVDS_GEN_CNTL, (aml_read_reg32(P_LVDS_GEN_CNTL) | (1 << 0)) );  //fifo enable  
    //aml_write_reg32(P_LVDS_GEN_CNTL, (aml_read_reg32(P_LVDS_GEN_CNTL) | (1 << 3))); // enable fifo
	
	//PRINT_INFO("lvds fifo clk = %d.\n", clk_util_clk_msr(LVDS_FIFO_CLK));	
}

static void set_control_mlvds(Lcd_Config_t *pConf)
{
    PRINT_INFO("%s\n", __FUNCTION__);
	
	int test_bit_num = pConf->lcd_basic.lcd_bits;
    int test_pair_num = pConf->lvds_mlvds_config.mlvds_config->test_pair_num;
    int test_dual_gate = pConf->lvds_mlvds_config.mlvds_config->test_dual_gate;
    int scan_function = pConf->lvds_mlvds_config.mlvds_config->scan_function;     //0:U->D,L->R  //1:D->U,R->L
    int mlvds_insert_start;
    unsigned int reset_offset;
    unsigned int reset_length;

    unsigned long data32;
    
    mlvds_insert_start = test_dual_gate ?
                           ((test_bit_num == 8) ? ((test_pair_num == 6) ? 0x9f : 0xa9) :
                                                  ((test_pair_num == 6) ? pConf->lvds_mlvds_config.mlvds_config->mlvds_insert_start : 0xa7)
                           ) :
                           (
                             (test_pair_num == 6) ? ((test_bit_num == 8) ? 0xa9 : 0xa7) :
                                                    ((test_bit_num == 8) ? 0xae : 0xad)
                           );

    // Enable the LVDS PHY (power down bits)
    aml_write_reg32(P_LVDS_PHY_CNTL1,aml_read_reg32(P_LVDS_PHY_CNTL1) | (0x7F << 8) );

    data32 = (0x00 << LVDS_blank_data_r) |
             (0x00 << LVDS_blank_data_g) |
             (0x00 << LVDS_blank_data_b) ;
    aml_write_reg32(P_LVDS_BLANK_DATA_HI,  (data32 >> 16));
    aml_write_reg32(P_LVDS_BLANK_DATA_LO, (data32 & 0xffff));

    data32 = 0x7fffffff; //  '0'x1 + '1'x32 + '0'x2
    aml_write_reg32(P_MLVDS_RESET_PATTERN_HI,  (data32 >> 16));
    aml_write_reg32(P_MLVDS_RESET_PATTERN_LO, (data32 & 0xffff));
    data32 = 0x8000; // '0'x1 + '1'x32 + '0'x2
    aml_write_reg32(P_MLVDS_RESET_PATTERN_EXT,  (data32 & 0xffff));

    reset_length = 1+32+2;
    reset_offset = test_bit_num - (reset_length%test_bit_num);

    data32 = (reset_offset << mLVDS_reset_offset) |
             (reset_length << mLVDS_reset_length) |
             ((test_pair_num == 6) << mLVDS_data_write_toggle) |
             ((test_pair_num != 6) << mLVDS_data_write_ini) |
             ((test_pair_num == 6) << mLVDS_data_latch_1_toggle) |
             (0 << mLVDS_data_latch_1_ini) |
             ((test_pair_num == 6) << mLVDS_data_latch_0_toggle) |
             (1 << mLVDS_data_latch_0_ini) |
             ((test_pair_num == 6) << mLVDS_reset_1_select) |
             (mlvds_insert_start << mLVDS_reset_start);
    aml_write_reg32(P_MLVDS_CONFIG_HI,  (data32 >> 16));
    aml_write_reg32(P_MLVDS_CONFIG_LO, (data32 & 0xffff));

    data32 = (1 << mLVDS_double_pattern) |  //POL double pattern
			 (0x3f << mLVDS_ins_reset) |
             (test_dual_gate << mLVDS_dual_gate) |
             ((test_bit_num == 8) << mLVDS_bit_num) |
             ((test_pair_num == 6) << mLVDS_pair_num) |
             (0 << mLVDS_msb_first) |
             (0 << mLVDS_PORT_SWAP) |
             ((scan_function==1 ? 1:0) << mLVDS_MLSB_SWAP) |
             (0 << mLVDS_PN_SWAP) |
             (1 << mLVDS_en);
    aml_write_reg32(P_MLVDS_CONTROL,  (data32 & 0xffff));

    aml_write_reg32(P_LVDS_PACK_CNTL_ADDR,
                   ( 0 ) | // repack
                   ( 0<<2 ) | // odd_even
                   ( 0<<3 ) | // reserve
                   ( 0<<4 ) | // lsb first
                   ( 0<<5 ) | // pn swap
                   ( 0<<6 ) | // dual port
                   ( 0<<7 ) | // use tcon control
                   ( 1<<8 ) | // 0:10bits, 1:8bits, 2:6bits, 3:4bits.
                   ( (scan_function==1 ? 2:0)<<10 ) |  //r_select // 0:R, 1:G, 2:B, 3:0
                   ( 1<<12 ) |                        //g_select
                   ( (scan_function==1 ? 0:2)<<14 ));  //b_select

    aml_write_reg32(P_L_POL_CNTL_ADDR,  (1 << LCD_DCLK_SEL) |
       //(0x1 << LCD_HS_POL) |
       (0x1 << LCD_VS_POL)
    );

    //aml_write_reg32(P_LVDS_GEN_CNTL, (aml_read_reg32(P_LVDS_GEN_CNTL) | (1 << 3))); // enable fifo
}

static void init_lvds_phy(Lcd_Config_t *pConf)
{
    PRINT_INFO("%s\n", __FUNCTION__);
	
	unsigned tmp_add_data;
	
    aml_write_reg32(P_LVDS_PHY_CNTL3, 0x0ee0);      //0x0ee1
    aml_write_reg32(P_LVDS_PHY_CNTL4 ,0);
	
    tmp_add_data  = 0;
    tmp_add_data |= (pConf->lvds_mlvds_config.lvds_phy_control->lvds_prem_ctl & 0xf) << 0; //LVDS_PREM_CTL<3:0>=<1111>
    tmp_add_data |= (pConf->lvds_mlvds_config.lvds_phy_control->lvds_swing_ctl & 0xf) << 4; //LVDS_SWING_CTL<3:0>=<0011>    
    tmp_add_data |= (pConf->lvds_mlvds_config.lvds_phy_control->lvds_vcm_ctl & 0x7) << 8; //LVDS_VCM_CTL<2:0>=<001>
	tmp_add_data |= (pConf->lvds_mlvds_config.lvds_phy_control->lvds_ref_ctl & 0x1f) << 11; //LVDS_REFCTL<4:0>=<01111> 
	aml_write_reg32(P_LVDS_PHY_CNTL5, tmp_add_data);

	aml_write_reg32(P_LVDS_PHY_CNTL0,0x001f);
	aml_write_reg32(P_LVDS_PHY_CNTL1,0xffff);	

    aml_write_reg32(P_LVDS_PHY_CNTL6,0xcccc);
    aml_write_reg32(P_LVDS_PHY_CNTL7,0xcccc);
    aml_write_reg32(P_LVDS_PHY_CNTL8,0xcccc);
	
    //aml_write_reg32(P_LVDS_PHY_CNTL4, aml_read_reg32(P_LVDS_PHY_CNTL4) | (0x7f<<0));  //enable LVDS phy port..	
}

#include <mach/mod_gate.h>

static void switch_lcd_gates(Lcd_Type_t lcd_type)
{
    switch(lcd_type){
        case LCD_DIGITAL_TTL:
#ifdef CONFIG_ARCH_MESON6
            switch_mod_gate_by_name("tcon", 1);
            switch_mod_gate_by_name("lvds", 0);
#endif
            break;
        case LCD_DIGITAL_LVDS:
        case LCD_DIGITAL_MINILVDS:
#ifdef CONFIG_ARCH_MESON6
            switch_mod_gate_by_name("lvds", 1);
            switch_mod_gate_by_name("tcon", 0);
#endif
            break;
        default:
            break;
    }
}

static inline void _init_display_driver(Lcd_Config_t *pConf)
{ 
    int lcd_type;
	const char* lcd_type_table[]={
		"NULL",
		"TTL",
		"LVDS",
		"miniLVDS",
		"invalid",
	}; 
	
	lcd_type = pDev->conf.lcd_basic.lcd_type;
	printk("\nInit LCD type: %s.\n", lcd_type_table[lcd_type]);
	printk("lcd frame rate=%d/%d.\n", pDev->conf.lcd_timing.sync_duration_num, pDev->conf.lcd_timing.sync_duration_den);
	
    switch_lcd_gates(lcd_type);

	switch(lcd_type){
        case LCD_DIGITAL_TTL:
            set_pll_ttl(pConf);
            venc_set_ttl(pConf);
			set_tcon_ttl(pConf);    
            break;
        case LCD_DIGITAL_LVDS:
        	set_pll_lvds(pConf);        	
            venc_set_lvds(pConf);        	
        	set_control_lvds(pConf);        	
        	init_lvds_phy(pConf);
			set_tcon_lvds(pConf);  	
            break;
        case LCD_DIGITAL_MINILVDS:
			set_pll_mlvds(pConf);
			venc_set_mlvds(pConf);
			set_control_mlvds(pConf);
			init_lvds_phy(pConf);
			set_tcon_mlvds(pConf);  	
            break;
        default:
            printk("Not valid LCD type.\n");
			break;
    }
}

static inline void _enable_vsync_interrupt(void)
{
    if ((aml_read_reg32(P_ENCT_VIDEO_EN) & 1) || (aml_read_reg32(P_ENCL_VIDEO_EN) & 1)) {
        aml_write_reg32(P_VENC_INTCTRL, 0x200);
#if 0
        while ((aml_read_reg32(P_VENC_INTFLAG) & 0x200) == 0) {
            u32 line1, line2;

            line1 = line2 = aml_read_reg32(P_VENC_ENCP_LINE);

            while (line1 >= line2) {
                line2 = line1;
                line1 = aml_read_reg32(P_VENC_ENCP_LINE);
            }

            aml_read_reg32(P_VENC_INTFLAG);
            aml_read_reg32(P_VENC_INTFLAG);
            aml_read_reg32(P_VENC_INTFLAG);
            aml_read_reg32(P_VENC_INTFLAG);
            if (aml_read_reg32(P_VENC_INTFLAG) & 0x200) {
                break;
            }

            aml_write_reg32(P_ENCP_VIDEO_EN, 0);
            aml_read_reg32(P_VENC_INTFLAG);
            aml_read_reg32(P_VENC_INTFLAG);
            aml_read_reg32(P_VENC_INTFLAG);
            aml_read_reg32(P_VENC_INTFLAG);
            aml_read_reg32(P_VENC_INTFLAG);

            aml_write_reg32(P_ENCP_VIDEO_EN, 1);
            aml_read_reg32(P_VENC_INTFLAG);
            aml_read_reg32(P_VENC_INTFLAG);
            aml_read_reg32(P_VENC_INTFLAG);
            aml_read_reg32(P_VENC_INTFLAG);
            aml_read_reg32(P_VENC_INTFLAG);
        }
#endif
    }
    else{
        aml_write_reg32(P_VENC_INTCTRL, 0x2);
    }
}
static void _enable_backlight(u32 brightness_level)
{
    pDev->conf.lcd_power_ctrl.backlight_ctrl?pDev->conf.lcd_power_ctrl.backlight_ctrl(ON):0;
}
static void _disable_backlight(void)
{
    pDev->conf.lcd_power_ctrl.backlight_ctrl?pDev->conf.lcd_power_ctrl.backlight_ctrl(OFF):0;
}
static void _lcd_module_enable(void)
{
    BUG_ON(pDev==NULL);
    pDev->conf.lcd_power_ctrl.power_ctrl?pDev->conf.lcd_power_ctrl.power_ctrl(ON):0;

	_init_display_driver(&pDev->conf);	
    
    _enable_vsync_interrupt();
}

static const vinfo_t *lcd_get_current_info(void)
{
    return &pDev->lcd_info;
}

static int lcd_set_current_vmode(vmode_t mode)
{
    if (mode != VMODE_LCD)
        return -EINVAL;
#ifdef CONFIG_AM_TV_OUTPUT2
    vpp2_sel = 0;
#endif    
    aml_write_reg32(P_VPP_POSTBLEND_H_SIZE, pDev->lcd_info.width);
    _lcd_module_enable();
    if (VMODE_INIT_NULL == pDev->lcd_info.mode)
        pDev->lcd_info.mode = VMODE_LCD;
    _enable_backlight(BL_MAX_LEVEL);
    return 0;
}

#ifdef CONFIG_AM_TV_OUTPUT2
static int lcd_set_current_vmode2(vmode_t mode)
{
    if (mode != VMODE_LCD)
        return -EINVAL;
    vpp2_sel = 1;
    aml_write_reg32(P_VPP2_POSTBLEND_H_SIZE, pDev->lcd_info.width);
    _lcd_module_enable();
    if (VMODE_INIT_NULL == pDev->lcd_info.mode)
        pDev->lcd_info.mode = VMODE_LCD;
    _enable_backlight(BL_MAX_LEVEL);
    return 0;
}
#endif
static vmode_t lcd_validate_vmode(char *mode)
{
    if ((strncmp(mode, PANEL_NAME, strlen(PANEL_NAME))) == 0)
        return VMODE_LCD;
    
    return VMODE_MAX;
}
static int lcd_vmode_is_supported(vmode_t mode)
{
    mode&=VMODE_MODE_BIT_MASK;
    if(mode == VMODE_LCD )
    return true;
    return false;
}
static int lcd_module_disable(vmode_t cur_vmod)
{
    BUG_ON(pDev==NULL);
    _disable_backlight();
    pDev->conf.lcd_power_ctrl.power_ctrl?pDev->conf.lcd_power_ctrl.power_ctrl(OFF):0;
    
#ifdef CONFIG_ARCH_MESON6
    switch_mod_gate_by_name("tcon", 0);
    switch_mod_gate_by_name("lvds", 0);
#endif

    return 0;
}
#ifdef  CONFIG_PM
static int lcd_suspend(void)
{
    BUG_ON(pDev==NULL);
    PRINT_INFO("lcd_suspend \n");
    _disable_backlight();
    pDev->conf.lcd_power_ctrl.power_ctrl?pDev->conf.lcd_power_ctrl.power_ctrl(0):0;

    return 0;
}
static int lcd_resume(void)
{
    PRINT_INFO("lcd_resume\n");
    _lcd_module_enable();
    _enable_backlight(BL_MAX_LEVEL);
    return 0;
}
#endif
static vout_server_t lcd_vout_server={
    .name = "lcd_vout_server",
    .op = {    
        .get_vinfo = lcd_get_current_info,
        .set_vmode = lcd_set_current_vmode,
        .validate_vmode = lcd_validate_vmode,
        .vmode_is_supported=lcd_vmode_is_supported,
        .disable=lcd_module_disable,
#ifdef  CONFIG_PM  
        .vout_suspend=lcd_suspend,
        .vout_resume=lcd_resume,
#endif
    },
};

#ifdef CONFIG_AM_TV_OUTPUT2
static vout_server_t lcd_vout2_server={
    .name = "lcd_vout2_server",
    .op = {    
        .get_vinfo = lcd_get_current_info,
        .set_vmode = lcd_set_current_vmode2,
        .validate_vmode = lcd_validate_vmode,
        .vmode_is_supported=lcd_vmode_is_supported,
        .disable=lcd_module_disable,
#ifdef  CONFIG_PM  
        .vout_suspend=lcd_suspend,
        .vout_resume=lcd_resume,
#endif
    },
};
#endif
static void _init_vout(lcd_dev_t *pDev)
{		
    printk("ddr_pll_clk: %d\n", ddr_pll_clk);
    if (!ddr_pll_clk){
//\\        ddr_pll_clk = clk_util_clk_msr(CTS_DDR_CLK);
        printk("(CTS_DDR_CLK) = %ldMHz\n", ddr_pll_clk);
    }
    if (ddr_pll_clk==516) { //use ddr pll 
        pDev->conf.lcd_timing.clk_ctrl = 0x100e;
        pDev->conf.lcd_timing.sync_duration_num = 516;
    } else if (ddr_pll_clk==508) { //use ddr pll 
        pDev->conf.lcd_timing.clk_ctrl = 0x100e;
        pDev->conf.lcd_timing.sync_duration_num = 508;
    } else if (ddr_pll_clk==486){ //use ddr pll 
        pDev->conf.lcd_timing.clk_ctrl = 0x100f;
        pDev->conf.lcd_timing.sync_duration_num = 508;
    } else if (ddr_pll_clk==474){ //use ddr pll 
        pDev->conf.lcd_timing.clk_ctrl = 0x100d;
        pDev->conf.lcd_timing.sync_duration_num = 508;
    }
	
    pDev->lcd_info.name = PANEL_NAME;
    pDev->lcd_info.mode = VMODE_LCD;
    pDev->lcd_info.width = pDev->conf.lcd_basic.h_active;
    pDev->lcd_info.height = pDev->conf.lcd_basic.v_active;
    pDev->lcd_info.field_height = pDev->conf.lcd_basic.v_active;
    pDev->lcd_info.aspect_ratio_num = pDev->conf.lcd_basic.screen_ratio_width;
    pDev->lcd_info.aspect_ratio_den = pDev->conf.lcd_basic.screen_ratio_height;
    pDev->lcd_info.sync_duration_num = pDev->conf.lcd_timing.sync_duration_num;
    pDev->lcd_info.sync_duration_den = pDev->conf.lcd_timing.sync_duration_den;

    vout_register_server(&lcd_vout_server);
#ifdef CONFIG_AM_TV_OUTPUT2
   vout2_register_server(&lcd_vout2_server);
#endif   
}

static void _lcd_init(Lcd_Config_t *pConf)
{
    //logo_object_t  *init_logo_obj=NULL;
    
    _init_vout(pDev);
    //init_logo_obj = get_current_logo_obj();    
    //if(NULL==init_logo_obj ||!init_logo_obj->para.loaded)
        //_lcd_module_enable();
}
#include <mach/lcd_aml.h>
static int lcd_probe(struct platform_device *pdev)
{
    struct aml_lcd_platform *pdata;    

    pdata = pdev->dev.platform_data;
    pDev = (lcd_dev_t *)kmalloc(sizeof(lcd_dev_t), GFP_KERNEL);    

    if (!pDev) {
        PRINT_INFO("[tcon]: Not enough memory.\n");
        return -ENOMEM;
    }    

//    extern Lcd_Config_t m6skt_lcd_config;
//    pDev->conf = m6skt_lcd_config;
    pDev->conf = *(Lcd_Config_t *)(pdata->lcd_conf);        //*(Lcd_Config_t *)(s->start);    

    printk("LCD probe ok\n");    

    _lcd_init(&pDev->conf);    

    return 0;
}

static int lcd_remove(struct platform_device *pdev)
{
    kfree(pDev);
    
    return 0;
}

static struct platform_driver lcd_driver = {
    .probe      = lcd_probe,
    .remove     = lcd_remove,
    .driver     = {
        .name   = "mesonlcd",    // removed "tcon-dev"
    }
};

static int __init lcd_init(void)
{
    printk("LCD driver init\n");
    if (platform_driver_register(&lcd_driver)) {
        PRINT_INFO("failed to register tcon driver module\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit lcd_exit(void)
{
    platform_driver_unregister(&lcd_driver);
}

subsys_initcall(lcd_init);
module_exit(lcd_exit);

MODULE_DESCRIPTION("Meson LCD Panel Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
