/*
 * TVIN Modules Exported Header File
 *
 * Author: Lin Xu <lin.xu@amlogic.com>
 *         Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __TVIN_H
#define __TVIN_H

// ***************************************************************************
// *** TVIN general definition/enum/struct ***********************************
// ***************************************************************************

/* tvin input port select */
typedef enum tvin_port_e {
    TVIN_PORT_NULL    = 0x00000000,
    TVIN_PORT_MPEG0   = 0x00000100,
    TVIN_PORT_BT656   = 0x00000200,
    TVIN_PORT_BT601,
    TVIN_PORT_CAMERA,
    TVIN_PORT_VGA0    = 0x00000400,
    TVIN_PORT_VGA1,
    TVIN_PORT_VGA2,
    TVIN_PORT_VGA3,
    TVIN_PORT_VGA4,
    TVIN_PORT_VGA5,
    TVIN_PORT_VGA6,
    TVIN_PORT_VGA7,
    TVIN_PORT_COMP0   = 0x00000800,
    TVIN_PORT_COMP1,
    TVIN_PORT_COMP2,
    TVIN_PORT_COMP3,
    TVIN_PORT_COMP4,
    TVIN_PORT_COMP5,
    TVIN_PORT_COMP6,
    TVIN_PORT_COMP7,
    TVIN_PORT_CVBS0   = 0x00001000,
    TVIN_PORT_CVBS1,
    TVIN_PORT_CVBS2,
    TVIN_PORT_CVBS3,
    TVIN_PORT_CVBS4,
    TVIN_PORT_CVBS5,
    TVIN_PORT_CVBS6,
    TVIN_PORT_CVBS7,
    TVIN_PORT_SVIDEO0 = 0x00002000,
    TVIN_PORT_SVIDEO1,
    TVIN_PORT_SVIDEO2,
    TVIN_PORT_SVIDEO3,
    TVIN_PORT_SVIDEO4,
    TVIN_PORT_SVIDEO5,
    TVIN_PORT_SVIDEO6,
    TVIN_PORT_SVIDEO7,
    TVIN_PORT_HDMI0   = 0x00004000,
    TVIN_PORT_HDMI1,
    TVIN_PORT_HDMI2,
    TVIN_PORT_HDMI3,
    TVIN_PORT_HDMI4,
    TVIN_PORT_HDMI5,
    TVIN_PORT_HDMI6,
    TVIN_PORT_HDMI7,
    TVIN_PORT_DVIN0   = 0x00008000,
    TVIN_PORT_MAX     = 0x80000000,
} tvin_port_t;

const char * tvin_port_str(enum tvin_port_e port);


