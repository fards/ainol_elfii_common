/*
 * Amlogic M2
 * HDMI RX
 * Copyright (C) 2010 Amlogic, Inc.
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
 */


#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
//#include <linux/amports/canvas.h>
#include <asm/uaccess.h>
#include <asm/delay.h>
#include <mach/am_regs.h>
#include <mach/power_gate.h>

#include <linux/tvin/tvin.h>
/* Local include */
#include "hdmirx.h"
#include "hdmi_regs.h"
#include "hdmirx_cec.h"

//#define FIFO_ENABLE_AFTER_RESET
#undef FIFO_ENABLE_AFTER_RESET
#undef FIFO_BYPASS
#undef HDMIRX_WITH_IRQ

#define RESET_AFTER_CLK_STABLE
#ifdef RESET_AFTER_CLK_STABLE
static unsigned char reset_flag = 0;
static unsigned char reset_mode = 0;
#endif

#define FOR_LS_DIG2090__DVP_5986K

#undef HPD_AUTO_MODE

#define HW_MONITOR_TIME_UNIT    (1000/HDMI_STATE_CHECK_FREQ)
/* parameters begin*/
int hdmirx_log_flag = 0x1;
static int sm_pause = 0;
static int force_vic = 0;
static int audio_enable = 1;
static int eq_enable = 1;
static int eq_config = 0x7;
static int signal_check_mode = 0xb; /*new config 0xb, check if have_avi_info; old config 7, check vic=0 */;
static int signal_recover_mode = 1; /*0x1, reset for all case ; 0x31, not reset for case of "VIC=0" */
static int switch_mode = 0;
static int edid_mode = 0; /*for internal edid, 0, 8 bit mode; 1, deep color mode */
static int dvi_mode = 0;
static int color_format_mode = 0; /* output color format: 0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422; other same as input color format */
static int check_color_depth_mode = 0;
    /*ms*/
static int delay1 = 20; //100;
static int delay2 = 20; //100;
static int delay3 = 200; //500;
static int delay4 = 500; //1000;
static int hpd_ready_time = 200;
static int hpd_start_time = 0;
static int audio_stable_time = 1000;
static int vendor_specific_info_check_period = 500;
    /**/
static int clk_stable_threshold = 20;
static int clk_unstable_threshold = 3;
static int reset_threshold = HDMI_STATE_CHECK_FREQ;
static int audio_sample_rate_stable_count_th = 15; //4;
static int audio_channel_status_stable_th = 4;

static int dvi_detect_wait_avi_th = 3*HDMI_STATE_CHECK_FREQ;
static int general_detect_wait_avi_th = 8*HDMI_STATE_CHECK_FREQ;

static int audio_check_period = 200; //ms
static int aud_channel_status_modify_th = 5;
static int aud_channel_status_all_0_th = 0; //set it to 0 to disable it, as if it is 5 , for some normal case, this condition will also be triggered //5;
static int sample_rate_change_th = 100;

static int powerdown_enable = 0;
static int skip_unstable_frame = 0;

static int eq_mode = 0;
static int eq_mode_max = 4;
    /*
    internal_mode:
        bit [15:0] reserved for internal flag
            bit[0]: 1, support full format; 0, support basic format
            bit[1]: 1, disable active_lines check
            bit[2]: 0, use audio_recover_clock as sample_rate; 1, use sample_rate from "audio info frame" or "channel status bits"
        bit [30:16] check value, should be 0x3456
        bit [31] not used
    */
#define INT_MODE_FULL_FORMAT                0x1
#define INT_MODE_DISABLE_LINES_CHECK        0x2
#define INT_MODE_USE_AUD_INFO_FRAME         0x4    
static int internal_mode = 0;

/* bit 0~3, for port 0; bit 4~7 for port 1, bit 8~11 for port 2: 0xf=disable; 0=PRBS 11; 1=PRBS 15; 2=PRBS 7; 3=PRBS 31 */
unsigned prbs_port_mode = 0xffffffff; 
int prbs_check_count = 0;
int prbs_ch1_err_num = 0;
int prbs_ch2_err_num = 0;
int prbs_ch3_err_num = 0;
/*parameters end*/
static int test_flag = 0;

static int msr_par1=23, msr_par2=50; //hdmi_pixel_clk
#define OTHER_CLK_INDEX     12
static unsigned long clk_util_clk_msr( unsigned long   clk_mux, unsigned long   uS_gate_time );
static int eq_mode_monitor(void);
static int internal_mode_monitor(void);
static unsigned char internal_mode_valid(void);


#define TMDS_CLK_FACTOR   91541
#define TMDS_CLK(measure_value)  (measure_value*TMDS_CLK_FACTOR/1000000)
#define PIXEL_CLK ((int)clk_util_clk_msr(msr_par1,msr_par2))
/****************
* register write/read
*
***************/
#define Wr(reg,val) WRITE_MPEG_REG(reg,val)
#define Rd(reg)   READ_MPEG_REG(reg)
#define Wr_reg_bits(reg, val, start, len) \
  Wr(reg, (Rd(reg) & ~(((1L<<(len))-1)<<(start)))|((unsigned int)(val) << (start)))

static void hdmi_wr_reg(unsigned long addr, unsigned long data);

static void hdmi_wr_only_reg(unsigned long addr, unsigned long data);

static unsigned int hdmi_rd_reg(unsigned long addr);

#if 1
#define HDMIRX_HW_LOG hdmirx_print
#else
int HDMIRX_HW_LOG(const char *fmt, ...)
{
    va_list args;
    if(hdmirx_log_flag&1){
        va_start(args, fmt);
        vprintk(fmt, args);
        va_end(args);
    }
    return 0;
}
#endif

static unsigned int hdmi_rd_reg(unsigned long addr)
{
    unsigned long data;
    WRITE_APB_REG(HDMI_ADDR_PORT, addr);
    WRITE_APB_REG(HDMI_ADDR_PORT, addr);

    data = READ_APB_REG(HDMI_DATA_PORT);

    return (data);
}


static void hdmi_wr_only_reg(unsigned long addr, unsigned long data)
{
    WRITE_APB_REG(HDMI_ADDR_PORT, addr);
    WRITE_APB_REG(HDMI_ADDR_PORT, addr);

    WRITE_APB_REG(HDMI_DATA_PORT, data);
}

static void hdmi_wr_reg(unsigned long addr, unsigned long data)
{
    unsigned long rd_data;

    WRITE_APB_REG(HDMI_ADDR_PORT, addr);
    WRITE_APB_REG(HDMI_ADDR_PORT, addr);

    WRITE_APB_REG(HDMI_DATA_PORT, data);
    rd_data = hdmi_rd_reg (addr);
    if (rd_data != data)
    {
       //while(1){};
    }
}

static unsigned int hdmi_wr_only_reg_bits(unsigned long addr, unsigned long val, unsigned int start, unsigned int len)
{
    unsigned int data;
    data  = hdmi_rd_reg(addr);
    data &= (~(((1<<len)-1)<<start));
    data |= ((val & ((1<<len)-1)) << start);
    hdmi_wr_reg(addr, data);
    return (data);
}

static void hdmi_wr_reg_bits(unsigned long addr, unsigned long val, unsigned int start, unsigned int len)
{
    unsigned long exp_data, act_data;
    exp_data = hdmi_wr_only_reg_bits(addr, val, start, len);
    act_data = hdmi_rd_reg (addr);
    if (exp_data != act_data)
    {
        printk("Error: (addr) ");
    }
}

/*
static void hdmi_rd_check_reg(unsigned long addr, unsigned long exp_data, unsigned long mask)
{
    unsigned long rd_data;
    rd_data = hdmi_rd_reg(addr);
    if ((rd_data | mask) != (exp_data | mask))
    {
        printk("Error: addr %x, rd_data %x, exp_data %x, mask %x\n", addr, rd_data, exp_data, mask);
    }
}*/
/****************
* init function
*
***************/
//static DEFINE_SPINLOCK(hdmi_print_lock);

//#define RX_INPUT_COLOR_DEPTH    1                   // Pixel bit width: 0=24-bit; 1=30-bit; 2=36-bit; 3=48-bit.
#define RX_INPUT_COLOR_DEPTH    0                   // Pixel bit width: 0=24-bit; 1=30-bit; 2=36-bit; 3=48-bit.
//#define RX_OUTPUT_COLOR_DEPTH   1                   // Pixel bit width: 0=24-bit; 1=30-bit; 2=36-bit; 3=48-bit.
#define RX_OUTPUT_COLOR_DEPTH   0                   // Pixel bit width: 0=24-bit; 1=30-bit; 2=36-bit; 3=48-bit.
#define RX_INPUT_COLOR_FORMAT   0                   // Pixel format: 0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422.
//#define RX_OUTPUT_COLOR_FORMAT  1                   // Pixel format: 0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422.
#define RX_OUTPUT_COLOR_FORMAT  0                   // Pixel format: 0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422.
#define RX_INPUT_COLOR_RANGE    0                   // Pixel range: 0=16-235/240; 1=16-240; 2=1-254; 3=0-255.
#define RX_OUTPUT_COLOR_RANGE   0                   // Pixel range: 0=16-235/240; 1=16-240; 2=1-254; 3=0-255.
#define RX_I2S_SPDIF        1                       // 0=SPDIF; 1=I2S.
#define RX_I2S_8_CHANNEL    0                       // 0=I2S 2-channel; 1=I2S 4 x 2-channel.

// For Audio Clock Recovery
#define ACR_CONTROL_EXT     1                       // Select which ACR scheme:
                                                    // 0=Old ACR that is partly inside hdmi_rx_core, and partly outside;
                                                    // 1=New ACR that is entirely outside hdmi_rx_core, under hdmi_rx_top.
#define ACR_MODE            3                       // Select input clock for either old or new ACR module:
                                                    // 0=idle; 1=external oscillator;
                                                    // 2=internal oscillator, fast; 3=internal oscillator, slow.


#define AUDIN_FIFO_DIN_SOURCE   1                  // Select whose decoded data are to be the input of AUDIN FIFO:
                                                    // 0=Use the original SPDIF decoder;
                                                    // 1=Use the original I2S decoder;
                                                    // 2=Use the new audio decoder which can decode either SPDIF or I2S.
                                                    // E.g. If RX_I2S_SPDIF=0, use either 0 or 2; If RX_I2S_SPDIF=1, use either 1 or 2.


// HDCP keys from Efuse are encrypted by default, in this test HDCP keys are written by CPU with encryption manually added
#define ENCRYPT_KEY                                 0xbe
//#define ENCRYPT_KEY                                 0

static void hdmi_init(void);

static void hdmirx_phy_init(void);
static void set_eq_27M(int idx1, int idx2);
static void set_eq_27M_2(void);
static void restore_eq_gen(void);
static void hdmirx_reset_clock(void);
static void hdmirx_audio_recover_reset(void);
static void hdmirx_reset_digital(void);
static void hdmirx_reset_pixel_clock(void);
static void hdmirx_unpack_recover(void);
static void hdmirx_release_audio_reset(void);
static unsigned char have_avi_info(void);

void task_rx_edid_setting(void);
//static void hdmi_tx_hpd_detect(void);
//static void hdmirx_monitor_reg_init(void);
static void set_hdmi_audio_source_gate(unsigned char);
//static void hdmirx_monitor_clock(void);
//static void hdimrx_monitor_register(void);
void task_rx_key_setting(void);
static unsigned char is_frame_packing(void);

#define DUMP_FLAG_VIDEO_TIMING      0x1
#define DUMP_FLAG_AVI_INFO          0x2
#define DUMP_FLAG_VENDOR_SPEC_INFO  0x4
#define DUMP_FLAG_AUDIO_INFO        0x8
#define DUMP_FLAG_CLOCK_INFO        0x10
#define DUMP_FLAG_ECC_STATUS        0x20
static void dump_state(unsigned int dump_flag);


#define   CC_ITU601     1
#define   CC_ITU709       2

// -----------------------------------------------
// Global variables
// -----------------------------------------------


//static int monitor_register(void *data);
// Convertion: RGB_YUV mapping is different from HDMI spec's AVI infoFrame mapping
#define conv_color_format_to_spec_value(a) (((a)==0)? 0 : (((a)==1)? 2 : (((a)==2)? 3 : 1)))
#define conv_color_format_to_rtl_value(a) (((a)==0)? 0 : (((a)==1)? 3 : (((a)==2)? 1 : 2)))

#define HDMIRX_HWSTATE_5V_LOW             0
#define HDMIRX_HWSTATE_5V_HIGH            1
#define HDMIRX_HWSTATE_HPD_READY          2
#define HDMIRX_HWSTATE_SIG_UNSTABLE        3
#define HDMIRX_HWSTATE_SIG_STABLE          4
#define HDMIRX_HWSTATE_INIT                5

struct video_status_s{
    unsigned int active_pixels;
    unsigned int front_pixels;
    unsigned int hsync_pixels;
    unsigned int back_pixels;
    unsigned int active_lines;
    unsigned int eof_lines;
    unsigned int vsync_lines;
    unsigned int sof_lines;
    unsigned char video_scan; /* 0, progressive; 1, interlaced */
    unsigned char video_field;    /* progressive:0; interlace: 0, 1st; 1, 2nd */
    /**/
    unsigned char scan_stable; /* 0, not stable; 1, stable */
    unsigned char lines_stable;
    unsigned char vsync_stable;
    unsigned char pixels_stable;
    unsigned char hsync_stable;
    /**/
    unsigned char default_phase;
    unsigned char pixel_phase;
    unsigned char pixel_phase_ok;
    unsigned char gc_error; /* 0, no error; 1, error in packet data */
    unsigned char color_depth; /* 0, 24bit; 1, 30bit; 2, 36 bit; 3, 48 bit */
};

struct avi_info_s{
    unsigned int             vic;
    unsigned int             pixel_repeat;
    unsigned int             color_format;
    unsigned int             color_range;
    unsigned int             cc;
};

#define CHANNEL_STATUS_SIZE   24
struct aud_info_s{
    /* info frame*/
    unsigned char cc;
    unsigned char ct;
    unsigned char ss;
    unsigned char sf;
    /* channel status */
    unsigned char channel_status[CHANNEL_STATUS_SIZE];
    unsigned char channel_status_bak[CHANNEL_STATUS_SIZE];
    /**/
    unsigned int cts;
    unsigned int n;
    unsigned int audio_recovery_clock;
    /**/
    int channel_num;
    int sample_rate;
    int sample_size;
    int real_sample_rate;
};

struct vendor_specific_info_s{
    unsigned identifier;
    unsigned char hdmi_video_format;
    unsigned char _3d_structure;
    unsigned char _3d_ext_data;
};
#define TMDS_CLK_HIS_SIZE 32

typedef struct hdmirx_hw_stru_{
    unsigned int vendor_specific_info_check_time;
    unsigned int clock_monitor_time;
    int hpd_wait_time;
    int audio_wait_time;
    int audio_sample_rate_stable_count;
    unsigned int audio_reset_release_flag;
    unsigned int phy_init_flag;
    unsigned int state;
    unsigned int clk_stable_count;
    /**/
    unsigned int port; /*1, 2, 4 */
    unsigned int cur_pixel_repeat;
    unsigned int cur_vic;
    unsigned int guess_vic;
    unsigned int cur_color_format; /* ouput color format*/
    unsigned int cur_color_depth;
    unsigned int tmds_clk[TMDS_CLK_HIS_SIZE];
    unsigned int pixel_clock;
    unsigned int unstable_irq_count;
    struct video_status_s video_status;
    struct video_status_s video_status_pre;
    struct avi_info_s avi_info;
    struct avi_info_s avi_info_pre;
    struct aud_info_s aud_info;
#ifdef FOR_LS_DIG2090__DVP_5986K
    int audio_check_time;
    int aud_channel_status_modify_count;
    int aud_channel_status_unmodify_count;
    int aud_channel_status_all_0_count;
    int aud_buf_ptr_change_count;
    int aud_ok_flag;
#endif
    struct vendor_specific_info_s vendor_specific_info;
    struct vendor_specific_info_s vendor_specific_info_pre;
    unsigned int avi_info_change_flag;

    unsigned char cur_eq_mode;
    unsigned int cur_internal_mode;
    
    /**/
    int prbs_check_wait_time;
    unsigned char prbs_enable;
    unsigned char prbs_mode;
}hdmirx_hw_stru_t;

static hdmirx_hw_stru_t hdmirx_hw_stru;

unsigned int hdmirx_get_cur_vic(void)
{
    if(force_vic){
        return force_vic;
    }
    else if(hdmirx_hw_stru.cur_vic == 0){
        return hdmirx_hw_stru.guess_vic;
    }
    return hdmirx_hw_stru.cur_vic;
}

#ifdef HDMIRX_WITH_IRQ
static irqreturn_t intr_handler(int irq, void *dev_instance)
{
    unsigned int data32;

    data32 = hdmi_rd_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT);
    //printk("[HDMIRX Irq] Interrupt status %x\n", data32);
    if (data32 & (1<< 8)) {
        if(hdmirx_log_flag&0x10000)
            HDMIRX_HW_LOG("[HDMIRX Irq] HDMI VIC Change Interrupt Process_Irq\n");
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  1 << 8);
    }

    if (data32 & (1 << 9)) { // COLOR_DEPTH
        if(hdmirx_log_flag&0x10000)
            HDMIRX_HW_LOG("[HDMIRX Irq] HDMI RX COLOR_DEPTH Interrupt Process_Irq\n");
        //clear COLOR_DEPTH interrupt in hdmi rx
        hdmi_wr_only_reg_bits(RX_VIC_COLOR_DEPTH, 1, 5, 1);
        hdmi_wr_only_reg_bits(RX_VIC_COLOR_DEPTH, 0, 5, 1);
        //clear COLOR_DEPTH interrupt in hdmi top module
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  1 << 9);
    } /* COLOR_DEPTH */

    if (data32 & (1 << 10)) { // TMDS_CLK_UNSTABLE
        //printk("[HDMIRX Irq] Error: HDMI TMDS_CLK_UNSTABLE Interrupt Process_Irq\n");
        //clear TMDS_CLK_UNSTABLE interrupt in hdmi top module
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  1 << 10);
        hdmirx_hw_stru.unstable_irq_count++;
    } /* TMDS_CLK_UNSTABLE */

    if (data32 & (1 << 11)) { // EDID_ADDR
        if(hdmirx_log_flag&0x10000)
            HDMIRX_HW_LOG("[HDMIRX Irq] HDMI EDID_ADDR Interrupt Process_Irq\n");
        //clear EDID_ADDR interrupt in hdmi rx
        hdmi_wr_only_reg_bits(RX_HDCP_EDID_CONFIG, 1, 1, 1);
        hdmi_wr_only_reg_bits(RX_HDCP_EDID_CONFIG, 0, 1, 1);
        //clear EDID_ADDR interrupt in hdmi top module
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  1 << 11);
    } /* EDID_ADDR */

    if (data32 & (1 << 12)) { // AVMUTE_UPDATE
        if(hdmirx_log_flag&0x10000)
            HDMIRX_HW_LOG("[HDMIRX Irq] HDMI AVMUTE_UPDATE Interrupt Process_Irq\n");
        //clear AVMUTE_UPDATE interrupt in hdmi rx
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_RX_PACKET_INTR_CLR, 1 << 0);
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_RX_PACKET_INTR_CLR, 0);
        //clear AVMUTE_UPDATE interrupt in hdmi top module
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  1 << 12);
    } /* AVMUTE_UPDATE */

    if (data32 & (1 << 13)) { // AVI_UPDATE
        if(hdmirx_log_flag&0x10000)
            HDMIRX_HW_LOG("[HDMIRX Irq] HDMI AVI_UPDATE Interrupt Process_Irq\n");
        //clear AVI_UPDATE interrupt in hdmi rx
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_RX_PACKET_INTR_CLR, 1 << 1);
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_RX_PACKET_INTR_CLR, 0);
        //clear AVI_UPDATE interrupt in hdmi top module
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  1 << 13);
    } /* AVI_UPDATE */

    if (data32 & (1 << 14)) { // AUDIO_INFO_UPDATE
        if(hdmirx_log_flag&0x10000)
            HDMIRX_HW_LOG("[HDMIRX Irq] HDMI AUDIO_INFO_UPDATE Interrupt Process_Irq\n");
        //clear AUDIO_INFO_UPDATE interrupt in hdmi rx
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_RX_PACKET_INTR_CLR, 1 << 2);
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_RX_PACKET_INTR_CLR, 0);
        //clear AUDIO_INFO_UPDATE interrupt in hdmi top module
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  1 << 14);
    } /* AUDIO_INFO_UPDATE */

    if (data32 & (7 << 15)) { // RX_5V_RISE
        if(hdmirx_log_flag&0x10000)
            HDMIRX_HW_LOG("[HDMIRX Irq] HDMI RX_5V_RISE Interrupt Process_Irq\n");
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  7 << 15); //clear RX_5V_RISE interrupt in hdmi top module
    } /* RX_5V_RISE */

    if (data32 & (7 << 18)) { // RX_5V_FALL
        if(hdmirx_log_flag&0x10000)
            HDMIRX_HW_LOG("[HDMIRX Irq] Error: HDMI RX_5V_FALL Interrupt Process_Irq\n");
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  7 << 18); //clear RX_5V_FALL interrupt in hdmi top module
    } /* RX_5V_FALL */

    if (data32 & ~(0x1fff00)) {
        if(hdmirx_log_flag&0x10000)
            HDMIRX_HW_LOG("[HDMIRX Irq] Error: Unkown HDMI RX Interrupt source Process_Irq\n");
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  data32); //clear unkown interrupt in hdmi top module
    }


    Wr(A9_0_IRQ_IN1_INTR_STAT_CLR, 1 << 24);  //clear hdmi_rx interrupt

    return IRQ_HANDLED;

}

static void hdmirx_setupirq(void)
{
   int r;
   r = request_irq(INT_HDMI_RX, &intr_handler,
                    IRQF_SHARED, "amhdmirx",
                    (void *)"amhdmirx");


    Rd(A9_0_IRQ_IN1_INTR_STAT_CLR);
    Wr(A9_0_IRQ_IN1_INTR_MASK, Rd(A9_0_IRQ_IN1_INTR_MASK)|(1 << 24));

}
#endif

static void hdmi_rx_prbs_init(unsigned int prbs_mode)
{
	unsigned int tmp_add_data;

    // PRBS control

    tmp_add_data  = 0;
    tmp_add_data |= prbs_mode   << 6; // [7:6] hdmi_prbs_ana_i0.prbs_mode[1:0]. 0=PRBS 11; 1=PRBS 15; 2=PRBS 7; 3=PRBS 32.
    tmp_add_data |= 3           << 4; // [5:4] hdmi_prbs_ana_i0.mode[1:0]
    tmp_add_data |= 0           << 3; // [3]   hdmi_prbs_ana_i0.clr_ber_meter
    tmp_add_data |= 0           << 2; // [2]   hdmi_prbs_ana_i0.freeze_ber
    tmp_add_data |= 0           << 1; // [1]   hdmi_prbs_ana_i0.inverse_in
    tmp_add_data |= 0           << 0; // [0]   hdmi_prbs_ana_i0.Rsrv
    hdmi_wr_reg(RX_SYS2_PRBS0_CNTRL, tmp_add_data);

    tmp_add_data  = 0;
    tmp_add_data |= prbs_mode   << 6; // [7:6] hdmi_prbs_ana_i1.prbs_mode[1:0]. 0=PRBS 11; 1=PRBS 15; 2=PRBS 7; 3=PRBS 32.
    tmp_add_data |= 3           << 4; // [5:4] hdmi_prbs_ana_i1.mode[1:0]
    tmp_add_data |= 0           << 3; // [3]   hdmi_prbs_ana_i1.clr_ber_meter
    tmp_add_data |= 0           << 2; // [2]   hdmi_prbs_ana_i1.freeze_ber
    tmp_add_data |= 1           << 1; // [1]   hdmi_prbs_ana_i1.inverse_in
    tmp_add_data |= 0           << 0; // [0]   hdmi_prbs_ana_i1.Rsrv
    hdmi_wr_reg(RX_SYS2_PRBS1_CNTRL, tmp_add_data);

    tmp_add_data  = 0;
    tmp_add_data |= prbs_mode   << 6; // [7:6] hdmi_prbs_ana_i2.prbs_mode[1:0]. 0=PRBS 11; 1=PRBS 15; 2=PRBS 7; 3=PRBS 32.
    tmp_add_data |= 3           << 4; // [5:4] hdmi_prbs_ana_i2.mode[1:0]
    tmp_add_data |= 0           << 3; // [3]   hdmi_prbs_ana_i2.clr_ber_meter
    tmp_add_data |= 0           << 2; // [2]   hdmi_prbs_ana_i2.freeze_ber
    tmp_add_data |= 1           << 1; // [1]   hdmi_prbs_ana_i2.inverse_in
    tmp_add_data |= 0           << 0; // [0]   hdmi_prbs_ana_i2.Rsrv
    hdmi_wr_reg(RX_SYS2_PRBS2_CNTRL, tmp_add_data);

    tmp_add_data  = 0;
    tmp_add_data |= prbs_mode   << 6; // [7:6] hdmi_prbs_ana_i3.prbs_mode[1:0]. 0=PRBS 11; 1=PRBS 15; 2=PRBS 7; 3=PRBS 32.
    tmp_add_data |= 3           << 4; // [5:4] hdmi_prbs_ana_i3.mode[1:0]
    tmp_add_data |= 0           << 3; // [3]   hdmi_prbs_ana_i3.clr_ber_meter
    tmp_add_data |= 0           << 2; // [2]   hdmi_prbs_ana_i3.freeze_ber
    tmp_add_data |= 1           << 1; // [1]   hdmi_prbs_ana_i3.inverse_in
    tmp_add_data |= 0           << 0; // [0]   hdmi_prbs_ana_i3.Rsrv
    hdmi_wr_reg(RX_SYS2_PRBS3_CNTRL, tmp_add_data);

    hdmi_wr_reg(RX_SYS2_PRBS_ERR_THR, 0x00); // prbs_err_thr[7:0]

    hdmi_wr_reg(RX_SYS2_PRBS_TIME_WINDOW0, 0xff); // prbs_time_window[7:0]
} /* hdmi_rx_prbs_init */

static void turn_on_prbs_mode(int prbs_mode)
{
    hdmirx_hw_stru.prbs_mode = prbs_mode;
    /* reset tmds_config */
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(1<<6));
    mdelay(10);
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~(1<<6)));
  	/**/
  	hdmi_rx_prbs_init(hdmirx_hw_stru.prbs_mode);
  	mdelay(10);

    prbs_ch1_err_num = 0;
    prbs_ch2_err_num = 0;
    prbs_ch3_err_num = 0;
    prbs_check_count = 0;

    hdmirx_hw_stru.prbs_enable = 1;   
    printk("enable prbs mode %d\n", hdmirx_hw_stru.prbs_mode);
}

