/*
 *gc2015 - This code emulates a real video device with v4l2 api
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the BSD Licence, GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <media/videobuf-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#include <linux/i2c.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-i2c-drv.h>
#include <media/amlogic/aml_camera.h>

#include <mach/am_regs.h>
#include <mach/pinmux.h>
#include <media/amlogic/656in.h>
#include "common/plat_ctrl.h"
#include "common/vmapi.h"
#include <mach/mod_gate.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend gc2015_early_suspend;
#endif

#define GC2015_CAMERA_MODULE_NAME "gc2015"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */

#define GC2015_CAMERA_MAJOR_VERSION 0
#define GC2015_CAMERA_MINOR_VERSION 7
#define GC2015_CAMERA_RELEASE 0
#define GC2015_CAMERA_VERSION \
	KERNEL_VERSION(GC2015_CAMERA_MAJOR_VERSION, GC2015_CAMERA_MINOR_VERSION, GC2015_CAMERA_RELEASE)

MODULE_DESCRIPTION("gc2015 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

static unsigned video_nr = -1;  /* videoX start number, -1 is autodetect. */

static unsigned debug;
//module_param(debug, uint, 0644);
//MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
//module_param(vid_limit, uint, 0644);
//MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");

static int gc2015_have_open=0;

#define V4L2_IDENT_SENSOR 0x2015

//for internel driver debug
#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

#define DEV_DBG_EN   		0 
#if(DEV_DBG_EN == 1)		
#define csi_dev_dbg(x,arg...) printk(KERN_INFO"[CSI_DEBUG][GC2015]"x,##arg)
#else
#define csi_dev_dbg(x,arg...) 
#endif

#define csi_dev_err(x,arg...) printk(KERN_INFO"[CSI_ERR][GC2015]"x,##arg)
#define csi_dev_print(x,arg...) printk(KERN_INFO"[CSI][GC2015]"x,##arg)

#define REG_ADDR_STEP 1
#define REG_DATA_STEP 1
#define REG_STEP 			(REG_ADDR_STEP+REG_DATA_STEP)

enum v4l2_whiteblance{
    V4L2_WB_AUTO = 0,
    V4L2_WB_CLOUD,
    V4L2_WB_DAYLIGHT,
    V4L2_WB_INCANDESCENCE,
    V4L2_WB_FLUORESCENT,
    V4L2_WB_TUNGSTEN,
};

struct regval_list {
	unsigned char reg_num[REG_ADDR_STEP];
	unsigned char value[REG_DATA_STEP];
};


/* supported controls */
static struct v4l2_queryctrl gc2015_qctrl[] = {
    {
		.id            = V4L2_CID_DO_WHITE_BALANCE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "white balance",
		.minimum       = 0,
		.maximum       = 5,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_AUTO_WHITE_BALANCE,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "auto white balance",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_EXPOSURE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "exposure",
		.minimum       = -4,
		.maximum       = 4,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_EXPOSURE_AUTO,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "auto exposure",
		.minimum       = 0,
		.maximum       = 3,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
	    .id            = V4L2_CID_CONTRAST,
	    .type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "contrast",
		.minimum       = -4,
		.maximum       = 4,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
	    .id            = V4L2_CID_SATURATION,
	    .type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "saturation",
		.minimum       = -4,
		.maximum       = 4,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_COLORFX,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "effect",
		.minimum       = 0,
		.maximum       = 9,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
	    .id            = V4L2_CID_VFLIP,
	    .type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "vflip",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
	    .id            = V4L2_CID_HFLIP,
	    .type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "hflip",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},
};

/* ------------------------------------------------------------------
	Basic structures
   ------------------------------------------------------------------*/

struct gc2015_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct gc2015_fmt formats[] = {
	{
		.name     = "RGB565 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
		.depth    = 16,
	},

	{
		.name     = "RGB888 (24)",
		.fourcc   = V4L2_PIX_FMT_RGB24, /* 24  RGB-8-8-8 */
		.depth    = 24,
	},
	{
		.name     = "BGR888 (24)",
		.fourcc   = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
		.depth    = 24,
	},
	{
		.name     = "12  Y/CbCr 4:2:0",
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,
	},
	{
		.name     = "12  Y/CbCr 4:2:0",
		.fourcc   = V4L2_PIX_FMT_NV21,
		.depth    = 12,
	},
	{
		.name     = "YUV420P",
		.fourcc   = V4L2_PIX_FMT_YUV420,
		.depth    = 12,
	}
#if 0
	{
		.name     = "4:2:2, packed, YUYV",
		.fourcc   = V4L2_PIX_FMT_VYUY,
		.depth    = 16,
	},
	{
		.name     = "RGB565 (LE)",
		.fourcc   = V4L2_PIX_FMT_RGB565, /* gggbbbbb rrrrrggg */
		.depth    = 16,
	},
	{
		.name     = "RGB555 (LE)",
		.fourcc   = V4L2_PIX_FMT_RGB555, /* gggbbbbb arrrrrgg */
		.depth    = 16,
	},
	{
		.name     = "RGB555 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB555X, /* arrrrrgg gggbbbbb */
		.depth    = 16,
	},
#endif
};

static struct gc2015_fmt *get_format(struct v4l2_format *f)
{
	struct gc2015_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}

struct sg_to_addr {
	int pos;
	struct scatterlist *sg;
};

/* buffer for one video frame */
struct gc2015_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct gc2015_fmt        *fmt;
};

struct gc2015_dmaqueue {
	struct list_head       active;

	/* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
	/* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

static LIST_HEAD(gc2015_devicelist);

struct gc2015_device {
	struct list_head			gc2015_devicelist;
	struct v4l2_subdev			sd;
	struct v4l2_device			v4l2_dev;

	spinlock_t                 slock;
	struct mutex				mutex;

	int                        users;

	/* various device info */
	struct video_device        *vdev;

	struct gc2015_dmaqueue       vidq;

	/* Several counters */
	unsigned long              jiffies;

	/* Input Number */
	int			   input;

	/* platform device data from board initting. */
	aml_plat_cam_data_t platform_dev_data;

	/* Control 'registers' */
	int 			   qctl_regs[ARRAY_SIZE(gc2015_qctrl)];
};

static inline struct gc2015_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc2015_device, sd);
}

struct gc2015_fh {
	struct gc2015_device            *dev;

	/* video capture */
	struct gc2015_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	enum v4l2_buf_type         type;
	int			   input; 	/* Input Number on bars */
	int  stream_on;
};

static inline struct gc2015_fh *to_fh(struct gc2015_device *dev)
{
	return container_of(dev, struct gc2015_fh, dev);
}

static struct v4l2_frmsize_discrete gc2015_prev_resolution[2]= //should include 320x240 and 640x480, those two size are used for recording
{
	{352,288},
	{640,480},
};

static struct v4l2_frmsize_discrete gc2015_pic_resolution[1]=
{
	{640,480},
};

/* ------------------------------------------------------------------
	reg spec of GC2015
   ------------------------------------------------------------------*/
static struct regval_list sensor_default_regs[] = {
	{{0xfe},{0x80}}, //soft reset
	{{0xfe},{0x80}}, //soft reset
	{{0xfe},{0x80}}, //soft reset
	     
	{{0xfe},{0x00}}, //page0
	{{0x45},{0x00}}, //output_disable
	//////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////preview capture switch /////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////
	//preview
	{{0x02},{0x01}},//preview mode
	{{0x2a},{0xca}},//[7]col_binning  [6]even skip
	{{0x48},{0x40}},//manual_gain
	      
	      
	{{0x7d} , {0x86}},// r ratio
	{{0x7e} , {0x80}},// g ratio
	{{0x7f} , {0x80}}, //b ratio 

	{{0xfe},{0x01}},//page1
	////////////////////////////////////////////////////////////////////////
	////////////////////////// preview LSC /////////////////////////////////
	////////////////////////////////////////////////////////////////////////

	{{0xb0},{0x03}},//0x13//[4]Y_LSC_en [3]lsc_compensate [2]signed_b4 [1:0]pixel array select
	{{0xb1},{0x20}},//P_LSC_red_b2
	{{0xb2},{0x20}},//P_LSC_green_b2
	{{0xb3},{0x20}},//P_LSC_blue_b2
	{{0xb4},{0x20}},//P_LSC_red_b4
	{{0xb5},{0x20}},//P_LSC_green_b4
	{{0xb6},{0x20}},//P_LSC_blue_b4
	{{0xb7},{0x00}},//P_LSC_compensate_b2
	{{0xb8},{0x80}},//P_LSC_row_center  344   (600/2-100)/2=100
	{{0xb9},{0x80}},//P_LSC_col_center  544   (800/2-200)/2=100