/* tvin signal format table */
typedef enum tvin_sig_fmt_e {
    TVIN_SIG_FMT_NULL = 0,
    //VGA Formats
    TVIN_SIG_FMT_VGA_512X384P_60D147,
    TVIN_SIG_FMT_VGA_560X384P_60D147,
    TVIN_SIG_FMT_VGA_640X200P_59D924,
    TVIN_SIG_FMT_VGA_640X350P_85D080,
    TVIN_SIG_FMT_VGA_640X400P_59D940,
    TVIN_SIG_FMT_VGA_640X400P_85D080,
    TVIN_SIG_FMT_VGA_640X400P_59D638,
    TVIN_SIG_FMT_VGA_640X400P_56D416,
    TVIN_SIG_FMT_VGA_640X480P_66D619,
    TVIN_SIG_FMT_VGA_640X480P_66D667,
    TVIN_SIG_FMT_VGA_640X480P_59D940,
    TVIN_SIG_FMT_VGA_640X480P_60D000,
    TVIN_SIG_FMT_VGA_640X480P_72D809,
    TVIN_SIG_FMT_VGA_640X480P_75D000_A,
    TVIN_SIG_FMT_VGA_640X480P_85D008,
    TVIN_SIG_FMT_VGA_640X480P_59D638,
    TVIN_SIG_FMT_VGA_640X480P_75D000_B,
    TVIN_SIG_FMT_VGA_640X870P_75D000,
    TVIN_SIG_FMT_VGA_720X350P_70D086,
    TVIN_SIG_FMT_VGA_720X400P_85D039,
    TVIN_SIG_FMT_VGA_720X400P_70D086,
    TVIN_SIG_FMT_VGA_720X400P_87D849,
    TVIN_SIG_FMT_VGA_720X400P_59D940,
    TVIN_SIG_FMT_VGA_720X480P_59D940,
    TVIN_SIG_FMT_VGA_768X480P_59D896,
    TVIN_SIG_FMT_VGA_800X600P_56D250,
    TVIN_SIG_FMT_VGA_800X600P_60D000,
    TVIN_SIG_FMT_VGA_800X600P_60D000_A,
    TVIN_SIG_FMT_VGA_800X600P_60D317,
    TVIN_SIG_FMT_VGA_800X600P_72D188, //30
    TVIN_SIG_FMT_VGA_800X600P_75D000,
    TVIN_SIG_FMT_VGA_800X600P_85D061,
    TVIN_SIG_FMT_VGA_832X624P_75D087,
    TVIN_SIG_FMT_VGA_848X480P_84D751,
    TVIN_SIG_FMT_VGA_960X600P_59D635,
    TVIN_SIG_FMT_VGA_1024X768P_59D278,
    TVIN_SIG_FMT_VGA_1024X768P_60D000,
    TVIN_SIG_FMT_VGA_1024X768P_60D000_A,
    TVIN_SIG_FMT_VGA_1024X768P_60D000_B,
    TVIN_SIG_FMT_VGA_1024X768P_74D927, //40
    TVIN_SIG_FMT_VGA_1024X768P_60D004,
    TVIN_SIG_FMT_VGA_1024X768P_70D069,
    TVIN_SIG_FMT_VGA_1024X768P_75D029,
    TVIN_SIG_FMT_VGA_1024X768P_84D997,
    TVIN_SIG_FMT_VGA_1024X768P_74D925,
    TVIN_SIG_FMT_VGA_1024X768P_75D020,
    TVIN_SIG_FMT_VGA_1024X768P_70D008,
    TVIN_SIG_FMT_VGA_1024X768P_75D782,
    TVIN_SIG_FMT_VGA_1024X768P_77D069,
    TVIN_SIG_FMT_VGA_1024X768P_71D799,  //50
    TVIN_SIG_FMT_VGA_1024X1024P_60D000,
    TVIN_SIG_FMT_VGA_1152X864P_60D000,
    TVIN_SIG_FMT_VGA_1152X864P_70D012,
    TVIN_SIG_FMT_VGA_1152X864P_75D000,
    TVIN_SIG_FMT_VGA_1152X864P_84D999,
    TVIN_SIG_FMT_VGA_1152X870P_75D062,
    TVIN_SIG_FMT_VGA_1152X900P_65D950,
    TVIN_SIG_FMT_VGA_1152X900P_66D004,
    TVIN_SIG_FMT_VGA_1152X900P_76D047,
    TVIN_SIG_FMT_VGA_1152X900P_76D149,  //60
    TVIN_SIG_FMT_VGA_1280X720P_59D855,
    TVIN_SIG_FMT_VGA_1280X720P_60D000_A,
    TVIN_SIG_FMT_VGA_1280X720P_60D000_B,
    TVIN_SIG_FMT_VGA_1280X720P_60D000_C,
    TVIN_SIG_FMT_VGA_1280X720P_60D000_D,
    TVIN_SIG_FMT_VGA_1280X768P_59D870,
    TVIN_SIG_FMT_VGA_1280X768P_59D995,
    TVIN_SIG_FMT_VGA_1280X768P_60D100,
    TVIN_SIG_FMT_VGA_1280X768P_60D100_A,
    TVIN_SIG_FMT_VGA_1280X768P_74D893,  //70
    TVIN_SIG_FMT_VGA_1280X768P_84D837,
    TVIN_SIG_FMT_VGA_1280X800P_59D810,
    TVIN_SIG_FMT_VGA_1280X800P_59D810_A,
    TVIN_SIG_FMT_VGA_1280X800P_60D000,
    TVIN_SIG_FMT_VGA_1280X800P_60D000_A,
    TVIN_SIG_FMT_VGA_1280X960P_60D000,
    TVIN_SIG_FMT_VGA_1280X960P_60D000_A,
    TVIN_SIG_FMT_VGA_1280X960P_75D000,
    TVIN_SIG_FMT_VGA_1280X960P_85D002,
    TVIN_SIG_FMT_VGA_1280X1024P_60D020,  //80
    TVIN_SIG_FMT_VGA_1280X1024P_60D020_A,
    TVIN_SIG_FMT_VGA_1280X1024P_75D025,
    TVIN_SIG_FMT_VGA_1280X1024P_85D024,
    TVIN_SIG_FMT_VGA_1280X1024P_59D979,
    TVIN_SIG_FMT_VGA_1280X1024P_72D005,
    TVIN_SIG_FMT_VGA_1280X1024P_60D002,
    TVIN_SIG_FMT_VGA_1280X1024P_67D003,
    TVIN_SIG_FMT_VGA_1280X1024P_74D112,
    TVIN_SIG_FMT_VGA_1280X1024P_76D179,
    TVIN_SIG_FMT_VGA_1280X1024P_66D718,  //90
    TVIN_SIG_FMT_VGA_1280X1024P_66D677,
    TVIN_SIG_FMT_VGA_1280X1024P_76D107,
    TVIN_SIG_FMT_VGA_1280X1024P_59D996,
    TVIN_SIG_FMT_VGA_1280X1024P_60D000,
    TVIN_SIG_FMT_VGA_1360X768P_59D799,
    TVIN_SIG_FMT_VGA_1360X768P_60D015,
    TVIN_SIG_FMT_VGA_1360X768P_60D015_A,
    TVIN_SIG_FMT_VGA_1360X850P_60D000,
    TVIN_SIG_FMT_VGA_1360X1024P_60D000,
    TVIN_SIG_FMT_VGA_1366X768P_59D790,  //100
    TVIN_SIG_FMT_VGA_1366X768P_60D000,
    TVIN_SIG_FMT_VGA_1400X1050P_59D978,
    TVIN_SIG_FMT_VGA_1440X900P_59D887,
    TVIN_SIG_FMT_VGA_1440X1080P_60D000,
    TVIN_SIG_FMT_VGA_1600X900P_60D000,
    TVIN_SIG_FMT_VGA_1600X1024P_60D000,
    TVIN_SIG_FMT_VGA_1600X1200P_60D000,
    TVIN_SIG_FMT_VGA_1600X1200P_65D000,
    TVIN_SIG_FMT_VGA_1600X1200P_70D000,
    TVIN_SIG_FMT_VGA_1680X1080P_60D000,  //110
    TVIN_SIG_FMT_VGA_1920X1080P_59D963,
    TVIN_SIG_FMT_VGA_1920X1080P_60D000,
    TVIN_SIG_FMT_VGA_1920X1200P_59D950, // 113
    TVIN_SIG_FMT_VGA_MAX,
    //Component Formats
    TVIN_SIG_FMT_COMP_480P_60D000, //99
    TVIN_SIG_FMT_COMP_480I_59D940,
    TVIN_SIG_FMT_COMP_576P_50D000,
    TVIN_SIG_FMT_COMP_576I_50D000,
    TVIN_SIG_FMT_COMP_720P_59D940,
    TVIN_SIG_FMT_COMP_720P_50D000,
    TVIN_SIG_FMT_COMP_1080P_23D976,
    TVIN_SIG_FMT_COMP_1080P_24D000,
    TVIN_SIG_FMT_COMP_1080P_25D000,
    TVIN_SIG_FMT_COMP_1080P_30D000,
    TVIN_SIG_FMT_COMP_1080P_50D000,
    TVIN_SIG_FMT_COMP_1080P_60D000,
    TVIN_SIG_FMT_COMP_1080I_47D952,
    TVIN_SIG_FMT_COMP_1080I_48D000,
    TVIN_SIG_FMT_COMP_1080I_50D000_A,
    TVIN_SIG_FMT_COMP_1080I_50D000_B,
    TVIN_SIG_FMT_COMP_1080I_50D000_C,
    TVIN_SIG_FMT_COMP_1080I_60D000, //116
    TVIN_SIG_FMT_COMP_MAX, //117
    //HDMI Formats
    TVIN_SIG_FMT_HDMI_640x480P_60Hz, //118
    TVIN_SIG_FMT_HDMI_720x480P_60Hz,
    TVIN_SIG_FMT_HDMI_1280x720P_60Hz,
    TVIN_SIG_FMT_HDMI_1920x1080I_60Hz,
    TVIN_SIG_FMT_HDMI_1440x480I_60Hz,
    TVIN_SIG_FMT_HDMI_1440x240P_60Hz,
    TVIN_SIG_FMT_HDMI_2880x480I_60Hz,
    TVIN_SIG_FMT_HDMI_2880x240P_60Hz,
    TVIN_SIG_FMT_HDMI_1440x480P_60Hz,
    TVIN_SIG_FMT_HDMI_1920x1080P_60Hz,
    TVIN_SIG_FMT_HDMI_720x576P_50Hz,
    TVIN_SIG_FMT_HDMI_1280x720P_50Hz,
    TVIN_SIG_FMT_HDMI_1920x1080I_50Hz_A,
    TVIN_SIG_FMT_HDMI_1440x576I_50Hz,
    TVIN_SIG_FMT_HDMI_1440x288P_50Hz,
    TVIN_SIG_FMT_HDMI_2880x576I_50Hz,
    TVIN_SIG_FMT_HDMI_2880x288P_50Hz,
    TVIN_SIG_FMT_HDMI_1440x576P_50Hz,
    TVIN_SIG_FMT_HDMI_1920x1080P_50Hz,
    TVIN_SIG_FMT_HDMI_1920x1080P_24Hz,
    TVIN_SIG_FMT_HDMI_1920x1080P_25Hz,
    TVIN_SIG_FMT_HDMI_1920x1080P_30Hz,
    TVIN_SIG_FMT_HDMI_2880x480P_60Hz,
    TVIN_SIG_FMT_HDMI_2880x576P_60Hz,
    TVIN_SIG_FMT_HDMI_1920x1080I_50Hz_B,
    TVIN_SIG_FMT_HDMI_1920x1080I_100Hz,
    TVIN_SIG_FMT_HDMI_1280x720P_100Hz,
    TVIN_SIG_FMT_HDMI_720x576P_100Hz,
    TVIN_SIG_FMT_HDMI_1440x576I_100Hz,
    TVIN_SIG_FMT_HDMI_1920x1080I_120Hz,
    TVIN_SIG_FMT_HDMI_1280x720P_120Hz,
    TVIN_SIG_FMT_HDMI_720x480P_120Hz,
    TVIN_SIG_FMT_HDMI_1440x480I_120Hz,
    TVIN_SIG_FMT_HDMI_720x576P_200Hz,
    TVIN_SIG_FMT_HDMI_1440x576I_200Hz,
    TVIN_SIG_FMT_HDMI_720x480P_240Hz,
    TVIN_SIG_FMT_HDMI_1440x480I_240Hz,
    TVIN_SIG_FMT_HDMI_1280x720P_24Hz,
    TVIN_SIG_FMT_HDMI_1280x720P_25Hz,
    TVIN_SIG_FMT_HDMI_1280x720P_30Hz,
    TVIN_SIG_FMT_HDMI_1920x1080P_120Hz,
    TVIN_SIG_FMT_HDMI_1920x1080P_100Hz, //159
    TVIN_SIG_FMT_HDMI_1280x720P_60Hz_FRAME_PACKING, //160
    TVIN_SIG_FMT_HDMI_1280x720P_50Hz_FRAME_PACKING,
    TVIN_SIG_FMT_HDMI_1280x720P_24Hz_FRAME_PACKING,
    TVIN_SIG_FMT_HDMI_1280x720P_30Hz_FRAME_PACKING,
    TVIN_SIG_FMT_HDMI_1920x1080I_60Hz_FRAME_PACKING,
    TVIN_SIG_FMT_HDMI_1920x1080I_50Hz_FRAME_PACKING,
    TVIN_SIG_FMT_HDMI_1920x1080P_24Hz_FRAME_PACKING,
    TVIN_SIG_FMT_HDMI_1920x1080P_30Hz_FRAME_PACKING, //167
    TVIN_SIG_FMT_HDMI_800x600, //168
    TVIN_SIG_FMT_HDMI_1024x768,
    TVIN_SIG_FMT_HDMI_720_400,
    TVIN_SIG_FMT_HDMI_1280_768,
    TVIN_SIG_FMT_HDMI_1280_800,
    TVIN_SIG_FMT_HDMI_1280_960,
    TVIN_SIG_FMT_HDMI_1280_1024,
    TVIN_SIG_FMT_HDMI_1360_768,
    TVIN_SIG_FMT_HDMI_1366_768,
    TVIN_SIG_FMT_HDMI_1600_1200,
    TVIN_SIG_FMT_HDMI_1920_1200,
    TVIN_SIG_FMT_HDMI_RESERVE1,
    TVIN_SIG_FMT_HDMI_RESERVE2,
    TVIN_SIG_FMT_HDMI_RESERVE3,
    TVIN_SIG_FMT_HDMI_RESERVE4,
    TVIN_SIG_FMT_HDMI_RESERVE5,
    TVIN_SIG_FMT_HDMI_RESERVE6,
    TVIN_SIG_FMT_HDMI_RESERVE7,
    TVIN_SIG_FMT_HDMI_RESERVE8,
    TVIN_SIG_FMT_HDMI_RESERVE9,
    TVIN_SIG_FMT_HDMI_RESERVE10,
    TVIN_SIG_FMT_HDMI_RESERVE11,
    TVIN_SIG_FMT_HDMI_RESERVE12,
    TVIN_SIG_FMT_HDMI_RESERVE13,
    TVIN_SIG_FMT_HDMI_RESERVE14,
    TVIN_SIG_FMT_HDMI_720x480P_60Hz_FRAME_PACKING,
    TVIN_SIG_FMT_HDMI_720x576P_50Hz_FRAME_PACKING, //194
    TVIN_SIG_FMT_HDMI_MAX, //195
    //Video Formats
    TVIN_SIG_FMT_CVBS_NTSC_M, //196
    TVIN_SIG_FMT_CVBS_NTSC_443,
    TVIN_SIG_FMT_CVBS_PAL_I,
    TVIN_SIG_FMT_CVBS_PAL_M,
    TVIN_SIG_FMT_CVBS_PAL_60,
    TVIN_SIG_FMT_CVBS_PAL_CN,
    TVIN_SIG_FMT_CVBS_SECAM, //202
    //656 Formats
    TVIN_SIG_FMT_BT656IN_576I, //203
    TVIN_SIG_FMT_BT656IN_480I,
    //601 Formats
    TVIN_SIG_FMT_BT601IN_576I,
    TVIN_SIG_FMT_BT601IN_480I,
    //Camera Formats
    TVIN_SIG_FMT_CAMERA_640X480P_30Hz,
    TVIN_SIG_FMT_CAMERA_800X600P_30Hz,
    TVIN_SIG_FMT_CAMERA_1024X768P_30Hz,
    TVIN_SIG_FMT_CAMERA_1920X1080P_30Hz,
    TVIN_SIG_FMT_CAMERA_1280X720P_30Hz, //211
    TVIN_SIG_FMT_MAX, //212
} tvin_sig_fmt_t;