static void hdmi_rx_prbs_detect(unsigned int prbs_mode)
{
    int j;
	unsigned int tmp_add_data;
	unsigned int rd_data;
    unsigned char prbs_ber_ok[4];
    // --------------------------------------------------------
    // Check channel 0-3 BER result
    // --------------------------------------------------------
    for (j = 0; j <= 3; j ++) {
        //stimulus_print("[TEST.C] Check channel PRBS results\n");
        prbs_ber_ok[j] = 0;
        // Select to view channel 0's BER result
        tmp_add_data  = 0;
        tmp_add_data |= 0   << 8; // [7:6] Rsrv
        tmp_add_data |= j   << 4; // [5:4] prbs_status_select[1:0]
        tmp_add_data |= 0   << 0; // [3:0] prbs_time_window[11:8]
        hdmi_wr_reg(RX_SYS2_PRBS_TIME_WINDOW1, tmp_add_data);

        // Freeze BER result
        tmp_add_data  = 0;
        tmp_add_data |= prbs_mode           << 6; // [7:6] hdmi_prbs_ana_i(0-3).prbs_mode[1:0]. 0=PRBS 11; 1=PRBS 15; 2=PRBS 7; 3=PRBS 32.
        tmp_add_data |= 3                   << 4; // [5:4] hdmi_prbs_ana_i(0-3).mode[1:0]
        tmp_add_data |= 0                   << 3; // [3]   hdmi_prbs_ana_i(0-3).clr_ber_meter
        tmp_add_data |= 1                   << 2; // [2]   hdmi_prbs_ana_i(0-3).freeze_ber
        tmp_add_data |= ((j == 0)? 0 : 1)   << 1; // [1]   hdmi_prbs_ana_i(0-3).inverse_in
        tmp_add_data |= 0                   << 0; // [0]   hdmi_prbs_ana_i(0-3).Rsrv
        hdmi_wr_reg(RX_SYS2_PRBS0_CNTRL+j, tmp_add_data);

        //if(hdmi_rd_reg(RX_SYSST_EXT_PRBS_BER_METER_0) != 0x00){
            printk("CH%d:RX_SYSST_EXT_PRBS_BER_METER_0 = %x\n", j, hdmi_rd_reg(RX_SYSST_EXT_PRBS_BER_METER_0));
        //}
        //if(hdmi_rd_reg(RX_SYSST_EXT_PRBS_BER_METER_1) != 0x00){
            printk("CH%d:RX_SYSST_EXT_PRBS_BER_METER_1 = %x\n", j, hdmi_rd_reg(RX_SYSST_EXT_PRBS_BER_METER_1));
        //}
        //if(hdmi_rd_reg(RX_SYSST_EXT_PRBS_BER_METER_2) != 0x00){
            printk("CH%d:RX_SYSST_EXT_PRBS_BER_METER_2 = %x\n", j, hdmi_rd_reg(RX_SYSST_EXT_PRBS_BER_METER_2));
        //}
        if((hdmi_rd_reg(RX_SYSST_EXT_PRBS_BER_METER_0)==0)&&
            (hdmi_rd_reg(RX_SYSST_EXT_PRBS_BER_METER_1)==0)&&
            (hdmi_rd_reg(RX_SYSST_EXT_PRBS_BER_METER_2)==0)){
            prbs_ber_ok[j] = 1;
        }
        
        // Un-freeze BER result
        tmp_add_data  = 0;
        tmp_add_data |= prbs_mode           << 6; // [7:6] hdmi_prbs_ana_i(0-3).prbs_mode[1:0]. 0=PRBS 11; 1=PRBS 15; 2=PRBS 7; 3=PRBS 32.
        tmp_add_data |= 3                   << 4; // [5:4] hdmi_prbs_ana_i(0-3).mode[1:0]
        tmp_add_data |= 1                   << 3; // [3]   hdmi_prbs_ana_i(0-3).clr_ber_meter
        tmp_add_data |= 0                   << 2; // [2]   hdmi_prbs_ana_i(0-3).freeze_ber
        tmp_add_data |= ((j == 0)? 0 : 1)   << 1; // [1]   hdmi_prbs_ana_i(0-3).inverse_in
        tmp_add_data |= 0                   << 0; // [0]   hdmi_prbs_ana_i(0-3).Rsrv
        hdmi_wr_reg(RX_SYS2_PRBS0_CNTRL+j, tmp_add_data);

        hdmi_wr_reg(RX_SYS2_PRBS0_CNTRL+j, hdmi_rd_reg(RX_SYS2_PRBS0_CNTRL+j)&(~(1<<3))); //clear clr_ber_meter

    }

    // --------------------------------------------------------
    // Check all channels' PRBS status
    // --------------------------------------------------------
    //stimulus_print("[TEST.C] Check all channels' PRBS status\n");
    tmp_add_data  = 0;
    tmp_add_data |= 0   << 7; // [7]   hdmi_prbs_ana_i3.prbs_pattern_not_ok
    tmp_add_data |= 1   << 6; // [6]   hdmi_prbs_ana_i3.prbs_lock
    tmp_add_data |= 0   << 5; // [5]   hdmi_prbs_ana_i2.prbs_pattern_not_ok
    tmp_add_data |= 1   << 4; // [4]   hdmi_prbs_ana_i2.prbs_lock
    tmp_add_data |= 0   << 3; // [3]   hdmi_prbs_ana_i1.prbs_pattern_not_ok
    tmp_add_data |= 1   << 2; // [2]   hdmi_prbs_ana_i1.prbs_lock
    tmp_add_data |= 1   << 1; // [1]   hdmi_prbs_ana_i0.prbs_pattern_not_ok
    tmp_add_data |= 0   << 0; // [0]   hdmi_prbs_ana_i0.prbs_lock

    rd_data = hdmi_rd_reg(RX_SYSST_EXT_PRBS_STATUS);
    //if(rd_data!=tmp_add_data){
        printk("RX_SYSST_EXT_PRBS_STATUS = %x, CH1 %s, CH2 %s, CH3 %s\n", rd_data,
                (((rd_data>>2)&3)==0x1)?"ok ":"err",
                (((rd_data>>4)&3)==0x1)?"ok ":"err",
                (((rd_data>>6)&3)==0x1)?"ok ":"err"
                );
                
    if((((rd_data>>2)&3)!=0x1)||
        (prbs_ber_ok[1]==0))
        prbs_ch1_err_num++;
    if((((rd_data>>4)&3)!=0x1)||    
        (prbs_ber_ok[2]==0))
        prbs_ch2_err_num++;
    if((((rd_data>>6)&3)!=0x1)||
        (prbs_ber_ok[3]==0))
        prbs_ch3_err_num++;
    prbs_check_count++;
    //}
} /* hdmi_rx_prbs_detect */


void hdmirx_hw_init(tvin_port_t port)
{
    //unsigned long flags;
    //hdmi_sys_clk = clk_util_clk_msr( OTHER_CLK_INDEX, 50 )/((Rd(HHI_HDMI_CLK_CNTL)&0x7f)+1);

    cec_set_pending(TV_CEC_PENDING_ON);

    HDMIRX_HW_LOG("[HDMIRX] hdmirx_hw_init(%d)\n", port);

    memset(&hdmirx_hw_stru, 0, sizeof(hdmirx_hw_stru));

    internal_mode_monitor();
    switch (port)
    {
        case TVIN_PORT_HDMI1:
            hdmirx_hw_stru.port = 0x2;
            break;

        case TVIN_PORT_HDMI2:
            hdmirx_hw_stru.port = 0x4;
            break;
        default:
            hdmirx_hw_stru.port = 0x1;
            break;
    }
#if 1
/* reset the whole hdmi module */
    Wr(RESET2_REGISTER, Rd(RESET2_REGISTER)|(1<<15));
    mdelay(10);
    Wr(RESET2_REGISTER, Rd(RESET2_REGISTER)&(~(1<<15)));
#endif
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(3<<6));
    hdmi_init();
	
	/*cec config*/
    cec_init();
    cec_set_pending(TV_CEC_PENDING_OFF);

    //hdmirx_monitor_reg_init();

#ifdef HDMIRX_WITH_IRQ
    hdmirx_setupirq();
#endif

    return;
}

static void hdmirx_config_color_depth(unsigned int color_depth)
{
    if(color_depth!=hdmirx_hw_stru.cur_color_depth){
        hdmi_wr_only_reg(RX_BASE_ADDR+0x865,(hdmi_rd_reg(RX_BASE_ADDR+0x865)&(~0x3))|color_depth );
        hdmi_wr_only_reg(RX_BASE_ADDR+0x86D,(hdmi_rd_reg(RX_BASE_ADDR+0x86D)&(~0x3))|color_depth );
        hdmi_wr_only_reg(RX_BASE_ADDR+0x875,(hdmi_rd_reg(RX_BASE_ADDR+0x875)&(~0x3))|color_depth );
        hdmi_wr_only_reg(RX_BASE_ADDR+0x87D,(hdmi_rd_reg(RX_BASE_ADDR+0x87D)&(~0x3))|color_depth );

        hdmi_wr_reg(RX_VIDEO_DTV_MODE, (hdmi_rd_reg(RX_VIDEO_DTV_MODE)&(~0x3))|color_depth);
        hdmi_wr_reg(RX_VIDEO_DTV_OPTION_L, (hdmi_rd_reg(RX_VIDEO_DTV_OPTION_L)&(~0x3))|color_depth);

        HDMIRX_HW_LOG("[HDMIRX] config color depth %d\n", color_depth);
        hdmirx_hw_stru.cur_color_depth = color_depth;
    }
}

static void hdmirx_config_color(void)
{
    if(hdmirx_hw_stru.avi_info.cc == CC_ITU709){
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_B0, 0x7b);
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_B1, 0x12);
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_R0, 0x6c);
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_R1, 0x36);

        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_CB0, 0xf2);
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_CB1, 0x2f);
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_CR0, 0xd4);
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_CR1, 0x77);
    }
    else{
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_B0, 0x2f);
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_B1, 0x1d);
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_R0, 0x8b);
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_R1, 0x4c);

        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_CB0, 0x18);
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_CB1, 0x58);
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_CR0, 0xd0);
        hdmi_wr_reg(RX_VIDEO_CSC_COEFF_CR1, 0xb6);
    }

    hdmi_wr_reg(RX_VIDEO_DTV_OPTION_L, (hdmi_rd_reg(RX_VIDEO_DTV_OPTION_L)&(~(3<<4)))|(conv_color_format_to_rtl_value(hdmirx_hw_stru.avi_info.color_format)<<4)); //input color format

    hdmi_wr_reg(RX_VIDEO_DTV_OPTION_L, (hdmi_rd_reg(RX_VIDEO_DTV_OPTION_L)&(~(3<<6)))|(conv_color_format_to_rtl_value(hdmirx_hw_stru.cur_color_format)<<6)); //output color format

    hdmi_wr_reg_bits(RX_VIDEO_DTV_OPTION_H, (hdmirx_hw_stru.avi_info.color_range==2)?3:0, 0, 2); // [1:0] input_color_range:   0=16-235/240; 1=16-240; 2=1-254; 3=0-255.

    hdmi_wr_reg(RX_VIDEO_PROC_CONFIG0,(hdmi_rd_reg(RX_VIDEO_PROC_CONFIG0)&(~(0xf<<4)))|( hdmirx_hw_stru.avi_info.pixel_repeat << 4));

}

typedef struct{
    unsigned int vic;
#ifdef GET_COLOR_DEPTH_WORK_AROUND
    unsigned char frame_packing_flag;
#endif
    unsigned char vesa_format;
    unsigned int ref_freq; /* 8 bit tmds clock */
    unsigned int active_pixels;
    unsigned int active_lines;
    unsigned int active_lines_fp;
}freq_ref_t;

/*
format_en0[bit 0] is for vic1, bit1 is for vic2, ...
format_en1[bit 0] is for vic33, bit2 is for vic34, ...
format_en2[bit 0] is for vic65, bit2 is for vic66, ...
*/

static int format_en0_default = 0xc03f807f;
static int format_en1_default = 0x00000003;
static int format_en2_default = 0x00000447;
static int format_en3_default = 0x00000000;

static int format_en0 = 0xc03f807f;
static int format_en1 = 0x00000003;
static int format_en2 = 0x00000447;
static int format_en3 = 0x00000000;

static freq_ref_t freq_ref[]=
{
/* basic format*/
	{HDMI_640x480p60, 0, 25000, 640, 480, 480},
	{HDMI_480p60, 0, 27000, 720, 480, 1005},
    {HDMI_480p60_16x9, 0, 27000, 720, 480, 1005},
    {HDMI_480i60, 0, 27000, 1440, 240, 240},
    {HDMI_480i60_16x9, 0, 27000, 1440, 240, 240},
    {HDMI_576p50, 0, 27000, 720, 576, 1201},
    {HDMI_576p50_16x9, 0, 27000, 720, 576, 1201},
    {HDMI_576i50, 0, 27000, 1440, 288, 288},
    {HDMI_576i50_16x9, 0, 27000, 1440, 288, 288},
    {HDMI_720p60, 0, 74250, 1280, 720, 1470},
    {HDMI_720p50, 0, 74250, 1280, 720, 1470},
    {HDMI_1080i60, 0, 74250, 1920, 540, 2228},
    {HDMI_1080i50, 0, 74250, 1920, 540, 2228},
    {HDMI_1080p24, 0, 74250, 1920, 1080, 2205},
    {HDMI_1080p30, 0, 74250, 1920, 1080, 2205},
    {HDMI_1080p60, 0, 148500, 1920, 1080, 1080},
    {HDMI_1080p50, 0, 148500, 1920, 1080, 1080},
/* extend format */
	{HDMI_1440x240p60, 0, 27000, 1440, 240, 240}, //vic 8
	{HDMI_1440x240p60_16x9, 0, 27000, 1440, 240, 240}, //vic 9
	{HDMI_2880x480i60, 0, 54000, 2880, 240, 240}, //vic 10
	{HDMI_2880x480i60_16x9, 0, 54000, 2880, 240, 240}, //vic 11
	{HDMI_2880x240p60, 0, 54000, 2880, 240, 240}, //vic 12
	{HDMI_2880x240p60_16x9, 0, 54000, 2880, 240, 240}, //vic 13
	{HDMI_1440x480p60, 0, 54000, 1440, 480, 480}, //vic 14
	{HDMI_1440x480p60_16x9, 0, 54000, 1440, 480, 480}, //vic 15

	{HDMI_1440x288p50, 0, 27000, 1440, 288, 288}, //vic 23
	{HDMI_1440x288p50_16x9, 0, 27000, 1440, 288, 288}, //vic 24
	{HDMI_2880x576i50, 0, 54000, 2880, 288, 288}, //vic 25
	{HDMI_2880x576i50_16x9, 0, 54000, 2880, 288, 288}, //vic 26
	{HDMI_2880x288p50, 0, 54000, 2880, 288, 288}, //vic 27
	{HDMI_2880x288p50_16x9, 0, 54000, 2880, 288, 288}, //vic 28
	{HDMI_1440x576p50, 0, 54000, 1440, 576, 576}, //vic 29
	{HDMI_1440x576p50_16x9, 0, 54000, 1440, 576, 576}, //vic 30

	{HDMI_2880x480p60, 0, 108000, 2880, 480, 480}, //vic 35
	{HDMI_2880x480p60_16x9, 0, 108000, 2880, 480, 480}, //vic 36
	{HDMI_2880x576p50, 0, 108000, 2880, 576, 576}, //vic 37
	{HDMI_2880x576p50_16x9, 0, 108000, 2880, 576, 576}, //vic 38
	{HDMI_1080i50_1250, 0, 72000, 1920, 540, 540}, //vic 39

/* vesa format*/
	{HDMI_800_600, 1, 0, 800, 600, 600},
	{HDMI_1024_768, 1, 0, 1024, 768, 768},
  {HDMI_720_400,  1, 0, 720, 400, 400},
	{HDMI_1280_768, 1, 0, 1280, 768, 768},
	{HDMI_1280_800, 1, 0, 1280, 800, 800},
	{HDMI_1280_960, 1, 0, 1280, 960, 960},
	{HDMI_1280_1024, 1, 0, 1280, 1024, 1024},
	{HDMI_1360_768, 1, 0, 1360, 768, 768},
	{HDMI_1366_768, 1, 0, 1366, 768, 768},
	{HDMI_1600_1200, 1, 0, 1600, 1200, 1200},
	{HDMI_1920_1200, 1, 0, 1920, 1200, 1200},

	/* for AG-506 */
	{HDMI_480p60, 0, 27000, 720, 483, 483},
    {0, 0, 0}
};

static unsigned int get_vic_from_timing(void)
{
    int i;
    for(i = 0; freq_ref[i].vic; i++){
        if((hdmirx_hw_stru.video_status.active_pixels == freq_ref[i].active_pixels)&&
            ((hdmirx_hw_stru.video_status.active_lines == freq_ref[i].active_lines)||
            (hdmirx_hw_stru.video_status.active_lines == freq_ref[i].active_lines_fp))){
            break;
        }
    }
    return freq_ref[i].vic;
}

static unsigned int get_freq(void)
{
    int i;
#if 1
    return PIXEL_CLK;
#else
    if(hdmirx_hw_stru.avi_info.vic){
        for(i = 0; freq_ref[i].vic; i++){
            if((hdmirx_hw_stru.avi_info.vic == freq_ref[i].vic)){
                break;
            }
        }
        return freq_ref[i].ref_freq*1000;
    }
#endif
    return 0;
}


static unsigned char is_vesa_format(void)
{
    int i;
    unsigned char ret = 0;
    for(i = 0; freq_ref[i].vic; i++){
        if((hdmirx_hw_stru.video_status.active_pixels == freq_ref[i].active_pixels)&&
            (hdmirx_hw_stru.video_status.active_lines == freq_ref[i].active_lines)&&
            (freq_ref[i].vesa_format!=0)
            ){
            break;
        }
    }
    if(freq_ref[i].vic!=0){
        ret = 1;
    }
    return ret;
}

static unsigned char is_tmds_clock_stable(int diff_threshold, int count_threshold)
{
    int i;
    unsigned char ret = 1;
    for(i = 1; i<count_threshold; i++){
        if(hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1] > hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1-i]){
            if((hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1]-hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1-i])>diff_threshold){
                ret = 0;
                break;
            }
        }
        else{
            if((hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1-i]-hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1])>diff_threshold){
                ret = 0;
                break;
            }
        }
    }
    return ret;
}

static unsigned char is_tmds_clock_unstable(int diff_threshold, int count_threshold)
{
    int i;
    unsigned char ret = 1;
    for(i = 0; i<(count_threshold-1); i++){
        if(hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-count_threshold] > hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1-i]){
            if((hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-count_threshold]-hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1-i])<diff_threshold){
                ret = 0;
                break;
            }
        }
        else{
            if((hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1-i]-hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-count_threshold])<diff_threshold){
                ret = 0;
                break;
            }
        }
    }
    return ret;
}

static unsigned char signal_not_ok(void)
{
    int i;
    int ret = 0;
    if((have_avi_info()==0)&&
        (signal_check_mode&8)){
        ret |= 0x8;
    }

    if(dvi_mode&1){
        if((hdmi_rd_reg(RX_BASE_ADDR+RX_TMDSST_HDMI_STATUS)&0x60)==0x20){
            HDMIRX_HW_LOG("[HDMIRX] DVI mode is detected\n\n");
            printk("DDDDDDDDDDDDDDDDDDDDDD\n");
            printk("VVVVVVVVVVVVVVVVVVVVVV\n");
            printk("IIIIIIIIIIIIIIIIIIIIII\n");
            ret &= (~0x8);
        }
    }
    if(dvi_detect_wait_avi_th){
        if(hdmirx_hw_stru.clk_stable_count > dvi_detect_wait_avi_th){
            if(is_vesa_format()){
                ret &= (~0x8);
            }
        }
    }
    if(general_detect_wait_avi_th){
        if(hdmirx_hw_stru.clk_stable_count > general_detect_wait_avi_th){
            if(get_vic_from_timing() != 0){
                ret &= (~0x8);
            }
        }
    }

    if((hdmirx_hw_stru.avi_info.vic == 0)&&
        (signal_check_mode&4)){
            ret |= 0x4;
    }
    if(signal_check_mode&2){
        if(signal_check_mode&4){
            for(i = 0; freq_ref[i].vic; i++){
                if(freq_ref[i].vic == hdmirx_hw_stru.avi_info.vic){
                    if(hdmirx_hw_stru.video_status.active_pixels != freq_ref[i].active_pixels){
                        ret |= 0x2;
                    }
                    if((hdmirx_hw_stru.video_status.active_lines != freq_ref[i].active_lines)&&
                        (hdmirx_hw_stru.video_status.active_lines != freq_ref[i].active_lines_fp)){
                        ret |= 0x2;
                    }

                    if(internal_mode_valid()&&(internal_mode&INT_MODE_DISABLE_LINES_CHECK)
                        &&(hdmirx_hw_stru.video_status.active_pixels == freq_ref[i].active_pixels)){
                        ret &= (~0x2);
                    }
                    break;
                }
            }
        }
        else{
            /* if we do not check vic=0, it is possible vic is 0 */
            if(get_vic_from_timing() == 0){
                ret |= 0x2;
            }
        }
    }
    if(signal_check_mode&1){
        if(hdmirx_hw_stru.video_status.pixel_phase_ok == 0){
            ret |= 0x1;
        }
    }
    return ret;
}

static unsigned char wait_signal_ok(void)
{
    static int count=0;
    unsigned char ret = signal_not_ok();
    unsigned char signal_recover_type = signal_recover_mode&0xf;
    unsigned char signal_recover_mask = (signal_recover_mode>>4)&0xf;
    if(ret){
        count++;
        if(((count>reset_threshold)&&(hdmirx_hw_stru.clk_stable_count < (10*HDMI_STATE_CHECK_FREQ)))||
            (count>(reset_threshold*3)))  // reset period is 3 seconds after 10 seconds
        {
            HDMIRX_HW_LOG("[HDMIRX]============signal not ok %d\n", ret);
            dump_state(DUMP_FLAG_VIDEO_TIMING|DUMP_FLAG_AVI_INFO|DUMP_FLAG_VENDOR_SPEC_INFO|DUMP_FLAG_CLOCK_INFO);
            if((signal_recover_mask==0)||(ret&signal_recover_mask)){
                if((eq_config&0x200)==0){
                    if(PIXEL_CLK<28000000){
                        set_eq_27M_2();
                    }
                    else{
                        restore_eq_gen();
                    }
                }                
                if(signal_recover_type == 1){
                    hdmirx_reset_clock();
                    HDMIRX_HW_LOG("[HDMIRX]============hdmirx_reset_clock\n\n");
                }
                else if(signal_recover_type == 2){
                    hdmirx_reset();
                    HDMIRX_HW_LOG("[HDMIRX]============hdmirx_reset\n\n");
                }
                else if(signal_recover_type == 3){
                    hdmirx_phy_init();
                    HDMIRX_HW_LOG("[HDMIRX]============hdmirx_phy_init\n\n");
                }
                else if(signal_recover_type == 4){
                    hdmirx_reset_pixel_clock();
                    HDMIRX_HW_LOG("[HDMIRX]============hdmirx_reset_pixel_clock\n\n");
                }
                else if(signal_recover_type == 5){
                    hdmirx_unpack_recover();
                    HDMIRX_HW_LOG("[HDMIRX]============hdmirx_unpack_recover\n\n");
                }
                else{
                    HDMIRX_HW_LOG("[HDMIRX]============\n\n");
                }
            }
            else{
                HDMIRX_HW_LOG("[HDMIRX]============\n\n");
            }
            count = 0;
        }
    }
    else{
        count=0;
    }
    return ret;
}

#ifdef GET_COLOR_DEPTH_WORK_AROUND
static unsigned char is_frame_packing_fmt(int vic);

static unsigned int check_color_depth(void)
{
    int i;
    unsigned int ret_color_depth = hdmirx_hw_stru.video_status.color_depth;
    unsigned int ref_freq_8, ref_freq_10, ref_freq_12, cur_freq;
    for(i = 0; freq_ref[i].vic; i++){
        if((freq_ref[i].vic == hdmirx_hw_stru.avi_info.vic)
            &&(is_frame_packing_fmt(hdmirx_hw_stru.avi_info.vic) == freq_ref[i].frame_packing_flag)){
                ref_freq_8 = freq_ref[i].ref_freq;
                ref_freq_10 = ref_freq_8*10/8;
                ref_freq_12 = ref_freq_8*12/8;
                cur_freq = TMDS_CLK(hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1]);
                if( cur_freq > ref_freq_12){
                   ret_color_depth = 2;
                }
                else if(cur_freq > ref_freq_10){
                    if((ref_freq_12-cur_freq)<(cur_freq-ref_freq_10))
                        ret_color_depth = 2;
                    else
                        ret_color_depth = 1;
                }
                else if(cur_freq > ref_freq_8){
                    if((ref_freq_10-cur_freq)<(cur_freq-ref_freq_8))
                        ret_color_depth = 1;
                    else
                        ret_color_depth = 0;
                }
                else{
                    ret_color_depth = 0;
                }
                if(ret_color_depth!=(hdmi_rd_reg(RX_VIDEO_DTV_MODE)&0x3)){
                    printk("index %d, cur_freq %d,ref_freq_12 %d, ref_freq_10 %d, ref_freq_8 %d, ret_color_depth %d\n",
                         i, cur_freq, ref_freq_12, ref_freq_10, ref_freq_8, ret_color_depth);
                }
                break;
        }
    }
    return ret_color_depth;

}

static unsigned int check_color_depth_b(void)
{
    if( hdmirx_hw_stru.video_status.gc_error||
          hdmirx_hw_stru.video_status.color_depth == 0 ){
        return 0;
    }
    else{
        return hdmirx_hw_stru.video_status.color_depth;
    }
}
#endif

static int get_color_depth(void)
{
    int color_depth = hdmirx_hw_stru.video_status.color_depth;
#ifdef GET_COLOR_DEPTH_WORK_AROUND
    if(check_color_depth_mode == 1){
        color_depth = check_color_depth_b();
    }
    else if(check_color_depth_mode == 2){
        color_depth = check_color_depth(); //work around code: detect wrong color depth
    }
    else if(check_color_depth_mode == 3){
        if(hdmirx_hw_stru.video_status.gc_error){
            color_depth = check_color_depth(); //work around code: detect wrong color depth
        }
        else{
            color_depth = check_color_depth_b();
        }
    }
#endif
    return color_depth;
}

static  void hdmirx_hw_enable_clock(void )
{

    // -----------------------------------------
    // HDMI (90Mhz)
    // -----------------------------------------
    //         .clk_div            ( hi_hdmi_clk_cntl[6:0] ),
    //         .clk_en             ( hi_hdmi_clk_cntl[8]   ),
    //         .clk_sel            ( hi_hdmi_clk_cntl[11:9]),
#ifdef _SUPPORT_CEC_TV_MASTER_
    Wr( HHI_HDMI_CLK_CNTL,  ((1 << 9)  |   // select "other" PLL
                             (1 << 8)  |   // Enable gated clock
                             (0x1D << 0)) );  // Divide the "other" PLL output by 30
#else
    Wr( HHI_HDMI_CLK_CNTL,  ((1 << 9)  |   // select "other" PLL
                             (1 << 8)  |   // Enable gated clock
                             (3 << 0)) );  // Divide the "other" PLL output by 4
#endif
}

static void hdmirx_hw_disable_clock(void)
{
    //Wr(HHI_HDMI_CLK_CNTL,  Rd(HHI_HDMI_CLK_CNTL)&(~(1<<8)));
}

void hdmirx_hw_enable(void)
{
    HDMIRX_HW_LOG("[HDMIRX] hdmirx_hw_enable()\n");
    // --------------------------------------------------------
    // Set Clocks
    // --------------------------------------------------------

    // --------------------------------------------------------
    // Program core_pin_mux to enable HDMI pins
    // --------------------------------------------------------
    Wr(PERIPHS_PIN_MUX_3, Rd(PERIPHS_PIN_MUX_3) | ((1 << 31)    | // pm_gpioX_5_hdmi_pwr0
                               (1 << 30)    | // pm_gpioX_6_hdmi_hpd0
                               (1 << 29)    | // pm_gpioX_7_hdmi_pwr1
                               (1 << 28)    | // pm_gpioX_8_hdmi_hpd1
                               (1 << 27)    | // pm_gpioX_9_hdmi_pwr2
                               (1 << 26)));    // pm_gpioX_10_hdmi_hpd2

    Wr(PERIPHS_PIN_MUX_6, Rd(PERIPHS_PIN_MUX_6) | ((1 << 22)    | // pm_gpioD_0_hdmi_cec
                               (1 << 21)    | // pm_gpioD_1_hdmi_sda0
                               (1 << 20)    | // pm_gpioD_2_hdmi_scl0
                               (1 << 19)    | // pm_gpioD_3_hdmi_sda1
                               (1 << 18)    | // pm_gpioD_4_hdmi_scl1
                               (1 << 17)    | // pm_gpioD_5_hdmi_sda2
                               (1 << 16)));    // pm_gpioD_6_hdmi_scl2

    hdmirx_hw_enable_clock();

}