	////////////////////////////////////////////////////////////////////////
	////////////////////////// capture LSC ///////////////////////////
	////////////////////////////////////////////////////////////////////////
	{{0xba},{0x13}}, //0x03//[4]Y_LSC_en [3]lsc_compensate [2]signed_b4 [1:0]pixel array select
	{{0xbb},{0x20}}, //C_LSC_red_b2
	{{0xbc},{0x20}}, //C_LSC_green_b2
	{{0xbd},{0x20}}, //C_LSC_blue_b2
	{{0xbe},{0x20}}, //C_LSC_red_b4
	{{0xbf},{0x20}}, //C_LSC_green_b4
	{{0xc0},{0x20}}, //C_LSC_blue_b4
	{{0xc1},{0x00}}, //C_Lsc_compensate_b2
	{{0xc2},{0x80}}, //C_LSC_row_center  344   (1200/2-344)/2=128
	{{0xc3},{0x80}}, //C_LSC_col_center  544   (1600/2-544)/2=128
	    
	{{0xfe},{0x00}}, //page0

	////////////////////////////////////////////////////////////////////////
	////////////////////////// analog configure ///////////////////////////
	////////////////////////////////////////////////////////////////////////
	{{0x29},{0x00}}, //cisctl mode 1
	{{0x2b},{0x06}}, //cisctl mode 3	
	{{0x32},{0x34}}, //analog mode 1  0x0c  for update
	{{0x33},{0x0f}}, //analog mode 2
	{{0x34},{0x00}}, //[6:4]da_rsg
	            
	{{0x35},{0x88}}, //Vref_A25
	{{0x37},{0x16}}, //Drive Current
	     
	//////////////////////////////////////////////////////////////////
	/////////////////////////ISP Related//////////////////////////////
	///////////////////////////////////////////////////////////////////
	{{0x40},{0xff}}, 
	{{0x41},{0x20}}, //[5]skin_detectionenable[2]auto_gray  [1]y_gamma
	{{0x42},{0xf6}}, //0x76//[7]auto_sa[6]auto_ee[5]auto_dndd[4]auto_lsc[3]na[2]abs  [1]awb
	{{0x4b},{0xea}}, //[1]AWB_gain_mode  1:atpregain0:atpostgain
	{{0x4d},{0x03}}, //[1]inbf_en
	{{0x4f},{0x01}}, //AEC enable
	     
	//////////////////////////////////////////////////////////////////
	///////////////////////// BLK  ///////////////////////////////////
	//////////////////////////////////////////////////////////////////
	{{0x63},{0x77}},//BLK mode 1
	{{0x66},{0x00}},//BLK global offset
	{{0x6d},{0x00}}, //0x04
	{{0x6e},{0x1a}},//BLK offset submode,offset R  0x18
	{{0x6f},{0x20}},//0x10
	{{0x70},{0x1a}},//0x18
	{{0x71},{0x20}},//0x10
	{{0x73},{0x00}},//0x00
	      
	{{0x77},{0x80}}, //Channel_gain_R
	{{0x78},{0x80}},//Channel_gain_G
	{{0x79},{0x90}},	//Channel_gain_B   
	//////////////////////////////////////////////////////////////////
	///////////////////////// DNDD ////////////////////////////////
	//////////////////////////////////////////////////////////////////
	{{0x80},{0x07}}, //[7]dn_inc_or_dec [4]zero_weight_mode[3]share [2]c_weight_adap [1]dn_lsc_mode [0]dn_b
	{{0x82},{0x08}}, //DN lilat b base
	{{0x83},{0x03}}, 
	//////////////////////////////////////////////////////////////////
	///////////////////////// EEINTP ////////////////////////////////
	//////////////////////////////////////////////////////////////////
	{{0x8a},{0x7c}},
	{{0x8c},{0x02}},
	{{0x8e},{0x02}},
	{{0x8f},{0x48}},//[7:4] edge effect use 5x5 template, [3:0] edge effect use 3x3 template
	     
	//////////////////////////////////////////////////////////////////
	//////////////////////// CC_t ///////////////////////////////
	//////////////////////////////////////////////////////////////////
	{{0xb0},{0x48}}, //0x44
	{{0xb1},{0xfe}},
	{{0xb2},{0x00}},
	{{0xb3},{0xf0}},//0xf8
	{{0xb4},{0x50}},//0x48
	{{0xb5},{0xf8}},
	{{0xb6},{0x00}},
	{{0xb7},{0x00}},//0x04
	{{0xb8},{0x00}},

	{{0xd3},{0x34}},//contrast
	      
	///////////////////////////////////////////////////////////////////
	///////////////////////// GAMMA ///////////////////////////////////
	///////////////////////////////////////////////////////////////////
	//RGB_gamma
#if 0
	{{0xbf},{0x0e}},
	{{0xc0},{0x1c}},
	{{0xc1},{0x34}},
	{{0xc2},{0x48}},
	{{0xc3},{0x5a}},
	{{0xc4},{0x6b}},
	{{0xc5},{0x7b}},
	{{0xc6},{0x95}},
	{{0xc7},{0xab}},
	{{0xc8},{0xbf}},
	{{0xc9},{0xce}},
	{{0xca},{0xd9}},
	{{0xcb},{0xe4}},
	{{0xcc},{0xec}},
	{{0xcd},{0xf7}},
	{{0xce},{0xfd}},
	{{0xcf},{0xff}},
#endif
	{{0xbF},{ 0x0B}}, 
	{{0xc0},{ 0x16}}, 
	{{0xc1},{ 0x29}}, 
	{{0xc2},{ 0x3C}}, 
	{{0xc3},{ 0x4F}}, 
	{{0xc4},{ 0x5F}}, 
	{{0xc5},{ 0x6F}}, 
	{{0xc6},{ 0x8A}}, 
	{{0xc7},{ 0x9F}}, 
	{{0xc8},{ 0xB4}}, 
	{{0xc9},{ 0xC6}}, 
	{{0xcA},{ 0xD3}}, 
	{{0xcB},{ 0xDD}},  
	{{0xcC},{ 0xE5}},  
	{{0xcD},{ 0xF1}}, 
	{{0xcE},{ 0xFA}}, 
	{{0xcF},{ 0xFF}}, 
	//////////////////////////////////////////////////////////////////
	//////////////////////// YCP_t  ///////////////////////////////
	//////////////////////////////////////////////////////////////////
	{{0xd1},{0x38}}, //saturation
	{{0xd2},{0x38}}, //saturation
	{{0xdd},{0x38}}, //edge _dec
	{{0xde},{0x21}}, //auto_gray 
	      
	//////////////////////////////////////////////////////////////////
	///////////////////////// ASDE ////////////////////////////////
	//////////////////////////////////////////////////////////////////
	{{0x98},{0x30}},
	{{0x99},{0xf0}},
	{{0x9b},{0x00}},
	            
	{{0xfe},{0x01}}, //page1
	//////////////////////////////////////////////////////////////////
	///////////////////////// AEC  ////////////////////////////////
	//////////////////////////////////////////////////////////////////
	{{0x10},{0x45}},//AEC mode 1
	{{0x11},{0x32}},//[7]fix target  
	{{0x13},{0x68}},//0x60
	{{0x17},{0x00}},
	{{0x1b},{0x97}},//aec fast
	{{0x1c},{0x96}},
	{{0x1e},{0x11}},
	{{0x21},{0xa0}},//max_post_gain
	{{0x22},{0x40}},//max_pre_gain
	{{0x2d},{0x06}},//P_N_AEC_exp_level_1[12:8]
	{{0x2e},{0x00}},//P_N_AEC_exp_level_1[7:0]
	{{0x1e},{0x32}},
	{{0x33},{0x00}},//[6:5]max_exp_level [4:0]min_exp_level
	     
	/////////////////////////////////////////////////////////////////
	////////////////////////  AWB  ////////////////////////////////
	/////////////////////////////////////////////////////////////////
	{{0x57},{0x40}}, //number limit
	{{0x5d},{0x44}}, //
	{{0x5c},{0x35}}, //show mode,close dark_mode
	{{0x5e},{0x29}}, //close color temp
	{{0x5f},{0x50}},
	{{0x60},{0x50}}, 
	{{0x65},{0xc0}},
	      
	//////////////////////////////////////////////////////////////////
	/////////////////////////  ABS  ////////////////////////////////
	//////////////////////////////////////////////////////////////////
	{{0x80},{0x82}},
	{{0x81},{0x00}},
	{{0x83},{0x10}}, //ABS Y stretch limit   old value:0x00
	            
	{{0xfe},{0x00}},
	//////////////////////////////////////////////////////////////////
	/////////////////////////  OUT  ////////////////////////////////
	////////////////////////////////////////////////////////////////
	{{0x44},{0xa2}}, //YUV sequence
	{{0x45},{0x0f}}, //output enable
	{{0x46},{0x03}}, //sync mode