//tvin signal status
typedef enum tvin_sig_status_e {
    TVIN_SIG_STATUS_NULL = 0, // processing status from init to the finding of the 1st confirmed status
    TVIN_SIG_STATUS_NOSIG,    // no signal - physically no signal
    TVIN_SIG_STATUS_UNSTABLE, // unstable - physically bad signal
    TVIN_SIG_STATUS_NOTSUP,   // not supported - physically good signal & not supported
    TVIN_SIG_STATUS_STABLE,   // stable - physically good signal & supported
} tvin_sig_status_t;

const char *tvin_sig_status_str(enum tvin_sig_status_e status);

// tvin parameters
#define TVIN_PARM_FLAG_CAP      0x00000001 //tvin_parm_t.flag[ 0]: 1/enable or 0/disable frame capture function
#define TVIN_PARM_FLAG_CAL      0x00000002 //tvin_parm_t.flag[ 1]: 1/enable or 0/disable adc calibration
#define TVIN_PARM_FLAG_2D_TO_3D 0x00000004 //tvin_parm_t.flag[ 2]: 1/enable or 0/disable 2D->3D mode

typedef enum tvin_trans_fmt {
    TVIN_TFMT_2D = 0,
    TVIN_TFMT_3D_LRH_OLOR,  // Primary: Side-by-Side(Half) Odd/Left picture, Odd/Right p
    TVIN_TFMT_3D_LRH_OLER,  // Primary: Side-by-Side(Half) Odd/Left picture, Even/Right picture
    TVIN_TFMT_3D_LRH_ELOR,  // Primary: Side-by-Side(Half) Even/Left picture, Odd/Right picture
    TVIN_TFMT_3D_LRH_ELER,  // Primary: Side-by-Side(Half) Even/Left picture, Even/Right picture
    TVIN_TFMT_3D_TB,   // Primary: Top-and-Bottom
    TVIN_TFMT_3D_FP,   // Primary: Frame Packing
    TVIN_TFMT_3D_FA,   // Secondary: Field Alternative
    TVIN_TFMT_3D_LA,   // Secondary: Line Alternative
    TVIN_TFMT_3D_LRF,  // Secondary: Side-by-Side(Full)
    TVIN_TFMT_3D_LD,   // Secondary: L+depth
    TVIN_TFMT_3D_LDGD, // Secondary: L+depth+Graphics+Graphics-depth
} tvin_trans_fmt_t;