static void phy_powerdown(unsigned char flag)
{
#if 1
    if((powerdown_enable==0)&&(flag==0)){
        return;
    }
    hdmi_wr_reg(RX_BASE_ADDR+0x864, hdmi_rd_reg(RX_BASE_ADDR+0x864)    | (1<<7)); // rx0_eq_pd = 1
    hdmi_wr_reg(RX_BASE_ADDR+0x86C, hdmi_rd_reg(RX_BASE_ADDR+0x86C)    | (1<<7)); // rx1_eq_pd = 1
    hdmi_wr_reg(RX_BASE_ADDR+0x874, hdmi_rd_reg(RX_BASE_ADDR+0x874)    | (1<<7)); // rx2_eq_pd = 1
    hdmi_wr_reg(RX_BASE_ADDR+0x87C, hdmi_rd_reg(RX_BASE_ADDR+0x87C)    | (1<<7)); // rx3_eq_pd = 1
    pr_info("hdmirx_hw: rx3_eq_pd\n");
    hdmi_wr_reg(RX_BASE_ADDR+0x866, hdmi_rd_reg(RX_BASE_ADDR+0x866)    | (1<<6)); // cdr0_pd = 1
    hdmi_wr_reg(RX_BASE_ADDR+0x86E, hdmi_rd_reg(RX_BASE_ADDR+0x86E)    | (1<<6)); // cdr1_pd = 1
    hdmi_wr_reg(RX_BASE_ADDR+0x876, hdmi_rd_reg(RX_BASE_ADDR+0x876)    | (1<<6)); // cdr2_pd = 1
    hdmi_wr_reg(RX_BASE_ADDR+0x87E, hdmi_rd_reg(RX_BASE_ADDR+0x87E)    | (1<<6)); // cdr3_pd = 1
    pr_info("hdmirx_hw: cdr3_pd\n");
    hdmi_wr_reg(RX_BASE_ADDR+0x005, 0<<0); // [2:0] port_en = 0

    hdmi_wr_reg(RX_BASE_ADDR+0x097, hdmi_rd_reg(RX_BASE_ADDR+0x097)|
                                (1<<7)| // rxref_pd = 1
                                (1<<6)| // rxref_pd_vgap = 1
                                (1<<5));// rxref_pd_op2 = 1
#else
/* it will block at hdmi_wr_reg(RX_BASE_ADDR+0x864, ...) for some board when unplug */
    printk("1\n");
    hdmi_wr_reg(RX_BASE_ADDR+0x097, hdmi_rd_reg(RX_BASE_ADDR+0x097)|
                                (1<<7)| // rxref_pd = 1
                                (1<<6)| // rxref_pd_vgap = 1
                                (1<<5));// rxref_pd_op2 = 1
    printk("2\n");
    hdmi_wr_reg(RX_BASE_ADDR+0x864, hdmi_rd_reg(RX_BASE_ADDR+0x864)    | (1<<7)); // rx0_eq_pd = 1
    printk("2.1\n");
    hdmi_wr_reg(RX_BASE_ADDR+0x86C, hdmi_rd_reg(RX_BASE_ADDR+0x86C)    | (1<<7)); // rx1_eq_pd = 1
    printk("2.2\n");
    hdmi_wr_reg(RX_BASE_ADDR+0x874, hdmi_rd_reg(RX_BASE_ADDR+0x874)    | (1<<7)); // rx2_eq_pd = 1
    printk("2.3\n");
    hdmi_wr_reg(RX_BASE_ADDR+0x87C, hdmi_rd_reg(RX_BASE_ADDR+0x87C)    | (1<<7)); // rx3_eq_pd = 1
    printk("3\n");

    hdmi_wr_reg(RX_BASE_ADDR+0x866, hdmi_rd_reg(RX_BASE_ADDR+0x866)    | (1<<6)); // cdr0_pd = 1
    printk("3.1\n");
    hdmi_wr_reg(RX_BASE_ADDR+0x86E, hdmi_rd_reg(RX_BASE_ADDR+0x86E)    | (1<<6)); // cdr1_pd = 1
    printk("3.2\n");
    hdmi_wr_reg(RX_BASE_ADDR+0x876, hdmi_rd_reg(RX_BASE_ADDR+0x876)    | (1<<6)); // cdr2_pd = 1
    printk("3.3\n");
    hdmi_wr_reg(RX_BASE_ADDR+0x87E, hdmi_rd_reg(RX_BASE_ADDR+0x87E)    | (1<<6)); // cdr3_pd = 1
    printk("4\n");
    hdmi_wr_reg(RX_BASE_ADDR+0x005, 0<<0); // [2:0] port_en = 0
    printk("5\n");
#endif
}

static void phy_poweron(void)
{
    hdmi_wr_reg(RX_BASE_ADDR+0x097, hdmi_rd_reg(RX_BASE_ADDR+0x097)&
                                (~((1<<7)|
                                (1<<6)|
                                (1<<5))));
    hdmi_wr_reg(RX_BASE_ADDR+0x864, hdmi_rd_reg(RX_BASE_ADDR+0x864)    & (~(1<<7)));
    hdmi_wr_reg(RX_BASE_ADDR+0x86C, hdmi_rd_reg(RX_BASE_ADDR+0x86C)    & (~(1<<7)));
    hdmi_wr_reg(RX_BASE_ADDR+0x874, hdmi_rd_reg(RX_BASE_ADDR+0x874)    & (~(1<<7)));
    hdmi_wr_reg(RX_BASE_ADDR+0x87C, hdmi_rd_reg(RX_BASE_ADDR+0x87C)    & (~(1<<7)));

    hdmi_wr_reg(RX_BASE_ADDR+0x866, hdmi_rd_reg(RX_BASE_ADDR+0x866)    & (~(1<<6)));
    hdmi_wr_reg(RX_BASE_ADDR+0x86E, hdmi_rd_reg(RX_BASE_ADDR+0x86E)    & (~(1<<6)));
    hdmi_wr_reg(RX_BASE_ADDR+0x876, hdmi_rd_reg(RX_BASE_ADDR+0x876)    & (~(1<<6)));
    hdmi_wr_reg(RX_BASE_ADDR+0x87E, hdmi_rd_reg(RX_BASE_ADDR+0x87E)    & (~(1<<6)));
    //hdmi_wr_reg(RX_BASE_ADDR+0x005, 0<<0); // [2:0] port_en = 0
    hdmi_wr_reg(RX_BASE_ADDR+0x005, hdmirx_hw_stru.port); // 0x04

}

void hdmirx_hw_disable(unsigned char flag)
{
    HDMIRX_HW_LOG("[HDMIRX] hdmirx_hw_disable()\n");
    set_hdmi_audio_source_gate(0);
    phy_powerdown(flag);
    pr_info("hdmirx_hw: phy_powerdown\n");
    hdmirx_hw_disable_clock();
    return;
}


void hdmirx_hw_uninit(void)
{
#ifdef HDMIRX_WITH_IRQ
    Rd(A9_0_IRQ_IN1_INTR_STAT_CLR);
    Wr(A9_0_IRQ_IN1_INTR_MASK, Rd(A9_0_IRQ_IN1_INTR_MASK)&(~(1 << 24)));
    free_irq(INT_HDMI_RX, (void *)"amhdmirx");
#endif
}

static void hdmi_init(void)
{
    unsigned int tmp_add_data;

    // Enable APB3 fail on error
    //*((volatile unsigned long *) HDMI_CTRL_PORT) |= (1 << 15);        //APB3 err_en
    WRITE_APB_REG(HDMI_CTRL_PORT, READ_APB_REG(HDMI_CTRL_PORT)|(1<<15)); //APB3 err_en

    // Mask AVI InfoFrame: only to interrupt on VIC, color format, pixel repetition
    hdmi_wr_reg(OTHER_BASE_ADDR + HDMI_OTHER_AVI_INTR_MASKN0, 0x7f0c0060);
    hdmi_wr_reg(OTHER_BASE_ADDR + HDMI_OTHER_AVI_INTR_MASKN1, 0x0000000f);
    // Mask Audio InfoFrame: only to interrupt on channel count, sample frequency
    hdmi_wr_reg(OTHER_BASE_ADDR + HDMI_OTHER_RX_AINFO_INTR_MASKN0, 0x00001c07);
    hdmi_wr_reg(OTHER_BASE_ADDR + HDMI_OTHER_RX_AINFO_INTR_MASKN1, 0x00000000);
    // Enable HDMI RX interrupts on:
    // [9] COLOR_DEPTH, [10] TMDS_CLK_UNSTABLE, [11] EDID_ADDR, [12] AVMUTE_UPDATE,
    // [13] AVI_UPDATE, [14] AUDIO_INFO_UPDATE, [17:15] RX_5V_RISE, [20:18] RX_5V_FALL
    //hdmi_wr_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_MASKN, 0x1ffe00);

    // Enable RX interrupts:
    hdmi_wr_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_MASKN, 0x1fff00);


    // SOFT_RESET [23:16]
    tmp_add_data  = 0;
    tmp_add_data |= 0 << 7; // [7]   ~rx_i2s_config_rstn
    tmp_add_data |= 0 << 6; // [6]   rsvd
    tmp_add_data |= 3 << 4; // [5:4] rx_acr_rst_config[1:0]
    tmp_add_data |= 0 << 3; // [3]   ~rx_config_eye_rstn_ch3
    tmp_add_data |= 0 << 2; // [2]   ~rx_config_eye_rstn_ch2
    tmp_add_data |= 0 << 1; // [1]   ~rx_config_eye_rstn_ch1
    tmp_add_data |= 0 << 0; // [0]   ~rx_config_eye_rstn_ch0
    hdmi_wr_reg(RX_BASE_ADDR+0xe4, tmp_add_data); // 0x30

    // Program hdmi_rx_osc_ext_clk if it is selected to be the ACR input clock
    if (ACR_MODE == 1) {
        // Configure clk_rst_tst.cts_hdmi_rx_osc_ext_clk
        tmp_add_data  = 0;
        tmp_add_data |= 0 << 3; // [7:3] Rsrv
        tmp_add_data |= 1 << 0; // [2:0] clk_sel
        hdmi_wr_reg(RX_SYS4_OSC_EXT_CLK_CNTL_1, tmp_add_data);
        tmp_add_data  = 0;
        tmp_add_data |= 1 << 7; // [7]   clk_en
        tmp_add_data |= 1 << 0; // [6:0] clk_div
        hdmi_wr_reg(RX_SYS4_OSC_EXT_CLK_CNTL_0, tmp_add_data);
    }

    // Div Pre
    tmp_add_data = 0xff;    // div_pre[7:0]
    hdmi_wr_reg(RX_SYS3_RX_ACR_0, tmp_add_data);
    tmp_add_data  = 0;
    tmp_add_data |= ACR_MODE<< 6;   // [7:6] acr_mode[1:0]
    tmp_add_data |= 1       << 5;   // [5]   ~force div_main
    tmp_add_data |= 1       << 4;   // [4]   ~force div_pre
    tmp_add_data |= 0xb     << 0;   // [3:0] div_pre[11:8]
    hdmi_wr_reg(RX_SYS3_RX_ACR_1, tmp_add_data); // 0xfb

    // SOFT_RESET [23:16]: need it to kick start ACR clocks
    tmp_add_data  = 0;
    tmp_add_data |= 0 << 7; // [7]   ~rx_i2s_config_rstn
    tmp_add_data |= 0 << 6; // [6]   Rsvd
    tmp_add_data |= 1 << 4; // [5:4] rx_acr_rst_config[1:0]
    tmp_add_data |= 0 << 3; // [3]   ~rx_config_eye_rstn_ch3
    tmp_add_data |= 0 << 2; // [2]   ~rx_config_eye_rstn_ch2
    tmp_add_data |= 0 << 1; // [1]   ~rx_config_eye_rstn_ch1
    tmp_add_data |= 0 << 0; // [0]   ~rx_config_eye_rstn_ch0
    hdmi_wr_reg(RX_BASE_ADDR+0xe4, tmp_add_data); // 0x10

    // SOFT_RESET [23:16]
    tmp_add_data  = 0;
    tmp_add_data |= 0 << 7; // [7]   ~rx_i2s_config_rstn
    tmp_add_data |= 0 << 6; // [6]   Rsvd
    tmp_add_data |= 0 << 4; // [5:4] rx_acr_rst_config[1:0]
    tmp_add_data |= 0 << 3; // [3]   ~rx_config_eye_rstn_ch3
    tmp_add_data |= 0 << 2; // [2]   ~rx_config_eye_rstn_ch2
    tmp_add_data |= 0 << 1; // [1]   ~rx_config_eye_rstn_ch1
    tmp_add_data |= 0 << 0; // [0]   ~rx_config_eye_rstn_ch0
    hdmi_wr_reg(RX_BASE_ADDR+0xe4, tmp_add_data); // 0x00

    //task_rx_key_setting();

    HDMIRX_HW_LOG("[HDMIRX Init] Setting HDMI RX EDID\n");
    task_rx_edid_setting();
    //printk("[HDMIRX Init] HDMI RX EDID Setting is done\n");

    // Set RX video/pixel/audio/packet source to DATA_PATH
    hdmi_wr_reg(RX_CORE_DATA_CAPTURE_2, 0x0000 );

    // Port Enable
    tmp_add_data  = 0;
    tmp_add_data |= 0           << 7; // [7]   cdr3_force_datafd_data
    tmp_add_data |= 0           << 6; // [6]   cdr2_force_datafd_data
    tmp_add_data |= 0           << 5; // [5]   cdr1_force_datafd_data
    tmp_add_data |= 0           << 4; // [4]   cdr0_force_datafd_data
    tmp_add_data |= 0           << 3; // [3]   rsvd
    tmp_add_data |= hdmirx_hw_stru.port << 0; // [2:0] port_en [2:0]
    if(switch_mode&0x10){
        hdmi_wr_reg(RX_BASE_ADDR+0x005, tmp_add_data); // 0x04
    }
    // Set CDR ch0 pixel_clk / tmds_clk ratio
    tmp_add_data  = 0;
    tmp_add_data |= 0                       << 5; // [7:5] hogg_adj ??
    tmp_add_data |= 0                       << 2; // [4:2] dp_div_cfg
    tmp_add_data |= RX_INPUT_COLOR_DEPTH    << 0; // [1:0] hdmi_div_cfg: pixel_clk/tmds_clk

    //hdmi_wr_reg(RX_BASE_ADDR+0x865, tmp_add_data); // 0x01  //analog module init, removed
    // Enable CDR ch0
    tmp_add_data  = 0;
    tmp_add_data |= 1   << 5; // [5] cdr0_en_clk_ch
    //hdmi_wr_reg(RX_BASE_ADDR+0x866, tmp_add_data); // 0x20 //analog module init, removed
    // Set CDR ch1 pixel_clk / tmds_clk ratio
    tmp_add_data  = 0;
    tmp_add_data |= 0                       << 5; // [7:5] hogg_adj ??
    tmp_add_data |= 0                       << 2; // [4:2] dp_div_cfg
    tmp_add_data |= RX_INPUT_COLOR_DEPTH    << 0; // [1:0] hdmi_div_cfg: pixel_clk/tmds_clk
    //hdmi_wr_reg(RX_BASE_ADDR+0x86D, tmp_add_data); // 0x01 //analog module init, removed

    // Enable CDR ch1
    tmp_add_data  = 0;
    tmp_add_data |= 0   << 5; // [5] cdr1_en_clk_ch
    //hdmi_wr_reg(RX_BASE_ADDR+0x86E, tmp_add_data); // 0x00 //analog module init, removed

    // Set CDR ch2 pixel_clk / tmds_clk ratio
    tmp_add_data  = 0;
    tmp_add_data |= 0                       << 5; // [7:5] hogg_adj ??
    tmp_add_data |= 0                       << 2; // [4:2] dp_div_cfg
    tmp_add_data |= RX_INPUT_COLOR_DEPTH    << 0; // [1:0] hdmi_div_cfg: pixel_clk/tmds_clk
    //hdmi_wr_reg(RX_BASE_ADDR+0x875, tmp_add_data); // 0x01 //analog module init, removed

    // Enable CDR ch2
    tmp_add_data  = 0;
    tmp_add_data |= 0   << 5; // [5] cdr2_en_clk_ch
    //hdmi_wr_reg(RX_BASE_ADDR+0x876, tmp_add_data); // 0x00 //analog module init, removed

    // Set CDR ch3 pixel_clk / tmds_clk ratio
    tmp_add_data  = 0;
    tmp_add_data |= 0                       << 5; // [7:5] hogg_adj ??
    tmp_add_data |= 0                       << 2; // [4:2] dp_div_cfg
    tmp_add_data |= RX_INPUT_COLOR_DEPTH    << 0; // [1:0] hdmi_div_cfg: pixel_clk/tmds_clk
    //hdmi_wr_reg(RX_BASE_ADDR+0x87D, tmp_add_data); // 0x01 //analog module init, removed

    // Enable CDR ch3
    tmp_add_data  = 0;
    tmp_add_data |= 0   << 5; // [5] cdr3_en_clk_ch
    //hdmi_wr_reg(RX_BASE_ADDR+0x87E, tmp_add_data); // 0x00 //analog module init, removed

    // Enable AFE FIFO
    tmp_add_data  = 0;
    tmp_add_data |= 0xf << 0; // [3:0] hdmidp_rx_afe_connect.fifo_enable
#ifdef FIFO_BYPASS
    hdmi_wr_reg(RX_BASE_ADDR+0xA1, 0xf0);
#elif (!(defined FIFO_ENABLE_AFTER_RESET))
    hdmi_wr_reg(RX_BASE_ADDR+0xA1, tmp_add_data); // 0x0f
#endif
    // CHANNEL_SWITCH A4
    tmp_add_data  = 0;
    tmp_add_data |= 0   << 0; // [0] polarity_0
    tmp_add_data |= 0   << 1; // [1] polarity_1
    tmp_add_data |= 0   << 2; // [2] polarity_2
    tmp_add_data |= 0   << 3; // [3] polarity_3
    tmp_add_data |= 0   << 4; // [4] bitswap_0
    tmp_add_data |= 0   << 5; // [5] bitswap_1
    tmp_add_data |= 0   << 6; // [6] bitswap_2
    tmp_add_data |= 0   << 7; // [7] bitswap_3
    hdmi_wr_reg(RX_BASE_ADDR+0x0A4, tmp_add_data); // 0x00

    // CHANNEL_SWITCH A5
    tmp_add_data  = 0;
    tmp_add_data |= 0   << 0; // [1:0] source_0
    tmp_add_data |= 1   << 2; // [3:2] source_1
    tmp_add_data |= 2   << 4; // [5:4] source_2
    tmp_add_data |= 3   << 6; // [7:6] source_3
    hdmi_wr_reg(RX_BASE_ADDR+0x0A5, tmp_add_data); // 0xe4

    // CHANNEL_SWITCH A6
    tmp_add_data  = 0;
    tmp_add_data |= 0   << 0; // [2:0] skew_0
    tmp_add_data |= 0   << 3; // [3]   enable_0
    tmp_add_data |= 0   << 4; // [6:4] skew_1
    tmp_add_data |= 1   << 7; // [7]   enable_1
//    hdmi_wr_reg(RX_BASE_ADDR+0x0A6, tmp_add_data); // 0x80
    hdmi_wr_reg(RX_BASE_ADDR+0x0A6, 0x88); // annie

    // CHANNEL_SWITCH A7
    tmp_add_data  = 0;
    tmp_add_data |= 0   << 0; // [2:0] skew_2
    tmp_add_data |= 1   << 3; // [3]   enable_2
    tmp_add_data |= 0   << 4; // [6:4] skew_3
    tmp_add_data |= 1   << 7; // [7]   enable_3
    hdmi_wr_reg(RX_BASE_ADDR+0x0A7, tmp_add_data); // 0x88

    tmp_add_data  = 0;
    tmp_add_data |= 0   << 7; // [7]   Force DTV timing
    tmp_add_data |= 0   << 6; // [6]   Force Video Scan
    tmp_add_data |= 0   << 5; // [5]   Force Video field
    tmp_add_data |= 0   << 0; // [4:0] Rsrv
    hdmi_wr_reg(RX_VIDEO_DTV_TIMING, tmp_add_data); // 0x00

    tmp_add_data  = 0;
    tmp_add_data |= 0                       << 7; // [7]   forced_default_phase
    tmp_add_data |= 1                       << 3; // [3]   forced_color_depth
    tmp_add_data |= RX_INPUT_COLOR_DEPTH    << 0; // [1:0] color_depth_config
    hdmi_wr_reg(RX_VIDEO_DTV_MODE, tmp_add_data); // 0x09

    tmp_add_data  = 0;
    tmp_add_data |= RX_OUTPUT_COLOR_FORMAT  << 6; // [7:6] output_color_format: 0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422.
    tmp_add_data |= RX_INPUT_COLOR_FORMAT   << 4; // [5:4] input_color_format:  0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422.
    tmp_add_data |= RX_OUTPUT_COLOR_DEPTH   << 2; // [3:2] output_color_depth:  0=24-b; 1=30-b; 2=36-b; 3=48-b.
    tmp_add_data |= RX_INPUT_COLOR_DEPTH    << 0; // [1:0] input_color_depth:   0=24-b; 1=30-b; 2=36-b; 3=48-b.
    hdmi_wr_reg(RX_VIDEO_DTV_OPTION_L, tmp_add_data); // 0x55

    tmp_add_data  = 0;
    tmp_add_data |= 0                       << 4; // [7:4] Rsrv
    tmp_add_data |= RX_OUTPUT_COLOR_RANGE   << 2; // [3:2] output_color_range:  0=16-235/240; 1=16-240; 2=1-254; 3=0-255.
    tmp_add_data |= RX_INPUT_COLOR_RANGE    << 0; // [1:0] input_color_range:   0=16-235/240; 1=16-240; 2=1-254; 3=0-255.
    hdmi_wr_reg(RX_VIDEO_DTV_OPTION_H, tmp_add_data); // 0x00

    tmp_add_data  = 0;
    tmp_add_data |= 0   << 6; // [7:6] hdcp_source_select[1:0]
    tmp_add_data |= 0   << 4; // [5:4] tmds_decode_source_select[1:0]
    tmp_add_data |= 0   << 2; // [3:2] tmds_align_source_select[1:0]
    tmp_add_data |= 0   << 0; // [1:0] tmds_channel_source_select[1:0]
    hdmi_wr_reg(RX_CORE_DATA_CAPTURE_1, tmp_add_data); // 0x00

    tmp_add_data  = 0;
    tmp_add_data |= 0   << 7; // [7]   forced_hdmi
    tmp_add_data |= 0   << 6; // [6]   hdmi_config
    tmp_add_data |= 0   << 5; // [5]   hdmi_reset_enable
    tmp_add_data |= 0   << 4; // [4]   1 rsvd
    tmp_add_data |= 0   << 3; // [3]   bit_swap
    tmp_add_data |= 0   << 0; // [2:0] channel_swap[2:0]
    hdmi_wr_reg(RX_TMDS_MODE, tmp_add_data); // 0x00

    tmp_add_data  = 0;
    tmp_add_data |= 0 << 3; // [7:3] Rsrv
    tmp_add_data |= 1 << 2; // [2]   acr_div_adjust.adjust_enable
    tmp_add_data |= 0 << 1; // [1]   clock_adjust_fsm.glitch_filter_enable
    tmp_add_data |= 0 << 0; // [2]   clock_adjust_fsm.fsm_select
    hdmi_wr_reg(RX_AUDIO_RSV2, tmp_add_data); // 0x04

    hdmi_wr_reg(RX_SYS1_CLOCK_CONTROL_K1_IN_4, 0x10);

    hdmi_wr_reg(RX_SYS1_CLOCK_CONTROL_K1_IN_6, 0x50); // [7:4] k0_in

    // Configure Audio Clock Recovery scheme
    tmp_add_data  = 0;
    tmp_add_data |= ACR_CONTROL_EXT << 7;   // [7]   clock_control_ext:
                                            //       0=audio clocks are generated from the old clock_control module inside hdmi_rx_core;
                                            //       1=audio clocks are generated from the new acr module outside hdmi_rx_core.
    tmp_add_data |= 0               << 4;   // [6:4] clock_control_config -- manual audio_fifo_status_source
    tmp_add_data |= 0               << 3;   // [3]   clock_control_source: 0=from received info; 1=from registers.
    tmp_add_data |= 0               << 2;   // [2]   audio_fifo_status_source: 0=from audio fifo status; 1=from bit[6:4].
    tmp_add_data |= 0               << 1;   // [1]   Manual new_m_n
    tmp_add_data |= 0               << 0;   // [0]   1=force idle on internal clock_control module.
                                            //       Note: internal clock_control is automatically idle if ACR_CONTROL_EXT=1
    hdmi_wr_reg(RX_SYS1_CLOCK_CONTROL_FSM, tmp_add_data); // 0x80

    // RX_SYS1_CLOCK_CONTROL_TIMER_0/1 mean differently depending on which ACR is used
    if (ACR_CONTROL_EXT) {
        tmp_add_data = (RX_I2S_8_CHANNEL? 16-1 : 64-1); // [7:0] acr_sample_clk_div[7:0]
        hdmi_wr_reg(RX_SYS1_CLOCK_CONTROL_TIMER_0, tmp_add_data); // 0x3f

        tmp_add_data  = 0;
        tmp_add_data |= ((ACR_MODE==1)? 0 : ACR_MODE)   << 6; // [7:6] acr_ref_clk_sel:
                                                              //       0=osc_ext_clk; 1=acr_pre_clk; 2=osc_int_clk/2; 3=osc_int_clk/4
        tmp_add_data |= 0                               << 4; // [5:4] Rsrv
        tmp_add_data |= 0                               << 0; // [3:0] acr_sample_clk_div[11:8]
        hdmi_wr_reg(RX_SYS1_CLOCK_CONTROL_TIMER_1, tmp_add_data); // 0xc0
    } else {
        tmp_add_data  = 0;
        tmp_add_data |= 3   << 4; // [7:4] clock_control.cycles_per_iter[3:0]
        tmp_add_data |= 9   << 0; // [3:0] clock_control.meas_tolerance[3:0]
        hdmi_wr_reg(RX_SYS1_CLOCK_CONTROL_TIMER_0, tmp_add_data);

        tmp_add_data = 51; // CLK_CNTRL_UPDATE_TIMER
        hdmi_wr_reg(RX_SYS1_CLOCK_CONTROL_TIMER_1, tmp_add_data);
    }

    hdmi_wr_reg(RX_SYS1_CLOCK_CONTROL_N_EXP_0, 186);
    hdmi_wr_reg(RX_SYS1_CLOCK_CONTROL_N_EXP_1, 104);
    hdmi_wr_reg(RX_SYS1_CLOCK_CONTROL_M_EXP_1, 16);

    // div_main[7:0]. Set it to close to actual result to speed up audio clock recovery.
    hdmi_wr_reg(RX_BASE_ADDR+0x9E, 0x01);

    tmp_add_data  = 0;
    tmp_add_data |= (RX_I2S_8_CHANNEL? 0 : 2)   << 6; // rx_audio_master_div_sel[3:2]: audio_sample_clk ratio; 0=master/16 for 8-ch I2S, 1=master/32, 2=master/64 for 2-ch I2S, 3=master/128
    tmp_add_data |= 0                           << 4; // rx_audio_master_div_sel[1:0]: i2s_clk ratio; 0=master/2, 1=master/4, 2=master/8, 3=master/16
    tmp_add_data |= 0                           << 0; // div_main[11:8]
    hdmi_wr_reg(RX_BASE_ADDR+0x9F, tmp_add_data); // 0x00

    tmp_add_data  = 0;
    tmp_add_data |= 3   << 4; // [7:4] depth[3:0]
    tmp_add_data |= 3   << 2; // [3:2] crit_threshold[1:0]
    tmp_add_data |= 1   << 0; // [1:0] nom_threshold[1:0]
    hdmi_wr_reg(RX_AUDIO_FIFO, tmp_add_data); // 0x3d

    hdmi_wr_reg(RX_AUDIO_RSV1, 16); // RX_CLOCK_ADJUST[7:0]

    tmp_add_data  = RX_I2S_8_CHANNEL ? 0xff : 0x03; // RX_AUDIO_CHANNEL_ALLOC[7:0]
    hdmi_wr_reg(RX_AUDIO_SAMPLE, tmp_add_data);

    tmp_add_data  = 0;
    tmp_add_data |= 0   << 7; // [7]   forced_audio_fifo_clear
    tmp_add_data |= 1   << 6; // [6]   auto_audio_fifo_clear
    tmp_add_data |= 0   << 0; // [5:0] Rsrv
    hdmi_wr_reg(RX_AUDIO_CONTROL, tmp_add_data); // 0x40

    tmp_add_data  = 0;
    tmp_add_data |= RX_I2S_SPDIF    << 7; // I2S | SPDIF
    tmp_add_data |= RX_I2S_8_CHANNEL<< 6; // 8 channel | 2 channel
    tmp_add_data |= 2               << 4; // serial format[1:0]
    tmp_add_data |= 3               << 2; // bit width[1:0]
    tmp_add_data |= 0               << 1; // WS polarity: 0=WS low is left; 1=WS high is left
    tmp_add_data |= 1               << 0; // channel status manual | auto
    hdmi_wr_reg(RX_AUDIO_FORMAT, tmp_add_data); // 0xed

    /*
    if(RX_I2S_SPDIF) {
        tmp_add_data = 1;
        hdmi_wr_reg(RX_AUDIO_I2S, tmp_add_data);
    } else {
        tmp_add_data = 1;
        hdmi_wr_reg(RX_AUDIO_SPDIF, tmp_add_data);
    }
    */
    hdmi_wr_reg(RX_AUDIO_I2S, 0x0);
    hdmi_wr_reg(RX_AUDIO_SPDIF, 0x0);

    tmp_add_data = 0x00; // tmds_clock_meter.ref_cycles[7:0]
    hdmi_wr_reg(RX_BASE_ADDR+0x64, tmp_add_data);

    tmp_add_data = 0x10; // tmds_clock_meter.ref_cycles[15:8]
    hdmi_wr_reg(RX_BASE_ADDR+0x65, tmp_add_data);

    tmp_add_data = 0; // tmds_clock_meter.ref_cycles[23:16]
    hdmi_wr_reg(RX_BASE_ADDR+0x66, tmp_add_data);

    tmp_add_data  = 0;
    tmp_add_data |= 0   << 7; // [7]   forced_tmds_clock_int
    tmp_add_data |= 0   << 6; // [6]   tmds_clock_int_config
    tmp_add_data |= 0   << 5; // [5]   tmds_clock_int_forced_clear
    tmp_add_data |= 1   << 4; // [4]   tmds_clock_int_auto_clear
    tmp_add_data |= 0x9 << 0; // [3:0] tmds_clock_meter.meas_tolerance[3:0]
    hdmi_wr_reg(RX_BASE_ADDR+0x67, tmp_add_data); // 0x19



    //tmp_add_data = 9; // time_divider[7:0] for DDC I2C bus clock
    tmp_add_data = 1; // time_divider[7:0] for DDC I2C bus clock
    hdmi_wr_reg(RX_HDCP_CONFIG3, tmp_add_data);

    tmp_add_data  = 0;
    tmp_add_data |= 0   << 6; // [7:6] feed_through_mode
    tmp_add_data |= 0   << 5; // [5]   gated_hpd
