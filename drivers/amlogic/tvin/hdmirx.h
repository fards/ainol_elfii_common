/*******************************************************************
*  Copyright C 2010 by Amlogic, Inc. All Rights Reserved.
*  File name: hdmirx.h
*  Description: IO function, structure, enum, used in TVIN AFE sub-module processing
*******************************************************************/

#ifndef _TVHDMI_H
#define _TVHDMI_H


#include <linux/tvin/tvin.h>
#include "tvin_global.h"
#include "tvin_format_table.h"

#define HDMIRX_VER "Ref.2011Dec08a"

#define HDMI_STATE_CHECK_FREQ     20
/******************************Definitions************************************/

#define ABS(x) ( (x)<0 ? -(x) : (x))

// ***************************************************************************
// *** enum definitions *********************************************
// ***************************************************************************

typedef enum hdmirx_src_type_e {
    TVHDMI_SRC_TYPE_NULL = 0,
} hdmirx_src_type_t;

// ***************************************************************************
// *** structure definitions *********************************************
// ***************************************************************************

typedef enum HDMI_Video_Type_ {
/* add new value at the end, do not insert new value in the middle to avoid wrong VIC value !!! */
    HDMI_Unkown = 0 ,
    HDMI_640x480p60 = 1 ,
    HDMI_480p60,
    HDMI_480p60_16x9,
    HDMI_720p60,
    HDMI_1080i60,
    HDMI_480i60,
    HDMI_480i60_16x9,

    HDMI_1440x240p60,  //8
    HDMI_1440x240p60_16x9, //9
    HDMI_2880x480i60, //10
    HDMI_2880x480i60_16x9, //11
    HDMI_2880x240p60, //12
    HDMI_2880x240p60_16x9, //13

    HDMI_1440x480p60 = 14 ,
    HDMI_1440x480p60_16x9 = 15 ,
    HDMI_1080p60 = 16,
    HDMI_576p50,
    HDMI_576p50_16x9,
    HDMI_720p50,
    HDMI_1080i50,
    HDMI_576i50,
    HDMI_576i50_16x9,
    
    HDMI_1440x288p50, //23
    HDMI_1440x288p50_16x9, //24
    HDMI_2880x576i50, //25
    HDMI_2880x576i50_16x9, //26
    HDMI_2880x288p50, //27
    HDMI_2880x288p50_16x9, //28
    HDMI_1440x576p50, //29
    HDMI_1440x576p50_16x9, //30
    
    HDMI_1080p50 = 31,
    HDMI_1080p24,
    HDMI_1080p25,
    HDMI_1080p30,
    
    HDMI_2880x480p60, //35
    HDMI_2880x480p60_16x9, //36
    HDMI_2880x576p50, //37
    HDMI_2880x576p50_16x9, //38
    HDMI_1080i50_1250, //39 

    HDMI_1080I120=46,
	HDMI_720p120,
	HDMI_1080p120=63,

	HDMI_800_600=65, //65
	HDMI_1024_768, //66
  HDMI_720_400, // 67
	HDMI_1280_768, //68
	HDMI_1280_800, //69
	HDMI_1280_960, //70
	HDMI_1280_1024, //71
	HDMI_1360_768, //72
	HDMI_1366_768, //73
	HDMI_1600_900, //74
	HDMI_1600_1200, //75
	HDMI_1920_1200, //76
	
} HDMI_Video_Codes_t ;

extern enum tvin_sig_fmt_e hdmirx_hw_get_fmt(void);
extern void hdmirx_hw_monitor(void);
extern bool hdmirx_hw_is_nosig(void);
extern bool hdmirx_hw_pll_lock(void);
extern void hdmirx_reset(void);

extern void hdmirx_hw_init(tvin_port_t port);
extern void hdmirx_hw_uninit(void);
extern unsigned int hdmirx_get_cur_vic(void);


extern void hdmirx_hw_enable(void);
extern void hdmirx_hw_disable(unsigned char flag);

extern void hdmirx_fill_edid_buf(const char* buf, int size);
extern int hdmirx_read_edid_buf(char* buf, int max_size);
extern int hdmirx_debug(const char* buf, int size);
extern int hdmirx_hw_get_color_fmt(void);
extern int hdmirx_hw_get_3d_structure(unsigned char* _3d_structure, unsigned char* _3d_ext_data);
extern int hdmirx_hw_get_pixel_repeat(void);
extern bool hdmirx_hw_check_frame_skip(void);

extern int hdmirx_print(const char *fmt, ...);
extern int hdmirx_log_flag;

extern int hdmirx_hw_dump_reg(unsigned char* buf, int size);
#endif  // _TVHDMI_H