const char *tvin_trans_fmt_str(enum tvin_trans_fmt trans_fmt);

typedef struct tvin_info_s {
    enum tvin_trans_fmt    trans_fmt;
    enum tvin_sig_fmt_e    fmt;
    enum tvin_sig_status_e status;
    unsigned int           reserved;
}tvin_info_t;

typedef struct tvin_buf_info_s {
    unsigned int vf_size;
    unsigned int buf_count;
    unsigned int buf_width;
    unsigned int buf_height;
    unsigned int buf_size;
    unsigned int wr_list_size;
} tvin_buf_info_t;

typedef struct tvin_video_buf_s {
    unsigned int index;
    unsigned int reserved;
} tvin_video_buf_t;

// hs=he=vs=ve=0 is to disable Cut Window
typedef struct tvin_cutwin_s {
    unsigned int hs;
    unsigned int he;
    unsigned int vs;
    unsigned int ve;
} tvin_cutwin_t;

typedef struct tvin_parm_s {
    int                         index;    // index of frontend for vdin
    enum tvin_port_e            port;     // must set port in IOCTL
    struct tvin_info_s          info;
    struct tvin_cutwin_s        cutwin;
    unsigned short              histgram[64];
    unsigned int                flag;
    unsigned int                reserved;
} tvin_parm_t;



// ***************************************************************************
// *** AFE module definition/enum/struct *************************************
// ***************************************************************************

typedef enum tvafe_cmd_status_e {
    TVAFE_CMD_STATUS_IDLE = 0,   // idle, be ready for TVIN_IOC_S_AFE_VGA_AUTO command
    TVAFE_CMD_STATUS_PROCESSING, // TVIN_IOC_S_AFE_VGA_AUTO command is in process
    TVAFE_CMD_STATUS_SUCCESSFUL, // TVIN_IOC_S_AFE_VGA_AUTO command is done with success
    TVAFE_CMD_STATUS_FAILED,     // TVIN_IOC_S_AFE_VGA_AUTO command is done with failure
    TVAFE_CMD_STATUS_TERMINATED, // TVIN_IOC_S_AFE_VGA_AUTO command is terminated by others related
} tvafe_cmd_status_t;

typedef struct tvafe_vga_edid_s {
    unsigned char value[256]; //256 byte EDID
} tvafe_vga_edid_t;

typedef struct tvafe_comp_wss_s {
    unsigned int wss1[5];
    unsigned int wss2[5];
} tvafe_comp_wss_t;

typedef struct tvafe_vga_parm_s {
      signed short clk_step;  // clock < 0, tune down clock freq
                              // clock > 0, tune up clock freq
    unsigned short phase;     // phase is 0~31, it is absolute value
      signed short hpos_step; // hpos_step < 0, shift display to left
                              // hpos_step > 0, shift display to right
      signed short vpos_step; // vpos_step < 0, shift display to top
                              // vpos_step > 0, shift display to bottom
    unsigned int   vga_in_clean;  // flage for vga clean screen
} tvafe_vga_parm_t;

#define TVAFE_ADC_CAL_VALID 0x00000001
typedef struct tvafe_adc_cal_s {
    // ADC A
    unsigned short a_analog_clamp;    // 0x00~0x7f
    unsigned short a_analog_gain;     // 0x00~0xff, means 0dB~6dB
    unsigned short a_digital_offset1; // offset for fine-tuning
                                      // s11.0:   signed value, 11 integer bits,  0 fraction bits
    unsigned short a_digital_gain;    // 0~3.999
                                      // u2.10: unsigned value,  2 integer bits, 10 fraction bits
    unsigned short a_digital_offset2; // offset for format
                                      // s11.0:   signed value, 11 integer bits,  0 fraction bits
    // ADC B
    unsigned short b_analog_clamp;    // ditto to ADC A
    unsigned short b_analog_gain;
    unsigned short b_digital_offset1;
    unsigned short b_digital_gain;
    unsigned short b_digital_offset2;
    // ADC C
    unsigned short c_analog_clamp;    // ditto to ADC A
    unsigned short c_analog_gain;
    unsigned short c_digital_offset1;
    unsigned short c_digital_gain;
    unsigned short c_digital_offset2;
    // ADC D
    unsigned short d_analog_clamp;    // ditto to ADC A
    unsigned short d_analog_gain;
    unsigned short d_digital_offset1;
    unsigned short d_digital_gain;
    unsigned short d_digital_offset2;
    unsigned int   reserved;          // bit[ 0]: TVAFE_ADC_CAL_VALID
} tvafe_adc_cal_t;