#ifdef HPD_AUTO_MODE
    tmp_add_data |= 0   << 4; // [4]   forced_hpd
#else
    tmp_add_data |= 1   << 4; // [4]   forced_hpd
#endif
    //if(switch_mode&0x20){
    //    tmp_add_data |= 1<<3;
   // }
    //else{
        tmp_add_data |= 0<< 3; // [3]   hpd_config
    //}
    tmp_add_data |= 1   << 2; // [2]   forced_ksv:0=automatic read after hpd; 1=manually triggered read
    tmp_add_data |= 1   << 1; // [1]   ksv_config:0=disable; 1=enable
    tmp_add_data |= 0   << 0; // [0]   read_km
    hdmi_wr_reg(RX_HDCP_CONFIG0, tmp_add_data); // 0x06
}

static void set_hdmi_audio_source_gate(unsigned char enable)
{
    if(enable){
        if(RX_I2S_SPDIF) {
            hdmi_wr_reg(RX_AUDIO_I2S, 0x1);
        } else {
            hdmi_wr_reg(RX_AUDIO_SPDIF, 0x1);
        }
    }
    else{
        hdmi_wr_reg(RX_AUDIO_I2S, 0x0);
        hdmi_wr_reg(RX_AUDIO_SPDIF, 0x0);
    }
}

static unsigned char is_aud_buf_ptr_change(void)
{
    unsigned char ret = 0;
    static unsigned audin_fifo_ptr_pre = 0;
    if((hdmi_rd_reg(RX_AUDIO_I2S)==1)||
        (hdmi_rd_reg(RX_AUDIO_SPDIF)==1)){
        if(audin_fifo_ptr_pre!=Rd(AUDIN_FIFO0_PTR)){
            ret = 1;
        }
    }
    audin_fifo_ptr_pre=Rd(AUDIN_FIFO0_PTR);
    return ret;
}

/*
static unsigned char is_no_error(void)
{
    if((hdmi_rd_reg(RX_BASE_ADDR+0x170)==0)
        &&(hdmi_rd_reg(RX_BASE_ADDR+0x171)==0)
        &&(hdmi_rd_reg(RX_BASE_ADDR+0x172)==0)
        &&(hdmi_rd_reg(RX_BASE_ADDR+0x173)==0)
        &&(hdmi_rd_reg(RX_BASE_ADDR+0x174)==0)
        ){
            return 1;
    }
    return 0;

}
*/

/*
hdmirx interface function
*/
#ifdef GET_COLOR_DEPTH_WORK_AROUND
static unsigned char is_frame_packing_fmt(int vic)
{
    unsigned char ret = 0;
    if(is_frame_packing()){
        if((vic == HDMI_720p60)||(vic == HDMI_1080i60)
            ||(vic == HDMI_1080p24) || (vic == HDMI_720p50)
            ||(vic == HDMI_1080i50)){
                ret = 1;
        }

    }
    return ret;
}
#endif

enum tvin_sig_fmt_e hdmirx_hw_get_fmt(void)
{
    /* to do:
    TVIN_SIG_FMT_HDMI_1280x720P_24Hz_FRAME_PACKING,
    TVIN_SIG_FMT_HDMI_1280x720P_30Hz_FRAME_PACKING,

    TVIN_SIG_FMT_HDMI_1920x1080P_24Hz_FRAME_PACKING,
    TVIN_SIG_FMT_HDMI_1920x1080P_30Hz_FRAME_PACKING, // 150
    */
    unsigned char support_flag = 0;
    enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
    unsigned int vic = hdmirx_hw_stru.cur_vic;
    if(force_vic){
        vic = force_vic;
    }
    else if(hdmirx_hw_stru.cur_vic == 0){
        vic = hdmirx_hw_stru.guess_vic;
    }

    /* check format is in enable list */
    if(vic>96){
        if((format_en3>>(vic-96-1))&0x1){
            support_flag = 1;
        }
    }
    else if(vic>64){
        if((format_en2>>(vic-64-1))&0x1){
            support_flag = 1;
        }
    }
    else if(vic>32){
        if((format_en1>>(vic-32-1))&0x1){
            support_flag = 1;
        }
    }
    else{
        if((format_en0>>(vic-1))&0x1){
            support_flag = 1;
        }
    }
    if(support_flag==0){
        return TVIN_SIG_FMT_NULL;
    }
    /**/

    switch(vic){
/* basic format */
        case HDMI_640x480p60:
            fmt = TVIN_SIG_FMT_HDMI_640x480P_60Hz;
            break;
        case HDMI_480p60: /*2*/
        case HDMI_480p60_16x9: /*3*/
            if(is_frame_packing()){
                fmt = TVIN_SIG_FMT_HDMI_720x480P_60Hz_FRAME_PACKING;
            }
            else{
            fmt = TVIN_SIG_FMT_HDMI_720x480P_60Hz;
            }
            break;
        case HDMI_720p60: /*4*/
            if(is_frame_packing()){
                fmt = TVIN_SIG_FMT_HDMI_1280x720P_60Hz_FRAME_PACKING;
            }
            else{
                fmt = TVIN_SIG_FMT_HDMI_1280x720P_60Hz;
            }
            break;
        case HDMI_1080i60: /*5*/
            if(is_frame_packing()){
                fmt = TVIN_SIG_FMT_HDMI_1920x1080I_60Hz_FRAME_PACKING;
            }
            else{
                fmt = TVIN_SIG_FMT_HDMI_1920x1080I_60Hz;
            }
            break;
        case HDMI_480i60: /*6*/
        case HDMI_480i60_16x9: /*7*/
            fmt = TVIN_SIG_FMT_HDMI_1440x480I_60Hz;
            break;
        case HDMI_1080p60: /*16*/
            fmt = TVIN_SIG_FMT_HDMI_1920x1080P_60Hz;
            break;
        case HDMI_1080p24: /*32 */
            if(is_frame_packing()){
                fmt = TVIN_SIG_FMT_HDMI_1920x1080P_24Hz_FRAME_PACKING;
            }
            else{
                fmt = TVIN_SIG_FMT_HDMI_1920x1080P_24Hz;
            }
            break;
        case HDMI_576p50: /*17*/
        case HDMI_576p50_16x9: /*18*/
            if(is_frame_packing()){
                fmt = TVIN_SIG_FMT_HDMI_720x576P_50Hz_FRAME_PACKING;
            }
            else{
            fmt = TVIN_SIG_FMT_HDMI_720x576P_50Hz;
            }
            break;
        case HDMI_720p50: /*19*/
            if(is_frame_packing()){
                fmt = TVIN_SIG_FMT_HDMI_1280x720P_50Hz_FRAME_PACKING;
            }
            else{
                fmt = TVIN_SIG_FMT_HDMI_1280x720P_50Hz;
            }
            break;
        case HDMI_1080i50: /*20*/
            if(is_frame_packing()){
                fmt = TVIN_SIG_FMT_HDMI_1920x1080I_50Hz_FRAME_PACKING;
            }
            else{
                fmt = TVIN_SIG_FMT_HDMI_1920x1080I_50Hz_A;
            }
            break;
        case HDMI_576i50: /*21*/
        case HDMI_576i50_16x9: /*22*/
            fmt = TVIN_SIG_FMT_HDMI_1440x576I_50Hz;
            break;
        case HDMI_1080p50: /*31*/
            fmt = TVIN_SIG_FMT_HDMI_1920x1080P_50Hz;
            break;
		case HDMI_1080p25: /*33*/
            fmt = TVIN_SIG_FMT_HDMI_1920x1080P_25Hz;
            break;
		case HDMI_1080p30: /*34*/
			if(is_frame_packing()){
                fmt = TVIN_SIG_FMT_HDMI_1920x1080P_30Hz_FRAME_PACKING;
            }
            else{
                fmt = TVIN_SIG_FMT_HDMI_1920x1080P_30Hz;
            }
            break;

/* extend format */
    case HDMI_1440x240p60:
    case HDMI_1440x240p60_16x9:
            fmt = TVIN_SIG_FMT_HDMI_1440x240P_60Hz;
            break;
    case HDMI_2880x480i60:
    case HDMI_2880x480i60_16x9:
            fmt = TVIN_SIG_FMT_HDMI_2880x480I_60Hz;
            break;
    case HDMI_2880x240p60:
    case HDMI_2880x240p60_16x9:
            fmt = TVIN_SIG_FMT_HDMI_2880x240P_60Hz;
            break;
    case HDMI_1440x480p60:
    case HDMI_1440x480p60_16x9:
            fmt = TVIN_SIG_FMT_HDMI_1440x480P_60Hz;
            break;
    case HDMI_1440x288p50:
    case HDMI_1440x288p50_16x9:
            fmt = TVIN_SIG_FMT_HDMI_1440x288P_50Hz;
            break;
    case HDMI_2880x576i50:
    case HDMI_2880x576i50_16x9:
            fmt = TVIN_SIG_FMT_HDMI_2880x576I_50Hz;
            break;
    case HDMI_2880x288p50:
    case HDMI_2880x288p50_16x9:
            fmt = TVIN_SIG_FMT_HDMI_2880x288P_50Hz;
            break;
    case HDMI_1440x576p50:
    case HDMI_1440x576p50_16x9:
            fmt = TVIN_SIG_FMT_HDMI_1440x576P_50Hz;
            break;

    case HDMI_2880x480p60:
    case HDMI_2880x480p60_16x9:
            fmt = TVIN_SIG_FMT_HDMI_2880x480P_60Hz;
            break;

    case HDMI_2880x576p50:
    case HDMI_2880x576p50_16x9:
            fmt = TVIN_SIG_FMT_HDMI_2880x576P_60Hz; //????, should be TVIN_SIG_FMT_HDMI_2880x576P_50Hz
            break;

    case HDMI_1080i50_1250:
            fmt = TVIN_SIG_FMT_HDMI_1920x1080I_50Hz_B;
            break;
		case HDMI_1080I120: /*46*/
            fmt = TVIN_SIG_FMT_HDMI_1920x1080I_120Hz;
            break;
		case HDMI_720p120: /*47*/
            fmt = TVIN_SIG_FMT_HDMI_1280x720P_120Hz;
            break;
		case HDMI_1080p120: /*63*/
            fmt = TVIN_SIG_FMT_HDMI_1920x1080P_120Hz;
            break;

/* vesa format*/
		case HDMI_800_600: /*65*/
            fmt = TVIN_SIG_FMT_HDMI_800x600;
            break;
		case HDMI_1024_768: /*66*/
            fmt = TVIN_SIG_FMT_HDMI_1024x768;
            break;
  case HDMI_720_400:
            fmt = TVIN_SIG_FMT_HDMI_720_400;
            break;
	case HDMI_1280_768:
            fmt = TVIN_SIG_FMT_HDMI_1280_768;
            break;
	case HDMI_1280_800:
            fmt = TVIN_SIG_FMT_HDMI_1280_800;
            break;
	case HDMI_1280_960:
            fmt = TVIN_SIG_FMT_HDMI_1280_960;
            break;
	case HDMI_1280_1024:
            fmt = TVIN_SIG_FMT_HDMI_1280_1024;
            break;
	case HDMI_1360_768:
            fmt = TVIN_SIG_FMT_HDMI_1360_768;
            break;
	case HDMI_1366_768:
            fmt = TVIN_SIG_FMT_HDMI_1366_768;
            break;
	case HDMI_1600_1200:
            fmt = TVIN_SIG_FMT_HDMI_1600_1200;
            break;
	case HDMI_1920_1200:
            fmt = TVIN_SIG_FMT_HDMI_1920_1200;
            break;
        default:
            break;
    }
    return fmt;
}

int hdmirx_hw_get_color_fmt(void)
{
    /*let vdin  use rx "output color format" instead of "input color format" */
    return hdmirx_hw_stru.cur_color_format;
}

int hdmirx_hw_get_3d_structure(unsigned char* _3d_structure, unsigned char* _3d_ext_data)
{
    if((hdmirx_hw_stru.vendor_specific_info.identifier == 0x000c03)&&
        (hdmirx_hw_stru.vendor_specific_info.hdmi_video_format == 0x2)){
        *_3d_structure = hdmirx_hw_stru.vendor_specific_info._3d_structure;
        *_3d_ext_data = hdmirx_hw_stru.vendor_specific_info._3d_ext_data;
        return 0;
    }
    return -1;
}

static unsigned char dump_flag = 0;

static unsigned char is_frame_packing(void)
{
    if((hdmirx_hw_stru.vendor_specific_info.identifier == 0x000c03)&&
        (hdmirx_hw_stru.vendor_specific_info.hdmi_video_format == 0x2)&&
        (hdmirx_hw_stru.vendor_specific_info._3d_structure == 0x0)){
        return 1;
    }
    return 0;
}

int hdmirx_hw_get_pixel_repeat(void)
{
    return hdmirx_hw_stru.cur_pixel_repeat+1;
}

static void read_video_timing(void)
{
    hdmirx_hw_stru.video_status.active_pixels = hdmi_rd_reg(RX_VIDEO_ST_ACTIVE_PIXELS_1)|
                                        ((hdmi_rd_reg(RX_VIDEO_ST_ACTIVE_PIXELS_2)&0xf)<<8);
    hdmirx_hw_stru.video_status.front_pixels =  hdmi_rd_reg(RX_VIDEO_ST_FRONT_PIXELS)|
                                        (((hdmi_rd_reg(RX_VIDEO_ST_ACTIVE_PIXELS_2)>>4)&0xf)<<8);
    hdmirx_hw_stru.video_status.hsync_pixels = hdmi_rd_reg(RX_VIDEO_ST_HSYNC_PIXELS)|
                                        (((hdmi_rd_reg(RX_VIDEO_ST_VSYNC_LINES)>>6)&0x3)<<8);
    hdmirx_hw_stru.video_status.back_pixels = hdmi_rd_reg(RX_VIDEO_ST_BACK_PIXELS)|
                                        (((hdmi_rd_reg(RX_VIDEO_ST_SOF_LINES)>>6)&0x3)<<8);
    hdmirx_hw_stru.video_status.active_lines = hdmi_rd_reg(RX_VIDEO_ST_ACTIVE_LINES_1)|
                                        ((hdmi_rd_reg(RX_VIDEO_ST_ACTIVE_LINES_2)&0xf)<<8);;
    hdmirx_hw_stru.video_status.eof_lines = hdmi_rd_reg(RX_VIDEO_ST_EOF_LINES)&0x3f;
    hdmirx_hw_stru.video_status.vsync_lines = hdmi_rd_reg(RX_VIDEO_ST_VSYNC_LINES)&0x3f;
    hdmirx_hw_stru.video_status.sof_lines = hdmi_rd_reg(RX_VIDEO_ST_SOF_LINES)&0x3f;

    hdmirx_hw_stru.video_status.video_scan = (hdmi_rd_reg(RX_VIDEO_ST_DTV_TIMING)>>6)&0x1; /* 0, progressive; 1, interlaced */
    hdmirx_hw_stru.video_status.video_field = (hdmi_rd_reg(RX_VIDEO_ST_DTV_TIMING)>>5)&0x1;    /* progressive:0; interlace: 0, 1st; 1, 2nd */
    hdmirx_hw_stru.video_status.scan_stable = (hdmi_rd_reg(RX_VIDEO_ST_DTV_TIMING)>>4)&0x1; /* 0, not stable; 1, stable */
    hdmirx_hw_stru.video_status.lines_stable = (hdmi_rd_reg(RX_VIDEO_ST_DTV_TIMING)>>3)&0x1;
    hdmirx_hw_stru.video_status.vsync_stable = (hdmi_rd_reg(RX_VIDEO_ST_DTV_TIMING)>>2)&0x1;
    hdmirx_hw_stru.video_status.pixels_stable = (hdmi_rd_reg(RX_VIDEO_ST_DTV_TIMING)>>1)&0x1;
    hdmirx_hw_stru.video_status.hsync_stable = (hdmi_rd_reg(RX_VIDEO_ST_DTV_TIMING)>>0)&0x1;
    hdmirx_hw_stru.video_status.default_phase = (hdmi_rd_reg(RX_VIDEO_ST_DTV_MODE)>>7)&0x1;
    hdmirx_hw_stru.video_status.pixel_phase = (hdmi_rd_reg(RX_VIDEO_ST_DTV_MODE)>>4)&0x7;
    hdmirx_hw_stru.video_status.pixel_phase_ok = (hdmi_rd_reg(RX_VIDEO_ST_DTV_MODE)>>3)&0x1;
    hdmirx_hw_stru.video_status.gc_error = (hdmi_rd_reg(RX_VIDEO_ST_DTV_MODE)>>2)&0x1; /* 0, no error; 1, error in packet data */
    hdmirx_hw_stru.video_status.color_depth = (hdmi_rd_reg(RX_VIDEO_ST_DTV_MODE)>>0)&0x3; /* 0, 24bit; 1, 30bit; 2, 36 bit; 3, 48 bit */

    if(memcmp(&hdmirx_hw_stru.video_status, &hdmirx_hw_stru.video_status_pre, sizeof(struct video_status_s))){
        //dump_flag = 1;
    }
    memcpy(&hdmirx_hw_stru.video_status_pre, &hdmirx_hw_stru.video_status, sizeof(struct video_status_s));

}

static void dump_video_timing(void)
{
    HDMIRX_HW_LOG("[HDMIRX Vidoe Timing]:\n");
    HDMIRX_HW_LOG("[HDMIRX]active pixels %d, active lines %d\n",
        hdmirx_hw_stru.video_status.active_pixels,
        hdmirx_hw_stru.video_status.active_lines
        );
    HDMIRX_HW_LOG("[HDMIRX]front pixels %d, hsync pixels %d, back pixels %d, eof lines %d, vsync lines %d, sof lines %d\n",
        hdmirx_hw_stru.video_status.front_pixels,
        hdmirx_hw_stru.video_status.hsync_pixels,
        hdmirx_hw_stru.video_status.back_pixels,
        hdmirx_hw_stru.video_status.eof_lines,
        hdmirx_hw_stru.video_status.vsync_lines,
        hdmirx_hw_stru.video_status.sof_lines
        );
    HDMIRX_HW_LOG("[HDMIRX]scan %d, field %d\n",
        hdmirx_hw_stru.video_status.video_scan,
        hdmirx_hw_stru.video_status.video_field
        );
    HDMIRX_HW_LOG("[HDMIRX]scan stable %d, lines stable %d, vsync stable %d, pixels stable %d, hsync stable %d\n",
        hdmirx_hw_stru.video_status.scan_stable,
        hdmirx_hw_stru.video_status.lines_stable,
        hdmirx_hw_stru.video_status.vsync_stable,
        hdmirx_hw_stru.video_status.pixels_stable,
        hdmirx_hw_stru.video_status.hsync_stable
        );
    HDMIRX_HW_LOG("[HDMIRX]default phase %d, pixel phase %d, pixel phase ok %d, gc error %d, color_depth %d\n",
        hdmirx_hw_stru.video_status.default_phase,
        hdmirx_hw_stru.video_status.pixel_phase,
        hdmirx_hw_stru.video_status.pixel_phase_ok,
        hdmirx_hw_stru.video_status.gc_error,
        hdmirx_hw_stru.video_status.color_depth);
}

static void read_tmds_clk(void)
{
    int i;
    unsigned int tmds_clock=0;
    hdmi_wr_reg(RX_BASE_ADDR+0x64, 0x00);
    hdmi_wr_reg(RX_BASE_ADDR+0x65, 0x08);
    hdmi_wr_reg(RX_BASE_ADDR+0x66, 0x00);
    hdmi_wr_reg(RX_BASE_ADDR+0x67, 0x10|0x4);

    tmds_clock |= hdmi_rd_reg(RX_BASE_ADDR+0x1f4);
    tmds_clock |= (hdmi_rd_reg(RX_BASE_ADDR+0x1f5)<<8);
    tmds_clock |= (hdmi_rd_reg(RX_BASE_ADDR+0x1f6)<<16);
    for(i = 0; i<(TMDS_CLK_HIS_SIZE-1); i++){
        hdmirx_hw_stru.tmds_clk[i] = hdmirx_hw_stru.tmds_clk[i+1];
    }
    hdmirx_hw_stru.tmds_clk[i] = tmds_clock;
    if(((hdmirx_log_flag&0x10)&&(hdmirx_hw_stru.state!=HDMIRX_HWSTATE_SIG_STABLE))
        ||(hdmirx_log_flag&0x20)){
        HDMIRX_HW_LOG("[HDMIRX tmds clock] %d\n", tmds_clock);
    }

}

static unsigned char have_audio_info(void)
{
    unsigned char ret = 0;
    if(hdmi_rd_reg(RX_COREST_INTERRUPT_STATUS_0)&0x10){
        ret = 1;
    }
    else{
        ret = 0;
    }
    hdmi_wr_reg(RX_CORE_INTERRUPT_CLEAR_0, hdmi_rd_reg(RX_CORE_INTERRUPT_CLEAR_0)|0x10);
    hdmi_wr_reg(RX_CORE_INTERRUPT_CLEAR_0, hdmi_rd_reg(RX_CORE_INTERRUPT_CLEAR_0)&(~0x10));
    return ret;

}

static unsigned char is_audio_channel_status_valid(void)
{
    if(have_audio_info()){
        if((hdmirx_hw_stru.aud_info.cc == 0)
          ||(hdmirx_hw_stru.aud_info.ss == 0)
    //    ||(hdmirx_hw_stru.aud_info.sf == 0)
        ){
            return 1;
        }
    }
    return 0;
}

#ifdef FOR_LS_DIG2090__DVP_5986K
static void check_audio(void)
{
    int i;
    unsigned char channel_status_change;
    unsigned char channel_status_all_0;
    unsigned char channel_status_tmp;
    channel_status_change = 0;
    channel_status_all_0 = 1;

    for(i=0; i<CHANNEL_STATUS_SIZE; i++){
        if(hdmirx_hw_stru.aud_info.channel_status[i]!=0){
            channel_status_all_0 = 0;
        }
    }

    if(channel_status_all_0 == 0){
        for(i=0; i<CHANNEL_STATUS_SIZE; i++){
            if(hdmirx_hw_stru.aud_info.channel_status[i]!=hdmirx_hw_stru.aud_info.channel_status_bak[i]){
                channel_status_change = 1;
            }
            hdmirx_hw_stru.aud_info.channel_status_bak[i]=hdmirx_hw_stru.aud_info.channel_status[i];
        }
    }
    if(channel_status_change){
        hdmirx_hw_stru.aud_channel_status_modify_count++;
        hdmirx_hw_stru.aud_channel_status_unmodify_count = 0;
    }
    else if(hdmirx_hw_stru.aud_channel_status_unmodify_count > 0){
        hdmirx_hw_stru.aud_channel_status_modify_count = 0;
    }
    else{
        hdmirx_hw_stru.aud_channel_status_unmodify_count++;
    }

    if((channel_status_all_0)
        &&(is_audio_channel_status_valid())
        ){
        hdmirx_hw_stru.aud_channel_status_all_0_count++;
        if(is_aud_buf_ptr_change()){
            hdmirx_hw_stru.aud_buf_ptr_change_count++;
        }
    }
    else{
        hdmirx_hw_stru.aud_channel_status_all_0_count=0;
        hdmirx_hw_stru.aud_buf_ptr_change_count = 0;
    }
}
#endif


typedef struct{
    unsigned int sample_rate;
    unsigned char aud_info_sf;
    unsigned char channel_status_id;    
}sample_rate_info_t;
sample_rate_info_t sample_rate_info[]=
{
    {32000,  0x1,  0x3},
    {44100,  0x2,  0x0},
    {48000,  0x3,  0x2},
    {88200,  0x4,  0x8},
    {96000,  0x5,  0xa},
    {176400, 0x6,  0xc},
    {192000, 0x7,  0xe},
    //{768000, 0, 0x9},
    {0, 0, 0}
};

static unsigned is_use_audio_recover_clock(void)
{
    if(internal_mode_valid()&&(internal_mode&INT_MODE_USE_AUD_INFO_FRAME))
        return 0;
    else
        return 1;
}    