       {{0x50} , {0x01}},//out window for 800*600  crop enable
	{{0x51} , {0x00}},
	{{0x52} , {0x00}},
	{{0x53} , {0x00}},
	{{0x54} , {0x00}},
	{{0x55} , {0x02}},
	{{0x56} , {0x58}},// 600
	{{0x57} , {0x03}},
	{{0x58} , {0x20}},//800
	    
	//--------Updated By Mormo 2011/03/14 Start----------------//
	{{0xfe},{0x01}}, //page1
	{{0x34},{0x02}}, //Preview minimum exposure
	{{0xfe},{0x00}}, //page0
	//-------------Updated By Mormo 2011/03/14 End----------------//


	{{0x02},{0x01}},
	{{0x2a},{0xca}},
	{{0x55},{0x02}},
	{{0x56},{0x58}},
	{{0x57},{0x03}},
	{{0x58},{0x20}},
	{{0xfe},{0x00}},

	//frame rate
	{{0x05},{0x01}},
	{{0x06},{0xc1}},
	{{0x07},{0x00}},
	{{0x08},{0x40}},

	{{0xfe},{0x01}},
	{{0x29},{0x00}},
	{{0x2a},{0x80}},
	{{0x2b},{0x05}},
	{{0x2c},{0x00}},
	{{0x2d},{0x06}},
	{{0x2e},{0x00}},
	{{0x2f},{0x08}},
	{{0x30},{0x00}},
	{{0x31},{0x09}},
	{{0x32},{0x00}},
	{{0x33},{0x20}},

	{{0xfe},{0x00}},


#if 0	
////////////////////////////////////////////////////////////////////
/////////////////////////// ASDE ///////////////////////////////////
///////////below update for GC2015A   james 2012-2-9///////////////////

       {{0xfe},{0x00}},
	{{0x98},{ 0x3a}},
	{{0x99},{ 0x60}},
	{{0x9b},{ 0x00}},
	{{0x9f},{ 0x12}},
	{{0xa1},{ 0x80}},
	{{0xa2},{ 0x21}},
		