typedef enum tvafe_cvbs_video_e {
    TVAFE_CVBS_VIDEO_HV_UNLOCKED = 0,
    TVAFE_CVBS_VIDEO_H_LOCKED,
    TVAFE_CVBS_VIDEO_V_LOCKED,
    TVAFE_CVBS_VIDEO_HV_LOCKED,
} tvafe_cvbs_video_t;

// ***************************************************************************
// *** analog tuner module definition/enum/struct ****************************
// ***************************************************************************

typedef unsigned long long tuner_std_id;

/* one bit for each */
#define TUNER_STD_PAL_B     ((tuner_std_id)0x00000001)
#define TUNER_STD_PAL_B1    ((tuner_std_id)0x00000002)
#define TUNER_STD_PAL_G     ((tuner_std_id)0x00000004)
#define TUNER_STD_PAL_H     ((tuner_std_id)0x00000008)
#define TUNER_STD_PAL_I     ((tuner_std_id)0x00000010)
#define TUNER_STD_PAL_D     ((tuner_std_id)0x00000020)
#define TUNER_STD_PAL_D1    ((tuner_std_id)0x00000040)
#define TUNER_STD_PAL_K     ((tuner_std_id)0x00000080)
#define TUNER_STD_PAL_M     ((tuner_std_id)0x00000100)
#define TUNER_STD_PAL_N     ((tuner_std_id)0x00000200)
#define TUNER_STD_PAL_Nc    ((tuner_std_id)0x00000400)
#define TUNER_STD_PAL_60    ((tuner_std_id)0x00000800)
#define TUNER_STD_NTSC_M    ((tuner_std_id)0x00001000)
#define TUNER_STD_NTSC_M_JP ((tuner_std_id)0x00002000)
#define TUNER_STD_NTSC_443  ((tuner_std_id)0x00004000)
#define TUNER_STD_NTSC_M_KR ((tuner_std_id)0x00008000)
#define TUNER_STD_SECAM_B   ((tuner_std_id)0x00010000)
#define TUNER_STD_SECAM_D   ((tuner_std_id)0x00020000)
#define TUNER_STD_SECAM_G   ((tuner_std_id)0x00040000)
#define TUNER_STD_SECAM_H   ((tuner_std_id)0x00080000)
#define TUNER_STD_SECAM_K   ((tuner_std_id)0x00100000)
#define TUNER_STD_SECAM_K1  ((tuner_std_id)0x00200000)
#define TUNER_STD_SECAM_L   ((tuner_std_id)0x00400000)
#define TUNER_STD_SECAM_LC  ((tuner_std_id)0x00800000)

#define TUNER_DEMO_STAUS_PONR_BIT				0x01
#define TUNER_DEMO_STAUS_AFC_BIT				0x1e    // Fc-Fo in the unit of Hz
                                                        // Fc is the current freq
                                                        // Fo is the target freq
#define TUNER_DEMO_STAUS_FMIFL_BIT				0x20
#define TUNER_DEMO_STAUS_VIFL_BIT				0x40
#define TUNER_DEMO_STAUS_AFCWIN_BIT			    0x80

/* some merged standards */
#define TUNER_STD_MN       (TUNER_STD_PAL_M    |\
                            TUNER_STD_PAL_N    |\
                            TUNER_STD_PAL_Nc   |\
                            TUNER_STD_NTSC)
#define TUNER_STD_B        (TUNER_STD_PAL_B    |\
                            TUNER_STD_PAL_B1   |\
                            TUNER_STD_SECAM_B)
#define TUNER_STD_GH       (TUNER_STD_PAL_G    |\
                            TUNER_STD_PAL_H    |\
                            TUNER_STD_SECAM_G  |\
                            TUNER_STD_SECAM_H)
#define TUNER_STD_DK       (TUNER_STD_PAL_DK   |\
                            TUNER_STD_SECAM_DK)

/* some common needed stuff */
#define TUNER_STD_PAL_BG   (TUNER_STD_PAL_B     |\
                            TUNER_STD_PAL_B1    |\
                            TUNER_STD_PAL_G)
#define TUNER_STD_PAL_DK   (TUNER_STD_PAL_D     |\
                            TUNER_STD_PAL_D1    |\
                            TUNER_STD_PAL_K)
#define TUNER_STD_PAL      (TUNER_STD_PAL_BG    |\
                            TUNER_STD_PAL_DK    |\
                            TUNER_STD_PAL_H     |\
                            TUNER_STD_PAL_I)
#define TUNER_STD_NTSC     (TUNER_STD_NTSC_M    |\
                            TUNER_STD_NTSC_M_JP |\
                            TUNER_STD_NTSC_M_KR)
#define TUNER_STD_SECAM_DK (TUNER_STD_SECAM_D   |\
                            TUNER_STD_SECAM_K   |\
                            TUNER_STD_SECAM_K1)
#define TUNER_STD_SECAM    (TUNER_STD_SECAM_B   |\
                            TUNER_STD_SECAM_G   |\
                            TUNER_STD_SECAM_H   |\
                            TUNER_STD_SECAM_DK  |\
                            TUNER_STD_SECAM_L   |\
                            TUNER_STD_SECAM_LC)
#define TUNER_STD_525_60   (TUNER_STD_PAL_M     |\
                            TUNER_STD_PAL_60    |\
                            TUNER_STD_NTSC      |\
                            TUNER_STD_NTSC_443)
#define TUNER_STD_625_50   (TUNER_STD_PAL       |\
                            TUNER_STD_PAL_N     |\
                            TUNER_STD_PAL_Nc    |\
                            TUNER_STD_SECAM)
#define TUNER_STD_UNKNOWN   0
#define TUNER_STD_ALL      (TUNER_STD_525_60    |\
                            TUNER_STD_625_50)

typedef enum tuner_signal_status_e {
    TUNER_SIGNAL_STATUS_WEAK = 0, // RF PLL unlocked
    TUNER_SIGNAL_STATUS_STRONG,   // RF PLL   locked
}tuner_signal_status_t;

typedef struct tuner_parm_s {
    unsigned int               rangelow;  // tuner frequency is in the unit of Hz
    unsigned int               rangehigh; // tuner frequency is in the unit of Hz
    enum tuner_signal_status_e signal;
             int               			if_status;
    unsigned int               reserved;  // reserved
} tuner_parm_t;