static void read_audio_info(void)
{
    int i;
    /* get audio recovery clock */
    hdmirx_hw_stru.aud_info.cts = ((hdmi_rd_reg(RX_SYSST0_NCTS_STATUS)&0xf)<<16)|
                                        (hdmi_rd_reg(RX_SYSST0_CTS_STATUS_H)<<8)|
                                        (hdmi_rd_reg(RX_SYSST0_CTS_STATUS_L));
    hdmirx_hw_stru.aud_info.n = (hdmi_rd_reg(RX_SYSST0_N_STATUS_H)<<12)|
                                    (hdmi_rd_reg(RX_SYSST0_N_STATUS_L)<<4)|
                                    ((hdmi_rd_reg(RX_SYSST0_NCTS_STATUS)>>4)&0xf);
    if(hdmirx_hw_stru.aud_info.cts!=0){
        hdmirx_hw_stru.aud_info.audio_recovery_clock = (get_freq()/hdmirx_hw_stru.aud_info.cts)
                                            *hdmirx_hw_stru.aud_info.n/128;
    }
    else{
        hdmirx_hw_stru.aud_info.audio_recovery_clock = 0;
    }

    /* audio info frame */
    hdmirx_hw_stru.aud_info.cc = hdmi_rd_reg(RX_PKT_REG_AUDIO_INFO_BASE_ADDR + 1)&0x7;
    hdmirx_hw_stru.aud_info.ct = (hdmi_rd_reg(RX_PKT_REG_AUDIO_INFO_BASE_ADDR + 1)>>4)&0xf;
    hdmirx_hw_stru.aud_info.ss = hdmi_rd_reg(RX_PKT_REG_AUDIO_INFO_BASE_ADDR + 2)&0x3;
    hdmirx_hw_stru.aud_info.sf = (hdmi_rd_reg(RX_PKT_REG_AUDIO_INFO_BASE_ADDR + 2)>>2)&0x7;
    /* channel status */
    for(i=0; i<CHANNEL_STATUS_SIZE; i++){
        hdmirx_hw_stru.aud_info.channel_status[i] = hdmi_rd_reg(RX_IEC60958_ST_SUB1_OFFSET + i);
    }

    /*parse audio info*/
    if(hdmirx_hw_stru.aud_info.cc == 0){
        hdmirx_hw_stru.aud_info.channel_num = hdmirx_hw_stru.aud_info.channel_status[2]&0xf;
    }
    else{
        hdmirx_hw_stru.aud_info.channel_num = hdmirx_hw_stru.aud_info.cc + 1;
    }

    if(hdmirx_hw_stru.aud_info.ct == 0){

    }
    else{
        if(hdmirx_hw_stru.aud_info.ct == 1){ //pcm

        }
        else{
            HDMIRX_HW_LOG("[HDMIRX] Audio ct of %x , not support\n", hdmirx_hw_stru.aud_info.ct);
        }
    }

    if(hdmirx_hw_stru.aud_info.ss == 0){
        switch(hdmirx_hw_stru.aud_info.channel_status[4]&0xf){
            case 0x2:
                hdmirx_hw_stru.aud_info.sample_size = 16;
                break;
            case 0x3:
                hdmirx_hw_stru.aud_info.sample_size = 20;
                break;
            case 0xb:
                hdmirx_hw_stru.aud_info.sample_size = 24;
                break;
        }
    }
    else{
        hdmirx_hw_stru.aud_info.sample_size = 12 + (hdmirx_hw_stru.aud_info.ss*4);
    }

    if(hdmirx_hw_stru.aud_info.sf == 0){
        for(i=0; sample_rate_info[i].sample_rate; i++){
            if((hdmirx_hw_stru.aud_info.channel_status[3]&0xf) == 
                sample_rate_info[i].channel_status_id){
                hdmirx_hw_stru.aud_info.sample_rate = sample_rate_info[i].sample_rate;    
                break;
            }
        }
    }
    else{
        for(i=0; sample_rate_info[i].sample_rate; i++){
            if(hdmirx_hw_stru.aud_info.sf == 
                    sample_rate_info[i].aud_info_sf){
                hdmirx_hw_stru.aud_info.sample_rate = sample_rate_info[i].sample_rate;    
                break;
            }
        }
    }
    if((hdmirx_hw_stru.aud_info.audio_recovery_clock!=0)&&
        is_use_audio_recover_clock()){
        hdmirx_hw_stru.aud_info.real_sample_rate = hdmirx_hw_stru.aud_info.audio_recovery_clock;
        for(i=0; sample_rate_info[i].sample_rate; i++){
            if(hdmirx_hw_stru.aud_info.audio_recovery_clock > sample_rate_info[i].sample_rate){
                if((hdmirx_hw_stru.aud_info.audio_recovery_clock-sample_rate_info[i].sample_rate)<sample_rate_change_th){
                    hdmirx_hw_stru.aud_info.real_sample_rate = sample_rate_info[i].sample_rate;
                break;
        }
    }
    else{
                if((sample_rate_info[i].sample_rate - hdmirx_hw_stru.aud_info.audio_recovery_clock)<sample_rate_change_th){
                    hdmirx_hw_stru.aud_info.real_sample_rate = sample_rate_info[i].sample_rate;
                break;
        }
    }
        }
    }
    else{
        hdmirx_hw_stru.aud_info.real_sample_rate = hdmirx_hw_stru.aud_info.sample_rate;
    }

}

static unsigned char is_sample_rate_change(int sample_rate_pre, int sample_rate_cur)
{
    unsigned char ret = 0;
    if((sample_rate_cur!=0)&&
        (sample_rate_cur>31000)&&(sample_rate_cur<193000)){
        if(sample_rate_pre > sample_rate_cur){
            if((sample_rate_pre - sample_rate_cur)> sample_rate_change_th){
                ret = 1;
            }
        }
        else{
            if((sample_rate_cur - sample_rate_pre)> sample_rate_change_th){
                ret = 1;
            }
        }
    }
    return ret;
}

static void dump_audio_info(void)
{
    //int audio_master_clock = clk_util_clk_msr(24,50);
    HDMIRX_HW_LOG("[HDMIRX AUD Info] cc %x, ct %x, ss %x, sf %x\n",
       hdmirx_hw_stru.aud_info.cc, hdmirx_hw_stru.aud_info.ct, hdmirx_hw_stru.aud_info.ss, hdmirx_hw_stru.aud_info.sf);
    HDMIRX_HW_LOG("[HDMIRX] channel status[0,1,2,3,4,5]%02x %02x %02x %02x %02x %02x\n",
       hdmirx_hw_stru.aud_info.channel_status[0], hdmirx_hw_stru.aud_info.channel_status[1], hdmirx_hw_stru.aud_info.channel_status[2],
       hdmirx_hw_stru.aud_info.channel_status[3], hdmirx_hw_stru.aud_info.channel_status[4], hdmirx_hw_stru.aud_info.channel_status[5]);
    HDMIRX_HW_LOG("[HDMIRX] channel num %d, sample rate %d, sample size %d\n",
       hdmirx_hw_stru.aud_info.channel_num, hdmirx_hw_stru.aud_info.sample_rate, hdmirx_hw_stru.aud_info.sample_size);
    HDMIRX_HW_LOG("[HDMIRX] audio infoframe raw %s: %02x %02x %02x %02x %02x %02x %02x %02x\n",
        have_audio_info()?"":"(not come)",
        hdmi_rd_reg(RX_PKT_REG_AUDIO_INFO_BASE_ADDR), hdmi_rd_reg(RX_PKT_REG_AUDIO_INFO_BASE_ADDR+1),
        hdmi_rd_reg(RX_PKT_REG_AUDIO_INFO_BASE_ADDR+2), hdmi_rd_reg(RX_PKT_REG_AUDIO_INFO_BASE_ADDR+3),
        hdmi_rd_reg(RX_PKT_REG_AUDIO_INFO_BASE_ADDR+4), hdmi_rd_reg(RX_PKT_REG_AUDIO_INFO_BASE_ADDR+5),
        hdmi_rd_reg(RX_PKT_REG_AUDIO_INFO_BASE_ADDR+6), hdmi_rd_reg(RX_PKT_REG_AUDIO_INFO_BASE_ADDR+7));
    HDMIRX_HW_LOG("[HDMIRX]CTS %d N %d, audio recovery clock %d\n", hdmirx_hw_stru.aud_info.cts,
        hdmirx_hw_stru.aud_info.n, hdmirx_hw_stru.aud_info.audio_recovery_clock);
    HDMIRX_HW_LOG("[HDMIRX] real_sample_rate is %d\n", hdmirx_hw_stru.aud_info.real_sample_rate);
    //HDMIRX_HW_LOG("[HDMIRX] audio master clock (%d/128)=%d\n", audio_master_clock, audio_master_clock/128);
}

static unsigned char have_avi_info(void)
{
    unsigned char ret = 0;
    if(hdmi_rd_reg(RX_COREST_INTERRUPT_STATUS_0)&0x8){
        ret = 1;
    }
    else{
        ret = 0;
    }
    hdmi_wr_reg(RX_CORE_INTERRUPT_CLEAR_0, hdmi_rd_reg(RX_CORE_INTERRUPT_CLEAR_0)|0x8);
    hdmi_wr_reg(RX_CORE_INTERRUPT_CLEAR_0, hdmi_rd_reg(RX_CORE_INTERRUPT_CLEAR_0)&(~0x8));
    return ret;

}

static void read_avi_info(void)
{
    hdmirx_hw_stru.avi_info.color_format    = (hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+1) & 0x60) >> 5;
    hdmirx_hw_stru.avi_info.cc              = (hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+2) & 0xc0) >> 6;
    hdmirx_hw_stru.avi_info.color_range     = (hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+3) & 0x0c) >> 2;
    hdmirx_hw_stru.avi_info.vic             = hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+4) & 0x7f;
    hdmirx_hw_stru.avi_info.pixel_repeat    = hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+5) & 0x0f;

    // Patch for 1080P25Hz vic error issue
    if(hdmirx_hw_stru.avi_info.vic == 32){
        unsigned int sig_h_total = hdmirx_hw_stru.video_status.front_pixels +
                                   hdmirx_hw_stru.video_status.hsync_pixels +
                                   hdmirx_hw_stru.video_status.active_pixels +
                                   hdmirx_hw_stru.video_status.back_pixels;

        unsigned int sig_v_total = hdmirx_hw_stru.video_status.sof_lines +
                                   hdmirx_hw_stru.video_status.vsync_lines +
                                   hdmirx_hw_stru.video_status.active_lines +
                                   hdmirx_hw_stru.video_status.eof_lines;

        if ((ABS((signed int)sig_h_total- (signed int)tvin_fmt_tbl[TVIN_SIG_FMT_HDMI_1920x1080P_25Hz].h_total) <= 15) &&
            (ABS((signed int)sig_v_total- (signed int)tvin_fmt_tbl[TVIN_SIG_FMT_HDMI_1920x1080P_25Hz].v_total) <= 15)){
            hdmirx_hw_stru.avi_info.vic = 33;
            //printk("change vic\n");
        }
    }
    // Patch end

    if((color_format_mode>=0) && (color_format_mode<=3)){
        hdmirx_hw_stru.cur_color_format = color_format_mode;
    }
    else{
        hdmirx_hw_stru.cur_color_format = hdmirx_hw_stru.avi_info.color_format;
    }

    if(memcmp(&hdmirx_hw_stru.avi_info, &hdmirx_hw_stru.avi_info_pre, sizeof(struct avi_info_s))){
        dump_flag = 1;
        hdmirx_hw_stru.avi_info_change_flag = 1;
    }
    memcpy(&hdmirx_hw_stru.avi_info_pre, &hdmirx_hw_stru.avi_info, sizeof(struct avi_info_s));


}

static void dump_avi_info(void)
{
    HDMIRX_HW_LOG("[HDMIRX AVI Info %s] vic %d, repeat %d, color format %d, color range %d, cc %d\n\[HDMIRX]AVI raw:[%02x][%02x][%02x][%02x][%02x][%02x][%02x][%02x][%02x][%02x][%02x][%02x][%02x][%02x][%02x][%02x]\n",
        have_avi_info()?"":"(not received)",
       hdmirx_hw_stru.avi_info.vic, hdmirx_hw_stru.avi_info.pixel_repeat,
       hdmirx_hw_stru.avi_info.color_format, hdmirx_hw_stru.avi_info.color_range, hdmirx_hw_stru.avi_info.cc,
       hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR),hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+1),
       hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+2),hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+3),
       hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+4),hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+5),
       hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+6),hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+7),
       hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+8),hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+9),
       hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+10),hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+11),
       hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+12),hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+13),
       hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+14),hdmi_rd_reg(RX_PKT_REG_AVI_INFO_BASE_ADDR+15)
        );
}

static unsigned char have_vendor_specific_info(void)
{
    unsigned char ret = 0;
    if(hdmi_rd_reg(RX_COREST_INTERRUPT_STATUS_0)&2){
        ret = 1;
    }
    else{
        ret = 0;
    }
    hdmi_wr_reg(RX_CORE_INTERRUPT_CLEAR_0, hdmi_rd_reg(RX_CORE_INTERRUPT_CLEAR_0)|0x2);
    hdmi_wr_reg(RX_CORE_INTERRUPT_CLEAR_0, hdmi_rd_reg(RX_CORE_INTERRUPT_CLEAR_0)&(~0x2));
    return ret;
}

static void read_vendor_specific_info_frame(void)
{
    if(have_vendor_specific_info()){
        hdmirx_hw_stru.vendor_specific_info.identifier = (hdmi_rd_reg(RX_PKT_REG_VEND_INFO_BASE_ADDR+3)<<16)|
                                                    (hdmi_rd_reg(RX_PKT_REG_VEND_INFO_BASE_ADDR+2)<<8)|
                                                    hdmi_rd_reg(RX_PKT_REG_VEND_INFO_BASE_ADDR+1);
        hdmirx_hw_stru.vendor_specific_info.hdmi_video_format = (hdmi_rd_reg(RX_PKT_REG_VEND_INFO_BASE_ADDR+4) & 0xe0) >> 5;
        hdmirx_hw_stru.vendor_specific_info._3d_structure = (hdmi_rd_reg(RX_PKT_REG_VEND_INFO_BASE_ADDR+5) & 0xf0) >> 4;
        hdmirx_hw_stru.vendor_specific_info._3d_ext_data = (hdmi_rd_reg(RX_PKT_REG_VEND_INFO_BASE_ADDR+6) & 0xf0) >> 4;
    }
    else{
        hdmirx_hw_stru.vendor_specific_info.identifier = 0;
        hdmirx_hw_stru.vendor_specific_info.hdmi_video_format = 0;
        hdmirx_hw_stru.vendor_specific_info._3d_structure = 0;
        hdmirx_hw_stru.vendor_specific_info._3d_ext_data = 0;
    }
    if(memcmp(&hdmirx_hw_stru.vendor_specific_info, &hdmirx_hw_stru.vendor_specific_info_pre, sizeof(struct vendor_specific_info_s))){
        dump_flag = 1;
    }
    memcpy(&hdmirx_hw_stru.vendor_specific_info_pre, &hdmirx_hw_stru.vendor_specific_info, sizeof(struct vendor_specific_info_s));
}

static void dump_vendor_specific_info_frame(void)
{
    HDMIRX_HW_LOG("[HDMIRX Vendor Specific Info] identifier %x, video_format %x, 3d_structure %x, ext_data %x\n",
        hdmirx_hw_stru.vendor_specific_info.identifier,
        hdmirx_hw_stru.vendor_specific_info.hdmi_video_format,
        hdmirx_hw_stru.vendor_specific_info._3d_structure,
        hdmirx_hw_stru.vendor_specific_info._3d_ext_data );
}

static unsigned char dump_temp_buffer[128];
static void dump_tmds_clk(void)
{
    int i;
    int tmplen = 0;
    HDMIRX_HW_LOG("[HDMIRX Tmds Clk]:\n");
    for(i=0; i<TMDS_CLK_HIS_SIZE; i++){
        tmplen += sprintf(dump_temp_buffer+tmplen, "%d ", hdmirx_hw_stru.tmds_clk[i]);
        if(((i+1)&0xf)==0){
            HDMIRX_HW_LOG("[HDMIRX]%s\n", dump_temp_buffer);
            tmplen = 0;
        }
    }
}

static void dump_ecc_status(void)
{
    HDMIRX_HW_LOG("[HDMIRX ECC Status] %02x %02x %02x %02x %02x\n",
        hdmi_rd_reg(RX_BASE_ADDR+0x170),
        hdmi_rd_reg(RX_BASE_ADDR+0x171),
        hdmi_rd_reg(RX_BASE_ADDR+0x172),
        hdmi_rd_reg(RX_BASE_ADDR+0x173),
        hdmi_rd_reg(RX_BASE_ADDR+0x174));
}

static void dump_state(unsigned int flag)
{
#ifdef FIFO_BYPASS
        HDMIRX_HW_LOG("[HDMIRX] fifo_bypass ver %s, [0xa1]=%x\n", HDMIRX_VER, hdmi_rd_reg(RX_BASE_ADDR+0xA1));
#elif (defined FIFO_ENABLE_AFTER_RESET)
        HDMIRX_HW_LOG("[HDMIRX] fifo_enable_after_reset ver %s, [0xa1]=%x\n", HDMIRX_VER, hdmi_rd_reg(RX_BASE_ADDR+0xA1));
#else
        HDMIRX_HW_LOG("[HDMIRX] ver %s, eq_mode=%d, internal_mode=%x, eq_confit=%x, [0xa1]=%x\n", HDMIRX_VER,
                eq_mode, internal_mode, eq_config,
                hdmi_rd_reg(RX_BASE_ADDR+0xA1));
#endif
        if(flag&DUMP_FLAG_VIDEO_TIMING){
            read_video_timing();
            dump_video_timing();
        }
        if(flag&DUMP_FLAG_AVI_INFO){
            read_avi_info();
            dump_avi_info();
        }
        if(flag&DUMP_FLAG_VENDOR_SPEC_INFO){
            read_vendor_specific_info_frame();
            dump_vendor_specific_info_frame();
        }
        if(flag&DUMP_FLAG_AUDIO_INFO){
            read_audio_info();
            dump_audio_info();
        }
        if(flag&DUMP_FLAG_CLOCK_INFO){
            HDMIRX_HW_LOG("[HDMIRX] tmds clock %d (%d), pixel clock %d, active pixels %d, active lines %d, [ALIGN_STATUS]=%x [AUTH_STATUS]=%x\n",
                TMDS_CLK(hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1]),
                hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1],
                PIXEL_CLK,
                hdmirx_hw_stru.video_status.active_pixels,
                hdmirx_hw_stru.video_status.active_lines,
                hdmi_rd_reg(RX_BASE_ADDR+0x1fb),
                hdmi_rd_reg(RX_HDCP_ST_STATUS_3)
            );
        }
        if(flag&DUMP_FLAG_ECC_STATUS){
            dump_ecc_status();
        }
}

#ifdef CONFIG_AML_AUDIO_DSP
#define M2B_IRQ0_DSP_AUDIO_EFFECT (7)
#define DSP_CMD_SET_HDMI_SR   (6)
extern int  mailbox_send_audiodsp(int overwrite,int num,int cmd,const char *data,int len);
#endif

#ifndef HPD_AUTO_MODE
#define  DEASERT_HPD()  hdmi_wr_reg(RX_HDCP_CONFIG0, hdmi_rd_reg(RX_HDCP_CONFIG0)&(~(1<<3)))
#else
#define  DEASERT_HPD()
#endif

void hdmirx_hw_monitor(void)
{
    unsigned int tx_5v_status;
    int pre_sample_rate;
    if(sm_pause){
        return;
    }
    else if(hdmirx_hw_stru.prbs_enable){
        hdmirx_hw_stru.prbs_check_wait_time -= HW_MONITOR_TIME_UNIT;
        if(hdmirx_hw_stru.prbs_check_wait_time<=0){
            hdmirx_hw_stru.prbs_check_wait_time = 1000;
            hdmi_rx_prbs_detect(hdmirx_hw_stru.prbs_mode);
	      }
        return;
    }
    else{
        int prbs_mode_tmp = 0xf;
        switch(hdmirx_hw_stru.port){
            case 0x1:
                prbs_mode_tmp = prbs_port_mode&0xf;
                break;
            case 0x2:
                prbs_mode_tmp = (prbs_port_mode>>4)&0xf;
                break;
            case 0x4:
                prbs_mode_tmp = (prbs_port_mode>>8)&0xf;
                break;
        }                
        if(prbs_mode_tmp != 0xf){
            turn_on_prbs_mode(prbs_mode_tmp);
            return;
        }   
    }

    tx_5v_status = hdmi_rd_reg(OTHER_BASE_ADDR + HDMI_OTHER_STATUS0)&hdmirx_hw_stru.port;
    switch(hdmirx_hw_stru.state){
        case HDMIRX_HWSTATE_INIT:
            {
                Wr(RESET2_REGISTER, Rd(RESET2_REGISTER)|(1<<15));
                mdelay(10);
                Wr(RESET2_REGISTER, Rd(RESET2_REGISTER)&(~(1<<15)));

                hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(3<<6));
                hdmi_init();

                hdmirx_hw_stru.state = HDMIRX_HWSTATE_5V_LOW;

                HDMIRX_HW_LOG("[HDMIRX State] init->5v low\n");
            }
            break;

        case HDMIRX_HWSTATE_5V_LOW:
            if(tx_5v_status!=0){
                hdmirx_audio_recover_reset(); //make unplug can recover audio problem of LS_DIG2090__DVP_5986K
                hdmirx_hw_stru.state = HDMIRX_HWSTATE_5V_HIGH;
                HDMIRX_HW_LOG("[HDMIRX State] 5v low->5v high\n");
            }
            break;

        case HDMIRX_HWSTATE_5V_HIGH:
            if(tx_5v_status==0){
                hdmirx_hw_stru.state = HDMIRX_HWSTATE_5V_LOW;
                DEASERT_HPD();
                phy_powerdown(0);
                HDMIRX_HW_LOG("[HDMIRX State] 5v high->5v low\n");
            }
            else{
#if 1
                if(hpd_start_time)
                    mdelay(hpd_start_time);
#endif
#ifndef HPD_AUTO_MODE
                hdmi_wr_reg(RX_HDCP_CONFIG0, hdmi_rd_reg(RX_HDCP_CONFIG0)|(1<<3));
                phy_poweron();
                if((hdmi_rd_reg(RX_HDCP_ST_STATUS_0)&0x2)==0)
                    break;
#endif
                hdmirx_hw_stru.state = HDMIRX_HWSTATE_HPD_READY;
                HDMIRX_HW_LOG("[HDMIRX State] 5v high->hpd ready\n");
                hdmirx_hw_stru.hpd_wait_time = hpd_ready_time;
            }
            break;
        case HDMIRX_HWSTATE_HPD_READY:
            if(tx_5v_status==0){
                hdmirx_hw_stru.state = HDMIRX_HWSTATE_5V_LOW;
                DEASERT_HPD();
                phy_powerdown(0);
                HDMIRX_HW_LOG("[HDMIRX State] hpd ready ->5v low\n");
            }
            else{
                hdmirx_hw_stru.hpd_wait_time -= HW_MONITOR_TIME_UNIT;
                if(hdmirx_hw_stru.hpd_wait_time < 0){
                    hdmirx_hw_stru.state = HDMIRX_HWSTATE_SIG_UNSTABLE;
                    hdmirx_hw_stru.cur_color_depth = 0xff;
                    hdmirx_hw_stru.guess_vic = 0;
                    hdmirx_hw_stru.clk_stable_count = 0;
                    HDMIRX_HW_LOG("[HDMIRX State] hpd ready->unstable\n");
                    eq_mode_monitor(); //call it before hdmirx_phy_init(), which will set eq register
                    hdmirx_phy_init();
#ifdef RESET_AFTER_CLK_STABLE
                    reset_flag = 0;
#endif
                }
            }
            break;

        case HDMIRX_HWSTATE_SIG_UNSTABLE:
            if(tx_5v_status==0){
                hdmirx_hw_stru.state = HDMIRX_HWSTATE_5V_LOW;
                DEASERT_HPD();
                phy_powerdown(0);
                HDMIRX_HW_LOG("[HDMIRX State] unstable->5v low\n");
            }
            else{
                read_tmds_clk();
                if(is_tmds_clock_stable(16, clk_stable_threshold)){
                    hdmirx_hw_stru.clk_stable_count++;
#ifdef RESET_AFTER_CLK_STABLE
                    if(reset_flag == 0){
                        if(reset_mode==1){
                            hdmirx_reset_clock();
                        }
                        else if(reset_mode==2){
                            hdmirx_reset();
                        }
                        else if(reset_mode==3){
                            hdmirx_phy_init();
                        }
                        else if(reset_mode==4){
                            hdmi_init();
                            hdmirx_phy_init();
                        }
                        else if(reset_mode==5){
                            hdmirx_reset_digital();
                        }
                        if(reset_mode){
                            HDMIRX_HW_LOG("[HDMIRX] reset%d after clk stable\n",reset_mode);
                        }
                        reset_flag = 1;
                    }
#endif
                    read_video_timing();
                    read_avi_info();
                    hdmirx_config_color_depth(get_color_depth());
                    if(wait_signal_ok()==0){
                        hdmirx_config_color();

                        if(hdmirx_hw_stru.avi_info.vic == 0){
                            hdmirx_hw_stru.guess_vic = get_vic_from_timing();
                            HDMIRX_HW_LOG("[HDMIRX] guess vic %d\n", hdmirx_hw_stru.guess_vic);
                        }

                        hdmirx_hw_stru.avi_info_change_flag = 0;
                        hdmirx_hw_stru.unstable_irq_count = 0;
                        hdmirx_hw_stru.aud_info.real_sample_rate = 0;
                        hdmirx_hw_stru.audio_sample_rate_stable_count = audio_sample_rate_stable_count_th;
                        hdmirx_hw_stru.audio_wait_time = 0;
                        hdmirx_hw_stru.audio_reset_release_flag = 0;
#ifdef FOR_LS_DIG2090__DVP_5986K
                        hdmirx_hw_stru.aud_channel_status_modify_count = 0;
                        hdmirx_hw_stru.aud_channel_status_unmodify_count = 0;
                        hdmirx_hw_stru.aud_channel_status_all_0_count = 0;
                        hdmirx_hw_stru.aud_buf_ptr_change_count = 0;
                        hdmirx_hw_stru.aud_ok_flag = 0;
#endif
                        hdmirx_hw_stru.state = HDMIRX_HWSTATE_SIG_STABLE;

                        HDMIRX_HW_LOG("[HDMIRX State] unstable->stable\n");

                        dump_state(DUMP_FLAG_VIDEO_TIMING|DUMP_FLAG_AVI_INFO|DUMP_FLAG_VENDOR_SPEC_INFO|DUMP_FLAG_CLOCK_INFO|DUMP_FLAG_ECC_STATUS);
                        pr_info("[HDMIRX] tmds clock %d (%d), pixel clock %d, active pixels %d, active lines %d\n",
                            TMDS_CLK(hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1]),
                            hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1],
                            PIXEL_CLK,
                            hdmirx_hw_stru.video_status.active_pixels,
                            hdmirx_hw_stru.video_status.active_lines
                        );
                    }
                }
                else{
                    hdmirx_hw_stru.clk_stable_count = 0;
                }
                if(eq_mode_monitor()>0){
                    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x5, 0);
                    DEASERT_HPD();
                    mdelay(100);
                    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x5,hdmirx_hw_stru.port);

                    hdmirx_hw_stru.state = HDMIRX_HWSTATE_5V_LOW;
                    HDMIRX_HW_LOG("[HDMIRX] eq change to %d\n", eq_mode);
                    HDMIRX_HW_LOG("[HDMIRX State] unstable->5v low\n");
                }
            }
            break;
        case HDMIRX_HWSTATE_SIG_STABLE:
            if(tx_5v_status==0){
                set_hdmi_audio_source_gate(0);
                hdmirx_hw_stru.state = HDMIRX_HWSTATE_5V_LOW;
                DEASERT_HPD();
                phy_powerdown(0);
                HDMIRX_HW_LOG("[HDMIRX State] stable->5v low\n");
            }
            else if(is_tmds_clock_unstable(64, clk_unstable_threshold)
                ||(test_flag==2)){
                test_flag = 0;
                dump_tmds_clk();
                hdmirx_hw_stru.state = HDMIRX_HWSTATE_SIG_UNSTABLE;
                hdmirx_hw_stru.cur_color_depth = 0xff;
                hdmirx_hw_stru.guess_vic = 0;
                hdmirx_hw_stru.clk_stable_count = 0;

                HDMIRX_HW_LOG("[HDMIRX State] stable->unstable: unstable_irq_count %d\n",
                      hdmirx_hw_stru.unstable_irq_count);
                set_hdmi_audio_source_gate(0);
                if((switch_mode&0xf) == 0){
                    hdmirx_reset();
                }
                else if((switch_mode&0xf) == 1){
                    hdmirx_phy_init();
                }
#ifdef RESET_AFTER_CLK_STABLE
                reset_flag = 0;
#endif
            }
            else{
                read_tmds_clk();
                read_video_timing();

                read_avi_info();

                if(hdmirx_hw_stru.avi_info_change_flag){
                    hdmirx_config_color_depth(get_color_depth());
                    hdmirx_config_color();
                    hdmirx_hw_stru.avi_info_change_flag = 0;
                }

                hdmirx_hw_stru.vendor_specific_info_check_time += HW_MONITOR_TIME_UNIT;
                if(hdmirx_hw_stru.vendor_specific_info_check_time > vendor_specific_info_check_period){
                    read_vendor_specific_info_frame();
                    hdmirx_hw_stru.vendor_specific_info_check_time = 0;
                }
                if(is_tmds_clock_unstable(16, 2)){
                    dump_tmds_clk();
                }
#if 0
                if(dump_flag){
                    dump_avi_info();
                    dump_video_timing();
                    dump_vendor_specific_info_frame();
                    pr_info("[HDMIRX] tmds clock %d (%d), pixel clock %d, active pixels %d, active lines %d\n",
                        TMDS_CLK(hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1]),
                        hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1],
                        PIXEL_CLK,
                        hdmirx_hw_stru.video_status.active_pixels,
                        hdmirx_hw_stru.video_status.active_lines
                    );
                    dump_flag = 0;
                }