	{{0xfe},{ 0x01}},	//page1
	{{0xc5},{ 0x10}},
	{{0xc6},{ 0xff}},
	{{0xc7},{ 0xff}},
	{{0xc8},{ 0xff}},

////////////////////////////////////////////////////////////////////
/////////////////////////// AEC ////////////////////////////////////
////////////////////////////////////////////////////////////////////
	{{0x10},{ 0x09}},//AEC mode 1
	{{0x11},{ 0xb2}},//[7]fix target
	{{0x12},{ 0x20}},
	{{0x13},{ 0x78}},
	{{0x17},{ 0x00}},
	{{0x1c},{ 0x96}},
	{{0x1d},{ 0x04}},// sunlight step 
	{{0x1e},{ 0x11}},
	{{0x21},{ 0xc0}},//max_post_gain
	{{0x22},{ 0x60}},//max_pre_gain
	{{0x2d},{ 0x06}},//P_N_AEC_exp_level_1[12:8]
	{{0x2e},{ 0x00}},//P_N_AEC_exp_level_1[7:0]
	{{0x1e},{ 0x32}},
	{{0x33},{ 0x00}},//[6:5]max_exp_level [4:0]min_exp_level
	{{0x34},{ 0x04}},// min exp

////////////////////////////////////////////////////////////////////
/////////////////////////// Measure Window /////////////////////////
////////////////////////////////////////////////////////////////////
	{{0x06},{ 0x07);
	{{0x07},{ 0x03);
	{{0x08},{ 0x64);
	{{0x09},{ 0x4a);

////////////////////////////////////////////////////////////////////
/////////////////////////// AWB ////////////////////////////////////
////////////////////////////////////////////////////////////////////
	{{0x50},{ 0xf5);
	{{0x51},{ 0x18);
	{{0x53},{ 0x10);
	{{0x54},{ 0x20);
	{{0x55},{ 0x60);
	{{0x57},{ 0x33}},//number limit },{ 33 half },{ must <0x65 base on measure wnd now
	{{0x5d},{ 0x52}},//44
	{{0x5c},{ 0x25}},//show mode},{close dark_mode
	{{0x5e},{ 0x19}},//close color temp
	{{0x5f},{ 0x50}},//50
	{{0x60},{ 0x57}},//50
	{{0x61},{ 0xdf);
	{{0x62},{ 0x80}},//7b
	{{0x63},{ 0x08}},//20
	{{0x64},{ 0x5B);
	{{0x65},{ 0x90);
	{{0x66},{ 0xd0);
	{{0x67},{ 0x80}},//5a
	{{0x68},{ 0x68}},//68
	{{0x69},{ 0x90}},//80
#endif
};

/*
 * The white balance settings
 * Here only tune the R G B channel gain. 
 * The white balance enalbe bit is modified in sensor_s_autowb and sensor_s_wb
 */
static struct regval_list sensor_wb_auto_regs[] = {
	{{0xfe},{0x00}},
};

static struct regval_list sensor_wb_cloud_regs[] = {
	{{0xfe},{0x00}},
	{{0x7a},{0x8c}},
	{{0x7b},{0x50}},
	{{0x7c},{0x40}},
};

static struct regval_list sensor_wb_daylight_regs[] = {
	//tai yang guang
	 //Sunny 
	{{0xfe},{0x00}},
	{{0x7a},{0x74}},
	{{0x7b},{0x52}},
	{{0x7c},{0x40}},
};

static struct regval_list sensor_wb_incandescence_regs[] = {
	//bai re guang	
	{{0xfe},{0x00}},
	{{0x42},{0x74}},
	{{0x7a},{0x48}},
	{{0x7b},{0x40}},
	{{0x7c},{0x5c}},
};

static struct regval_list sensor_wb_fluorescent_regs[] = {
	//ri guang deng
	{{0xfe},{0x00}},
	{{0x42},{0x74}},
	{{0x7a},{0x40}},
	{{0x7b},{0x42}},
	{{0x7c},{0x50}},
};

static struct regval_list sensor_wb_tungsten_regs[] = {
	//wu si deng
	{{0xfe},{0x00}},
	{{0x42},{0x74}},
	{{0x7a},{0x40}},
	{{0x7b},{0x54}},
	{{0x7c},{0x70}},
};

/*
 * The color effect settings
 */
static struct regval_list sensor_colorfx_none_regs[] = {
	{{0xfe},{0x00}},
	{{0x43},{0x00}},
};

static struct regval_list sensor_colorfx_bw_regs[] = {
	
};

static struct regval_list sensor_colorfx_sepia_regs[] = {
	{{0xfe},{0x00}},
	{{0x43},{0x02}},
	{{0xda},{0xd0}},
	{{0xdb},{0x28}},
};

static struct regval_list sensor_colorfx_negative_regs[] = {
	{{0xfe},{0x00}},
	{{0x43},{0x01}},
};

static struct regval_list sensor_colorfx_emboss_regs[] = {
	{{0xfe},{0x00}},
	{{0x43},{0x02}},
	{{0xda},{0x00}},
	{{0xdb},{0x00}},
};

static struct regval_list sensor_colorfx_sketch_regs[] = {
//NULL
};

static struct regval_list sensor_colorfx_sky_blue_regs[] = {
	{{0xfe},{0x00}},
	{{0x43},{0x02}},
	{{0xda},{0x50}},
	{{0xdb},{0xe0}},
};

static struct regval_list sensor_colorfx_grass_green_regs[] = {
	{{0xfe},{0x00}},
	{{0x43},{0x02}},
	{{0xda},{0xc0}},
	{{0xdb},{0xc0}},	
};

static struct regval_list sensor_colorfx_skin_whiten_regs[] = {
//NULL
};

static struct regval_list sensor_colorfx_vivid_regs[] = {
//NULL
};

/*
 * The contrast setttings
 */
static struct regval_list sensor_contrast_neg4_regs[] = {
	 {{0xfe}, {0x00}},    
	 {{0xd3}, {0x28}}
};

static struct regval_list sensor_contrast_neg3_regs[] = {
	 {{0xfe}, {0x00}},    
	 {{0xd3}, {0x2c}} 
};

static struct regval_list sensor_contrast_neg2_regs[] = {
	   {{0xfe}, {0x00}},    
	   {{0xd3}, {0x30}}
};

static struct regval_list sensor_contrast_neg1_regs[] = {
	   {{0xfe}, {0x00}},        
	   {{0xd3}, {0x38}}
};

static struct regval_list sensor_contrast_zero_regs[] = {
	   {{0xfe}, {0x00}},    
	   {{0xd3}, {0x40}}
};

static struct regval_list sensor_contrast_pos1_regs[] = {
	   {{0xfe}, {0x00}},        
	   {{0xd3}, {0x48}}
};

static struct regval_list sensor_contrast_pos2_regs[] = {
	   {{0xfe}, {0x00}},    
	   {{0xd3}, {0x50}}
};

static struct regval_list sensor_contrast_pos3_regs[] = {
	   {{0xfe}, {0x00}},    
	   {{0xd3}, {0x58}}
};

static struct regval_list sensor_contrast_pos4_regs[] = {
	   {{0xfe}, {0x00}},    
	   {{0xd3}, {0x5c}}
};

/*
 * The exposure target setttings
 */
static struct regval_list sensor_ev_neg4_regs[] = {
	{{0xfe}, {0x01}},
	{{0x13}, {0x40}}, //AEC_target_Y  
	{{0xfe}, {0x00}},
	{{0xd5}, {0xc0}}// Luma_offset 
};

static struct regval_list sensor_ev_neg3_regs[] = {
	{{0xfe}, {0x01}},
	{{0x13}, {0x48}}, //AEC_target_Y  
	{{0xfe}, {0x00}},
	{{0xd5}, {0xd0}}// Luma_offset 
};

static struct regval_list sensor_ev_neg2_regs[] = {
	{{0xfe}, {0x01}},
	{{0x13}, {0x50}}, //AEC_target_Y  
	{{0xfe}, {0x00}},
	{{0xd5}, {0xe0}}// Luma_offset  
};

static struct regval_list sensor_ev_neg1_regs[] = {
	{{0xfe}, {0x01}},
	{{0x13}, {0x58}}, //AEC_target_Y  
	{{0xfe}, {0x00}},
	{{0xd5}, {0xf0}}// Luma_offset 
};

static struct regval_list sensor_ev_zero_regs[] = {
	{{0xfe}, {0x01}},
	{{0x13}, {0x60}}, //AEC_target_Y  48
	{{0xfe}, {0x00}},
	{{0xd5}, {0x00}}// Luma_offset  c0
};

static struct regval_list sensor_ev_pos1_regs[] = {
	{{0xfe},{0x01}},
	{{0x13},{0x68}}, //AEC_target_Y  
	{{0xfe},{0x00}},
	{{0xd5},{0x10}}// Luma_offset  
};

static struct regval_list sensor_ev_pos2_regs[] = {
	{{0xfe}, {0x01}},
	{{0x13}, {0x70}}, //AEC_target_Y  
	{{0xfe}, {0x00}},
	{{0xd5}, {0x20}}// Luma_offset 
};

static struct regval_list sensor_ev_pos3_regs[] = {
	{{0xfe}, {0x01}},
	{{0x13}, {0x78}}, //AEC_target_Y  
	{{0xfe}, {0x00}},
	{{0xd5}, {0x30}}// Luma_offset 
};

static struct regval_list sensor_ev_pos4_regs[] = {
	{{0xfe}, {0x01}},
	{{0x13}, {0x80}}, //AEC_target_Y  
	{{0xfe}, {0x00}},
	{{0xd5}, {0x40}}// Luma_offset 
};


/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 * 
 */

static struct regval_list sensor_fmt_yuv422_yuyv[] = {
	{{0xfe},{0x00}},
	{{0x44},{0xa2}}	//YCbYCr
};


static struct regval_list sensor_fmt_yuv422_yvyu[] = {
	{{0xfe},{0x00}},
	{{0x44},{0xa3}}	//YCrYCb
};

static struct regval_list sensor_fmt_yuv422_vyuy[] = {
	{{0xfe},{0x00}},
	{{0x44},{0xa1}}	//CrYCbY
};

static struct regval_list sensor_fmt_yuv422_uyvy[] = {
	{{0xfe},{0x00}},
	{{0x44},{0xa0}}	//CbYCrY
};

static struct regval_list sensor_fmt_raw[] = {
	{{0xfe},{0x00}},
	{{0x44},{0xb7}}//raw
};



/*
 * Low-level register I/O.
 *
 */


/*
 * On most platforms, we'd rather do straight i2c I/O.
 */
static int sensor_read(struct v4l2_subdev *sd, unsigned char *reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 data[REG_STEP];
	struct i2c_msg msg;
	int ret,i;
	
	for(i = 0; i < REG_ADDR_STEP; i++)
		data[i] = reg[i];
	
	data[REG_ADDR_STEP] = 0xff;
	/*
	 * Send out the register address...
	 */
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = REG_ADDR_STEP;
	msg.buf = data;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		csi_dev_err("Error %d on register write\n", ret);
		return ret;
	}
	/*
	 * ...then read back the result.
	 */
	
	msg.flags = I2C_M_RD;
	msg.len = REG_DATA_STEP;
	msg.buf = &data[REG_ADDR_STEP];
	
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0) {
		for(i = 0; i < REG_DATA_STEP; i++)
			value[i] = data[i+REG_ADDR_STEP];
		ret = 0;
	}
	else {
		csi_dev_err("Error %d on register read\n", ret);
	}
	return ret;
}


static int sensor_write(struct v4l2_subdev *sd, unsigned char *reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg;
	unsigned char data[REG_STEP];
	int ret,i;
	
	for(i = 0; i < REG_ADDR_STEP; i++)
			data[i] = reg[i];
	for(i = REG_ADDR_STEP; i < REG_STEP; i++)
			data[i] = value[i-REG_ADDR_STEP];
	
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = REG_STEP;
	msg.buf = data;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0) {
		ret = 0;
	}
	else if (ret < 0) {
		csi_dev_err("sensor_write error!\n");
	}
	return ret;
}



/*
 * Write a list of register settings;
 */
static int sensor_write_array(struct v4l2_subdev *sd, struct regval_list *vals , uint size)
{
	int i,ret;
	if (size == 0)
		return -EINVAL;
	
	for(i = 0; i < size ; i++)
	{
		if(vals->reg_num[0] == 0xff && vals->reg_num[1] == 0xff) {
			msleep(vals->value[0]);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				{
					csi_dev_err("sensor_write_err!\n");
					return ret;
				}	
		}
		
		vals++;
	}

	return 0;
}


static int sensor_detect(struct v4l2_subdev *sd)
{
	int ret;
	struct regval_list regs;
	
	regs.reg_num[0] = 0xfe;
	regs.value[0] = 0x00; //PAGE 0x00
	ret = sensor_write(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_detect!\n");
		return ret;
	}
	
	regs.reg_num[0] = 0x00;
	ret = sensor_read(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_read err at sensor_detect!\n");
		return ret;
	}
	
	if(regs.value[0] != 0x20)
		return -ENODEV;
	
	regs.reg_num[0] = 0x01;
	ret = sensor_read(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_read err at sensor_detect!\n");
		return ret;
	}
	
	if(regs.value[0] != 0x05)
		return -ENODEV;
	
	return 0;
}

static int GC2015_init_regs(struct gc2015_device *dev)
{
	int ret;
	csi_dev_dbg("sensor_init\n");
	/*Make sure it is a target sensor*/
	ret = sensor_detect(&dev->sd);
	if (ret) {
		csi_dev_err("chip found is not an target chip.\n");
		return ret;
	}
	
	return sensor_write_array(&dev->sd, sensor_default_regs , ARRAY_SIZE(sensor_default_regs));
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int value)
{
	int ret;
	struct regval_list regs;
	
	regs.reg_num[0] = 0xfe;
	regs.value[0] = 0x00;		//page 0
	ret = sensor_write(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_s_hflip!\n");
		return ret;
	}
	regs.reg_num[0] = 0x29;
	ret = sensor_read(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_read err at sensor_s_hflip!\n");
		return ret;
	}
	
	switch (value) {
		case 0:
		  regs.value[0] &= 0xfe;
			break;
		case 1:
			regs.value[0] |= 0x01;
			break;
		default:
			return -EINVAL;
	}
	ret = sensor_write(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_s_hflip!\n");
		return ret;
	}
	
	msleep(200);

	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int value)
{
	int ret;
	struct regval_list regs;
	
	regs.reg_num[0] = 0xfe;
	regs.value[0] = 0x00;		//page 0
	ret = sensor_write(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_s_vflip!\n");
		return ret;
	}
	
	regs.reg_num[0] = 0x29;
	ret = sensor_read(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_read err at sensor_s_vflip!\n");
		return ret;
	}
	
	switch (value) {
		case 0:
		  regs.value[0] &= 0xfd;
			break;
		case 1:
			regs.value[0] |= 0x02;
			break;
		default:
			return -EINVAL;
	}
	ret = sensor_write(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_s_vflip!\n");
		return ret;
	}
	
	msleep(200);

	return 0;
}

static int sensor_s_autoexp(struct v4l2_subdev *sd,
		enum v4l2_exposure_auto_type value)
{
	int ret;
	struct regval_list regs;
	
	regs.reg_num[0] = 0xfe;
	regs.value[0] = 0x00; //page 0
	
	ret = sensor_write(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_s_autoexp!\n");
		return ret;
	}
	
	regs.reg_num[0] = 0x4f;
	ret = sensor_read(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_read err at sensor_s_autoexp!\n");
		return ret;
	}

	switch (value) {
		case V4L2_EXPOSURE_AUTO:
		  regs.value[0] |= 0x01;
			break;
		case V4L2_EXPOSURE_MANUAL:
			regs.value[0] &= 0xfe;
			break;
		case V4L2_EXPOSURE_SHUTTER_PRIORITY:
			return -EINVAL;    
		case V4L2_EXPOSURE_APERTURE_PRIORITY:
			return -EINVAL;
		default:
			return -EINVAL;
	}
		
	ret = sensor_write(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_s_autoexp!\n");
		return ret;
	}
	
	msleep(10);
	
	return 0;
}

static int sensor_s_autowb(struct v4l2_subdev *sd, int value)
{
	int ret;
	struct regval_list regs;
	
	ret = sensor_write_array(sd, sensor_wb_auto_regs, ARRAY_SIZE(sensor_wb_auto_regs));
	if (ret < 0) {
		csi_dev_err("sensor_write_array err at sensor_s_autowb!\n");
		return ret;
	}
	
	regs.reg_num[0] = 0x42;
	ret = sensor_read(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_read err at sensor_s_autowb!\n");
		return ret;
	}

	switch(value) {
	case 0:
		regs.value[0] &= 0xfd;
		break;
	case 1:
		regs.value[0] |= 0x02;
		break;
	default:
		break;
	}	
	ret = sensor_write(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_s_autowb!\n");
		return ret;
	}
	
	msleep(100);

	return 0;
}

static int sensor_s_contrast(struct v4l2_subdev *sd, int value)
{
	int ret;

	switch (value) {
		case -4:
		  ret = sensor_write_array(sd, sensor_contrast_neg4_regs, ARRAY_SIZE(sensor_contrast_neg4_regs));
			break;
		case -3:
			ret = sensor_write_array(sd, sensor_contrast_neg3_regs, ARRAY_SIZE(sensor_contrast_neg3_regs));
			break;
		case -2:
			ret = sensor_write_array(sd, sensor_contrast_neg2_regs, ARRAY_SIZE(sensor_contrast_neg2_regs));
			break;   
		case -1:
			ret = sensor_write_array(sd, sensor_contrast_neg1_regs, ARRAY_SIZE(sensor_contrast_neg1_regs));
			break;
		case 0:   
			ret = sensor_write_array(sd, sensor_contrast_zero_regs, ARRAY_SIZE(sensor_contrast_zero_regs));
			break;
		case 1:
			ret = sensor_write_array(sd, sensor_contrast_pos1_regs, ARRAY_SIZE(sensor_contrast_pos1_regs));
			break;
		case 2:
			ret = sensor_write_array(sd, sensor_contrast_pos2_regs, ARRAY_SIZE(sensor_contrast_pos2_regs));
			break;	
		case 3:
			ret = sensor_write_array(sd, sensor_contrast_pos3_regs, ARRAY_SIZE(sensor_contrast_pos3_regs));
			break;
		case 4:
			ret = sensor_write_array(sd, sensor_contrast_pos4_regs, ARRAY_SIZE(sensor_contrast_pos4_regs));
			break;
		default:
			return -EINVAL;
	}
	
	if (ret < 0) {
		csi_dev_err("sensor_write_array err at sensor_s_contrast!\n");
		return ret;
	}
	
	msleep(10);

	return 0;
}

static int sensor_s_exp(struct v4l2_subdev *sd, int value)
{
	int ret;

	switch (value) {
		case -4:
		  ret = sensor_write_array(sd, sensor_ev_neg4_regs, ARRAY_SIZE(sensor_ev_neg4_regs));
			break;
		case -3:
			ret = sensor_write_array(sd, sensor_ev_neg3_regs, ARRAY_SIZE(sensor_ev_neg3_regs));
			break;
		case -2:
			ret = sensor_write_array(sd, sensor_ev_neg2_regs, ARRAY_SIZE(sensor_ev_neg2_regs));
			break;   
		case -1:
			ret = sensor_write_array(sd, sensor_ev_neg1_regs, ARRAY_SIZE(sensor_ev_neg1_regs));
			break;
		case 0:   
			ret = sensor_write_array(sd, sensor_ev_zero_regs, ARRAY_SIZE(sensor_ev_zero_regs));
			break;
		case 1:
			ret = sensor_write_array(sd, sensor_ev_pos1_regs, ARRAY_SIZE(sensor_ev_pos1_regs));
			break;
		case 2:
			ret = sensor_write_array(sd, sensor_ev_pos2_regs, ARRAY_SIZE(sensor_ev_pos2_regs));
			break;	
		case 3:
			ret = sensor_write_array(sd, sensor_ev_pos3_regs, ARRAY_SIZE(sensor_ev_pos3_regs));
			break;
		case 4:
			ret = sensor_write_array(sd, sensor_ev_pos4_regs, ARRAY_SIZE(sensor_ev_pos4_regs));
			break;
		default:
			return -EINVAL;
	}
	
	if (ret < 0) {
		csi_dev_err("sensor_write_array err at sensor_s_exp!\n");
		return ret;
	}
	
	msleep(10);

	return 0;
}

static int sensor_s_wb(struct v4l2_subdev *sd,
		enum v4l2_whiteblance value)
{
	int ret;

	if (value == V4L2_WB_AUTO) {
		ret = sensor_s_autowb(sd, 1);
		return ret;
	} 
	else {
		ret = sensor_s_autowb(sd, 0);
		if(ret < 0) {
			csi_dev_err("sensor_s_autowb error, return %x!\n",ret);
			return ret;
		}
		
		switch (value) {
			case V4L2_WB_CLOUD:
			  ret = sensor_write_array(sd, sensor_wb_cloud_regs, ARRAY_SIZE(sensor_wb_cloud_regs));
				break;
			case V4L2_WB_DAYLIGHT:
				ret = sensor_write_array(sd, sensor_wb_daylight_regs, ARRAY_SIZE(sensor_wb_daylight_regs));
				break;
			case V4L2_WB_INCANDESCENCE:
				ret = sensor_write_array(sd, sensor_wb_incandescence_regs, ARRAY_SIZE(sensor_wb_incandescence_regs));
				break;    
			case V4L2_WB_FLUORESCENT:
				ret = sensor_write_array(sd, sensor_wb_fluorescent_regs, ARRAY_SIZE(sensor_wb_fluorescent_regs));
				break;
			case V4L2_WB_TUNGSTEN:   
				ret = sensor_write_array(sd, sensor_wb_tungsten_regs, ARRAY_SIZE(sensor_wb_tungsten_regs));
				break;
			default:
				return -EINVAL;
		} 
	}
	
	if (ret < 0) {
		csi_dev_err("sensor_s_wb error, return %x!\n",ret);
		return ret;
	}
	
	msleep(10);

	return 0;
}

static int sensor_s_colorfx(struct v4l2_subdev *sd,
		enum v4l2_colorfx value)
{
	int ret;

	switch (value) {
	case V4L2_COLORFX_NONE:
	  ret = sensor_write_array(sd, sensor_colorfx_none_regs, ARRAY_SIZE(sensor_colorfx_none_regs));
		break;
	case V4L2_COLORFX_BW:
		ret = sensor_write_array(sd, sensor_colorfx_bw_regs, ARRAY_SIZE(sensor_colorfx_bw_regs));
		break;  
	case V4L2_COLORFX_SEPIA:
		ret = sensor_write_array(sd, sensor_colorfx_sepia_regs, ARRAY_SIZE(sensor_colorfx_sepia_regs));
		break;   
	case V4L2_COLORFX_NEGATIVE:
		ret = sensor_write_array(sd, sensor_colorfx_negative_regs, ARRAY_SIZE(sensor_colorfx_negative_regs));
		break;
	case V4L2_COLORFX_EMBOSS:   
		ret = sensor_write_array(sd, sensor_colorfx_emboss_regs, ARRAY_SIZE(sensor_colorfx_emboss_regs));
		break;
	case V4L2_COLORFX_SKETCH:     
		ret = sensor_write_array(sd, sensor_colorfx_sketch_regs, ARRAY_SIZE(sensor_colorfx_sketch_regs));
		break;
	case V4L2_COLORFX_SKY_BLUE:
		ret = sensor_write_array(sd, sensor_colorfx_sky_blue_regs, ARRAY_SIZE(sensor_colorfx_sky_blue_regs));
		break;
	case V4L2_COLORFX_GRASS_GREEN:
		ret = sensor_write_array(sd, sensor_colorfx_grass_green_regs, ARRAY_SIZE(sensor_colorfx_grass_green_regs));
		break;
	case V4L2_COLORFX_SKIN_WHITEN:
		ret = sensor_write_array(sd, sensor_colorfx_skin_whiten_regs, ARRAY_SIZE(sensor_colorfx_skin_whiten_regs));
		break;
	case V4L2_COLORFX_VIVID:
		ret = sensor_write_array(sd, sensor_colorfx_vivid_regs, ARRAY_SIZE(sensor_colorfx_vivid_regs));
		break;
	default:
		return -EINVAL;
	}
	
	if (ret < 0) {
		csi_dev_err("sensor_s_colorfx error, return %x!\n",ret);
		return ret;
	}
	
	msleep(10);

	return 0;
}

static int gc2015_setting(struct gc2015_device *dev,int PROP_ID,int value )
{
	int ret=0;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch(PROP_ID)  {
	case V4L2_CID_DO_WHITE_BALANCE:
		if(gc2015_qctrl[0].default_value!=value){
			gc2015_qctrl[0].default_value=value;
			sensor_s_wb(&dev->sd, (enum v4l2_whiteblance) value);	
			printk(KERN_INFO " set camera  white_balance=%d. \n ",value);
		}
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		if(gc2015_qctrl[1].default_value!=value){
			gc2015_qctrl[1].default_value=value;
			sensor_s_autowb(&dev->sd, value);	
			printk(KERN_INFO " set camera  white_balance=%d. \n ",value);
		}
		break;
	case V4L2_CID_EXPOSURE:
		if(gc2015_qctrl[2].default_value!=value){
			gc2015_qctrl[2].default_value=value;
			sensor_s_exp(&dev->sd, value);
			printk(KERN_INFO " set camera  exposure=%d. \n ",value);
		}
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		if(gc2015_qctrl[3].default_value!=value){
			gc2015_qctrl[3].default_value=value;
			sensor_s_autoexp(&dev->sd, value);
			printk(KERN_INFO " set camera  exposure=%d. \n ",value);
		}
		break;
	case V4L2_CID_CONTRAST:
	    if(gc2015_qctrl[4].default_value!=value){
			gc2015_qctrl[4].default_value=value;
			sensor_s_contrast(&dev->sd, value);
			printk(KERN_INFO " set camera  exposure=%d. \n ",value);
		}
		break;
	case V4L2_CID_COLORFX:
		if(gc2015_qctrl[5].default_value!=value){
			gc2015_qctrl[5].default_value=value;
			sensor_s_colorfx(&dev->sd, (enum v4l2_colorfx) value);
			printk(KERN_INFO " set camera  effect=%d. \n ",value);
		}
		break;
	case V4L2_CID_VFLIP:
		if(gc2015_qctrl[6].default_value!=value){
			gc2015_qctrl[6].default_value=value;
			sensor_s_vflip(&dev->sd, value);
			printk(KERN_INFO " set camera  effect=%d. \n ",value);
		}
		break;
	case V4L2_CID_HFLIP:
	    if(gc2015_qctrl[7].default_value!=value){
			gc2015_qctrl[7].default_value=value;
			sensor_s_hflip(&dev->sd, value);
			printk(KERN_INFO " set camera  effect=%d. \n ",value);
		}
		break;
	default:
		ret=-1;
		break;
	}
	return ret;

}

static void power_down_gc2015(struct gc2015_device *dev)
{

}

/* ------------------------------------------------------------------
	DMA and thread functions
   ------------------------------------------------------------------*/

#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void gc2015_fillbuff(struct gc2015_fh *fh, struct gc2015_buffer *buf)
{
	struct gc2015_device *dev = fh->dev;
	void *vbuf = videobuf_to_vmalloc(&buf->vb);
	dprintk(dev,1,"%s\n", __func__);
	if (!vbuf)
		return;
 /*  0x18221223 indicate the memory type is MAGIC_VMAL_MEM*/
    vm_fill_buffer(&buf->vb,fh->fmt->fourcc ,0x18221223,vbuf);
	buf->vb.state = VIDEOBUF_DONE;
}

static void gc2015_thread_tick(struct gc2015_fh *fh)
{
	struct gc2015_buffer *buf;
	struct gc2015_device *dev = fh->dev;
	struct gc2015_dmaqueue *dma_q = &dev->vidq;

	unsigned long flags = 0;

	dprintk(dev, 1, "Thread tick\n");

	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
		dprintk(dev, 1, "No active queue to serve\n");
		goto unlock;
	}

	buf = list_entry(dma_q->active.next,
			 struct gc2015_buffer, vb.queue);
    dprintk(dev, 1, "%s\n", __func__);
    dprintk(dev, 1, "list entry get buf is %x\n",(unsigned)buf);

	/* Nobody is waiting on this buffer, return */
	if (!waitqueue_active(&buf->vb.done))
		goto unlock;

	list_del(&buf->vb.queue);

	do_gettimeofday(&buf->vb.ts);

	/* Fill buffer */
	spin_unlock_irqrestore(&dev->slock, flags);
	gc2015_fillbuff(fh, buf);
	dprintk(dev, 1, "filled buffer %p\n", buf);

	wake_up(&buf->vb.done);
	dprintk(dev, 2, "[%p/%d] wakeup\n", buf, buf->vb. i);
	return;
unlock:
	spin_unlock_irqrestore(&dev->slock, flags);
	return;
}

#define frames_to_ms(frames)					\
	((frames * WAKE_NUMERATOR * 1000) / WAKE_DENOMINATOR)

static void gc2015_sleep(struct gc2015_fh *fh)
{
	struct gc2015_device *dev = fh->dev;
	struct gc2015_dmaqueue *dma_q = &dev->vidq;

	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
		(unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
		goto stop_task;

	/* Calculate time to wake up */
	//timeout = msecs_to_jiffies(frames_to_ms(1));

	gc2015_thread_tick(fh);

	schedule_timeout_interruptible(2);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int gc2015_thread(void *data)
{
	struct gc2015_fh  *fh = data;
	struct gc2015_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (;;) {
		gc2015_sleep(fh);

		if (kthread_should_stop())
			break;
	}
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int gc2015_start_thread(struct gc2015_fh *fh)
{
	struct gc2015_device *dev = fh->dev;
	struct gc2015_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(gc2015_thread, fh, "gc2015");

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}
	/* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void gc2015_stop_thread(struct gc2015_dmaqueue  *dma_q)
{
	struct gc2015_device *dev = container_of(dma_q, struct gc2015_device, vidq);

	dprintk(dev, 1, "%s\n", __func__);
	/* shutdown control thread */
	if (dma_q->kthread) {
		kthread_stop(dma_q->kthread);
		dma_q->kthread = NULL;
	}
}

/* ------------------------------------------------------------------
	Videobuf operations
   ------------------------------------------------------------------*/
static int
buffer_setup(struct videobuf_queue *vq, unsigned int *count, unsigned int *size)
{
	struct gc2015_fh  *fh = vq->priv_data;
	struct gc2015_device *dev  = fh->dev;
    //int bytes = fh->fmt->depth >> 3 ;
	*size = fh->width*fh->height*fh->fmt->depth >> 3;
	if (0 == *count)
		*count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
		(*count)--;

	dprintk(dev, 1, "%s, count=%d, size=%d\n", __func__,
		*count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq, struct gc2015_buffer *buf)
{
	struct gc2015_fh  *fh = vq->priv_data;
	struct gc2015_device *dev  = fh->dev;

	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);

	if (in_interrupt())
		BUG();

	videobuf_vmalloc_free(&buf->vb);
	dprintk(dev, 1, "free_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 1024
#define norm_maxh() 768
static int
buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
						enum v4l2_field field)
{
	struct gc2015_fh     *fh  = vq->priv_data;
	struct gc2015_device    *dev = fh->dev;
	struct gc2015_buffer *buf = container_of(vb, struct gc2015_buffer, vb);
	int rc;
    //int bytes = fh->fmt->depth >> 3 ;
	dprintk(dev, 1, "%s, field=%d\n", __func__, field);

	BUG_ON(NULL == fh->fmt);

	if (fh->width  < 48 || fh->width  > norm_maxw() ||
	    fh->height < 32 || fh->height > norm_maxh())
		return -EINVAL;

	buf->vb.size = fh->width*fh->height*fh->fmt->depth >> 3;
	if (0 != buf->vb.baddr  &&  buf->vb.bsize < buf->vb.size)
		return -EINVAL;

	/* These properties only change when queue is idle, see s_fmt */
	buf->fmt       = fh->fmt;
	buf->vb.width  = fh->width;
	buf->vb.height = fh->height;
	buf->vb.field  = field;

	//precalculate_bars(fh);

	if (VIDEOBUF_NEEDS_INIT == buf->vb.state) {
		rc = videobuf_iolock(vq, &buf->vb, NULL);
		if (rc < 0)
			goto fail;
	}

	buf->vb.state = VIDEOBUF_PREPARED;

	return 0;

fail:
	free_buffer(vq, buf);
	return rc;
}

static void
buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct gc2015_buffer    *buf  = container_of(vb, struct gc2015_buffer, vb);
	struct gc2015_fh        *fh   = vq->priv_data;
	struct gc2015_device       *dev  = fh->dev;
	struct gc2015_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct gc2015_buffer   *buf  = container_of(vb, struct gc2015_buffer, vb);
	struct gc2015_fh       *fh   = vq->priv_data;
	struct gc2015_device      *dev  = (struct gc2015_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops gc2015_video_qops = {
	.buf_setup      = buffer_setup,
	.buf_prepare    = buffer_prepare,
	.buf_queue      = buffer_queue,
	.buf_release    = buffer_release,
};

/* ------------------------------------------------------------------
	IOCTL vidioc handling
   ------------------------------------------------------------------*/
static int vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *cap)
{
	struct gc2015_fh  *fh  = priv;
	struct gc2015_device *dev = fh->dev;

	strcpy(cap->driver, "gc2015");
	strcpy(cap->card, "gc2015");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = GC2015_CAMERA_VERSION;
	cap->capabilities =	V4L2_CAP_VIDEO_CAPTURE |
				V4L2_CAP_STREAMING     |
				V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	struct gc2015_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct gc2015_fh *fh = priv;

	f->fmt.pix.width        = fh->width;
	f->fmt.pix.height       = fh->height;
	f->fmt.pix.field        = fh->vb_vidq.field;
	f->fmt.pix.pixelformat  = fh->fmt->fourcc;
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fh->fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	return (0);
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_format *f)
{
	struct gc2015_fh  *fh  = priv;
	struct gc2015_device *dev = fh->dev;
	struct gc2015_fmt *fmt;
	enum v4l2_field field;
	unsigned int maxw, maxh;

	fmt = get_format(f);
	if (!fmt) {
		dprintk(dev, 1, "Fourcc format (0x%08x) invalid.\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	field = f->fmt.pix.field;

	if (field == V4L2_FIELD_ANY) {
		field = V4L2_FIELD_INTERLACED;
	} else if (V4L2_FIELD_INTERLACED != field) {
		dprintk(dev, 1, "Field type invalid.\n");
		return -EINVAL;
	}

	maxw  = norm_maxw();
	maxh  = norm_maxh();

	f->fmt.pix.field = field;
	v4l_bound_align_image(&f->fmt.pix.width, 48, maxw, 2,
			      &f->fmt.pix.height, 32, maxh, 0, 0);
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	return 0;
}

/*FIXME: This seems to be generic enough to be at videodev2 */
static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct gc2015_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;

	int ret = vidioc_try_fmt_vid_cap(file, fh, f);
	if (ret < 0)
		return ret;

	mutex_lock(&q->vb_lock);

	if (videobuf_queue_is_busy(&fh->vb_vidq)) {
		dprintk(fh->dev, 1, "%s queue busy\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	fh->fmt           = get_format(f);
	fh->width         = f->fmt.pix.width;
	fh->height        = f->fmt.pix.height;
	fh->vb_vidq.field = f->fmt.pix.field;
	fh->type          = f->type;

	ret = 0;
out:
	mutex_unlock(&q->vb_lock);

	return ret;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct gc2015_fh  *fh = priv;

	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc2015_fh  *fh = priv;

	return (videobuf_querybuf(&fh->vb_vidq, p));
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc2015_fh *fh = priv;

	return (videobuf_qbuf(&fh->vb_vidq, p));
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc2015_fh  *fh = priv;

	return (videobuf_dqbuf(&fh->vb_vidq, p,
				file->f_flags & O_NONBLOCK));
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct gc2015_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct gc2015_fh  *fh = priv;
    tvin_parm_t para;
    int ret = 0 ;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;

    para.port  = TVIN_PORT_CAMERA;
    para.fmt_info.fmt = TVIN_SIG_FMT_MAX+1;//TVIN_SIG_FMT_MAX+1;TVIN_SIG_FMT_CAMERA_1280X720P_30Hz
	para.fmt_info.frame_rate = 150;
	para.fmt_info.h_active = 640;
	para.fmt_info.v_active = 480;
	para.fmt_info.hsync_phase = 0;
	para.fmt_info.vsync_phase  = 1;	
	ret =  videobuf_streamon(&fh->vb_vidq);
	if(ret == 0){
    start_tvin_service(0,&para);
	    fh->stream_on        = 1;
	}
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct gc2015_fh  *fh = priv;

    int ret = 0 ;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;
	ret = videobuf_streamoff(&fh->vb_vidq);
	if(ret == 0 ){
    stop_tvin_service(0);
	    fh->stream_on        = 0;
	}
	return ret;
}

static int vidioc_enum_framesizes(struct file *file, void *fh,struct v4l2_frmsizeenum *fsize)
{
	int ret = 0,i=0;
	struct gc2015_fmt *fmt = NULL;
	struct v4l2_frmsize_discrete *frmsize = NULL;
	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		if (formats[i].fourcc == fsize->pixel_format){
			fmt = &formats[i];
			break;
		}
	}
	if (fmt == NULL)
		return -EINVAL;
	if (fmt->fourcc == V4L2_PIX_FMT_NV21){
		if (fsize->index >= ARRAY_SIZE(gc2015_prev_resolution))
			return -EINVAL;
		frmsize = &gc2015_prev_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	else if(fmt->fourcc == V4L2_PIX_FMT_RGB24){
		if (fsize->index >= ARRAY_SIZE(gc2015_pic_resolution))
			return -EINVAL;
		frmsize = &gc2015_pic_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	return ret;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id *i)
{
	return 0;
}

/* only one input in this sample driver */
static int vidioc_enum_input(struct file *file, void *priv,
				struct v4l2_input *inp)
{
	//if (inp->index >= NUM_INPUTS)
		//return -EINVAL;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	inp->std = V4L2_STD_525_60;
	sprintf(inp->name, "Camera %u", inp->index);

	return (0);
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct gc2015_fh *fh = priv;
	struct gc2015_device *dev = fh->dev;

	*i = dev->input;

	return (0);
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct gc2015_fh *fh = priv;
	struct gc2015_device *dev = fh->dev;

	//if (i >= NUM_INPUTS)
		//return -EINVAL;

	dev->input = i;
	//precalculate_bars(fh);

	return (0);
}

	/* --- controls ---------------------------------------------- */
static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gc2015_qctrl); i++)
		if (qc->id && qc->id == gc2015_qctrl[i].id) {
			memcpy(qc, &(gc2015_qctrl[i]),
				sizeof(*qc));
			return (0);
		}

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct gc2015_fh *fh = priv;
	struct gc2015_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(gc2015_qctrl); i++)
		if (ctrl->id == gc2015_qctrl[i].id) {
			ctrl->value = dev->qctl_regs[i];
			return 0;
		}

	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct gc2015_fh *fh = priv;
	struct gc2015_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(gc2015_qctrl); i++)
		if (ctrl->id == gc2015_qctrl[i].id) {
			if (ctrl->value < gc2015_qctrl[i].minimum ||
			    ctrl->value > gc2015_qctrl[i].maximum ||
			    gc2015_setting(dev,ctrl->id,ctrl->value)<0) {
				return -ERANGE;
			}
			dev->qctl_regs[i] = ctrl->value;
			return 0;
		}
	return -EINVAL;
}

/* ------------------------------------------------------------------
	File operations for the device
   ------------------------------------------------------------------*/

static int gc2015_open(struct file *file)
{
	struct gc2015_device *dev = video_drvdata(file);
	struct gc2015_fh *fh = NULL;
	int retval = 0;
	gc2015_have_open=1;
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 1);
#endif	
	if(dev->platform_dev_data.device_init) {
		dev->platform_dev_data.device_init();
		printk("+++found a init function, and run it..\n");
	}
	GC2015_init_regs(dev);
	msleep(100);//40
	mutex_lock(&dev->mutex);
	dev->users++;
	if (dev->users > 1) {
		dev->users--;
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}

	dprintk(dev, 1, "open %s type=%s users=%d\n",
		video_device_node_name(dev->vdev),
		v4l2_type_names[V4L2_BUF_TYPE_VIDEO_CAPTURE], dev->users);

    	/* init video dma queues */
	INIT_LIST_HEAD(&dev->vidq.active);
	init_waitqueue_head(&dev->vidq.wq);
    spin_lock_init(&dev->slock);
	/* allocate + initialize per filehandle data */
	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (NULL == fh) {
		dev->users--;
		retval = -ENOMEM;
	}
	mutex_unlock(&dev->mutex);

	if (retval)
		return retval;

	file->private_data = fh;
	fh->dev      = dev;

	fh->type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fh->fmt      = &formats[0];
	fh->width    = 640;
	fh->height   = 480;
	fh->stream_on = 0 ;
	/* Resets frame counters */
	dev->jiffies = jiffies;

//    TVIN_SIG_FMT_CAMERA_640X480P_30Hz,
//    TVIN_SIG_FMT_CAMERA_800X600P_30Hz,
//    TVIN_SIG_FMT_CAMERA_1024X768P_30Hz, // 190
//    TVIN_SIG_FMT_CAMERA_1920X1080P_30Hz,
//    TVIN_SIG_FMT_CAMERA_1280X720P_30Hz,

	videobuf_queue_vmalloc_init(&fh->vb_vidq, &gc2015_video_qops,
			NULL, &dev->slock, fh->type, V4L2_FIELD_INTERLACED,
			sizeof(struct gc2015_buffer), fh,NULL);

	gc2015_start_thread(fh);

	return 0;
}

static ssize_t
gc2015_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct gc2015_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
					file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int
gc2015_poll(struct file *file, struct poll_table_struct *wait)
{
	struct gc2015_fh        *fh = file->private_data;
	struct gc2015_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int gc2015_close(struct file *file)
{
	struct gc2015_fh         *fh = file->private_data;
	struct gc2015_device *dev       = fh->dev;
	struct gc2015_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);
	gc2015_have_open=0;

	gc2015_stop_thread(vidq);
	videobuf_stop(&fh->vb_vidq);
	if(fh->stream_on){
	    stop_tvin_service(0);
	}
	videobuf_mmap_free(&fh->vb_vidq);

	kfree(fh);

	mutex_lock(&dev->mutex);
	dev->users--;
	mutex_unlock(&dev->mutex);

	dprintk(dev, 1, "close called (dev=%s, users=%d)\n",
		video_device_node_name(vdev), dev->users);
#if 1
	gc2015_qctrl[0].default_value=0;
	gc2015_qctrl[1].default_value=4;
	gc2015_qctrl[2].default_value=0;
	gc2015_qctrl[3].default_value=0;
	gc2015_qctrl[4].default_value=0;

	//power_down_gc2015(dev);
#endif
	if(dev->platform_dev_data.device_uninit) {
		dev->platform_dev_data.device_uninit();
		printk("+++found a uninit function, and run it..\n");
	}
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 0);
#endif	
	return 0;
}

static int gc2015_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct gc2015_fh  *fh = file->private_data;
	struct gc2015_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations gc2015_fops = {
	.owner		= THIS_MODULE,
	.open           = gc2015_open,
	.release        = gc2015_close,
	.read           = gc2015_read,
	.poll		= gc2015_poll,
	.ioctl          = video_ioctl2, /* V4L2 ioctl handler */
	.mmap           = gc2015_mmap,
};

static const struct v4l2_ioctl_ops gc2015_ioctl_ops = {
	.vidioc_querycap      = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap  = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap     = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap   = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs       = vidioc_reqbufs,
	.vidioc_querybuf      = vidioc_querybuf,
	.vidioc_qbuf          = vidioc_qbuf,
	.vidioc_dqbuf         = vidioc_dqbuf,
	.vidioc_s_std         = vidioc_s_std,
	.vidioc_enum_input    = vidioc_enum_input,
	.vidioc_g_input       = vidioc_g_input,
	.vidioc_s_input       = vidioc_s_input,
	.vidioc_queryctrl     = vidioc_queryctrl,
	.vidioc_g_ctrl        = vidioc_g_ctrl,
	.vidioc_s_ctrl        = vidioc_s_ctrl,
	.vidioc_streamon      = vidioc_streamon,
	.vidioc_streamoff     = vidioc_streamoff,
	.vidioc_enum_framesizes = vidioc_enum_framesizes,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf          = vidiocgmbuf,
#endif
};

static struct video_device gc2015_template = {
	.name		= "gc2015_v4l",
	.fops           = &gc2015_fops,
	.ioctl_ops 	= &gc2015_ioctl_ops,
	.release	= video_device_release,

	.tvnorms              = V4L2_STD_525_60,
	.current_norm         = V4L2_STD_NTSC_M,
};

static int gc2015_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SENSOR, 0);
}

static const struct v4l2_subdev_core_ops gc2015_core_ops = {
	.g_chip_ident = gc2015_g_chip_ident,
};

static const struct v4l2_subdev_ops gc2015_ops = {
	.core = &gc2015_core_ops,
};
static struct i2c_client *this_client;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void aml_gc2015_early_suspend(struct early_suspend *h)
{
	printk("enter -----> %s \n",__FUNCTION__);
	if(h && h->param && gc2015_have_open) {
		aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)h->param;
		if (plat_dat && plat_dat->early_suspend) {
			plat_dat->early_suspend();
		}
	}
}

static void aml_gc2015_late_resume(struct early_suspend *h)
{
	aml_plat_cam_data_t* plat_dat;
	if(gc2015_have_open){
	    printk("enter -----> %s \n",__FUNCTION__);
	    if(h && h->param) {
		    plat_dat= (aml_plat_cam_data_t*)h->param;
		    if (plat_dat && plat_dat->late_resume) {
			    plat_dat->late_resume();
		    }
	    }
	}
}
#endif

static int gc2015_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	aml_plat_cam_data_t* plat_dat;
	int err;
	struct gc2015_device *t;
	struct v4l2_subdev *sd;
	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return -ENOMEM;
	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &gc2015_ops);
	plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
	
	/* test if devices exist. */
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_PROBE
	unsigned char buf[4]; 
	buf[0]=0;
	plat_dat->device_init();
	err=i2c_get_byte_add8(client,buf);
	plat_dat->device_uninit();
	if(err<0) return  -ENODEV;
#endif
	/* Now create a video4linux device */
	mutex_init(&t->mutex);

	/* Now create a video4linux device */
	t->vdev = video_device_alloc();
	if (t->vdev == NULL) {
		kfree(t);
		kfree(client);
		return -ENOMEM;
	}
	memcpy(t->vdev, &gc2015_template, sizeof(*t->vdev));

	video_set_drvdata(t->vdev, t);

	this_client=client;
	/* Register it */
	if (plat_dat) {
		t->platform_dev_data.device_init=plat_dat->device_init;
		t->platform_dev_data.device_uninit=plat_dat->device_uninit;
		if(plat_dat->video_nr>=0)  video_nr=plat_dat->video_nr;
			if(t->platform_dev_data.device_init) {
			t->platform_dev_data.device_init();
			printk("+++found a init function, and run it..\n");
		    }
			//power_down_gc2015(t);
			if(t->platform_dev_data.device_uninit) {
			t->platform_dev_data.device_uninit();
			printk("+++found a uninit function, and run it..\n");
		    }
	}
	err = video_register_device(t->vdev, VFL_TYPE_GRABBER, video_nr);
	if (err < 0) {
		video_device_release(t->vdev);
		kfree(t);
		return err;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
    gc2015_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	gc2015_early_suspend.suspend = aml_gc2015_early_suspend;
	gc2015_early_suspend.resume = aml_gc2015_late_resume;
	gc2015_early_suspend.param = plat_dat;
	register_early_suspend(&gc2015_early_suspend);
#endif

	return 0;
}

static int gc2015_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2015_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
	kfree(t);
	return 0;
}
static int gc2015_suspend(struct i2c_client *client, pm_message_t state)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2015_device *t = to_dev(sd);
	struct gc2015_fh  *fh = to_fh(t);
	if (gc2015_have_open) {
	    if(fh->stream_on == 1){
		    stop_tvin_service(0);
	    }
	    power_down_gc2015(t);
	}
	return 0;
}

static int gc2015_resume(struct i2c_client *client)
{
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct gc2015_device *t = to_dev(sd);
    struct gc2015_fh  *fh = to_fh(t);
    tvin_parm_t para;
    if (gc2015_have_open) {
        para.port  = TVIN_PORT_CAMERA;
        para.fmt_info.fmt = TVIN_SIG_FMT_MAX+1;
        GC2015_init_regs(t);
        if(fh->stream_on == 1){
            start_tvin_service(0,&para);
	    }
    }
    return 0;
}


static const struct i2c_device_id gc2015_id[] = {
	{ "gc2015_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2015_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "gc2015",
	.probe = gc2015_probe,
	.remove = gc2015_remove,
	.suspend = gc2015_suspend,
	.resume = gc2015_resume,
	.id_table = gc2015_id,
};