#ifdef CONFIG_TVIN_TUNER_SI2176
/*cmd TVIN_IOC_G_TUNER_STATUS reply struct */
typedef struct si2176_tuner_status_struct_s{ 
      unsigned char   tcint;
      unsigned char   rssilint;
      unsigned char   rssihint;
               int    vco_code;
      unsigned char   tc;
      unsigned char   rssil;
      unsigned char   rssih;
               char   rssi;
      unsigned int   freq;
      unsigned char   mode;
   }  si2176_tuner_status_struct_t;

  typedef struct si2176_atv_status_struct_s{
      unsigned char   chlint;
      unsigned char   pclint;
      unsigned char   dlint;
      unsigned char   snrlint;
      unsigned char   snrhint;
      unsigned char   audio_chan_bw;
      unsigned char   chl;
      unsigned char   pcl;
      unsigned char   dl;
      unsigned char   snrl;
      unsigned char   snrh;
      unsigned char   video_snr;
               int    afc_freq;
               int    video_sc_spacing;
      unsigned char   video_sys;
      unsigned char   color;
      unsigned char   trans;
      unsigned char   audio_sys;
      unsigned char   audio_demod_mode;
   }  si2176_atv_status_struct_t;
typedef struct si2176_tuner_status_s{
    struct si2176_tuner_status_struct_s si2176_tuner_s;
    struct si2176_atv_status_struct_s   si2176_atv_s;
    }si2176_tuner_status_t;
#endif
typedef struct tuner_freq_s {
    unsigned int freq;     // tuner frequency is in the unit of Hz
    unsigned int reserved; // reserved
} tuner_freq_s;

// for pin selection
typedef enum tvafe_adc_pin_e {
    TVAFE_ADC_PIN_NULL = 0,
    TVAFE_ADC_PIN_A_PGA_0,
    TVAFE_ADC_PIN_A_PGA_1,
    TVAFE_ADC_PIN_A_PGA_2,
    TVAFE_ADC_PIN_A_PGA_3,
    TVAFE_ADC_PIN_A_PGA_4,
    TVAFE_ADC_PIN_A_PGA_5,
    TVAFE_ADC_PIN_A_PGA_6,
    TVAFE_ADC_PIN_A_PGA_7,
    TVAFE_ADC_PIN_A_0,
    TVAFE_ADC_PIN_A_1,
    TVAFE_ADC_PIN_A_2,
    TVAFE_ADC_PIN_A_3,
    TVAFE_ADC_PIN_A_4,
    TVAFE_ADC_PIN_A_5,
    TVAFE_ADC_PIN_A_6,
    TVAFE_ADC_PIN_A_7,
    TVAFE_ADC_PIN_B_0,
    TVAFE_ADC_PIN_B_1,
    TVAFE_ADC_PIN_B_2,
    TVAFE_ADC_PIN_B_3,
    TVAFE_ADC_PIN_B_4,
    TVAFE_ADC_PIN_B_5,
    TVAFE_ADC_PIN_B_6,
    TVAFE_ADC_PIN_B_7,
    TVAFE_ADC_PIN_C_0,
    TVAFE_ADC_PIN_C_1,
    TVAFE_ADC_PIN_C_2,
    TVAFE_ADC_PIN_C_3,
    TVAFE_ADC_PIN_C_4,
    TVAFE_ADC_PIN_C_5,
    TVAFE_ADC_PIN_C_6,
    TVAFE_ADC_PIN_C_7,
    TVAFE_ADC_PIN_D_0,
    TVAFE_ADC_PIN_D_1,
    TVAFE_ADC_PIN_D_2,
    TVAFE_ADC_PIN_D_3,
    TVAFE_ADC_PIN_D_4,
    TVAFE_ADC_PIN_D_5,
    TVAFE_ADC_PIN_D_6,
    TVAFE_ADC_PIN_D_7,
    TVAFE_ADC_PIN_SOG_0,
    TVAFE_ADC_PIN_SOG_1,
    TVAFE_ADC_PIN_SOG_2,
    TVAFE_ADC_PIN_SOG_3,
    TVAFE_ADC_PIN_SOG_4,
    TVAFE_ADC_PIN_SOG_5,
    TVAFE_ADC_PIN_SOG_6,
    TVAFE_ADC_PIN_SOG_7,
    TVAFE_ADC_PIN_MAX,
} tvafe_adc_pin_t;

typedef enum tvafe_src_sig_e {
    CVBS0_Y = 0,
    CVBS0_SOG,
    CVBS1_Y,
    CVBS1_SOG,
    CVBS2_Y,
    CVBS2_SOG,
    CVBS3_Y,
    CVBS3_SOG,
    CVBS4_Y,
    CVBS4_SOG,
    CVBS5_Y,
    CVBS5_SOG,
    CVBS6_Y,
    CVBS6_SOG,
    CVBS7_Y,
    CVBS7_SOG,
    S_VIDEO0_Y,
    S_VIDEO0_C,
    S_VIDEO0_SOG,
    S_VIDEO1_Y,
    S_VIDEO1_C,
    S_VIDEO1_SOG,
    S_VIDEO2_Y,
    S_VIDEO2_C,
    S_VIDEO2_SOG,
    S_VIDEO3_Y,
    S_VIDEO3_C,
    S_VIDEO3_SOG,
    S_VIDEO4_Y,
    S_VIDEO4_C,
    S_VIDEO4_SOG,
    S_VIDEO5_Y,
    S_VIDEO5_C,
    S_VIDEO5_SOG,
    S_VIDEO6_Y,
    S_VIDEO6_C,
    S_VIDEO6_SOG,
    S_VIDEO7_Y,
    S_VIDEO7_C,
    S_VIDEO7_SOG,
    VGA0_G,
    VGA0_B,
    VGA0_R,
    VGA0_SOG,
    VGA1_G,
    VGA1_B,
    VGA1_R,
    VGA1_SOG,
    VGA2_G,
    VGA2_B,
    VGA2_R,
    VGA2_SOG,
    VGA3_G,
    VGA3_B,
    VGA3_R,
    VGA3_SOG,
    VGA4_G,
    VGA4_B,
    VGA4_R,
    VGA4_SOG,
    VGA5_G,
    VGA5_B,
    VGA5_R,
    VGA5_SOG,
    VGA6_G,
    VGA6_B,
    VGA6_R,
    VGA6_SOG,
    VGA7_G,
    VGA7_B,
    VGA7_R,
    VGA7_SOG,
    COMP0_Y,
    COMP0_PB,
    COMP0_PR,
    COMP0_SOG,
    COMP1_Y,
    COMP1_PB,
    COMP1_PR,
    COMP1_SOG,
    COMP2_Y,
    COMP2_PB,
    COMP2_PR,
    COMP2_SOG,
    COMP3_Y,
    COMP3_PB,
    COMP3_PR,
    COMP3_SOG,
    COMP4_Y,
    COMP4_PB,
    COMP4_PR,
    COMP4_SOG,
    COMP5_Y,
    COMP5_PB,
    COMP5_PR,
    COMP5_SOG,
    COMP6_Y,
    COMP6_PB,
    COMP6_PR,
    COMP6_SOG,
    COMP7_Y,
    COMP7_PB,
    COMP7_PR,
    COMP7_SOG,
    SCART0_G,
    SCART0_B,
    SCART0_R,
    SCART0_CVBS,
    SCART1_G,
    SCART1_B,
    SCART1_R,
    SCART1_CVBS,
    SCART2_G,
    SCART2_B,
    SCART2_R,
    SCART2_CVBS,
    SCART3_G,
    SCART3_B,
    SCART3_R,
    SCART3_CVBS,
    SCART4_G,
    SCART4_B,
    SCART4_R,
    SCART4_CVBS,
    SCART5_G,
    SCART5_B,
    SCART5_R,
    SCART5_CVBS,
    SCART6_G,
    SCART6_B,
    SCART6_R,
    SCART6_CVBS,
    SCART7_G,
    SCART7_B,
    SCART7_R,
    SCART7_CVBS,
    TVAFE_SRC_SIG_MAX_NUM,
} tvafe_src_sig_t;