#endif
                if(audio_enable){
                    if(hdmirx_hw_stru.audio_reset_release_flag==0){
                        if(have_audio_info()||
                            is_use_audio_recover_clock()){
                            hdmirx_hw_stru.audio_reset_release_flag = audio_channel_status_stable_th;
                            set_hdmi_audio_source_gate(0);
                            hdmirx_release_audio_reset();
                        }
                    }
                    else if(hdmirx_hw_stru.audio_reset_release_flag>1){
                        hdmirx_hw_stru.audio_reset_release_flag--;
                    }
                    else{
                        pre_sample_rate = hdmirx_hw_stru.aud_info.real_sample_rate;
                        read_audio_info();

#ifdef FOR_LS_DIG2090__DVP_5986K
                        hdmirx_hw_stru.audio_check_time += HW_MONITOR_TIME_UNIT;
                        if(hdmirx_hw_stru.audio_check_time >= audio_check_period){
                            hdmirx_hw_stru.audio_check_time = 0;
                            check_audio();
                            if(((aud_channel_status_modify_th!=0)&&(hdmirx_hw_stru.aud_channel_status_modify_count>aud_channel_status_modify_th))
                                ||((aud_channel_status_all_0_th!=0)&&(hdmirx_hw_stru.aud_buf_ptr_change_count>0)&&(hdmirx_hw_stru.aud_channel_status_all_0_count>aud_channel_status_all_0_th))
                                ){
                                hdmirx_audio_recover_reset();

                                HDMIRX_HW_LOG("[HDMIRX]aud_channel_status_modify_count %d aud_channel_status_all_0_count %d aud_buf_ptr_change_count %d\n",
                                    hdmirx_hw_stru.aud_channel_status_modify_count,
                                    hdmirx_hw_stru.aud_channel_status_all_0_count,
                                    hdmirx_hw_stru.aud_buf_ptr_change_count);
                                HDMIRX_HW_LOG("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
                                HDMIRX_HW_LOG("[HDMIRX] reset HDMI_OTHER_CTRL0 bit 0\n");
                                HDMIRX_HW_LOG("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
                                hdmirx_hw_stru.aud_channel_status_modify_count = 0;
                                hdmirx_hw_stru.aud_channel_status_unmodify_count = 0;
                                hdmirx_hw_stru.aud_channel_status_all_0_count = 0;
                                hdmirx_hw_stru.aud_buf_ptr_change_count = 0;
                                break;
                            }
                        }
                        //if(hdmirx_hw_stru.aud_ok_flag == 0){
                            if(hdmi_rd_reg(RX_AUDIOST_AUDIO_STATUS)&0x2){
                                int rrr=0;
                                for(rrr=0; rrr<10; rrr++){
                                    if((hdmi_rd_reg(RX_AUDIOST_AUDIO_STATUS)&0x2)==0){
                                        break;
                                    }
                                    mdelay(20);
                                }
                                if(rrr==10){
                                    HDMIRX_HW_LOG("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
                                    HDMIRX_HW_LOG("[HDMIRX]RX_AUDIOST_AUDIO_STATUS is %x, reset HDMI_OTHER_CTRL0 bit 0\n",
                                                hdmi_rd_reg(RX_AUDIOST_AUDIO_STATUS));
                                    HDMIRX_HW_LOG("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
                                    hdmirx_audio_recover_reset();
                                }
                            }
                            /*
                            else{
                                int rrr=0;
                                for(rrr=0; rrr<5; rrr++){
                                    if(hdmi_rd_reg(RX_AUDIOST_AUDIO_STATUS)&0x2){
                                        break;
                                    }
                                    mdelay(20);
                                }
                                if(rrr==5){
                                    hdmirx_hw_stru.aud_ok_flag = 1;
                                }
                            }*/
                        //}
#endif
                        if(test_flag==1){
                            test_flag = 0;
                            set_hdmi_audio_source_gate(0);
                            DEASERT_HPD();
                            phy_powerdown(0);
                            HDMIRX_HW_LOG("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
                            HDMIRX_HW_LOG("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
                            HDMIRX_HW_LOG("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");

                            hdmirx_hw_stru.state = HDMIRX_HWSTATE_INIT;
                            HDMIRX_HW_LOG("[HDMIRX State] stable->init\n");
                            break;
                        }
                        if(is_sample_rate_change(pre_sample_rate, hdmirx_hw_stru.aud_info.real_sample_rate)){
                            set_hdmi_audio_source_gate(0);
                            dump_audio_info();
                            hdmirx_hw_stru.audio_sample_rate_stable_count = 0;
                        }
                        else{
                            if(hdmirx_hw_stru.audio_sample_rate_stable_count<audio_sample_rate_stable_count_th){
                                hdmirx_hw_stru.audio_sample_rate_stable_count++;
                                if(hdmirx_hw_stru.audio_sample_rate_stable_count==audio_sample_rate_stable_count_th){
#ifdef CONFIG_AML_AUDIO_DSP
                 		                mailbox_send_audiodsp(1, M2B_IRQ0_DSP_AUDIO_EFFECT, DSP_CMD_SET_HDMI_SR, (char *)&hdmirx_hw_stru.aud_info.real_sample_rate,sizeof(hdmirx_hw_stru.aud_info.real_sample_rate));
#endif
                                    hdmirx_hw_stru.audio_wait_time = audio_stable_time;
                                }
                            }
                        }
                        if(hdmirx_hw_stru.audio_wait_time > 0 ){
                            hdmirx_hw_stru.audio_wait_time -= HW_MONITOR_TIME_UNIT;
                            if(hdmirx_hw_stru.audio_wait_time <= 0){
                                set_hdmi_audio_source_gate(1);
                            }
                        }
                    }

                }
                hdmirx_hw_stru.cur_vic = hdmirx_hw_stru.avi_info.vic;
                hdmirx_hw_stru.cur_pixel_repeat = hdmirx_hw_stru.avi_info.pixel_repeat;

                if(eq_mode_monitor()>0){
                    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x5, 0);
                    DEASERT_HPD();
                    mdelay(100);
                    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x5,hdmirx_hw_stru.port);

                    hdmirx_hw_stru.state = HDMIRX_HWSTATE_5V_LOW;
                    HDMIRX_HW_LOG("[HDMIRX] eq change to %d\n", eq_mode);
                    HDMIRX_HW_LOG("[HDMIRX State] stable->5v low\n");
                }
            }
            break;
        default:
            if(tx_5v_status==0){
                hdmirx_hw_stru.state = HDMIRX_HWSTATE_5V_LOW;
                DEASERT_HPD();
                phy_powerdown(0);
            }
            break;
    }
}

bool hdmirx_hw_check_frame_skip(void)
{
    if(skip_unstable_frame){
        if(hdmirx_hw_stru.state == HDMIRX_HWSTATE_SIG_STABLE)
            return false;
        else
            return true;
    }
    return false;
}

bool hdmirx_hw_is_nosig(void)
{
    return ((hdmirx_hw_stru.state == HDMIRX_HWSTATE_5V_LOW) ||
            ((hdmirx_hw_stru.state == HDMIRX_HWSTATE_SIG_UNSTABLE) &&
             ((hdmirx_hw_stru.video_status.active_pixels == 0) ||
              (hdmirx_hw_stru.video_status.active_lines == 0) ||
              (hdmirx_hw_stru.clk_stable_count == 0))));
}

bool hdmirx_hw_pll_lock(void)
{
    return (hdmirx_hw_stru.state==HDMIRX_HWSTATE_SIG_STABLE);
}

void hdmirx_reset(void)
{
#ifndef FIFO_BYPASS
#ifdef FIFO_ENABLE_AFTER_RESET
    hdmi_wr_reg(RX_BASE_ADDR+0xA1, 0); // 0x0f
    mdelay(10);
#endif
#endif
    //reset tmds, pixel clock , bit 6,7
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(3<<6));

    mdelay(10);

    hdmi_wr_only_reg( RX_BASE_ADDR+0x866, hdmi_rd_reg(RX_BASE_ADDR+0x866)|0x80);
    hdmi_wr_only_reg( RX_BASE_ADDR+0x86e, hdmi_rd_reg(RX_BASE_ADDR+0x86e)|0x80);
    hdmi_wr_only_reg( RX_BASE_ADDR+0x876, hdmi_rd_reg(RX_BASE_ADDR+0x876)|0x80);
    hdmi_wr_only_reg( RX_BASE_ADDR+0x87e, hdmi_rd_reg(RX_BASE_ADDR+0x87e)|0x80);
    mdelay(10);

    hdmi_wr_only_reg( RX_BASE_ADDR+0x866, hdmi_rd_reg(RX_BASE_ADDR+0x866)&(~0x80));
    hdmi_wr_only_reg( RX_BASE_ADDR+0x86e, hdmi_rd_reg(RX_BASE_ADDR+0x86e)&(~0x80));
    hdmi_wr_only_reg( RX_BASE_ADDR+0x876, hdmi_rd_reg(RX_BASE_ADDR+0x876)&(~0x80));
    hdmi_wr_only_reg( RX_BASE_ADDR+0x87e, hdmi_rd_reg(RX_BASE_ADDR+0x87e)&(~0x80));
    mdelay(10);
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(1<<5)); //Write RX_AUDIO_MASTER_CONFIG_RSTN = 1
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(1<<4)); //Write RX_AUDIO_SAMPLE_CONFIG_RSTN = 1
    hdmi_wr_reg(RX_BASE_ADDR+0xe4, hdmi_rd_reg(RX_BASE_ADDR+0xe4)|(1<<7)); //reset i2s config

    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|0xf); //reset config ch0~3
    mdelay(10);

    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~0xf)); //release config ch0~3
    mdelay(10);
    //release tmds, pixel clock
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~(3<<6)));

#ifndef FIFO_BYPASS
#ifdef FIFO_ENABLE_AFTER_RESET
    mdelay(10);
    hdmi_wr_reg(RX_BASE_ADDR+0xA1, 0xf); // 0x0f
#endif
#endif
}

static void hdmirx_release_audio_reset(void)
{
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~(1<<5)));
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~(1<<4)));
    hdmi_wr_reg(RX_BASE_ADDR+0xe4, hdmi_rd_reg(RX_BASE_ADDR+0xe4)& (~(1<<7))); //release i2s config
}

static void hdmirx_reset_clock(void)
{

    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(3<<6));
    mdelay(10);
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~(3<<6)));

}

static void hdmirx_reset_digital(void)
{
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(3<<6));
    mdelay(10);
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|0xf); //reset config ch0~3
    mdelay(10);
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~0xf)); //release config ch0~3
    mdelay(10);
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~(3<<6)));

}


static void hdmirx_reset_pixel_clock(void)
{

    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(1<<7));
    mdelay(10);
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~(1<<7)));

}

static void hdmirx_audio_recover_reset(void)
{
    WRITE_APB_REG(HDMI_CTRL_PORT, READ_APB_REG(HDMI_CTRL_PORT)|(1<<16));
    hdmi_wr_reg(OTHER_BASE_ADDR+HDMI_OTHER_CTRL0, 0x1);
    mdelay(10);
    hdmi_wr_reg(OTHER_BASE_ADDR+HDMI_OTHER_CTRL0, 0x0);
    WRITE_APB_REG(HDMI_CTRL_PORT, READ_APB_REG(HDMI_CTRL_PORT)&(~(1<<16)));
}

static void hdmirx_unpack_recover(void)
{
    int timeout = 0;
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(3<<6));
    mdelay(10);
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~(1<<6))); //release tmds clock
    /*check tmds clock stable*/
    timeout = 0;
    while((hdmi_rd_reg(RX_BASE_ADDR+0x160)&0x80)!=0x80){
        mdelay(10);
        timeout++;
        if(timeout>1000){
            printk("+++++++++++++++++check tmds clock timeout\n");
            goto exit;
        }
    }
    timeout = 0;
    while((hdmi_rd_reg(RX_BASE_ADDR+0x161)&0x90)!=0x90){
        mdelay(10);
        timeout++;
        if(timeout>1000){
            printk("+++++++++++++++++check tmds clock timeout\n");
            goto exit;
        }
    }
    timeout = 0;
    while((hdmi_rd_reg(RX_BASE_ADDR+0x162)&0x80)!=0x80){
        mdelay(10);
        timeout++;
        if(timeout>1000){
            printk("+++++++++++++++++check tmds clock timeout\n");
            goto exit;
        }
    }
    timeout = 0;
    while((hdmi_rd_reg(RX_BASE_ADDR+0x163)&0x90)!=0x90){
        mdelay(10);
        timeout++;
        if(timeout>1000){
            printk("+++++++++++++++++check tmds clock timeout\n");
            goto exit;
        }
    }
    timeout = 0;
    while((hdmi_rd_reg(RX_BASE_ADDR+0x164)&0x80)!=0x80){
        mdelay(10);
        timeout++;
        if(timeout>1000){
            printk("+++++++++++++++++check tmds clock timeout\n");
            goto exit;
        }
    }
    timeout = 0;
    while((hdmi_rd_reg(RX_BASE_ADDR+0x165)&0x90)!=0x90){
        mdelay(10);
        timeout++;
        if(timeout>1000){
            printk("+++++++++++++++++check tmds clock timeout\n");
            goto exit;
        }
    }
    timeout = 0;
    while((hdmi_rd_reg(RX_BASE_ADDR+0x166)&0x80)!=0x80){
        mdelay(10);
        timeout++;
        if(timeout>1000){
            printk("+++++++++++++++++check tmds clock timeout\n");
            goto exit;
        }
    }
    timeout = 0;
    while((hdmi_rd_reg(RX_BASE_ADDR+0x167)&0x90)!=0x90){
        mdelay(10);
        timeout++;
        if(timeout>1000){
            printk("+++++++++++++++++check tmds clock timeout\n");
            goto exit;
        }
    }
    timeout = 0;
    while((hdmi_rd_reg(RX_BASE_ADDR+0x1fb)&0x3f)!=0x3f){
        mdelay(10);
        timeout++;
        if(timeout>1000){
            printk("+++++++++++++++++check tmds clock timeout\n");
            goto exit;
        }
    }
exit:
    if(timeout>1000){
        printk("[1fb] %x [160]~[167]:%x %x %x %x %x %x %x %x\n", hdmi_rd_reg(RX_BASE_ADDR+0x1fb),
            hdmi_rd_reg(RX_BASE_ADDR+0x160),
            hdmi_rd_reg(RX_BASE_ADDR+0x161),
            hdmi_rd_reg(RX_BASE_ADDR+0x162),
            hdmi_rd_reg(RX_BASE_ADDR+0x163),
            hdmi_rd_reg(RX_BASE_ADDR+0x164),
            hdmi_rd_reg(RX_BASE_ADDR+0x165),
            hdmi_rd_reg(RX_BASE_ADDR+0x166),
            hdmi_rd_reg(RX_BASE_ADDR+0x167)
            );
    }
    /**/
    mdelay(250);//wait at least two vsync
    hdmirx_config_color_depth(get_color_depth());
    hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~(1<<7))); //release pixel clock


}
/****************
* phy init sequence
* Amlogic_Init_sequence_Address_data.txt
*
***************/
static unsigned char use_general_eq = 1;

static void hdmirx_phy_init(void)
{
    use_general_eq = 1;
#ifndef FIFO_BYPASS
#ifdef FIFO_ENABLE_AFTER_RESET
    hdmi_wr_reg(RX_BASE_ADDR+0xA1, 0); // 0x0f
    mdelay(10);
#endif
#endif
    { //reset tmds, pixel clock , bit 6,7
        hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(3<<6));
    }

    HDMIRX_HW_LOG("[HDMIRX] Phy Init For M2B\n");
    hdmi_wr_only_reg(    RX_BASE_ADDR+0x5,hdmirx_hw_stru.port  );
    hdmi_wr_only_reg( RX_BASE_ADDR+0x866, hdmi_rd_reg(RX_BASE_ADDR+0x866)|0x80);
    hdmi_wr_only_reg( RX_BASE_ADDR+0x86e, hdmi_rd_reg(RX_BASE_ADDR+0x86e)|0x80);
    hdmi_wr_only_reg( RX_BASE_ADDR+0x876, hdmi_rd_reg(RX_BASE_ADDR+0x876)|0x80);
    hdmi_wr_only_reg( RX_BASE_ADDR+0x87e, hdmi_rd_reg(RX_BASE_ADDR+0x87e)|0x80);
    mdelay(delay1);//delay_us(1000000/10);

    hdmi_wr_only_reg( RX_BASE_ADDR+0x866, hdmi_rd_reg(RX_BASE_ADDR+0x866)&(~0x80));
    hdmi_wr_only_reg( RX_BASE_ADDR+0x86e, hdmi_rd_reg(RX_BASE_ADDR+0x86e)&(~0x80));
    hdmi_wr_only_reg( RX_BASE_ADDR+0x876, hdmi_rd_reg(RX_BASE_ADDR+0x876)&(~0x80));
    hdmi_wr_only_reg( RX_BASE_ADDR+0x87e, hdmi_rd_reg(RX_BASE_ADDR+0x87e)&(~0x80));
    mdelay(delay2); //delay_us(1000000/10);

    //MonsoonHDMI_set_Amlogic                         ## Register Name
    if(eq_enable){
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0xAC,0xC   );//  #RX_LS_CLK_SEL,3
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x8,0xFF   );//  #RX_EQ_EN_CH0_0_7,255
        if(eq_config&0x100){
            hdmi_wr_only_reg(    RX_BASE_ADDR+  0x9, 0xff  );//  #RX_EQ_EN_CH0_8_13,63
        }
        else{
            hdmi_wr_only_reg(    RX_BASE_ADDR+  0x9,/*0xff*/0x3F   );//  #RX_EQ_EN_CH0_8_13,63
        }
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0xA,0xFF   );//  #RX_EQ_EN_CH1_0_7,255
        if(eq_config&0x100){
            hdmi_wr_only_reg(    RX_BASE_ADDR+  0xB, 0xff  );//  #RX_EQ_EN_CH1_8_13,63
        }
        else{
            hdmi_wr_only_reg(    RX_BASE_ADDR+  0xB,/*0xff*/0x3F   );//  #RX_EQ_EN_CH1_8_13,63
        }
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0xC,0xFF   );//  #RX_EQ_EN_CH2_0_7,255
        if(eq_config&0x100){
            hdmi_wr_only_reg(    RX_BASE_ADDR+  0xD, 0xff  );//  #RX_EQ_EN_CH2_8_13,63
        }
        else{
            hdmi_wr_only_reg(    RX_BASE_ADDR+  0xD,/*0xff*/0x3F   );//  #RX_EQ_EN_CH2_8_13,63
        }
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0xE,0xFF   );//  #RX_EQ_EN_CH3_0_7,255
        if(eq_config&0x100){
            hdmi_wr_only_reg(    RX_BASE_ADDR+  0xF, 0xff   );//  #RX_EQ_EN_CH3_8_13,63
        }
        else{
            hdmi_wr_only_reg(    RX_BASE_ADDR+  0xF,/*0xff*/0x3F   );//  #RX_EQ_EN_CH3_8_13,63
        }
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x57,0x8   );//  #RX_EQ_MAP_SEL,1
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0xFE,0x80  );//  #RX_EQ_FORCED,1
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x863,(0x38&(~(7<<3)))|((eq_config&0xf)<<3) );//  #rx_rx0_eq_biastrimeq,7
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x86B,(0x38&(~(7<<3)))|((eq_config&0xf)<<3) );//  #rx_rx1_eq_biastrimeq,7
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x873,(0x38&(~(7<<3)))|((eq_config&0xf)<<3) );//  #rx_rx2_eq_biastrimeq,7
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x87B,(0x38&(~(7<<3)))|((eq_config&0xf)<<3) );//  #rx_rx3_eq_biastrimeq,7
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x863,(0x3F&(~(7<<3)))|((eq_config&0xf)<<3) );//  #rx_rx0_eq_biastrimsf,7
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x86B,(0x3F&(~(7<<3)))|((eq_config&0xf)<<3) );//  #rx_rx1_eq_biastrimsf,7
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x873,(0x3F&(~(7<<3)))|((eq_config&0xf)<<3) );//  #rx_rx2_eq_biastrimsf,7
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x87B,(0x3F&(~(7<<3)))|((eq_config&0xf)<<3) );//  #rx_rx3_eq_biastrimsf,7
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x860,0x5  );//  #rx_rx0_eq_eqcap1_trim,5
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x868,0x5  );//  #rx_rx1_eq_eqcap1_trim,5
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x870,0x5  );//  #rx_rx2_eq_eqcap1_trim,5
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x878,0x5  );//  #rx_rx3_eq_eqcap1_trim,5
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x861,0x5  );//  #rx_rx0_eq_eqcap2_trim,5
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x869,0x5  );//  #rx_rx1_eq_eqcap2_trim,5
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x871,0x5  );//  #rx_rx2_eq_eqcap2_trim,5
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x879,0x5  );//  #rx_rx3_eq_eqcap2_trim,5
    }
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x5,hdmirx_hw_stru.port ); // rx_port_en" , 4
    if((eq_config&0x10)==0){
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x863,0x20    ); // rx_rx0_eq_biastrimeq",4
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x86B,0x20	); // rx_rx1_eq_biastrimeq",4
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x873,0x20	); // rx_rx2_eq_biastrimeq",4
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x87B,0x20	); // rx_rx3_eq_biastrimeq",4
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x863,0x22	); // rx_rx0_eq_biastrimsf",2
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x86B,0x23	); // rx_rx1_eq_biastrimsf",3
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x873,0x23	); // rx_rx2_eq_biastrimsf",3
        hdmi_wr_only_reg(    RX_BASE_ADDR+  0x87B,0x23	); // rx_rx3_eq_biastrimsf",3
    }
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x864,0x10	); // rx_rx0_dp_pixel_clk_pd",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x86C,0x10	); // rx_rx1_dp_pixel_clk_pd",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x874,0x10	); // rx_rx2_dp_pixel_clk_pd",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x87C,0x10	); // rx_rx3_dp_pixel_clk_pd",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x865,0x80	); // rx_cdr0_hogg_adj",4
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x86D,0x80	); // rx_cdr1_hogg_adj",4
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x875,0x80	); // rx_cdr2_hogg_adj",4
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x87D,0x80	); // rx_cdr3_hogg_adj",4
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x865,0x90	); // rx_cdr0_dp_div_cfg",4
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x86D,0x9C	); // rx_cdr1_dp_div_cfg",7
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x875,0x90	); // rx_cdr2_dp_div_cfg",4
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x87D,0x90	); // rx_cdr3_dp_div_cfg",4
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x83,0x0         ); // Was 1 Not needed       ## rx_cdr1_en_clkfd",0
    //Monsoon_set_ana_RX
    //AnalogReceiver(hdrx)
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x99,0x7		); // rx_ra,7
    //'AUX(hdaux)
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x90,0x1		 ); // rx_rxaux_slope_cntrl",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x90,0x9		 ); // rx_rxaux_leak_cur_en",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x90,0x49		 ); // rx_rxaux_mvedge",1
    //''Reference(hdref)
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x95,0x2                                     ); // rx_rxref_cur_tun1",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x96,0x4		 ); // rx_rxref_test_vgapa",4
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x96,0x14		 ); // rx_rxref_test_vgapb",2
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x93,0x4		 ); // rx_rxg_ch_vsh",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x93,0x6		 ); // rx_rxg_aux_vsh",2
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x93,0x46		 ); // rx_rxg_shield_en",1
    //'CDR
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x857,0x80      ); // moved from 0x81,0x80      ); // "rx_cdr0_en_clkfd_loop",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x859,0x80      ); // moved from 0x83,0x80	  ); // "rx_cdr1_en_clkfd_loop",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x85B,0x80      ); // moved from 0x85,0x80	  ); // "rx_cdr2_en_clkfd_loop",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x85D,0x80      ); // moved from 0x87,0x80	  ); // "rx_cdr3_en_clkfd_loop",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x857,0xC0      ); // moved from 0x81,0xC0	  ## "rx_cdr0_en_datafd_loop",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x859,0xC0      ); // moved from 0x83,0xC0	  ## "rx_cdr1_en_datafd_loop",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x85B,0xC0      ); // moved from 0x85,0xC0	  ## "rx_cdr2_en_datafd_loop",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x85D,0xC0      ); // moved from 0x87,0xC0	  ## "rx_cdr3_en_datafd_loop",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x857,0xE0      ); // moved from 0x81,0xE0	  ## "rx_cdr0_pd_loop",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x859,0xE0      ); // moved from 0x83,0xE0	  ## "rx_cdr1_pd_loop",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x85B,0xE0      ); // moved from 0x85,0xE0	  ## "rx_cdr2_pd_loop",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x85D,0xE0      ); // moved from 0x87,0xE0	  ## "rx_cdr3_pd_loop",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x857,0xF0      ); // moved from 0x81,0xF0	  ## "rx_cdr0_datfd_timer",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x859,0xF0      ); // moved from 0x83,0xF0	  ## "rx_cdr1_datafd_timer",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x85B,0xF0      ); // moved from 0x85,0xF0	  ## "rx_cdr2_datafd_timer",1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x85D,0xF0      ); // moved from 0x87,0xF0	  ## "rx_cdr3_datafd_timer",1
    //'rx_lab_default
    //'Rescalibration
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xA0,0x0                                     ); // "RX_HDMIDP_AFE_CONN_RCALIB_MODE" , 0
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x98,0x1		 ); // "rx_rcalib_mask" , 1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x98,0x9		 ); // "rx_rcalib_trigger" , 2
    //sleep 0.5						  ##
    mdelay(delay3); //delay_us(500000);
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x98,0x1		 ); // "rx_rcalib_trigger" , 0
    //'RX_DIG_Clockssetting					  ); //
#ifdef FIFO_BYPASS
    hdmi_wr_reg(RX_BASE_ADDR+0xA1, 0xf0);