typedef struct tvafe_pin_mux_s {
	enum tvafe_adc_pin_e pin[TVAFE_SRC_SIG_MAX_NUM];
} tvafe_pin_mux_t;

// ***************************************************************************
// *** IOCTL command definition **********************************************
// ***************************************************************************

#define TVIN_IOC_MAGIC 'T'

//GENERAL
#define TVIN_IOC_OPEN               _IOW(TVIN_IOC_MAGIC, 0x01, struct tvin_parm_s)
#define TVIN_IOC_START_DEC          _IOW(TVIN_IOC_MAGIC, 0x02, struct tvin_parm_s)
#define TVIN_IOC_STOP_DEC           _IO( TVIN_IOC_MAGIC, 0x03)
#define TVIN_IOC_CLOSE              _IO( TVIN_IOC_MAGIC, 0x04)
#define TVIN_IOC_G_PARM             _IOR(TVIN_IOC_MAGIC, 0x05, struct tvin_parm_s)
#define TVIN_IOC_S_PARM             _IOW(TVIN_IOC_MAGIC, 0x06, struct tvin_parm_s)
#define TVIN_IOC_G_SIG_INFO         _IOR(TVIN_IOC_MAGIC, 0x07, struct tvin_info_s)
#define TVIN_IOC_G_BUF_INFO         _IOR(TVIN_IOC_MAGIC, 0x08, struct tvin_buf_info_s)
#define TVIN_IOC_START_GET_BUF      _IO( TVIN_IOC_MAGIC, 0x09)
#define TVIN_IOC_GET_BUF            _IOR(TVIN_IOC_MAGIC, 0x10, struct tvin_video_buf_s)
#define TVIN_IOC_PAUSE_DEC          _IO(TVIN_IOC_MAGIC, 0x41)
#define TVIN_IOC_RESUME_DEC         _IO(TVIN_IOC_MAGIC, 0x42)
#define TVIN_IOC_VF_REG             _IO(TVIN_IOC_MAGIC, 0x43)
#define TVIN_IOC_VF_UNREG           _IO(TVIN_IOC_MAGIC, 0x44)


//TVAFE
#define TVIN_IOC_S_AFE_ADC_CAL      _IOW(TVIN_IOC_MAGIC, 0x11, struct tvafe_adc_cal_s)
#define TVIN_IOC_G_AFE_ADC_CAL      _IOR(TVIN_IOC_MAGIC, 0x12, struct tvafe_adc_cal_s)
#define TVIN_IOC_G_AFE_COMP_WSS     _IOR(TVIN_IOC_MAGIC, 0x13, struct tvafe_comp_wss_s)
#define TVIN_IOC_S_AFE_VGA_EDID     _IOW(TVIN_IOC_MAGIC, 0x14, struct tvafe_vga_edid_s)
#define TVIN_IOC_G_AFE_VGA_EDID     _IOR(TVIN_IOC_MAGIC, 0x15, struct tvafe_vga_edid_s)
#define TVIN_IOC_S_AFE_VGA_PARM     _IOW(TVIN_IOC_MAGIC, 0x16, struct tvafe_vga_parm_s)
#define TVIN_IOC_G_AFE_VGA_PARM     _IOR(TVIN_IOC_MAGIC, 0x17, struct tvafe_vga_parm_s)
#define TVIN_IOC_S_AFE_VGA_AUTO     _IO( TVIN_IOC_MAGIC, 0x18)
#define TVIN_IOC_G_AFE_CMD_STATUS   _IOR(TVIN_IOC_MAGIC, 0x19, enum tvafe_cmd_status_e)
#define TVIN_IOC_G_AFE_CVBS_LOCK    _IOR(TVIN_IOC_MAGIC, 0x1a, enum tvafe_cvbs_video_e)
#define TVIN_IOC_S_AFE_CVBS_STD     _IOW(TVIN_IOC_MAGIC, 0x1b, enum tvin_sig_fmt_e)

//TUNER
#define TVIN_IOC_G_TUNER_STD        _IOR(TVIN_IOC_MAGIC, 0x21, tuner_std_id)
#define TVIN_IOC_S_TUNER_STD        _IOW(TVIN_IOC_MAGIC, 0x22, tuner_std_id)
#define TVIN_IOC_G_TUNER_FREQ       _IOR(TVIN_IOC_MAGIC, 0x23, struct tuner_freq_s)
#define TVIN_IOC_S_TUNER_FREQ       _IOW(TVIN_IOC_MAGIC, 0x24, struct tuner_freq_s)
#define TVIN_IOC_G_TUNER_PARM       _IOR(TVIN_IOC_MAGIC, 0x25, struct tuner_parm_s)
#ifdef CONFIG_TVIN_TUNER_SI2176
#define TVIN_IOC_G_TUNER_STATUS     _IOR(TVIN_IOC_MAGIC, 0x26, struct si2176_tuner_status_s)
#endif

/*
    below macro defined applied to camera driver
*/
typedef enum camera_light_mode_e {
    ADVANCED_AWB = 0,
    SIMPLE_AWB,
    MANUAL_DAY,
    MANUAL_A,
    MANUAL_CWF,
    MANUAL_CLOUDY,
}camera_light_mode_t;

typedef enum camera_saturation_e {
    SATURATION_N4_STEP = 0,
    SATURATION_N3_STEP,
    SATURATION_N2_STEP,
    SATURATION_N1_STEP,
    SATURATION_0_STEP,
    SATURATION_P1_STEP,
    SATURATION_P2_STEP,
    SATURATION_P3_STEP,
    SATURATION_P4_STEP,
}camera_saturation_t;


typedef enum camera_brightness_e {
    BRIGHTNESS_N4_STEP = 0,
    BRIGHTNESS_N3_STEP,
    BRIGHTNESS_N2_STEP,
    BRIGHTNESS_N1_STEP,
    BRIGHTNESS_0_STEP,
    BRIGHTNESS_P1_STEP,
    BRIGHTNESS_P2_STEP,
    BRIGHTNESS_P3_STEP,
    BRIGHTNESS_P4_STEP,
}camera_brightness_t;

typedef enum camera_contrast_e {
    CONTRAST_N4_STEP = 0,
    CONTRAST_N3_STEP,
    CONTRAST_N2_STEP,
    CONTRAST_N1_STEP,
    CONTRAST_0_STEP,
    CONTRAST_P1_STEP,
    CONTRAST_P2_STEP,
    CONTRAST_P3_STEP,
    CONTRAST_P4_STEP,
}camera_contrast_t;

typedef enum camera_hue_e {
    HUE_N180_DEGREE = 0,
    HUE_N150_DEGREE,
    HUE_N120_DEGREE,
    HUE_N90_DEGREE,
    HUE_N60_DEGREE,
    HUE_N30_DEGREE,
    HUE_0_DEGREE,
    HUE_P30_DEGREE,
    HUE_P60_DEGREE,
    HUE_P90_DEGREE,
    HUE_P120_DEGREE,
    HUE_P150_DEGREE,
}camera_hue_t;

typedef enum camera_special_effect_e {
    SPECIAL_EFFECT_NORMAL = 0,
    SPECIAL_EFFECT_BW,
    SPECIAL_EFFECT_BLUISH,
    SPECIAL_EFFECT_SEPIA,
    SPECIAL_EFFECT_REDDISH,
    SPECIAL_EFFECT_GREENISH,
    SPECIAL_EFFECT_NEGATIVE,
}camera_special_effect_t;

typedef enum camera_exposure_e {
    EXPOSURE_N4_STEP = 0,
    EXPOSURE_N3_STEP,
    EXPOSURE_N2_STEP,
    EXPOSURE_N1_STEP,
    EXPOSURE_0_STEP,
    EXPOSURE_P1_STEP,
    EXPOSURE_P2_STEP,
    EXPOSURE_P3_STEP,
    EXPOSURE_P4_STEP,
}camera_exposure_t;


typedef enum camera_sharpness_e {
    SHARPNESS_1_STEP = 0,
    SHARPNESS_2_STEP,
    SHARPNESS_3_STEP,
    SHARPNESS_4_STEP,
    SHARPNESS_5_STEP,
    SHARPNESS_6_STEP,
    SHARPNESS_7_STEP,
    SHARPNESS_8_STEP,
    SHARPNESS_AUTO_STEP,
}camera_sharpness_t;

typedef enum camera_mirror_flip_e {
    MF_NORMAL = 0,
    MF_MIRROR,
    MF_FLIP,
    MF_MIRROR_FLIP,
}camera_mirror_flip_t;


typedef enum camera_wb_flip_e {
    CAM_WB_AUTO = 0,
    CAM_WB_CLOUD,
    CAM_WB_DAYLIGHT,
    CAM_WB_INCANDESCENCE,
    CAM_WB_TUNGSTEN,
    CAM_WB_FLUORESCENT,
    CAM_WB_MANUAL,
}camera_wb_flip_t;
typedef enum camera_night_mode_flip_e {
    CAM_NM_AUTO = 0,
	CAM_NM_ENABLE,
}camera_night_mode_flip_t;
typedef enum camera_effect_flip_e {
    CAM_EFFECT_ENC_NORMAL = 0,
	CAM_EFFECT_ENC_GRAYSCALE,
	CAM_EFFECT_ENC_SEPIA,
	CAM_EFFECT_ENC_SEPIAGREEN,
	CAM_EFFECT_ENC_SEPIABLUE,
	CAM_EFFECT_ENC_COLORINV,
}camera_effect_flip_t;



typedef struct camera_info_s {
	#define AMLOGIC_CAMERA_OV5640_NAME     			"camera_ov5640"
	#define AMLOGIC_CAMERA_GT2005_NAME     			"camera_gt2005"
	const char * camera_name;
    enum camera_saturation_e saturation;
    enum camera_brightness_e brighrness;
    enum camera_contrast_e contrast;
    enum camera_hue_e hue;
//  enum camera_special_effect_e special_effect;
    enum camera_exposure_e exposure;
    enum camera_sharpness_e sharpness;
    enum camera_mirror_flip_e mirro_flip;
    enum tvin_sig_fmt_e resolution;
	enum camera_wb_flip_e white_balance;
	enum camera_night_mode_flip_e night_mode;
	enum camera_effect_flip_e effect;
	int qulity;
	;
}camera_info_t;



    // ***************************************************************************
    // *** IOCTL command definitions *****************************************
    // ***************************************************************************

#define CAMERA_IOC_MAGIC 'C'


#define CAMERA_IOC_START        _IOW(CAMERA_IOC_MAGIC, 0x01, struct camera_info_s)
#define CAMERA_IOC_STOP         _IO(CAMERA_IOC_MAGIC, 0x02)
#define CAMERA_IOC_SET_PARA     _IOW(CAMERA_IOC_MAGIC, 0x03, struct camera_info_s)
#define CAMERA_IOC_GET_PARA     _IOR(CAMERA_IOC_MAGIC, 0x04, struct camera_info_s)
#define CAMERA_IOC_START_CAPTURE_PARA     _IOR(CAMERA_IOC_MAGIC, 0x05, struct camera_info_s)
#define CAMERA_IOC_STOP_CAPTURE_PARA     _IOR(CAMERA_IOC_MAGIC, 0x06, struct camera_info_s)



/*
    macro defined applied to camera driver is ending
*/
extern int start_tvin_service(int no ,tvin_parm_t *para);
extern int stop_tvin_service(int no);
extern void set_tvin_canvas_info(int start , int num);
extern void get_tvin_canvas_info(int* start , int* num);
#endif