#elif (!(defined FIFO_ENABLE_AFTER_RESET))
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xA1,0xF		 ); // "RX_HDMIDP_AFE_CONN_FIFO_EN" , 15
#endif
    //'Sleep1
    mdelay(delay4); //delay_us(1000000);
    //MonsoonHDMI_set_ana_RX
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x866,0x20	  ); // Stay at the same place   ); // "RX_CDR0_EN_CLK_CH" , 1
    //Nochannelswapping:
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xA4,0x0		 ); // "RX_HDMIDP_AFE_CONN_CH_SW_0_7", 0
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xA5,0xE4		 ); // "RX_HDMIDP_AFE_CONN_CH_SW_8_15", 228
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xA6,0x88		 ); // "RX_HDMIDP_AFE_CONN_CH_SW_16_23", 136
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xA7,0x88		 ); // "RX_HDMIDP_AFE_CONN_CH_SW_24_31", 136
    //Monson_digitalx3
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x5,hdmirx_hw_stru.port     ); // "rx_port_en" , 4
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xA0,0x70 );

    {
        hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(1<<5)); //Write RX_AUDIO_MASTER_CONFIG_RSTN = 1
        hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(1<<4)); //Write RX_AUDIO_SAMPLE_CONFIG_RSTN = 1
        hdmi_wr_reg(RX_BASE_ADDR+0xe4, hdmi_rd_reg(RX_BASE_ADDR+0xe4)|(1<<7)); //reset i2s config

        hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|0xf); //reset config ch0~3
        mdelay(10);
        hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~0xf)); //release config ch0~3
        mdelay(10);
        //release tmds, pixel clock
        hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~(3<<6)));
    }
#ifndef FIFO_BYPASS
#ifdef FIFO_ENABLE_AFTER_RESET
    mdelay(10);

    hdmi_wr_reg(RX_BASE_ADDR+0xA1, 0xf); // 0x0f
#endif
#endif
}

static void set_eq_27M(int idx1, int idx2)
{ /* idx1 from 0 to 7, idx2 from 0 to 31 */
    HDMIRX_HW_LOG("[HDMIRX] PIXEL_CLK is %d, call %s\n",PIXEL_CLK, __FUNCTION__);
//Enable EQ
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x8,0xFF   );//  #RX_EQ_EN_CH0_0_7,255
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x9,/*0xff*/0x3F   );//  #RX_EQ_EN_CH0_8_13,63
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xA,0xFF   );//  #RX_EQ_EN_CH1_0_7,255
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xB,/*0xff*/0x3F   );//  #RX_EQ_EN_CH1_8_13,63
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xC,0xFF   );//  #RX_EQ_EN_CH2_0_7,255
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xD,/*0xff*/0x3F   );//  #RX_EQ_EN_CH2_8_13,63
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xE,0xFF   );//  #RX_EQ_EN_CH3_0_7,255
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xF,/*0xff*/0x3F   );//  #RX_EQ_EN_CH3_8_13,63

    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x57,0x8   );//  #RX_EQ_MAP_SEL,1
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0xFE,0x80  );//  #RX_EQ_FORCED,1
 

// Set EQRS + rx(x)_Spare bit 0
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x62,0x10|(((idx1>>2)&0x1)<<2)|(((idx1>>1)&0x1)<<0));
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x64,0x10|(((idx1>>0)&0x1)<<0));
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x6A,0x10|(((idx1>>2)&0x1)<<2)|(((idx1>>1)&0x1)<<0)   );
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x6C,0x10|(((idx1>>0)&0x1)<<0)   );
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x72,0x10|(((idx1>>2)&0x1)<<2)|(((idx1>>1)&0x1)<<0)   );
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x74,0x10|(((idx1>>0)&0x1)<<0)   );
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x7A,0x10|(((idx1>>2)&0x1)<<2)|(((idx1>>1)&0x1)<<0)   );
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x7C,0x10|(((idx1>>0)&0x1)<<0)   );

//Set equalizers Option 1:
// Set Eq to diffrent num eq_cp_trim = 1

    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x60,(((idx2>>5)&0x1)<<2)|(((idx2>>4)&0x1)<<1)   );
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x61,(((idx2>>3)&0x1)<<4)|(((idx2>>2)&0x1)<<3)|(((idx2>>1)&0x1)<<1)|(((idx2>>0)&0x1)<<0));
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x68,(((idx2>>5)&0x1)<<2)|(((idx2>>4)&0x1)<<1)   );
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x69,(((idx2>>3)&0x1)<<4)|(((idx2>>2)&0x1)<<3)|(((idx2>>1)&0x1)<<1)|(((idx2>>0)&0x1)<<0)   );
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x70,(((idx2>>5)&0x1)<<2)|(((idx2>>4)&0x1)<<1)   );
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x71,(((idx2>>3)&0x1)<<4)|(((idx2>>2)&0x1)<<3)|(((idx2>>1)&0x1)<<1)|(((idx2>>0)&0x1)<<0)   );
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x78,(((idx2>>5)&0x1)<<2)|(((idx2>>4)&0x1)<<1)   );
    hdmi_wr_only_reg(    RX_BASE_ADDR+  0x79,(((idx2>>3)&0x1)<<4)|(((idx2>>2)&0x1)<<3)|(((idx2>>1)&0x1)<<1)|(((idx2>>0)&0x1)<<0)   );
}

typedef struct {
    unsigned int adr;
    unsigned int val; 
    unsigned int val_bak;   
}eq_setting_t;
static eq_setting_t eq_27M[]=
{
// bypass_hf bit[5] and dig_hf bit[3]  (bypass_hf=1 and dig_hf=1)
{ RX_BASE_ADDR+ 0x856, 0x28, 0 },
{ RX_BASE_ADDR+ 0x858, 0x28, 0 },
{ RX_BASE_ADDR+ 0x85A, 0x28, 0 },
{ RX_BASE_ADDR+ 0x85C, 0x28, 0 },

// biastrimsf bits [2:0] + biastrim_eq bits [5:3] (eq_biastrimsf = 3¡¯b001 and Biastrim_eq=3¡¯b111)
{ RX_BASE_ADDR+ 0x863, 0x39, 0 },
{ RX_BASE_ADDR+ 0x86B, 0x39, 0 },
{ RX_BASE_ADDR+ 0x873, 0x39, 0 },
{ RX_BASE_ADDR+ 0x87B, 0x39, 0 },

// EQ_EQRS [5:2] + [1:0]  = 6¡¯b11_1111
{ RX_BASE_ADDR+ 0x862, 0x3C, 0 },
{ RX_BASE_ADDR+ 0x86A, 0x3C, 0 },
{ RX_BASE_ADDR+ 0x872, 0x3C, 0 },
{ RX_BASE_ADDR+ 0x87A, 0x3C, 0 },
{ RX_BASE_ADDR+ 0x864, 0x13, 0 },
{ RX_BASE_ADDR+ 0x86C, 0x13, 0 },
{ RX_BASE_ADDR+ 0x874, 0x13, 0 },
{ RX_BASE_ADDR+ 0x87C, 0x13, 0 },

// EQ_VCNTR_TRIM + EQ_CAP_TRIM           (eq_vcntr_trim=3¡¯b111 eq_cap_trim=6¡¯b11_1111)
{ RX_BASE_ADDR+ 0x860, 0xFF, 0 },
{ RX_BASE_ADDR+ 0x861, 0x3 , 0 },
{ RX_BASE_ADDR+ 0x868, 0xFF, 0 },
{ RX_BASE_ADDR+ 0x869, 0x3 , 0 },
{ RX_BASE_ADDR+ 0x870, 0xFF, 0 },
{ RX_BASE_ADDR+ 0x871, 0x3 , 0 },
{ RX_BASE_ADDR+ 0x878, 0xFF, 0 },
{ RX_BASE_ADDR+ 0x879, 0x3 , 0 },
{0, 0, 0}
};

static void set_eq_27M_2(void)
{
 int i;
 HDMIRX_HW_LOG("[HDMIRX] call %s()\n",__func__);
 if(use_general_eq == 1){
     for( i=0; eq_27M[i].adr; i++){
        eq_27M[i].val_bak = hdmi_rd_reg(eq_27M[i].adr);
     }
 }
 for( i=0; eq_27M[i].adr; i++){
    hdmi_wr_only_reg(eq_27M[i].adr, eq_27M[i].val);
 }
 use_general_eq = 0;
}

static void restore_eq_gen(void)
{
 int i;
 if(use_general_eq == 0){
     HDMIRX_HW_LOG("[HDMIRX] call %s()\n",__func__);
     for( i=0; eq_27M[i].adr; i++){
        hdmi_wr_only_reg(eq_27M[i].adr, eq_27M[i].val_bak);
     }
     use_general_eq = 1;
 }
}

/****************
* debug function
*
***************/

static unsigned long clk_util_clk_msr( unsigned long   clk_mux, unsigned long   uS_gate_time )
{
    unsigned long dummy_rd;
    unsigned long   measured_val;
    unsigned long timeout = 0;
    // Set the measurement gate to 100uS
    Wr(MSR_CLK_REG0, (Rd(MSR_CLK_REG0) & ~(0xFFFF << 0)) | ((uS_gate_time-1) << 0) );
    // Disable continuous measurement
    // disable interrupts
    Wr(MSR_CLK_REG0, (Rd(MSR_CLK_REG0) & ~((1 << 18) | (1 << 17))) );
    Wr(MSR_CLK_REG0, (Rd(MSR_CLK_REG0) & ~(0xf << 20)) | ((clk_mux << 20) |  // Select MUX
                                                          (1 << 19) |     // enable the clock
                                                          (1 << 16)) );    // enable measuring
    dummy_rd = Rd(MSR_CLK_REG0);
    // Wait for the measurement to be done
    while( (Rd(MSR_CLK_REG0) & (1 << 31)) ) {
        mdelay(10);
        timeout++;
        if(timeout>10){
            return 0;
        }
    }
    // disable measuring
    Wr(MSR_CLK_REG0, (Rd(MSR_CLK_REG0) & ~(1 << 16)) | (0 << 16) );

    measured_val = Rd(MSR_CLK_REG2);
    if( measured_val == 65535 ) {
        return(0);
    } else {
        // Return value in Hz
        return(measured_val*(1000000/uS_gate_time));
    }
}

//int msr_par1=11,msr_par2=50; //ddr_pll_clk
#if 0
static unsigned monitor_register_adr[]=
{
    RX_BASE_ADDR+0x160,
    RX_BASE_ADDR+0x161,
    RX_BASE_ADDR+0x162,
    RX_BASE_ADDR+0x163,
    RX_BASE_ADDR+0x164,
    RX_BASE_ADDR+0x165,
    RX_BASE_ADDR+0x166,
    RX_BASE_ADDR+0x167,

    RX_BASE_ADDR+0x856,
    RX_BASE_ADDR+0x857,
    RX_BASE_ADDR+0x858,
    RX_BASE_ADDR+0x859,
    RX_BASE_ADDR+0x85a,
    RX_BASE_ADDR+0x85b,
    RX_BASE_ADDR+0x85c,
    RX_BASE_ADDR+0x85d,

    RX_BASE_ADDR+0x1fb,

    RX_BASE_ADDR+0x1a0,
    RX_BASE_ADDR+0x1a1,
    RX_BASE_ADDR+0x1a2,
    RX_BASE_ADDR+0x1a3,
    RX_BASE_ADDR+0x1a4,
    RX_BASE_ADDR+0x1a5,
    RX_BASE_ADDR+0x1a6,
    RX_BASE_ADDR+0x1a7,
    RX_BASE_ADDR+0x1a8,
    RX_BASE_ADDR+0x1a9,

};

static unsigned monitor_register_val[128];

static void hdmirx_monitor_reg_init(void)
{
    int i;
    int n=sizeof(monitor_register_adr)/sizeof(unsigned);
    for(i=0;i<n;i++){
        monitor_register_val[i]=0xffffffff;
    }
}

/*
static void hdmirx_monitor_clock(void)
{
    int clktmp;
    clktmp=clk_util_clk_msr(msr_par1,msr_par2);
    HDMIRX_HW_LOG("[clk_util_clk_msr(%d,%d)]=%d, unstable count=%d\n[msr_tmds]=%d, [0x1f7]=%x\n",
         msr_par1,msr_par2, clktmp, hdmirx_hw_stru.unstable_irq_count,
         hdmirx_hw_stru.tmds_clk[TMDS_CLK_HIS_SIZE-1], hdmi_rd_reg(RX_BASE_ADDR+0x1f7));
}
*/

static void hdimrx_monitor_register(void)
{
    unsigned char print_buf[128];
    int n=sizeof(monitor_register_adr)/sizeof(unsigned);
    unsigned tmp;
    int i;
    unsigned long flags;

    spin_lock_irqsave(&hdmi_print_lock, flags);
    for(i=0;i<n;i++){
        tmp=hdmi_rd_reg(monitor_register_adr[i]);
        if(tmp!=monitor_register_val[i]){
            sprintf(print_buf,"[%x(%x)]=%x\n", monitor_register_adr[i], monitor_register_val[i], tmp);
            HDMIRX_HW_LOG(print_buf);
            monitor_register_val[i]=tmp;
        }
    }
    spin_unlock_irqrestore(&hdmi_print_lock, flags);
}
#endif

/*
* Debug
*/
static void dump_reg(void)
{
    int i;
    for(i=0; i<0x900; i+=16){
        printk("[HDMIRX]%03x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            i,hdmi_rd_reg(RX_BASE_ADDR+i), hdmi_rd_reg(RX_BASE_ADDR+i+1), hdmi_rd_reg(RX_BASE_ADDR+i+2),hdmi_rd_reg(RX_BASE_ADDR+i+3),
            hdmi_rd_reg(RX_BASE_ADDR+i+4), hdmi_rd_reg(RX_BASE_ADDR+i+5), hdmi_rd_reg(RX_BASE_ADDR+i+6),hdmi_rd_reg(RX_BASE_ADDR+i+7),
            hdmi_rd_reg(RX_BASE_ADDR+i+8), hdmi_rd_reg(RX_BASE_ADDR+i+9), hdmi_rd_reg(RX_BASE_ADDR+i+10),hdmi_rd_reg(RX_BASE_ADDR+i+11),
            hdmi_rd_reg(RX_BASE_ADDR+i+12), hdmi_rd_reg(RX_BASE_ADDR+i+13), hdmi_rd_reg(RX_BASE_ADDR+i+14),hdmi_rd_reg(RX_BASE_ADDR+i+15));
    }    
    printk("[HDMIRX OTHER_BASE_ADDR] %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            hdmi_rd_reg(OTHER_BASE_ADDR), hdmi_rd_reg(OTHER_BASE_ADDR+1), hdmi_rd_reg(OTHER_BASE_ADDR+2),hdmi_rd_reg(OTHER_BASE_ADDR+3),
            hdmi_rd_reg(OTHER_BASE_ADDR+4), hdmi_rd_reg(OTHER_BASE_ADDR+5), hdmi_rd_reg(OTHER_BASE_ADDR+6),hdmi_rd_reg(OTHER_BASE_ADDR+7),
            hdmi_rd_reg(OTHER_BASE_ADDR+8), hdmi_rd_reg(OTHER_BASE_ADDR+9), hdmi_rd_reg(OTHER_BASE_ADDR+10),hdmi_rd_reg(OTHER_BASE_ADDR+11));

    i = 0;
    printk("[CECCFG]%03x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            i,hdmi_rd_reg(CEC0_BASE_ADDR+i),hdmi_rd_reg(CEC0_BASE_ADDR+i+1),hdmi_rd_reg(CEC0_BASE_ADDR+i+2),hdmi_rd_reg(CEC0_BASE_ADDR+i+3),
            hdmi_rd_reg(CEC0_BASE_ADDR+i+4),hdmi_rd_reg(CEC0_BASE_ADDR+i+5),hdmi_rd_reg(CEC0_BASE_ADDR+i+6),hdmi_rd_reg(CEC0_BASE_ADDR+i+7),
            hdmi_rd_reg(CEC0_BASE_ADDR+i+8),hdmi_rd_reg(CEC0_BASE_ADDR+i+9),hdmi_rd_reg(CEC0_BASE_ADDR+i+10),hdmi_rd_reg(CEC0_BASE_ADDR+i+11),
            hdmi_rd_reg(CEC0_BASE_ADDR+i+12),hdmi_rd_reg(CEC0_BASE_ADDR+i+13),hdmi_rd_reg(CEC0_BASE_ADDR+i+14),hdmi_rd_reg(CEC0_BASE_ADDR+i+15));
    i = 16;
    printk("[CECCFG]%03x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            i,hdmi_rd_reg(CEC0_BASE_ADDR+i),hdmi_rd_reg(CEC0_BASE_ADDR+i+1),hdmi_rd_reg(CEC0_BASE_ADDR+i+2),hdmi_rd_reg(CEC0_BASE_ADDR+i+3),
            hdmi_rd_reg(CEC0_BASE_ADDR+i+4),hdmi_rd_reg(CEC0_BASE_ADDR+i+5),hdmi_rd_reg(CEC0_BASE_ADDR+i+6),hdmi_rd_reg(CEC0_BASE_ADDR+i+7),
            hdmi_rd_reg(CEC0_BASE_ADDR+i+8),hdmi_rd_reg(CEC0_BASE_ADDR+i+9),hdmi_rd_reg(CEC0_BASE_ADDR+i+10),hdmi_rd_reg(CEC0_BASE_ADDR+i+11),
            hdmi_rd_reg(CEC0_BASE_ADDR+i+12));

    i = 0;
    printk("[CECSTU]%03x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            i,hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+1),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+2),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+3),
            hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+4),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+5),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+6),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+7),
            hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+8),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+9),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+10),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+11),
            hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+12),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+13),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+14),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+15));
    i = 16;
    printk("[CECSTU]%03x %02x %02x %02x %02x %02x\n",
            i,hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+1),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+2),hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+3),
            hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_0_HEADER+i+4));
}

static int sample_rate_temp;
static unsigned char eq_27M_index=0;
int hdmirx_debug(const char* buf, int size)
{
    char tmpbuf[128];
    int i=0;
    unsigned int adr;
    unsigned int value=0;
    while((buf[i])&&(buf[i]!=',')&&(buf[i]!=' ')){
        tmpbuf[i]=buf[i];
        i++;
    }
    tmpbuf[i]=0;
    if(strncmp(tmpbuf, "set_color_depth", 15) == 0){
        hdmirx_config_color_depth(tmpbuf[15]-'0');
        printk("set color depth %c\n", tmpbuf[15]);
    }
#ifdef RESET_AFTER_CLK_STABLE
    else if(strncmp(tmpbuf, "reset_mode", 10)==0){
        reset_mode = simple_strtoul(tmpbuf+10, NULL, 10);
        printk("set reset_mode %d\n", reset_mode);
    }
#endif
    else if(strncmp(tmpbuf, "reset", 5)==0){
        if(tmpbuf[5]=='0'){
            hdmirx_reset_clock();
        }
        else if(tmpbuf[5]=='1'){
            hdmirx_reset();
        }
        else if(tmpbuf[5]=='2'){
            hdmirx_phy_init();
        }
        else if(tmpbuf[5]=='3'){
            hdmi_init();
            hdmirx_phy_init();
        }
        else if(tmpbuf[5]=='4'){
            int ch_num = tmpbuf[6]-'0';
            if((ch_num < 0)||(ch_num > 3)){
                ch_num = 0;
            }
            hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)|(1<<ch_num)); //reset config ch0~3
            mdelay(10);
            hdmi_wr_reg(RX_BASE_ADDR+0xe2, hdmi_rd_reg(RX_BASE_ADDR+0xe2)& (~(1<<ch_num))); //release config ch0~3
        }
        else if(tmpbuf[5]=='5'){
            hdmirx_reset_digital();
        }
        else if(tmpbuf[5]=='6'){
            hdmirx_audio_recover_reset();
        }
    }
    else if(strncmp(tmpbuf, "set_state", 9)==0){
        hdmirx_hw_stru.state = simple_strtoul(tmpbuf+9, NULL, 10);
        printk("set state %d\n", hdmirx_hw_stru.state);
    }
    else if(strncmp(tmpbuf, "test", 4)==0){
        test_flag = simple_strtoul(tmpbuf+4, NULL, 10);;     
        printk("test %d\n", test_flag);
    }
    else if(strncmp(tmpbuf, "state", 5)==0){
        dump_state(0xff);
    }
    else if(strncmp(tmpbuf, "pause", 5)==0){
        sm_pause = simple_strtoul(tmpbuf+5, NULL, 10);
        printk("%s the state machine\n", sm_pause?"pause":"enable");
    }
    else if(strncmp(tmpbuf, "reg", 3)==0){
        dump_reg();
    }
    else if(strncmp(tmpbuf, "set_eq_27M", 10)==0){
        int tmp = simple_strtoul(tmpbuf+10, NULL, 10);
        if(tmp<0x100){
            eq_27M_index = tmp;
        }
        else{
            eq_27M_index++;
        }
        set_eq_27M(eq_27M_index&0x7, (eq_27M_index>>3)&0x1f);
        printk("eq_27M_index=%d, et set_eq_27M(%d,%d)\n",eq_27M_index, eq_27M_index&0x7, (eq_27M_index>>3)&0x1f);
    }
    else if(strncmp(tmpbuf, "log", 3)==0){
        hdmirx_log_flag = simple_strtoul(tmpbuf+3, NULL, 10);
        printk("set hdmirx_log_flag as %d\n", hdmirx_log_flag);
    }
    else if(strncmp(tmpbuf, "sample_rate", 11)==0){
        sample_rate_temp = simple_strtoul(tmpbuf+11, NULL, 10);
#ifdef CONFIG_AML_AUDIO_DSP
        mailbox_send_audiodsp(1, M2B_IRQ0_DSP_AUDIO_EFFECT, DSP_CMD_SET_HDMI_SR, (char *)&sample_rate_temp,sizeof(sample_rate_temp));
#endif
        printk("force audio sample rate as %d\n", sample_rate_temp);
    }
    else if(strncmp(tmpbuf, "eq_mode", 7)==0){
        if((eq_mode>=0)&&(eq_mode<=eq_mode_max)){
            eq_mode = simple_strtoul(tmpbuf+7, NULL, 10);
            printk("set eq_mode as %d\n", eq_mode);
        }
        else{
            printk("set eq_mode (%d) fail\n", eq_mode);
        }        
    }
    else if(strncmp(tmpbuf, "internal_mode", 13)==0){
        internal_mode = simple_strtoul(tmpbuf+13, NULL, 10);
        printk("set internal_mode as %d\n", internal_mode);
    }
    else if(strncmp(tmpbuf, "prbs", 4)==0){
        turn_on_prbs_mode(simple_strtoul(tmpbuf+4, NULL, 10));
    }
    else if(tmpbuf[0]=='w'){
        adr=simple_strtoul(tmpbuf+2, NULL, 16);
        value=simple_strtoul(buf+i+1, NULL, 16);
        if(buf[1]=='h'){
            hdmirx_hw_enable_clock();
            hdmi_wr_reg(adr, value);
        }
        else if(buf[1]=='c'){
            WRITE_MPEG_REG(adr, value);
            pr_info("write %x to CBUS reg[%x]\n",value,adr);
        }
        else if(buf[1]=='p'){
            WRITE_APB_REG(adr, value);
            pr_info("write %x to APB reg[%x]\n",value,adr);
        }
        else if(buf[1]=='l'){
            WRITE_MPEG_REG(MDB_CTRL, 2);
            WRITE_MPEG_REG(MDB_ADDR_REG, adr);
            WRITE_MPEG_REG(MDB_DATA_REG, value);
            pr_info("write %x to LMEM[%x]\n",value,adr);
        }
        else if(buf[1]=='r'){
            WRITE_MPEG_REG(MDB_CTRL, 1);
            WRITE_MPEG_REG(MDB_ADDR_REG, adr);
            WRITE_MPEG_REG(MDB_DATA_REG, value);
            pr_info("write %x to amrisc reg [%x]\n",value,adr);
        }
    }
    else if(tmpbuf[0]=='r'){
        adr=simple_strtoul(tmpbuf+2, NULL, 16);
        if(buf[1]=='h'){
            hdmirx_hw_enable_clock();
            value = hdmi_rd_reg(adr);
            pr_info("HDMI reg[%x]=%x\n", adr, value);
        }
        else if(buf[1]=='c'){
            value = READ_MPEG_REG(adr);
            pr_info("CBUS reg[%x]=%x\n", adr, value);
    }
        else if(buf[1]=='p'){
            value = READ_APB_REG(adr);
            pr_info("APB reg[%x]=%x\n", adr, value);
        }
        else if(buf[1]=='l'){
            WRITE_MPEG_REG(MDB_CTRL, 2);
            WRITE_MPEG_REG(MDB_ADDR_REG, adr);
            value = READ_MPEG_REG(MDB_DATA_REG);
            pr_info("LMEM[%x]=%x\n", adr, value);
        }
        else if(buf[1]=='r'){
            WRITE_MPEG_REG(MDB_CTRL, 1);
            WRITE_MPEG_REG(MDB_ADDR_REG, adr);
            value = READ_MPEG_REG(MDB_DATA_REG);
            pr_info("amrisc reg[%x]=%x\n", adr, value);
        }
    }
    else if(tmpbuf[0]=='v'){
        printk("------------------\nHdmirx driver version: %s\n", HDMIRX_VER);
        printk("------------------\n");

    }
    else if(strncmp(tmpbuf, "cec", 3)==0){
        int j = 0;
        int i_pre = 0;
        char buf_tmp[128];
        char tmp[128];
        if (size > 30)  return;
        
        memcpy(buf_tmp, buf, size);
        buf_tmp[size] = ' ';
        for(i = 0; ; i++){
            if (i > size)
                break;

            if (buf[i] == ' '){
                if (i_pre != 0){
                    tmp[j] = simple_strtoul(&buf[i_pre+1], NULL, 10);
                    j++;
                }

                i_pre = i;                
            }
        }

        cec_test_function(tmp, j);
    }

    return 0;
}

int hdmirx_hw_dump_reg(unsigned char* buf, int size)
{
    int i, j;
    for(i = 0; i<0x900; i++){
        buf[i] = hdmi_rd_reg(RX_BASE_ADDR + i);
    }
    for(j = 0; j<=0xc; j++){
        buf[j] = hdmi_rd_reg(OTHER_BASE_ADDR + j);
    }
    return (i+j);
}
/*
* EDID
*/
#define MAX_EDID_BUF_SIZE 1024
static char edid_buf[MAX_EDID_BUF_SIZE];
static int edid_size = 0;

int hdmirx_read_edid_buf(char* buf, int max_size)
{
    if(edid_size > max_size){
        pr_err("Error: %s, edid size %d is larger than the buf size of %d\n", __func__,  edid_size, max_size);
        return 0;
    }
    memcpy(buf, edid_buf, edid_size);
    pr_info("HDMIRX: read edid buf\n");
    return edid_size;
}

void hdmirx_fill_edid_buf(const char* buf, int size)
{
    if(size > MAX_EDID_BUF_SIZE){
        pr_err("Error: %s, edid size %d is larger than the max size of %d\n", __func__,  size, MAX_EDID_BUF_SIZE);
        return;
    }
    memcpy(edid_buf, buf, size);
    edid_size = size;
    pr_info("HDMIRX: fill edid buf, size %d\n", size);
}

//#define EDID_SW
#define EDID_3D_12BIT
//#define EDID_3D
static unsigned char hdmirx_8bit_3d_edid_port1[] =
{
//8 bit only with 3D
0x00 ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0x00 ,0x4d ,0x77 ,0x02 ,0x2c ,0x01 ,0x01 ,0x01 ,0x01,
0x01 ,0x15 ,0x01 ,0x03 ,0x80 ,0x85 ,0x4b ,0x78 ,0x0a ,0x0d ,0xc9 ,0xa0 ,0x57 ,0x47 ,0x98 ,0x27,
0x12 ,0x48 ,0x4c ,0x21 ,0x08 ,0x00 ,0x81 ,0x80 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01,
0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x02 ,0x3a ,0x80 ,0x18 ,0x71 ,0x38 ,0x2d ,0x40 ,0x58 ,0x2c,
0x45 ,0x00 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x00 ,0x72 ,0x51 ,0xd0 ,0x1e ,0x20,
0x6e ,0x28 ,0x55 ,0x00 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x00 ,0x00 ,0x00 ,0xfc ,0x00 ,0x53,
0x6b ,0x79 ,0x77 ,0x6f ,0x72 ,0x74 ,0x68 ,0x20 ,0x54 ,0x56 ,0x0a ,0x20 ,0x00 ,0x00 ,0x00 ,0xfd,
0x00 ,0x30 ,0x3e ,0x0e ,0x46 ,0x0f ,0x00 ,0x0a ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x01 ,0xdc,
0x02 ,0x03 ,0x3b ,0xf0 ,0x53 ,0x1f ,0x10 ,0x14 ,0x05 ,0x13 ,0x04 ,0x20 ,0x22 ,0x3c ,0x3e ,0x12,
0x16 ,0x03 ,0x07 ,0x11 ,0x15 ,0x02 ,0x06 ,0x01 ,0x23 ,0x09 ,0x07 ,0x01 ,0x83 ,0x01 ,0x00 ,0x00,
0x74 ,0x03 ,0x0c ,0x00 ,0x10 ,0x00 ,0x88 ,0x2d ,0x2f ,0xd0 ,0x0a ,0x01 ,0x40 ,0x00 ,0x7f ,0x20,
0x30 ,0x70 ,0x80 ,0x90 ,0x76 ,0xe2 ,0x00 ,0xfb ,0x02 ,0x3a ,0x80 ,0xd0 ,0x72 ,0x38 ,0x2d ,0x40,
0x10 ,0x2c ,0x45 ,0x80 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x00 ,0xbc ,0x52 ,0xd0,
0x1e ,0x20 ,0xb8 ,0x28 ,0x55 ,0x40 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x80 ,0xd0,
0x72 ,0x1c ,0x16 ,0x20 ,0x10 ,0x2c ,0x25 ,0x80 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x9e ,0x00 ,0x00,
0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x8e
};

static unsigned char hdmirx_8bit_3d_edid_port2[] =
{
//8 bit only with 3D
0x00 ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0x00 ,0x4d ,0x77 ,0x02 ,0x2c ,0x01 ,0x01 ,0x01 ,0x01,
0x01 ,0x15 ,0x01 ,0x03 ,0x80 ,0x85 ,0x4b ,0x78 ,0x0a ,0x0d ,0xc9 ,0xa0 ,0x57 ,0x47 ,0x98 ,0x27,
0x12 ,0x48 ,0x4c ,0x21 ,0x08 ,0x00 ,0x81 ,0x80 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01,
0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x02 ,0x3a ,0x80 ,0x18 ,0x71 ,0x38 ,0x2d ,0x40 ,0x58 ,0x2c,
0x45 ,0x00 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x00 ,0x72 ,0x51 ,0xd0 ,0x1e ,0x20,
0x6e ,0x28 ,0x55 ,0x00 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x00 ,0x00 ,0x00 ,0xfc ,0x00 ,0x53,
0x6b ,0x79 ,0x77 ,0x6f ,0x72 ,0x74 ,0x68 ,0x20 ,0x54 ,0x56 ,0x0a ,0x20 ,0x00 ,0x00 ,0x00 ,0xfd,
0x00 ,0x30 ,0x3e ,0x0e ,0x46 ,0x0f ,0x00 ,0x0a ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x01 ,0xdc,
0x02 ,0x03 ,0x3b ,0xf0 ,0x53 ,0x1f ,0x10 ,0x14 ,0x05 ,0x13 ,0x04 ,0x20 ,0x22 ,0x3c ,0x3e ,0x12,
0x16 ,0x03 ,0x07 ,0x11 ,0x15 ,0x02 ,0x06 ,0x01 ,0x23 ,0x09 ,0x07 ,0x01 ,0x83 ,0x01 ,0x00 ,0x00,
0x74 ,0x03 ,0x0c ,0x00 ,0x20 ,0x00 ,0x88 ,0x2d ,0x2f ,0xd0 ,0x0a ,0x01 ,0x40 ,0x00 ,0x7f ,0x20,
0x30 ,0x70 ,0x80 ,0x90 ,0x76 ,0xe2 ,0x00 ,0xfb ,0x02 ,0x3a ,0x80 ,0xd0 ,0x72 ,0x38 ,0x2d ,0x40,
0x10 ,0x2c ,0x45 ,0x80 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x00 ,0xbc ,0x52 ,0xd0,
0x1e ,0x20 ,0xb8 ,0x28 ,0x55 ,0x40 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x80 ,0xd0,
0x72 ,0x1c ,0x16 ,0x20 ,0x10 ,0x2c ,0x25 ,0x80 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x9e ,0x00 ,0x00,
0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x7e
};

static unsigned char hdmirx_8bit_3d_edid_port3[] =
{
//8 bit only with 3D
0x00 ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0x00 ,0x4d ,0x77 ,0x02 ,0x2c ,0x01 ,0x01 ,0x01 ,0x01,
0x01 ,0x15 ,0x01 ,0x03 ,0x80 ,0x85 ,0x4b ,0x78 ,0x0a ,0x0d ,0xc9 ,0xa0 ,0x57 ,0x47 ,0x98 ,0x27,
0x12 ,0x48 ,0x4c ,0x21 ,0x08 ,0x00 ,0x81 ,0x80 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01,
0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x02 ,0x3a ,0x80 ,0x18 ,0x71 ,0x38 ,0x2d ,0x40 ,0x58 ,0x2c,
0x45 ,0x00 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x00 ,0x72 ,0x51 ,0xd0 ,0x1e ,0x20,
0x6e ,0x28 ,0x55 ,0x00 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x00 ,0x00 ,0x00 ,0xfc ,0x00 ,0x53,
0x6b ,0x79 ,0x77 ,0x6f ,0x72 ,0x74 ,0x68 ,0x20 ,0x54 ,0x56 ,0x0a ,0x20 ,0x00 ,0x00 ,0x00 ,0xfd,
0x00 ,0x30 ,0x3e ,0x0e ,0x46 ,0x0f ,0x00 ,0x0a ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x01 ,0xdc,
0x02 ,0x03 ,0x3b ,0xf0 ,0x53 ,0x1f ,0x10 ,0x14 ,0x05 ,0x13 ,0x04 ,0x20 ,0x22 ,0x3c ,0x3e ,0x12,
0x16 ,0x03 ,0x07 ,0x11 ,0x15 ,0x02 ,0x06 ,0x01 ,0x23 ,0x09 ,0x07 ,0x01 ,0x83 ,0x01 ,0x00 ,0x00,
0x74 ,0x03 ,0x0c ,0x00 ,0x30 ,0x00 ,0x88 ,0x2d ,0x2f ,0xd0 ,0x0a ,0x01 ,0x40 ,0x00 ,0x7f ,0x20,
0x30 ,0x70 ,0x80 ,0x90 ,0x76 ,0xe2 ,0x00 ,0xfb ,0x02 ,0x3a ,0x80 ,0xd0 ,0x72 ,0x38 ,0x2d ,0x40,
0x10 ,0x2c ,0x45 ,0x80 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x00 ,0xbc ,0x52 ,0xd0,
0x1e ,0x20 ,0xb8 ,0x28 ,0x55 ,0x40 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x80 ,0xd0,
0x72 ,0x1c ,0x16 ,0x20 ,0x10 ,0x2c ,0x25 ,0x80 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x9e ,0x00 ,0x00,
0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x6e
};



static unsigned char hdmirx_default_edid [] =
{
#if (defined EDID_SW)
0x00 ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0x00 ,0x4d ,0x77 ,0x01 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00,
0x01 ,0x0f ,0x01 ,0x03 ,0x80 ,0x3c ,0x22 ,0x78 ,0x0a ,0x0d ,0xc9 ,0xa0 ,0x57 ,0x47 ,0x98 ,0x27,
0x12 ,0x48 ,0x4c ,0xbf ,0xee ,0x00 ,0x81 ,0x80 ,0x8b ,0x00 ,0x81 ,0xc0 ,0x61 ,0x40 ,0x45 ,0x40,
0x8b ,0xc0 ,0x01 ,0x01 ,0x01 ,0x01 ,0x8c ,0x0a ,0xd0 ,0x8a ,0x20 ,0xe0 ,0x2d ,0x10 ,0x10 ,0x3e,
0x96 ,0x00 ,0x13 ,0x8e ,0x21 ,0x00 ,0x00 ,0x18 ,0x01 ,0x1d ,0x80 ,0x18 ,0x71 ,0x1c ,0x16 ,0x20,
0x58 ,0x2c ,0x25 ,0x00 ,0xc4 ,0x8e ,0x21 ,0x00 ,0x00 ,0x9e ,0x00 ,0x00 ,0x00 ,0xfc ,0x00 ,0x53,
0x6b ,0x79 ,0x77 ,0x6f ,0x72 ,0x74 ,0x68 ,0x20 ,0x4c ,0x43 ,0x44 ,0x0a ,0x00 ,0x00 ,0x00 ,0xfd,
0x00 ,0x31 ,0x4c ,0x0f ,0x60 ,0x0e ,0x00 ,0x0a ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x01 ,0x21,
0x02 ,0x03 ,0x26 ,0x74 ,0x4b ,0x84 ,0x10 ,0x1f ,0x05 ,0x13 ,0x14 ,0x01 ,0x02 ,0x11 ,0x06 ,0x15,
0x26 ,0x09 ,0x7f ,0x03 ,0x11 ,0x7f ,0x18 ,0x83 ,0x01 ,0x00 ,0x00 ,0x6a ,0x03 ,0x0c ,0x00 ,0x10,
0x00 ,0xb8 ,0x2d ,0x2f ,0x80 ,0x00 ,0x01 ,0x1d ,0x00 ,0xbc ,0x52 ,0xd0 ,0x1e ,0x20 ,0xb8 ,0x28,
0x55 ,0x40 ,0xc4 ,0x8e ,0x21 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x80 ,0xd0 ,0x72 ,0x1c ,0x16 ,0x20,
0x10 ,0x2c ,0x25 ,0x80 ,0xc4 ,0x8e ,0x21 ,0x00 ,0x00 ,0x9e ,0x8c ,0x0a ,0xd0 ,0x8a ,0x20 ,0xe0,
0x2d ,0x10 ,0x10 ,0x3e ,0x96 ,0x00 ,0x13 ,0x8e ,0x21 ,0x00 ,0x00 ,0x18 ,0x8c ,0x0a ,0xd0 ,0x90,
0x20 ,0x40 ,0x31 ,0x20 ,0x0c ,0x40 ,0x55 ,0x00 ,0x13 ,0x8e ,0x21 ,0x00 ,0x00 ,0x18 ,0x00 ,0x00,
0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x9d,
#elif (defined EDID_3D_12BIT)
0x00 ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0x00 ,0x4d ,0xd9 ,0x02 ,0x2c ,0x01 ,0x01 ,0x01 ,0x01,
0x01 ,0x15 ,0x01 ,0x03 ,0x80 ,0x85 ,0x4b ,0x78 ,0x0a ,0x0d ,0xc9 ,0xa0 ,0x57 ,0x47 ,0x98 ,0x27,
0x12 ,0x48 ,0x4c ,0x21 ,0x08 ,0x00 ,0x81 ,0x80 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01,
0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x02 ,0x3a ,0x80 ,0x18 ,0x71 ,0x38 ,0x2d ,0x40 ,0x58 ,0x2c,
0x45 ,0x00 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x00 ,0x72 ,0x51 ,0xd0 ,0x1e ,0x20,
0x6e ,0x28 ,0x55 ,0x00 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x00 ,0x00 ,0x00 ,0xfc ,0x00 ,0x53,
0x4f ,0x4e ,0x59 ,0x20 ,0x54 ,0x56 ,0x0a ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x00 ,0x00 ,0x00 ,0xfd,
0x00 ,0x30 ,0x3e ,0x0e ,0x46 ,0x0f ,0x00 ,0x0a ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x01 ,0x1c,
0x02 ,0x03 ,0x3b ,0xf0 ,0x53 ,0x1f ,0x10 ,0x14 ,0x05 ,0x13 ,0x04 ,0x20 ,0x22 ,0x3c ,0x3e ,0x12,
0x16 ,0x03 ,0x07 ,0x11 ,0x15 ,0x02 ,0x06 ,0x01 ,0x26 ,0x09 ,0x07 ,0x07 ,0x15 ,0x07 ,0x50 ,0x83,
0x01 ,0x00 ,0x00 ,0x74 ,0x03 ,0x0c ,0x00 ,0x20 ,0x00 ,0xb8 ,0x2d ,0x2f ,0xd0 ,0x0a ,0x01 ,0x40,
0x00 ,0x7f ,0x20 ,0x30 ,0x70 ,0x80 ,0x90 ,0x76 ,0xe2 ,0x00 ,0xfb ,0x02 ,0x3a ,0x80 ,0xd0 ,0x72,
0x38 ,0x2d ,0x40 ,0x10 ,0x2c ,0x45 ,0x80 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x00,
0xbc ,0x52 ,0xd0 ,0x1e ,0x20 ,0xb8 ,0x28 ,0x55 ,0x40 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01,
0x1d ,0x80 ,0xd0 ,0x72 ,0x1c ,0x16 ,0x20 ,0x10 ,0x2c ,0x25 ,0x80 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00,
0x9e ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0xd9,
#elif (defined EDID_3D)
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x4c, 0x2d, 0x9b, 0x06, 0x01, 0x00, 0x00, 0x00,
    0x33, 0x13, 0x01, 0x03, 0x80, 0x66, 0x39, 0x78, 0x0a, 0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26,
    0x0f, 0x50, 0x54, 0xbd, 0xef, 0x80, 0x71, 0x4f, 0x81, 0x00, 0x81, 0x40, 0x81, 0x80, 0x95, 0x00,
    0x95, 0x0f, 0xb3, 0x00, 0xa9, 0x40, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
    0x45, 0x00, 0xa0, 0x5a, 0x00, 0x00, 0x00, 0x1e, 0x66, 0x21, 0x50, 0xb0, 0x51, 0x00, 0x1b, 0x30,
    0x40, 0x70, 0x36, 0x00, 0xa0, 0x5a, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x18,
    0x4b, 0x1a, 0x51, 0x17, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfc,
    0x00, 0x53, 0x41, 0x4d, 0x53, 0x55, 0x4e, 0x47, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x6b,
    0x02, 0x03, 0x3e, 0xf1, 0x4b, 0x90, 0x1f, 0x04, 0x13, 0x05, 0x14, 0x03, 0x12, 0x20, 0x21, 0x22,
    0x23, 0x09, 0x07, 0x07, 0x83, 0x01, 0x00, 0x00, 0xe2, 0x00, 0x0f, 0xe3, 0x05, 0x03, 0x01, 0x7e,
    0x03, 0x0c, 0x00, 0x40, 0x00, 0xb8, 0x2d, 0x20, 0xc0, 0x14, 0x00, 0x01, 0x01, 0x0c, 0x08, 0x20,
    0x18, 0x20, 0x28, 0x20, 0x38, 0x20, 0x48, 0x20, 0x58, 0x20, 0x88, 0x20, 0xa8, 0x20, 0x02, 0x3a,
    0x80, 0xd0, 0x72, 0x38, 0x2d, 0x40, 0x10, 0x2c, 0x45, 0x80, 0xa0, 0x5a, 0x00, 0x00, 0x00, 0x1e,
    0x01, 0x1d, 0x00, 0xbc, 0x52, 0xd0, 0x1e, 0x20, 0xb8, 0x28, 0x55, 0x40, 0xa0, 0x5a, 0x00, 0x00,
    0x00, 0x1e, 0x01, 0x1d, 0x80, 0xd0, 0x72, 0x1c, 0x16, 0x20, 0x10, 0x2c, 0x25, 0x80, 0xa0, 0x5a,
    0x00, 0x00, 0x00, 0x9e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5d
#else
0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x4D,0x77,0x00,0x32,0x00,0x00,0x00,0x00,
0x1B,0x12,0x01,0x03,0x80,0x46,0x28,0x78,0xCA,0x0D,0xC9,0xA0,0x57,0x47,0x98,0x27,
0x12,0x48,0x4C,0xBF,0xCF,0x00,0x61,0x00,0x61,0x40,0x61,0x80,0x61,0xC0,0x81,0x00,
0x81,0x40,0x81,0x80,0x81,0xC0,0x01,0x1D,0x00,0x72,0x51,0xD0,0x1E,0x20,0x6E,0x28,
0x55,0x00,0xC4,0x8E,0x21,0x00,0x00,0x1E,0x00,0x00,0x00,0xFD,0x00,0x2F,0x50,0x1E,
0x50,0x0F,0x00,0x0A,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xFC,0x00,0x73,
0x6B,0x79,0x77,0x6F,0x72,0x74,0x68,0x2D,0x6C,0x63,0x64,0x0A,0x00,0x00,0x00,0x10,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xF0,
0x02,0x03,0x21,0x71,0x4B,0x84,0x03,0x05,0x0A,0x12,0x13,0x14,0x15,0x10,0x1F,0x06,
0x23,0x0F,0x07,0x07,0x83,0x4F,0x00,0x00,0x68,0x03,0x0C,0x00,0x10,0x00,0x38,0x2D,
0x00,0x01,0x1D,0x00,0x72,0x51,0xD0,0x1E,0x20,0x6E,0x28,0x55,0x00,0xE8,0x12,0x11,
0x00,0x00,0x1E,0x8C,0x0A,0xD0,0x90,0x20,0x40,0x31,0x20,0x0C,0x40,0x55,0x00,0xE8,
0x12,0x11,0x00,0x00,0x18,0x8C,0x0A,0xD0,0x8A,0x20,0xE0,0x2D,0x10,0x10,0x3E,0x96,
0x00,0xE8,0x12,0x11,0x00,0x00,0x18,0x01,0x1D,0x80,0x18,0x71,0x1C,0x16,0x20,0x58,
0x2C,0x25,0x00,0xC4,0x8E,0x21,0x00,0x00,0x9E,0x01,0x1D,0x80,0xD0,0x72,0x1C,0x16,
0x20,0x10,0x2C,0x25,0x80,0xC4,0x8E,0x21,0x00,0x00,0x9E,0x00,0x00,0x00,0x00,0x0E
#endif
};

void task_rx_edid_setting(void)
{
    int i, ram_addr, byte_num;
    unsigned int value;
    //printk("HDMI_OTHER_CTRL2=%x\n", hdmi_rd_reg(OTHER_BASE_ADDR+HDMI_OTHER_CTRL2));

    if(edid_mode == 0x100){ //at least one block
        pr_info("edid: use custom edid\n");
        for (i = 0; i < edid_size; i++)
        {
            value = edid_buf[i];
            ram_addr = RX_EDID_OFFSET+i;
            hdmi_wr_reg(ram_addr, value ^ ENCRYPT_KEY);
        }
    }
    else{
        if(edid_mode == 0){
            unsigned char * p_edid_array;
            if (hdmirx_hw_stru.port == 0x4) {
                byte_num = sizeof(hdmirx_8bit_3d_edid_port3)/sizeof(unsigned char);
                p_edid_array =  hdmirx_8bit_3d_edid_port3;
            }
            else if (hdmirx_hw_stru.port == 0x2) {
                byte_num = sizeof(hdmirx_8bit_3d_edid_port2)/sizeof(unsigned char);
                p_edid_array =  hdmirx_8bit_3d_edid_port2;
            }
            else {
                byte_num = sizeof(hdmirx_8bit_3d_edid_port1)/sizeof(unsigned char);
                p_edid_array =  hdmirx_8bit_3d_edid_port1;
            }
            
            for (i = 0; i < byte_num; i++)
            {
                value = p_edid_array[i];
                ram_addr = RX_EDID_OFFSET+i;
                hdmi_wr_reg(ram_addr, value ^ ENCRYPT_KEY);
            }
        }
        else if(edid_mode == 1){
            byte_num = sizeof(hdmirx_default_edid)/sizeof(unsigned char);

            for (i = 0; i < byte_num; i++)
            {
                value = hdmirx_default_edid[i];
                ram_addr = RX_EDID_OFFSET+i;
                hdmi_wr_reg(ram_addr, value ^ ENCRYPT_KEY);
            }
        }
    }
}

static unsigned char internal_mode_valid(void)
{
    if(((internal_mode>>16)&0xffff)==0x3456){
        return 1;
    }
    else{
        return 0;
    }
}

static int eq_mode_monitor(void)
{
    int ret = 0;
    if(internal_mode_valid()){
        if(eq_mode != hdmirx_hw_stru.cur_eq_mode){
            hdmirx_hw_stru.cur_eq_mode = eq_mode&0xff;
            switch(hdmirx_hw_stru.cur_eq_mode){
                case 0:
                case 0xff:
                    eq_config = 0x7; //default value
                    break;
                case 1:
                    eq_config = 0x15;
                    break;
                case 2:
                    eq_config = 0x13;
                    break;
                case 3:
                    eq_config = 0x11;
                    break;
                case 4:
                    eq_config = 0x207;
                    break;
                default:
                    eq_config = 0x7;
                    break;
            }
            ret = 1;
        }
    }
    else{
        /* (((internal_mode>>16)&0xffff)==0x3455): for changging eq_config by uart console */
        if(((internal_mode>>16)&0xffff)!=0x3455){
            eq_config = 0x7;
        }
    }
    return ret;
}

static int internal_mode_monitor(void)
{
    int ret = 0;
    if(internal_mode_valid()){
        if(internal_mode&INT_MODE_FULL_FORMAT){
            format_en0 = 0xffffffff;
            format_en1 = 0xffffffff;
            format_en2 = 0xffffffff;
            format_en3 = 0xffffffff;
        }
        else{
            format_en0 = format_en0_default;
            format_en1 = format_en1_default;
            format_en2 = format_en2_default;
            format_en3 = format_en3_default;
        }
    }
    else{
        format_en0 = format_en0_default;
        format_en1 = format_en1_default;
        format_en2 = format_en2_default;
        format_en3 = format_en3_default;
    }
    return ret;
}

MODULE_PARM_DESC(hdmirx_log_flag, "\n hdmirx_log_flag \n");
module_param(hdmirx_log_flag, int, 0664);

MODULE_PARM_DESC(sm_pause, "\n sm_pause \n");
module_param(sm_pause, int, 0664);

MODULE_PARM_DESC(force_vic, "\n force_vic \n");
module_param(force_vic, int, 0664);

MODULE_PARM_DESC(audio_enable, "\n audio_enable \n");
module_param(audio_enable, int, 0664);

MODULE_PARM_DESC(eq_enable, "\n eq_enable \n");
module_param(eq_enable, int, 0664);

MODULE_PARM_DESC(eq_config, "\n eq_config \n");
module_param(eq_config, int, 0664);

MODULE_PARM_DESC(eq_mode, "\n eq_mode \n");
module_param(eq_mode, int, 0664);

MODULE_PARM_DESC(eq_mode_max, "\n eq_mode_max \n");
module_param(eq_mode_max, int, 0664);

MODULE_PARM_DESC(internal_mode, "\n internal_mode \n");
module_param(internal_mode, int, 0664);

MODULE_PARM_DESC(signal_check_mode, "\n signal_check_mode \n");
module_param(signal_check_mode, int, 0664);

MODULE_PARM_DESC(signal_recover_mode, "\n signal_recover_mode \n");
module_param(signal_recover_mode, int, 0664);

MODULE_PARM_DESC(switch_mode, "\n switch_mode \n");
module_param(switch_mode, int, 0664);

MODULE_PARM_DESC(edid_mode, "\n edid_mode \n");
module_param(edid_mode, int, 0664);

MODULE_PARM_DESC(dvi_mode, "\n dvi_mode \n");
module_param(dvi_mode, int, 0664);

MODULE_PARM_DESC(color_format_mode, "\n color_format_mode \n");
module_param(color_format_mode, int, 0664);

MODULE_PARM_DESC(check_color_depth_mode, "\n check_color_depth_mode \n");
module_param(check_color_depth_mode, int, 0664);

MODULE_PARM_DESC(reset_threshold, "\n reset_threshold \n");
module_param(reset_threshold, int, 0664);

MODULE_PARM_DESC(delay1, "\n delay1 \n");
module_param(delay1, int, 0664);

MODULE_PARM_DESC(delay2, "\n delay2 \n");
module_param(delay2, int, 0664);

MODULE_PARM_DESC(delay3, "\n delay3 \n");
module_param(delay3, int, 0664);

MODULE_PARM_DESC(delay4, "\n delay4 \n");
module_param(delay4, int, 0664);

MODULE_PARM_DESC(hpd_ready_time, "\n hpd_ready_time \n");
module_param(hpd_ready_time, int, 0664);

MODULE_PARM_DESC(hpd_start_time, "\n hpd_start_time \n");
module_param(hpd_start_time, int, 0664);

MODULE_PARM_DESC(audio_stable_time, "\n audio_stable_time \n");
module_param(audio_stable_time, int, 0664);

MODULE_PARM_DESC(vendor_specific_info_check_period, "\n vendor_specific_info_check_period \n");
module_param(vendor_specific_info_check_period, int, 0664);

MODULE_PARM_DESC(clk_stable_threshold, "\n clk_stable_threshold \n");
module_param(clk_stable_threshold, int, 0664);

MODULE_PARM_DESC(clk_unstable_threshold, "\n clk_unstable_threshold \n");
module_param(clk_unstable_threshold, int, 0664);

MODULE_PARM_DESC(dvi_detect_wait_avi_th, "\n dvi_detect_wait_avi_th \n");
module_param(dvi_detect_wait_avi_th, int, 0664);

MODULE_PARM_DESC(general_detect_wait_avi_th, "\n general_detect_wait_avi_th \n");
module_param(general_detect_wait_avi_th, int, 0664);

MODULE_PARM_DESC(aud_channel_status_modify_th, "\n aud_channel_status_modify_th \n");
module_param(aud_channel_status_modify_th, int, 0664);

MODULE_PARM_DESC(aud_channel_status_all_0_th, "\n aud_channel_status_all_0_th \n");
module_param(aud_channel_status_all_0_th, int, 0664);

MODULE_PARM_DESC(sample_rate_change_th, "\n sample_rate_change_th \n");
module_param(sample_rate_change_th, int, 0664);

MODULE_PARM_DESC(format_en0, "\n format_en0 \n");
module_param(format_en0, int, 0664);

MODULE_PARM_DESC(format_en1, "\n format_en1 \n");
module_param(format_en1, int, 0664);

MODULE_PARM_DESC(format_en2, "\n format_en2 \n");
module_param(format_en2, int, 0664);

MODULE_PARM_DESC(format_en3, "\n format_en3 \n");
module_param(format_en3, int, 0664);

MODULE_PARM_DESC(powerdown_enable, "\n powerdown_enable \n");
module_param(powerdown_enable, int, 0664);

MODULE_PARM_DESC(skip_unstable_frame, "\n skip_unstable_frame \n");
module_param(skip_unstable_frame, int, 0664);

MODULE_PARM_DESC(prbs_ch1_err_num, "\n prbs_ch1_err_num \n");
module_param(prbs_ch1_err_num, int, 0664);

MODULE_PARM_DESC(prbs_ch2_err_num, "\n prbs_ch2_err_num \n");
module_param(prbs_ch2_err_num, int, 0664);

MODULE_PARM_DESC(prbs_ch3_err_num, "\n prbs_ch3_err_num \n");
module_param(prbs_ch3_err_num, int, 0664);

MODULE_PARM_DESC(prbs_check_count, "\n prbs_check_count \n");
module_param(prbs_check_count, int, 0664);

MODULE_PARM_DESC(prbs_port_mode, "\n prbs_port_mode \n");
module_param(prbs_port_mode, int, 0664);

/*
for pioneer DV-400V 1080P, only below config make signal good

echo 0x11 > /sys/module/tvin_hdmirx/parameters/eq_config
cat /sys/module/tvin_hdmirx/parameters/eq_config

echo 0x10 > /sys/module/tvin_hdmirx/parameters/eq_config
cat /sys/module/tvin_hdmirx/parameters/eq_config

echo 0x110 > /sys/module/tvin_hdmirx/parameters/eq_config
cat /sys/module/tvin_hdmirx/parameters/eq_config

echo 0x111 > /sys/module/tvin_hdmirx/parameters/eq_config
cat /sys/module/tvin_hdmirx/parameters/eq_config
*/
