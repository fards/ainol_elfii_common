/*
 *gc0308 - This code emulates a real video device with v4l2 api
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
static struct early_suspend gc0308_early_suspend;
#endif

#define GC0308_CAMERA_MODULE_NAME "gc0308"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */

#define GC0307_CAMERA_MAJOR_VERSION 0
#define GC0307_CAMERA_MINOR_VERSION 7
#define GC0307_CAMERA_RELEASE 0
#define GC0307_CAMERA_VERSION \
	KERNEL_VERSION(GC0307_CAMERA_MAJOR_VERSION, GC0307_CAMERA_MINOR_VERSION, GC0307_CAMERA_RELEASE)

MODULE_DESCRIPTION("gc0307 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

static unsigned video_nr = -1;  /* videoX start number, -1 is autodetect. */

static unsigned debug;

static unsigned int vid_limit = 16;

//for internel driver debug
#define DEV_DBG_EN   		0 
#if(DEV_DBG_EN == 1)		
#define csi_dev_dbg(x,arg...) printk(KERN_INFO"[CSI_DEBUG][GC0307]"x,##arg)
#else
#define csi_dev_dbg(x,arg...) 
#endif
#define csi_dev_err(x,arg...) printk(KERN_INFO"[CSI_ERR][GC0307]"x,##arg)
#define csi_dev_print(x,arg...) printk(KERN_INFO"[CSI][GC0307]"x,##arg)

#define MCLK (12*1000*1000)
#define VREF_POL	CSI_HIGH
#define HREF_POL	CSI_HIGH
#define CLK_POL		CSI_RISING
#define IO_CFG		0						//0 for csi0

//define the voltage level of control signal
#define CSI_STBY_ON			1
#define CSI_STBY_OFF 		0
#define CSI_RST_ON			0
#define CSI_RST_OFF			1
#define CSI_PWR_ON			1
#define CSI_PWR_OFF			0


#define V4L2_IDENT_SENSOR 0x0307

#define REG_TERM 0xff
#define VAL_TERM 0xff


#define REG_ADDR_STEP 1
#define REG_DATA_STEP 1
#define REG_STEP 			(REG_ADDR_STEP+REG_DATA_STEP)

/*
 * The gc0307 sits on i2c with ID 0x42
 */
#define I2C_ADDR 0x42

static int gc0307_have_open=0;

static struct v4l2_queryctrl gc0307_qctrl[] = {
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
		.id            = V4L2_CID_EXPOSURE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "exposure",
		.minimum       = 0,
		.maximum       = 8,
		.step          = 0x1,
		.default_value = 4,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_COLORFX,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "effect",
		.minimum       = 0,
		.maximum       = 6,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
	    .id            = V4L2_CID_BRIGHTNESS,
	    .type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "brightness",
		.minimum       = -4,
		.maximum       = 4,
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
	    .id            = V4L2_CID_HFLIP,
	    .type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "hflip",
		.minimum       = -4,
		.maximum       = 4,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
	    .id            = V4L2_CID_VFLIP,
	    .type          = V4L2_CTRL_TYPE_BOOLEAN,
		.name          = "vflip",
		.minimum       = -4,
		.maximum       = 4,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}
};

#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

/* ------------------------------------------------------------------
	Basic structures
   ------------------------------------------------------------------*/

struct gc0307_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct gc0307_fmt formats[] = {
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

static struct gc0307_fmt *get_format(struct v4l2_format *f)
{
	struct gc0307_fmt *fmt;
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
struct gc0307_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct gc0307_fmt        *fmt;
};

struct gc0307_dmaqueue {
	struct list_head       active;

	/* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
	/* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

static LIST_HEAD(gc0307_devicelist);

struct gc0307_device {
	struct list_head gc0307_devicelist;
	struct v4l2_subdev sd;
	struct v4l2_device v4l2_dev;

	spinlock_t slock;
	struct mutex mutex;

	int users;

	/* various device info */
	struct video_device *vdev;

	struct gc0307_dmaqueue vidq;

	/* Several counters */
	unsigned long jiffies;

	/* Input Number */
	int input;

	/* platform device data from board initting. */
	aml_plat_cam_data_t platform_dev_data;

	/* Control 'registers' */
	int qctl_regs[ARRAY_SIZE(gc0307_qctrl)];
};


static inline struct gc0307_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc0307_device, sd);
}

struct gc0307_fh {
	struct gc0307_device            *dev;

	/* video capture */
	struct gc0307_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	enum v4l2_buf_type         type;
	int			   input; 	/* Input Number on bars */
	int  stream_on;
};

static inline struct gc0307_fh *to_fh(struct gc0307_device *dev)
{
	return container_of(dev, struct gc0307_fh, dev);
}

static struct v4l2_frmsize_discrete gc0307_prev_resolution[2]= //should include 320x240 and 640x480, those two size are used for recording
{
	{352,288},
	{640,480},
};

static struct v4l2_frmsize_discrete gc0307_pic_resolution[1]=
{
	{640,480},
};

/*
 * Information we maintain about a known sensor.
 */
struct sensor_format_struct;  /* coming later */
/*__csi_subdev_info_t ccm_info_con = 
{
	.mclk 	= MCLK,
	.vref 	= VREF_POL,
	.href 	= HREF_POL,
	.clock	= CLK_POL,
	.iocfg	= IO_CFG,
};*/

struct sensor_info {
	struct v4l2_subdev sd;
	struct sensor_format_struct *fmt;  /* Current format */
	//__csi_subdev_info_t *ccm_info;
	int	width;
	int	height;
	int brightness;
	int	contrast;
	int saturation;
	int hue;
	int hflip;
	int vflip;
	int gain;
	int autogain;
	int exp;
	enum v4l2_exposure_auto_type autoexp;
	int autowb;
	//enum v4l2_whiteblance wb;
	enum v4l2_colorfx clrfx;
	//enum v4l2_flash_mode flash_mode;
	u8 clkrc;			/* Clock divider value */
};

static inline struct sensor_info *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct sensor_info, sd);
}


struct regval_list {
	unsigned char reg_num[REG_ADDR_STEP];
	unsigned char value[REG_DATA_STEP];
};


/*
 * The default register settings
 *
 */
static struct regval_list sensor_default_regs[] = 
{

     // Initail Sequence Write In.
    //========= close output
{{0xf0},{0x00}},
{{0x43},{0x00}},
{{0x44},{0xa2}},
    
         //========= close some functions
          // open them after configure their parmameters
{{0x40},{0x10}},
{{0x41},{0x08}},     //00     
{{0x42},{0x10}},                      
{{0x47},{0x00}},//mode1,                  
{{0x48},{0xc1}},//mode2,
{{0x49},{0x00}},//dither_mode         
{{0x4a},{0x00}},//clock_gating_en
{{0x4b},{0x00}},//mode_reg3
{{0x4E},{0x23}},//sync mode   23 20120410
{{0x4F},{0x03}},//AWB, AEC, every N frame 01
     
                        //========= frame timing
{{0x01},{0x6a}},//HB
{{0x02},{0x70}},//VB//0c
{{0x1C},{0x00}},//Vs_st
{{0x1D},{0x00}},//Vs_et
{{0x10},{0x00}},//high 4 bits of VB, HB
{{0x11},{0x05}},//row_tail,  AD_pipe_number
     
{{0x03},{0x01}},//row_start
{{0x04},{0x2c}},
     
              //========= windowing
{{0x05},{0x00}},//row_start
{{0x06},{0x00}},
{{0x07},{0x00}},//col start
{{0x08},{0x00}},
{{0x09},{0x01}},//win height
{{0x0A},{0xE8}},
{{0x0B},{0x02}},//win width, pixel array only 640
{{0x0C},{0x80}},
     
                      //========= analog
{{0x0D},{0x22}},//rsh_width
{{0x0E},{0x02}},//CISCTL mode2,  
{{0x0F},{0x32}},//CISCTL mode1
     
     
{{0x12},{0x70}},//7 hrst, 6_4 darsg,
{{0x13},{0x00}},//7 CISCTL_restart, 0 apwd
{{0x14},{0x00}},//NA
{{0x15},{0xba}},//7_4 vref
{{0x16},{0x13}},//5to4 _coln_r,  __1to0__da18
{{0x17},{0x52}},//opa_r, ref_r, sRef_r
//{{0x18},{0xc0}},//analog_mode, best case for left band.
{{0x18},{0x00}},
      
{{0x1E},{0x0d}},//tsp_width          
{{0x1F},{0x32}},//sh_delay
     
                          //========= offset
{{0x47},{0x00}}, //7__test_image, __6__fixed_pga, //__5__auto_DN,__4__CbCr_fix, 
                   //__3to2__dark_sequence, __1__allow_pclk_vcync, __0__LSC_test_image
{{0x19},{0x06}}, //pga_o           
{{0x1a},{0x06}}, //pga_e           
     
{{0x31},{0x00}}, //4  //pga_oFFset ,   high 8bits of 11bits
{{0x3B},{0x00}}, //global_oFFset, low 8bits of 11bits
     
{{0x59},{0x0f}}, //offset_mode    
{{0x58},{0x88}}, //DARK_VALUE_RATIO_G,  DARK_VALUE_RATIO_RB
{{0x57},{0x08}}, //DARK_CURRENT_RATE
{{0x56},{0x77}}, //PGA_OFFSET_EVEN_RATIO, PGA_OFFSET_ODD_RATIO
      
             //========= blk
{{0x35},{0xd8}}, //blk_mode
     
{{0x36},{0x40}}, 
    
{{0x3C},{0x00}},
{{0x3D},{0x00}},
{{0x3E},{0x00}},
{{0x3F},{0x00}},
     
{{0xb5},{0x70}},
{{0xb6},{0x40}},
{{0xb7},{0x00}},
{{0xb8},{0x38}},
{{0xb9},{0xc3}},        
{{0xba},{0x0f}},
     
{{0x7e},{0x45}},
{{0x7f},{0x66}},
     
{{0x5c},{0x48}},//78
{{0x5d},{0x58}},//88
      
      
               //========= manual_gain 
{{0x61},{0x80}},//manual_gain_g1  
{{0x63},{0x80}},//manual_gain_r
{{0x65},{0x98}},//manual_gai_b, 0xa0=1.25, 0x98=1.1875
{{0x67},{0x80}},//manual_gain_g2
{{0x68},{0x18}},//global_manual_gain   2.4bits
     
                 //=========CC _R
{{0x69},{0x58}}, //54//58
{{0x6A},{0xf6}}, //ff
{{0x6B},{0xfb}}, //fe
{{0x6C},{0xf4}}, //ff
{{0x6D},{0x5a}}, //5f
{{0x6E},{0xe6}}, //e1
      
{{0x6f},{0x00}},  
      
           //=========lsc                            
{{0x70},{0x14}},
{{0x71},{0x1c}},
{{0x72},{0x20}},
      
{{0x73},{0x10}},  
{{0x74},{0x3c}},//480/8
{{0x75},{0x52}},// 640/8
     
                 //=========dn                                                                            
{{0x7d},{0x2f}}, //dn_mode    
{{0x80},{0x0c}},//when auto_dn, check 7e,7f
{{0x81},{0x0c}},
{{0x82},{0x44}},
                  
                   //dd                                                                           
{{0x83},{0x18}}, //DD_TH1                       
{{0x84},{0x18}}, //DD_TH2                       
{{0x85},{0x04}}, //DD_TH3                                                                                               
{{0x87},{0x34}}, //32 b DNDD_low_range X16,  DNDD_low_range_C_weight_center                   
     
     
               //=========intp-ee                                                                         
{{0x88},{0x04}},                                                     
{{0x89},{0x01}},                                        
{{0x8a},{0x50}},//60                                          
{{0x8b},{0x50}},//60                                          
{{0x8c},{0x07}},                                                                
                
{{0x50},{0x0c}},                                  
{{0x5f},{0x3c}},                                                                                   
                 
{{0x8e},{0x02}},                                                            
{{0x86},{0x02}},                                                                
              
{{0x51},{0x20}},                                                              
{{0x52},{0x08}}, 
{{0x53},{0x00}},
      
      
              //========= YCP 
                   //contrast_center                                                                             
{{0x77},{0x80}},//contrast_center                                                                   
{{0x78},{0x00}},//fixed_Cb                                                                          
{{0x79},{0x00}},//fixed_Cr                                                                          
//{{0x7a},{0x00}},//luma_offset                                                                                                                                                             
{{0x7b},{0x40}},//hue_cos                                                                           
{{0x7c},{0x00}},//hue_sin                                                                           
                       
            //saturation                                                                                  
{{0xa0},{0x40}},//global_saturation
{{0xa1},{0x40}},//luma_contrast                                                                     
{{0xa2},{0x34}},//saturation_Cb//0x34                                                                   
{{0xa3},{0x32}},// 34  saturation_Cr//0x34
          
{{0xa4},{0xc8}},                                                                
{{0xa5},{0x02}},
{{0xa6},{0x28}},                                                                            
{{0xa7},{0x02}},
     
          //skin                                                                                                
{{0xa8},{0xee}},                                                            
{{0xa9},{0x12}},                                                            
{{0xaa},{0x01}},                                                        
{{0xab},{0x20}},                                                    
{{0xac},{0xf0}},                                                        
{{0xad},{0x10}},                                                            
     
             //========= ABS
{{0xae},{0x18}},//  black_pixel_target_number 
{{0xaf},{0x74}},
{{0xb0},{0xe0}},    
{{0xb1},{0x20}},
{{0xb2},{0x6c}},
{{0xb3},{0x40}},
{{0xb4},{0x04}},
     
               //========= AWB 
{{0xbb},{0x42}},
{{0xbc},{0x60}},
{{0xbd},{0x50}},
{{0xbe},{0x50}},
     
{{0xbf},{0x0c}},
{{0xc0},{0x06}},
{{0xc1},{0x60}},
{{0xc2},{0xf1}}, //f4
{{0xc3},{0x40}},
{{0xc4},{0x1c}},//18
{{0xc5},{0x56}},
{{0xc6},{0x1d}},  

{{0xca},{0x70}},//0x70
{{0xcb},{0x70}},//0x70
{{0xcc},{0x78}},//0x78

{{0xcd},{0x80}},//R_ratio                                      
{{0xce},{0x80}},//G_ratio  , cold_white white                                    
{{0xcf},{0x80}},//B_ratio     

                   //=========  aecT  
{{0x20},{0x06}},//02
{{0x21},{0xc0}},
{{0x22},{0x40}},   
{{0x23},{0x88}},
{{0x24},{0x96}},
{{0x25},{0x30}},
{{0x26},{0xd0}},
{{0x27},{0x00}},
     
     
    /////23 M  
{{0x28},{0x04}},//AEC_exp_level_1bit11to8   
{{0x29},{0xb0}},//AEC_exp_level_1bit7to0    
{{0x2a},{0x04}},//AEC_exp_level_2bit11to8   
{{0x2b},{0xb0}},//AEC_exp_level_2bit7to0           
{{0x2c},{0x04}},//AEC_exp_level_3bit11to8   659 - 8FPS,  8ca - 6FPS  //    
{{0x2d},{0xb0}},//AEC_exp_level_3bit7to0           
{{0x2e},{0x04}},//AEC_exp_level_4bit11to8   4FPS 
{{0x2f},{0xb0}},//AEC_exp_level_4bit7to0   
    
{{0x30},{0x20}},                        
{{0x31},{0x00}},                     
{{0x32},{0x1c}},
{{0x33},{0x90}},            
{{0x34},{0x10}},  
     
{{0xd0},{0x34}},//[2]  1  before gamma,  0 after gamma
     
//{{0xd1},{0x50}},//AEC_target_Y//0x50                         
{{0xd2},{0x61}},//f2     
{{0xd4},{0x4b}},//96
{{0xd5},{0x01}},// 10 
{{0xd6},{0x4b}},//antiflicker_step //96                      
{{0xd7},{0x03}},//AEC_exp_time_min //10              
{{0xd8},{0x02}},
      
{{0xdd},{0x12}},
     
         //========= measure window                                      
{{0xe0},{0x03}},                       
{{0xe1},{0x02}},                           
{{0xe2},{0x27}},                               
{{0xe3},{0x1e}},               
{{0xe8},{0x3b}},                   
{{0xe9},{0x6e}},                       
{{0xea},{0x2c}},                   
{{0xeb},{0x50}},                   
{{0xec},{0x73}},       
      
              //========= close_frame                                                 
{{0xed},{0x00}},//close_frame_num1 ,can be use to reduce FPS               
{{0xee},{0x00}},//close_frame_num2  
{{0xef},{0x00}},//close_frame_num
     
        // page1
{{0xf0},{0x01}},//select page1 
      
{{0x00},{0x20}},                            
{{0x01},{0x20}},                            
{{0x02},{0x20}},                                  
{{0x03},{0x20}},                          
{{0x04},{0x78}},
{{0x05},{0x78}},                   
{{0x06},{0x78}},                                
{{0x07},{0x78}},                                   
     
     
     
{{0x10},{0x04}},                        
{{0x11},{0x04}},                            
{{0x12},{0x04}},                        
{{0x13},{0x04}},                            
{{0x14},{0x01}},                            
{{0x15},{0x01}},                            
{{0x16},{0x01}},                       
{{0x17},{0x01}},                       
      
      
{{0x20},{0x00}},                    
{{0x21},{0x00}},                    
{{0x22},{0x00}},                        
{{0x23},{0x00}},                        
{{0x24},{0x00}},                    
{{0x25},{0x00}},                        
{{0x26},{0x00}},                    
{{0x27},{0x00}},                        
     
{{0x40},{0x11}},
     
       //=============================lscP 
{{0x45},{0x06}},   
{{0x46},{0x06}},           
{{0x47},{0x05}},
     
{{0x48},{0x04}},  
{{0x49},{0x03}},       
{{0x4a},{0x03}},
     
     
{{0x62},{0xd8}},
{{0x63},{0x24}},
{{0x64},{0x24}},
{{0x65},{0x24}},
{{0x66},{0xd8}},
{{0x67},{0x24}},
    
{{0x5a},{0x00}},
{{0x5b},{0x00}},
{{0x5c},{0x00}},
{{0x5d},{0x00}},
{{0x5e},{0x00}},
{{0x5f},{0x00}},
     
     
           //============================= ccP 
     
{{0x69},{0x03}},//cc_mode
     
           //CC_G
{{0x70},{0x5d}},
{{0x71},{0xed}},
{{0x72},{0xff}},
{{0x73},{0xe5}},
{{0x74},{0x5f}},
{{0x75},{0xe6}},
      
         //CC_B
{{0x76},{0x41}},
{{0x77},{0xef}},
{{0x78},{0xff}},
{{0x79},{0xff}},
{{0x7a},{0x5f}},
{{0x7b},{0xfa}},   
     
     
          //============================= AGP
     
{{0x7e},{0x00}}, 
{{0x7f},{0x30}},   //  00    select gamma  
{{0x80},{0x48}}, //  c8
{{0x81},{0x06}}, 
{{0x82},{0x08}}, 
      
             
{{0x83},{0x23}}, 
{{0x84},{0x38}}, 
{{0x85},{0x4F}}, 
{{0x86},{0x61}}, 
{{0x87},{0x72}}, 
{{0x88},{0x80}}, 
{{0x89},{0x8D}}, 
{{0x8a},{0xA2}}, 
{{0x8b},{0xB2}}, 
{{0x8c},{0xC0}}, 
{{0x8d},{0xCA}}, 
{{0x8e},{0xD3}}, 
{{0x8f},{0xDB}}, 
{{0x90},{0xE2}}, 
{{0x91},{0xED}}, 
{{0x92},{0xF6}}, 
{{0x93},{0xFD}},
        /*
{{0x83},{0x13}}, // 相当于0x20对应的Gamma
{{0x84},{0x23}}, 
{{0x85},{0x35}}, 
{{0x86},{0x44}}, 
{{0x87},{0x53}}, 
{{0x88},{0x60}}, 
{{0x89},{0x6D}}, 
{{0x8a},{0x84}}, 
{{0x8b},{0x98}}, 
{{0x8c},{0xaa}}, 
{{0x8d},{0xb8}}, 
{{0x8e},{0xc6}}, 
{{0x8f},{0xd1}}, 
{{0x90},{0xdb}}, 
{{0x91},{0xea}}, 
{{0x92},{0xf5}}, 
{{0x93},{0xFb}},
     */ 
            //about gamma1 is hex r oct
{{0x94},{0x04}}, 
{{0x95},{0x0E}}, 
{{0x96},{0x1B}}, 
{{0x97},{0x28}}, 
{{0x98},{0x35}}, 
{{0x99},{0x41}}, 
{{0x9a},{0x4E}}, 
{{0x9b},{0x67}}, 
{{0x9c},{0x7E}}, 
{{0x9d},{0x94}}, 
{{0x9e},{0xA7}}, 
{{0x9f},{0xBA}}, 
{{0xa0},{0xC8}}, 
{{0xa1},{0xD4}}, 
{{0xa2},{0xE7}}, 
{{0xa3},{0xF4}}, 
{{0xa4},{0xFA}},
      
            //========= open functions  
{{0xf0},{0x00}},//set back to page0  
     
{{0x40},{0x7e}},
{{0x41},{0x2f}},
{{0x43},{0x40}},
{{0x44},{0xE2}},

{{0x0f},{0x02}},//02
{{0x45},{0x24}},
{{0x47},{0x20}},

};

static struct regval_list sensor_oe_disable[] =
{
	{{0xf0},{0x00}},
	{{0x44},{0xA2}},
};

/*
 * The white balance settings
 * Here only tune the R G B channel gain. 
 * The white balance enalbe bit is modified in sensor_s_autowb and sensor_s_wb
 */
static struct regval_list sensor_wb_auto_regs[] = {
	{{0xf0},{0x00}},
//	{{0x41},{0x2b}},
	{{0xc7},{0x4c}},
	{{0xc8},{0x40}},
	{{0xc9},{0x4a}},
};

static struct regval_list sensor_wb_cloud_regs[] = {
	{{0xf0},{0x00}},
//	{{0x41},{0x2b}},
	{{0xc7},{0x5a}},
	{{0xc8},{0x42}},
	{{0xc9},{0x40}},
};

static struct regval_list sensor_wb_daylight_regs[] = {
	//tai yang guang
	{{0xf0},{0x00}},
//	{{0x41},{0x2b}},	
	{{0xc7},{0x50}},
	{{0xc8},{0x45}},
	{{0xc9},{0x40}},
};

static struct regval_list sensor_wb_incandescence_regs[] = {
	//bai re guang
	{{0xf0},{0x00}},
//	{{0x41},{0x2b}},	
	{{0xc7},{0x48}},
	{{0xc8},{0x40}},
	{{0xc9},{0x5c}},
};

static struct regval_list sensor_wb_fluorescent_regs[] = {
	//ri guang deng
	{{0xf0},{0x00}},
//	{{0x41},{0x2b}},
	{{0xc7},{0x40}},
	{{0xc8},{0x42}},
	{{0xc9},{0x50}},
};

static struct regval_list sensor_wb_tungsten_regs[] = {
	//wu si deng
	{{0xf0},{0x00}},
//	{{0x41},{0x2b}},
	{{0xc7},{0x40}},
	{{0xc8},{0x54}},
	{{0xc9},{0x70}},
};

/*
 * The color effect settings
 */
static struct regval_list sensor_colorfx_none_regs[] = {
			{{0xf0},{0x00}},		
			{{0x41},{0x2f}},//  3f
			{{0x40},{0x7e}},
			{{0x42},{0x10}},
			{{0x47},{0x2c}},//
			{{0x48},{0xc3}},
			{{0x8a},{0x50}},//60
			{{0x8b},{0x50}},
			{{0x8c},{0x07}},
			{{0x50},{0x0c}},
			{{0x77},{0x80}},
			{{0xa1},{0x40}},
			{{0x7a},{0x00}},
			{{0x78},{0x00}},
			{{0x79},{0x00}},
			{{0x7b},{0x40}},
			{{0x7c},{0x00}},
};

static struct regval_list sensor_colorfx_bw_regs[] = {
      {{0xf0},{0x00}},
      {{0x41},{0x2f}},//2f  blackboard
			{{0x40},{0x7e}},
			{{0x42},{0x10}},
			{{0x47},{0x3c}},//2c
			{{0x48},{0xc3}},
			{{0x8a},{0x60}},
			{{0x8b},{0x60}},
			{{0x8c},{0x07}},
			{{0x50},{0x0c}},
			{{0x77},{0x80}},
			{{0xa1},{0x40}},
			{{0x7a},{0x00}},
			{{0x78},{0x00}},
			{{0x79},{0x00}},
			{{0x7b},{0x40}},
			{{0x7c},{0x00}},
};

static struct regval_list sensor_colorfx_sepia_regs[] = {
      {{0xf0},{0x00}},
      {{0x41},{0x2f}},
			{{0x40},{0x7e}},
			{{0x42},{0x10}},
			{{0x47},{0x3c}},
			{{0x48},{0xc3}},
			{{0x8a},{0x60}},
			{{0x8b},{0x60}},
			{{0x8c},{0x07}},
			{{0x50},{0x0c}},
			{{0x77},{0x80}},
			{{0xa1},{0x40}},
			{{0x7a},{0x00}},
			{{0x78},{0xc0}},
			{{0x79},{0x20}},
			{{0x7b},{0x40}},
			{{0x7c},{0x00}},	
};

static struct regval_list sensor_colorfx_negative_regs[] = {
      {{0xf0},{0x00}},
      {{0x41},{0x6f}},//[6]
			{{0x40},{0x7e}},
			{{0x42},{0x10}},
			{{0x47},{0x20}},//20
			{{0x48},{0xc3}},
			{{0x8a},{0x60}},
			{{0x8b},{0x60}},
			{{0x8c},{0x07}},
			{{0x50},{0x0c}},
			{{0x77},{0x80}},
			{{0xa1},{0x40}},
			{{0x7a},{0x00}},
			{{0x78},{0x00}},
			{{0x79},{0x00}},
			{{0x7b},{0x40}},
			{{0x7c},{0x00}},
			{{0x41},{0x6f}},
};

static struct regval_list sensor_colorfx_emboss_regs[] = {
	
};

static struct regval_list sensor_colorfx_sketch_regs[] = {
	    {{0xf0},{0x00}},
	    {{0x41},{0x2f}},
			{{0x40},{0x7e}},
			{{0x42},{0x10}},
			{{0x47},{0x3c}},
			{{0x48},{0xc3}},
			{{0x8a},{0x60}},
			{{0x8b},{0x60}},
			{{0x8c},{0x07}},
			{{0x50},{0x0c}},
			{{0x77},{0x80}},
			{{0xa1},{0x40}},
			{{0x7a},{0x00}},
			{{0x78},{0x00}},
			{{0x79},{0x00}},
			{{0x7b},{0x40}},
			{{0x7c},{0x00}},
};

static struct regval_list sensor_colorfx_sky_blue_regs[] = {
	    {{0xf0},{0x00}},
	    {{0x41},{0x2f}},
			{{0x40},{0x7e}},
			{{0x42},{0x10}},
			{{0x47},{0x2c}},
			{{0x48},{0xc3}},
			{{0x8a},{0x60}},
			{{0x8b},{0x60}},
			{{0x8c},{0x07}},
			{{0x50},{0x0c}},
			{{0x77},{0x80}},
			{{0xa1},{0x40}},
			{{0x7a},{0x00}},
			{{0x78},{0x70}},
			{{0x79},{0x00}},
			{{0x7b},{0x3f}},
			{{0x7c},{0xf5}},	
};

static struct regval_list sensor_colorfx_grass_green_regs[] = {
			{{0xf0},{0x00}},
			{{0x41},{0x2f}},
			{{0x40},{0x7e}},
			{{0x42},{0x10}},
			{{0x47},{0x3c}},
			{{0x48},{0xc3}},
			{{0x8a},{0x60}},
			{{0x8b},{0x60}},
			{{0x8c},{0x07}},
			{{0x50},{0x0c}},
			{{0x77},{0x80}},
			{{0xa1},{0x40}},
			{{0x7a},{0x00}},
			{{0x78},{0xc0}},
			{{0x79},{0xc0}},
			{{0x7b},{0x40}},
			{{0x7c},{0x00}},	
};

static struct regval_list sensor_colorfx_skin_whiten_regs[] = {
	
};

static struct regval_list sensor_colorfx_vivid_regs[] = {
//NULL
};

/*
 * The brightness setttings
 */
static struct regval_list sensor_brightness_neg4_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_neg3_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_neg2_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_neg1_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_zero_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_pos1_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_pos2_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_pos3_regs[] = {
//NULL
};

static struct regval_list sensor_brightness_pos4_regs[] = {
//NULL
};

/*
 * The contrast setttings
 */
static struct regval_list sensor_contrast_neg4_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_neg3_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_neg2_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_neg1_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_zero_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_pos1_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_pos2_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_pos3_regs[] = {
//NULL
};

static struct regval_list sensor_contrast_pos4_regs[] = {
//NULL
};

/*
 * The saturation setttings
 */
static struct regval_list sensor_saturation_neg4_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_neg3_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_neg2_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_neg1_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_zero_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_pos1_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_pos2_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_pos3_regs[] = {
//NULL
};

static struct regval_list sensor_saturation_pos4_regs[] = {
//NULL
};

/*
 * The exposure target setttings
 */
static struct regval_list sensor_ev_neg4_regs[] = {
	
	{{0x7a},{0xc0}},                   
	{{0xd1},{0x30}},                    	
};

static struct regval_list sensor_ev_neg3_regs[] = {
	
	{{0x7a},{0xd0}},                   
	{{0xd1},{0x38}},                    	
};

static struct regval_list sensor_ev_neg2_regs[] = {
	
	{{0x7a},{0xe0}},                   
	{{0xd1},{0x40}},                   	
};

static struct regval_list sensor_ev_neg1_regs[] = {
	
	{{0x7a},{0xf0}},                   
	{{0xd1},{0x48}},                    	
};

static struct regval_list sensor_ev_zero_regs[] = {
	
	
	{{0x7a},{0x00}},                   
	{{0xd1},{0x50}},                   	
};

static struct regval_list sensor_ev_pos1_regs[] = {
	
	{{0x7a},{0x10}},                   
	{{0xd1},{0x54}},                    	
};

static struct regval_list sensor_ev_pos2_regs[] = {
	
	{{0x7a},{0x20}},                   
	{{0xd1},{0x58}},                   	
};

static struct regval_list sensor_ev_pos3_regs[] = {
	
	{{0x7a},{0x30}},                   
	{{0xd1},{0x60}},                    	
};

static struct regval_list sensor_ev_pos4_regs[] = {
	
	{{0x7a},{0x40}},                   
	{{0xd1},{0x68}},                   
};


/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 * 
 */


static struct regval_list sensor_fmt_yuv422_yuyv[] = {
	//YCbYCr
	{{0xf0},{0x00}},
	{{0x44},{0xE2}},
};


static struct regval_list sensor_fmt_yuv422_yvyu[] = {
	//YCrYCb
	{{0xf0},{0x00}},
	{{0x44},{0xE3}},
};

static struct regval_list sensor_fmt_yuv422_vyuy[] = {
	//CrYCbY
	{{0xf0},{0x00}},
	{{0x44},{0xE1}},
};

static struct regval_list sensor_fmt_yuv422_uyvy[] = {
	//CbYCrY
	{{0xf0},{0x00}},
	{{0x44},{0xE0}},
};

//static struct regval_list sensor_fmt_raw[] = {
//	
//	//raw
//};



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
	
//	for(i = 0; i < REG_STEP; i++)
//		printk("data[%x]=%x\n",i,data[i]);
			
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
		ret = sensor_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			{
				csi_dev_err("sensor_write_err!\n");
				return ret;
			}
		udelay(100);
		vals++;
	}

	return 0;
}

/*
 * CSI GPIO control
 */
/*static void csi_gpio_write(struct v4l2_subdev *sd, user_gpio_set_t *gpio, int status)
{
	struct csi_dev *dev=(struct csi_dev *)dev_get_drvdata(sd->v4l2_dev->dev);
		
  if(gpio->port == 0xffff) {
    axp_gpio_set_io(gpio->port_num, 1);
    axp_gpio_set_value(gpio->port_num, status); 
  } else {
    gpio_write_one_pin_value(dev->csi_pin_hd,status,(char *)&gpio->gpio_name);
  }
}

static void csi_gpio_set_status(struct v4l2_subdev *sd, user_gpio_set_t *gpio, int status)
{
	struct csi_dev *dev=(struct csi_dev *)dev_get_drvdata(sd->v4l2_dev->dev);
		
  if(gpio->port == 0xffff) {
    axp_gpio_set_io(gpio->port_num, status);
  } else {
    gpio_set_one_pin_io_status(dev->csi_pin_hd,status,(char *)&gpio->gpio_name);
  }
}*/

/*
 * Stuff that knows about the sensor.
 */
 
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	struct csi_dev *dev=(struct csi_dev *)dev_get_drvdata(sd->v4l2_dev->dev);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
#if 0
  //insure that clk_disable() and clk_enable() are called in pair 
  //when calling CSI_SUBDEV_STBY_ON/OFF and CSI_SUBDEV_PWR_ON/OFF  
  switch(on)
	{
		case CSI_SUBDEV_STBY_ON:
			//csi_dev_dbg("CSI_SUBDEV_STBY_ON\n");
			//reset off io
			//csi_gpio_write(sd,&dev->reset_io,CSI_RST_OFF);
			mdelay(10);
			//disable oe
			//csi_dev_print("disalbe oe!\n");
			ret = sensor_write_array(sd,sensor_oe_disable,ARRAY_SIZE(sensor_oe_disable));
			if(ret < 0)
				csi_dev_err("sensor_oe_disable error\n");
			//make sure that no device can access i2c bus during sensor initial or power down
			//when using i2c_lock_adpater function, the following codes must not access i2c bus before calling i2c_unlock_adapter
			i2c_lock_adapter(client->adapter);
			//standby on io
			//csi_gpio_write(sd,&dev->standby_io,CSI_STBY_ON);
			mdelay(100);
			//csi_gpio_write(sd,&dev->standby_io,CSI_STBY_OFF);
			mdelay(100);
			//csi_gpio_write(sd,&dev->standby_io,CSI_STBY_ON);
			mdelay(100);
			//remember to unlock i2c adapter, so the device can access the i2c bus again
			i2c_unlock_adapter(client->adapter);	
			//inactive mclk after stadby in
			//clk_disable(dev->csi_module_clk);
			//reset on io
			//csi_gpio_write(sd,&dev->reset_io,CSI_RST_ON);
			mdelay(10);
			break;
		case CSI_SUBDEV_STBY_OFF:
			//csi_dev_dbg("CSI_SUBDEV_STBY_OFF\n");
			//make sure that no device can access i2c bus during sensor initial or power down
			//when using i2c_lock_adpater function, the following codes must not access i2c bus before calling i2c_unlock_adapter
			//i2c_lock_adapter(client->adapter);
			//active mclk before stadby out
			//clk_enable(dev->csi_module_clk);
			mdelay(10);
			//standby off io
			//csi_gpio_write(sd,&dev->standby_io,CSI_STBY_OFF);
			mdelay(10);
			//reset off io
			//csi_gpio_write(sd,&dev->reset_io,CSI_RST_OFF);
			mdelay(10);
			//csi_gpio_write(sd,&dev->reset_io,CSI_RST_ON);
			mdelay(100);
			//csi_gpio_write(sd,&dev->reset_io,CSI_RST_OFF);
			mdelay(100);
			//remember to unlock i2c adapter, so the device can access the i2c bus again
			i2c_unlock_adapter(client->adapter);	
			break;
		case CSI_SUBDEV_PWR_ON:
			csi_dev_dbg("CSI_SUBDEV_PWR_ON\n");
			//make sure that no device can access i2c bus during sensor initial or power down
			//when using i2c_lock_adpater function, the following codes must not access i2c bus before calling i2c_unlock_adapter
			i2c_lock_adapter(client->adapter);
			//power on reset
			//csi_gpio_set_status(sd,&dev->standby_io,1);//set the gpio to output
			//csi_gpio_set_status(sd,&dev->reset_io,1);//set the gpio to output
			//csi_gpio_write(sd,&dev->standby_io,CSI_STBY_ON);
			//reset on io
			//csi_gpio_write(sd,&dev->reset_io,CSI_RST_ON);
			mdelay(1);
			//active mclk before power on
			//clk_enable(dev->csi_module_clk);
			mdelay(10);
			//power supply
			//csi_gpio_write(sd,&dev->power_io,CSI_PWR_ON);
			mdelay(10);
			if(dev->dvdd) {
				regulator_enable(dev->dvdd);
				mdelay(10);
			}
			if(dev->avdd) {
				regulator_enable(dev->avdd);
				mdelay(10);
			}
			if(dev->iovdd) {
				regulator_enable(dev->iovdd);
				mdelay(10);
			}
			//standby off io
			csi_gpio_write(sd,&dev->standby_io,CSI_STBY_OFF);
			mdelay(10);
			//reset after power on
			csi_gpio_write(sd,&dev->reset_io,CSI_RST_OFF);
			mdelay(10);
			csi_gpio_write(sd,&dev->reset_io,CSI_RST_ON);
			mdelay(100);
			csi_gpio_write(sd,&dev->reset_io,CSI_RST_OFF);
			mdelay(100);
			//remember to unlock i2c adapter, so the device can access the i2c bus again
			i2c_unlock_adapter(client->adapter);	
			break;		
		case CSI_SUBDEV_PWR_OFF:
			csi_dev_dbg("CSI_SUBDEV_PWR_OFF\n");
			//make sure that no device can access i2c bus during sensor initial or power down
			//when using i2c_lock_adpater function, the following codes must not access i2c bus before calling i2c_unlock_adapter
			i2c_lock_adapter(client->adapter);
			//standby and reset io

			csi_gpio_write(sd,&dev->reset_io,1/*CSI_RST_ON*/);//mod for gc2015 conflict with camera i2c
			mdelay(100);
			csi_gpio_write(sd,&dev->reset_io,0/*CSI_RST_ON*/);//mod for gc2015 conflict with camera i2c
			mdelay(100);

			csi_gpio_write(sd,&dev->reset_io,1/*CSI_RST_ON*/);//mod for gc2015 conflict with camera i2c
			mdelay(100);			

			
			csi_gpio_write(sd,&dev->standby_io,CSI_STBY_ON);
			mdelay(100);
			
			//power supply off
			if(dev->iovdd) {
				regulator_disable(dev->iovdd);
				mdelay(10);
			}
			if(dev->avdd) {
				regulator_disable(dev->avdd);
				mdelay(10);
			}
			if(dev->dvdd) {
				regulator_disable(dev->dvdd);
				mdelay(10);	
			}
			csi_gpio_write(sd,&dev->power_io,1/*CSI_PWR_OFF*/);
			mdelay(10);
			//inactive mclk after power off
			clk_disable(dev->csi_module_clk);
			//set the io to hi-z
			//csi_gpio_set_status(sd,&dev->reset_io,0);//set the gpio to input
			//csi_gpio_set_status(sd,&dev->standby_io,0);//set the gpio to input
			//remember to unlock i2c adapter, so the device can access the i2c bus again
			i2c_unlock_adapter(client->adapter);	
			break;
		default:
			return -EINVAL;
	}		
	#endif

	return 0;
}
 
static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
#if 0
	struct csi_dev *dev=(struct csi_dev *)dev_get_drvdata(sd->v4l2_dev->dev);

	switch(val)
	{
		case CSI_SUBDEV_RST_OFF:
			//csi_dev_dbg("CSI_SUBDEV_RST_OFF\n");
			//csi_gpio_write(sd,&dev->reset_io,CSI_RST_OFF);
			mdelay(10);
			break;
		case CSI_SUBDEV_RST_ON:
			//csi_dev_dbg("CSI_SUBDEV_RST_ON\n");
			//csi_gpio_write(sd,&dev->reset_io,CSI_RST_ON);
			mdelay(10);
			break;
		case CSI_SUBDEV_RST_PUL:
			//csi_dev_dbg("CSI_SUBDEV_RST_PUL\n");
			//csi_gpio_write(sd,&dev->reset_io,CSI_RST_OFF);
			mdelay(10);
			//csi_gpio_write(sd,&dev->reset_io,CSI_RST_ON);
			mdelay(100);
			//csi_gpio_write(sd,&dev->reset_io,CSI_RST_OFF);
			mdelay(100);
			break;
		default:
			return -EINVAL;
	}
#endif
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
	
	if(regs.value[0] != 0x99)
		return -ENODEV;
	
	return 0;
}

static int GC0307_init_regs(struct gc0307_device *dev)
{
	int ret;
	csi_dev_dbg("sensor_init\n");

	/*Make sure it is a target sensor*/
	ret = sensor_detect(&dev->sd);
	if (ret) {
		csi_dev_err("chip found is not an target chip.\n");
		return ret;
	}
	msleep(10);
	return sensor_write_array(&dev->sd, sensor_default_regs , ARRAY_SIZE(sensor_default_regs));
}

/*
 * Store information about the video data format. 
 */
static struct sensor_format_struct {
	__u8 *desc;
	//__u32 pixelformat;
	enum v4l2_mbus_pixelcode mbus_code;//linux-3.0
	struct regval_list *regs;
	int	regs_size;
	int bpp;   /* Bytes per pixel */
} sensor_formats[] = {
	{
		.desc		= "YUYV 4:2:2",
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,//linux-3.0
		.regs 		= sensor_fmt_yuv422_yuyv,
		.regs_size = ARRAY_SIZE(sensor_fmt_yuv422_yuyv),
		.bpp		= 2,
	},
	{
		.desc		= "YVYU 4:2:2",
		.mbus_code	= V4L2_MBUS_FMT_YVYU8_2X8,//linux-3.0
		.regs 		= sensor_fmt_yuv422_yvyu,
		.regs_size = ARRAY_SIZE(sensor_fmt_yuv422_yvyu),
		.bpp		= 2,
	},
	{
		.desc		= "UYVY 4:2:2",
		.mbus_code	= V4L2_MBUS_FMT_UYVY8_2X8,//linux-3.0
		.regs 		= sensor_fmt_yuv422_uyvy,
		.regs_size = ARRAY_SIZE(sensor_fmt_yuv422_uyvy),
		.bpp		= 2,
	},
	{
		.desc		= "VYUY 4:2:2",
		.mbus_code	= V4L2_MBUS_FMT_VYUY8_2X8,//linux-3.0
		.regs 		= sensor_fmt_yuv422_vyuy,
		.regs_size = ARRAY_SIZE(sensor_fmt_yuv422_vyuy),
		.bpp		= 2,
	},
//	{
//		.desc		= "Raw RGB Bayer",
//		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,//linux-3.0
//		.regs 		= sensor_fmt_raw,
//		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
//		.bpp		= 1
//	},
};

static int sensor_s_hflip(struct v4l2_subdev *sd, int value)
{
	int ret,i;
	struct regval_list regs[3];

	regs[0].reg_num[0] = 0xf0;
	regs[0].value[0] = 0x00;		//page 0
	ret = sensor_write(sd, regs[0].reg_num, regs[0].value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_s_hflip!\n");
		return ret;
	}
	
	regs[0].reg_num[0] = 0x0f;
	regs[1].reg_num[0] = 0x45;
	regs[2].reg_num[0] = 0x47;
	
	for(i=0; i<3; i++) {
		ret = sensor_read(sd, regs[i].reg_num, regs[i].value);
		if (ret < 0) {
			csi_dev_err("sensor_read err at sensor_s_hflip!\n");
			return ret;
		}
	}
	
	switch (value) {
		case 0:
		  regs[0].value[0] &= 0xef;
		  regs[1].value[0] &= 0xfe;
		  regs[2].value[0] &= 0xfb;
			break;
		case 1:
			regs[0].value[0] |= 0x10;
		  regs[1].value[0] |= 0x01;
		  regs[2].value[0] |= 0x04;
			break;
		default:
			return -EINVAL;
	}
	
	for(i=0; i<3; i++) {
		ret = sensor_write(sd, regs[i].reg_num, regs[i].value);
		if (ret < 0) {
			csi_dev_err("sensor_write err at sensor_s_hflip!\n");
			return ret;
		}
	}
	
	mdelay(100);
	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int value)
{
	int ret,i;
	struct regval_list regs[3];
	
	regs[0].reg_num[0] = 0xf0;
	regs[0].value[0] = 0x00;		//page 0
	ret = sensor_write(sd, regs[0].reg_num, regs[0].value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_s_vflip!\n");
		return ret;
	}
	
	regs[0].reg_num[0] = 0x0f;
	regs[1].reg_num[0] = 0x45;
	regs[2].reg_num[0] = 0x47;
	
	for(i=0; i<3; i++) {
		ret = sensor_read(sd, regs[i].reg_num, regs[i].value);
		if (ret < 0) {
			csi_dev_err("sensor_read err at sensor_s_vflip!\n");
			return ret;
		}
	}
	
	switch (value) {
		case 0:
		  regs[0].value[0] &= 0xdf;
		  regs[1].value[0] &= 0xfd;
		  regs[2].value[0] &= 0xf7;
			break;
		case 1:
			regs[0].value[0] |= 0x20;
		  regs[1].value[0] |= 0x02;
		  regs[2].value[0] |= 0x08;
			break;
		default:
			return -EINVAL;
	}
	
	for(i=0; i<3; i++) {
		ret = sensor_write(sd, regs[i].reg_num, regs[i].value);
		if (ret < 0) {
			csi_dev_err("sensor_write err at sensor_s_vflip!\n");
			return ret;
		}
	}
	
	mdelay(100);
	
	return 0;
}

static int sensor_g_autogain(struct v4l2_subdev *sd, __s32 *value)
{
	return -EINVAL;
}

static int sensor_s_autogain(struct v4l2_subdev *sd, int value)
{
	return -EINVAL;
}

static int sensor_g_autoexp(struct v4l2_subdev *sd, __s32 *value)
{
	int ret;
	struct sensor_info *info = to_state(sd);
	struct regval_list regs;
	
	regs.reg_num[0] = 0xf0;
	regs.value[0] = 0x00;		//page 0
	ret = sensor_write(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_g_autoexp!\n");
		return ret;
	}
	
	regs.reg_num[0] = 0x41;
	ret = sensor_read(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_read err at sensor_g_autoexp!\n");
		return ret;
	}

	regs.value[0] &= 0x08;
	if (regs.value[0] == 0x08) {
		*value = V4L2_EXPOSURE_AUTO;
	}
	else
	{
		*value = V4L2_EXPOSURE_MANUAL;
	}
	
	info->autoexp = *value;
	return 0;
}

static int sensor_s_autoexp(struct v4l2_subdev *sd,
		enum v4l2_exposure_auto_type value)
{
	int ret;
	struct regval_list regs;
	
	regs.reg_num[0] = 0xf0;
	regs.value[0] = 0x00;		//page 0
	ret = sensor_write(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_s_autoexp!\n");
		return ret;
	}
	
	regs.reg_num[0] = 0x41;
	ret = sensor_read(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_read err at sensor_s_autoexp!\n");
		return ret;
	}

	switch (value) {
		case V4L2_EXPOSURE_AUTO:
		  regs.value[0] |= 0x08;
			break;
		case V4L2_EXPOSURE_MANUAL:
			regs.value[0] &= 0xf7;
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
	
	mdelay(60);

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
	
	regs.reg_num[0] = 0xf0;
	regs.value[0] = 0x00;		//page 0
	ret = sensor_write(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_s_autowb!\n");
		return ret;
	}
	
	regs.reg_num[0] = 0x41;
	ret = sensor_read(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_read err at sensor_s_autowb!\n");
		return ret;
	}

	switch(value) {
	case 0:
		regs.value[0] &= 0xfb;
		break;
	case 1:
		regs.value[0] |= 0x04;
		break;
	default:
		break;
	}	
	ret = sensor_write(sd, regs.reg_num, regs.value);
	if (ret < 0) {
		csi_dev_err("sensor_write err at sensor_s_autowb!\n");
		return ret;
	}
	
	mdelay(60);
	
	return 0;
}

/* *********************************************end of ******************************************** */

static int sensor_s_brightness(struct v4l2_subdev *sd, int value)
{
	int ret;
	
	switch (value) {
		case -4:
		  ret = sensor_write_array(sd, sensor_brightness_neg4_regs, ARRAY_SIZE(sensor_brightness_neg4_regs));
			break;
		case -3:
			ret = sensor_write_array(sd, sensor_brightness_neg3_regs, ARRAY_SIZE(sensor_brightness_neg3_regs));
			break;
		case -2:
			ret = sensor_write_array(sd, sensor_brightness_neg2_regs, ARRAY_SIZE(sensor_brightness_neg2_regs));
			break;   
		case -1:
			ret = sensor_write_array(sd, sensor_brightness_neg1_regs, ARRAY_SIZE(sensor_brightness_neg1_regs));
			break;
		case 0:   
			ret = sensor_write_array(sd, sensor_brightness_zero_regs, ARRAY_SIZE(sensor_brightness_zero_regs));
			break;
		case 1:
			ret = sensor_write_array(sd, sensor_brightness_pos1_regs, ARRAY_SIZE(sensor_brightness_pos1_regs));
			break;
		case 2:
			ret = sensor_write_array(sd, sensor_brightness_pos2_regs, ARRAY_SIZE(sensor_brightness_pos2_regs));
			break;	
		case 3:
			ret = sensor_write_array(sd, sensor_brightness_pos3_regs, ARRAY_SIZE(sensor_brightness_pos3_regs));
			break;
		case 4:
			ret = sensor_write_array(sd, sensor_brightness_pos4_regs, ARRAY_SIZE(sensor_brightness_pos4_regs));
			break;
		default:
			return -EINVAL;
	}
	
	if (ret < 0) {
		csi_dev_err("sensor_write_array err at sensor_s_brightness!\n");
		return ret;
	}
	
	mdelay(60);

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
	
	mdelay(60);

	return 0;
}

static int sensor_s_saturation(struct v4l2_subdev *sd, int value)
{
	int ret;

	switch (value) {
		case -4:
		  ret = sensor_write_array(sd, sensor_saturation_neg4_regs, ARRAY_SIZE(sensor_saturation_neg4_regs));
			break;
		case -3:
			ret = sensor_write_array(sd, sensor_saturation_neg3_regs, ARRAY_SIZE(sensor_saturation_neg3_regs));
			break;
		case -2:
			ret = sensor_write_array(sd, sensor_saturation_neg2_regs, ARRAY_SIZE(sensor_saturation_neg2_regs));
			break;   
		case -1:
			ret = sensor_write_array(sd, sensor_saturation_neg1_regs, ARRAY_SIZE(sensor_saturation_neg1_regs));
			break;
		case 0:   
			ret = sensor_write_array(sd, sensor_saturation_zero_regs, ARRAY_SIZE(sensor_saturation_zero_regs));
			break;
		case 1:
			ret = sensor_write_array(sd, sensor_saturation_pos1_regs, ARRAY_SIZE(sensor_saturation_pos1_regs));
			break;
		case 2:
			ret = sensor_write_array(sd, sensor_saturation_pos2_regs, ARRAY_SIZE(sensor_saturation_pos2_regs));
			break;	
		case 3:
			ret = sensor_write_array(sd, sensor_saturation_pos3_regs, ARRAY_SIZE(sensor_saturation_pos3_regs));
			break;
		case 4:
			ret = sensor_write_array(sd, sensor_saturation_pos4_regs, ARRAY_SIZE(sensor_saturation_pos4_regs));
			break;
		default:
			return -EINVAL;
	}
	
	if (ret < 0) {
		csi_dev_err("sensor_write_array err at sensor_s_saturation!\n");
		return ret;
	}
	
	mdelay(60);

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
	
	//mdelay(100);

	return 0;
}

enum v4l2_whiteblance{
    V4L2_WB_AUTO = 0,
    V4L2_WB_CLOUD,
    V4L2_WB_DAYLIGHT,
    V4L2_WB_INCANDESCENCE,
    V4L2_WB_FLUORESCENT,
    V4L2_WB_TUNGSTEN,
};
    

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
	
	mdelay(10);

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
	
	mdelay(10);
	
	return 0;
}

static int gc0307_setting(struct gc0307_device *dev,int PROP_ID,int value )
{
	int ret=0;
	switch(PROP_ID)  {
	case V4L2_CID_DO_WHITE_BALANCE:
		if(gc0307_qctrl[0].default_value!=value){
			gc0307_qctrl[0].default_value=value;
			sensor_s_wb(&dev->sd, (enum v4l2_whiteblance) value);	
			printk(KERN_INFO " set camera  white_balance=%d. \n ",value);
		}
		break;
	case V4L2_CID_EXPOSURE:
		if(gc0307_qctrl[1].default_value!=value){
			gc0307_qctrl[1].default_value=value;
			sensor_s_exp(&dev->sd, value);
			printk(KERN_INFO " set camera  exposure=%d. \n ",value);
		}
		break;
	case V4L2_CID_COLORFX:
		if(gc0307_qctrl[2].default_value!=value){
			gc0307_qctrl[2].default_value=value;
			sensor_s_colorfx(&dev->sd, (enum v4l2_colorfx)value);
			printk(KERN_INFO " set camera  effect=%d. \n ",value);
		}
		break;
    case V4L2_CID_BRIGHTNESS:
		if(gc0307_qctrl[3].default_value!=value){
			gc0307_qctrl[3].default_value=value;
			sensor_s_brightness(&dev->sd, value);
			printk(KERN_INFO " set camera  brightness=%d. \n ",value);
		}
		break;
    case V4L2_CID_CONTRAST:
		if(gc0307_qctrl[4].default_value!=value){
			gc0307_qctrl[4].default_value=value;
			sensor_s_contrast(&dev->sd, value);
			printk(KERN_INFO " set camera  brightness=%d. \n ",value);
		}
		break;
    case V4L2_CID_SATURATION:
		if(gc0307_qctrl[5].default_value!=value){
			gc0307_qctrl[5].default_value=value;
			sensor_s_saturation(&dev->sd, value);
			printk(KERN_INFO " set camera  saturation=%d. \n ",value);
		}
		break;
	case V4L2_CID_HFLIP:
		if(gc0307_qctrl[6].default_value!=value){
			gc0307_qctrl[6].default_value=value;
			sensor_s_hflip(&dev->sd, value);
			printk(KERN_INFO " set camera  hflip=%d. \n ",value);
		}
		break;
	case V4L2_CID_VFLIP:
		if(gc0307_qctrl[7].default_value!=value){
			gc0307_qctrl[7].default_value=value;
			sensor_s_vflip(&dev->sd, value);
			printk(KERN_INFO " set camera  Vflip=%d. \n ",value);
		}
		break;
	default:
		ret=-1;
		break;
	}
	return ret;

}

static void power_down_gc0307(struct gc0307_device *dev)
{
    printk("gc0307 power down\n");
	return;
}

/* ------------------------------------------------------------------
	DMA and thread functions
   ------------------------------------------------------------------*/

#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void gc0307_fillbuff(struct gc0307_fh *fh, struct gc0307_buffer *buf)
{
	struct gc0307_device *dev = fh->dev;
	void *vbuf = videobuf_to_vmalloc(&buf->vb);
	dprintk(dev,1,"%s\n", __func__);
	if (!vbuf)
		return;
 /*  0x18221223 indicate the memory type is MAGIC_VMAL_MEM*/
    vm_fill_buffer(&buf->vb,fh->fmt->fourcc ,0x18221223,vbuf);
	buf->vb.state = VIDEOBUF_DONE;
}

static void gc0307_thread_tick(struct gc0307_fh *fh)
{
	struct gc0307_buffer *buf;
	struct gc0307_device *dev = fh->dev;
	struct gc0307_dmaqueue *dma_q = &dev->vidq;

	unsigned long flags = 0;

	dprintk(dev, 1, "Thread tick\n");

	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
		dprintk(dev, 1, "No active queue to serve\n");
		goto unlock;
	}

	buf = list_entry(dma_q->active.next,
			 struct gc0307_buffer, vb.queue);
    dprintk(dev, 1, "%s\n", __func__);
    dprintk(dev, 1, "list entry get buf is %x\n",(unsigned)buf);

	/* Nobody is waiting on this buffer, return */
	if (!waitqueue_active(&buf->vb.done))
		goto unlock;

	list_del(&buf->vb.queue);

	do_gettimeofday(&buf->vb.ts);

	/* Fill buffer */
	spin_unlock_irqrestore(&dev->slock, flags);
	gc0307_fillbuff(fh, buf);
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

static void gc0307_sleep(struct gc0307_fh *fh)
{
	struct gc0307_device *dev = fh->dev;
	struct gc0307_dmaqueue *dma_q = &dev->vidq;

	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
		(unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
		goto stop_task;

	/* Calculate time to wake up */
	//timeout = msecs_to_jiffies(frames_to_ms(1));

	gc0307_thread_tick(fh);

	schedule_timeout_interruptible(2);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int gc0307_thread(void *data)
{
	struct gc0307_fh  *fh = data;
	struct gc0307_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (;;) {
		gc0307_sleep(fh);

		if (kthread_should_stop())
			break;
	}
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int gc0307_start_thread(struct gc0307_fh *fh)
{
	struct gc0307_device *dev = fh->dev;
	struct gc0307_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(gc0307_thread, fh, "gc0307");

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}
	/* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void gc0307_stop_thread(struct gc0307_dmaqueue  *dma_q)
{
	struct gc0307_device *dev = container_of(dma_q, struct gc0307_device, vidq);

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
	struct gc0307_fh  *fh = vq->priv_data;
	struct gc0307_device *dev  = fh->dev;
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

static void free_buffer(struct videobuf_queue *vq, struct gc0307_buffer *buf)
{
	struct gc0307_fh  *fh = vq->priv_data;
	struct gc0307_device *dev  = fh->dev;

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
	struct gc0307_fh     *fh  = vq->priv_data;
	struct gc0307_device    *dev = fh->dev;
	struct gc0307_buffer *buf = container_of(vb, struct gc0307_buffer, vb);
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
	struct gc0307_buffer    *buf  = container_of(vb, struct gc0307_buffer, vb);
	struct gc0307_fh        *fh   = vq->priv_data;
	struct gc0307_device       *dev  = fh->dev;
	struct gc0307_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct gc0307_buffer   *buf  = container_of(vb, struct gc0307_buffer, vb);
	struct gc0307_fh       *fh   = vq->priv_data;
	struct gc0307_device      *dev  = (struct gc0307_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops gc0307_video_qops = {
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
	struct gc0307_fh  *fh  = priv;
	struct gc0307_device *dev = fh->dev;

	strcpy(cap->driver, "gc0307");
	strcpy(cap->card, "gc0307");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = GC0307_CAMERA_VERSION;
	cap->capabilities =	V4L2_CAP_VIDEO_CAPTURE |
				V4L2_CAP_STREAMING     |
				V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	struct gc0307_fmt *fmt;

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
	struct gc0307_fh *fh = priv;

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
	struct gc0307_fh  *fh  = priv;
	struct gc0307_device *dev = fh->dev;
	struct gc0307_fmt *fmt;
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
	struct gc0307_fh *fh = priv;
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
	struct gc0307_fh  *fh = priv;

	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc0307_fh  *fh = priv;

	return (videobuf_querybuf(&fh->vb_vidq, p));
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc0307_fh *fh = priv;

	return (videobuf_qbuf(&fh->vb_vidq, p));
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc0307_fh  *fh = priv;

	return (videobuf_dqbuf(&fh->vb_vidq, p,
				file->f_flags & O_NONBLOCK));
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct gc0307_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct gc0307_fh  *fh = priv;
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
	struct gc0307_fh  *fh = priv;

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
	struct gc0307_fmt *fmt = NULL;
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
		if (fsize->index >= ARRAY_SIZE(gc0307_prev_resolution))
			return -EINVAL;
		frmsize = &gc0307_prev_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	else if(fmt->fourcc == V4L2_PIX_FMT_RGB24){
		if (fsize->index >= ARRAY_SIZE(gc0307_pic_resolution))
			return -EINVAL;
		frmsize = &gc0307_pic_resolution[fsize->index];
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
	struct gc0307_fh *fh = priv;
	struct gc0307_device *dev = fh->dev;

	*i = dev->input;

	return (0);
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct gc0307_fh *fh = priv;
	struct gc0307_device *dev = fh->dev;

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

	for (i = 0; i < ARRAY_SIZE(gc0307_qctrl); i++)
		if (qc->id && qc->id == gc0307_qctrl[i].id) {
			memcpy(qc, &(gc0307_qctrl[i]),
				sizeof(*qc));
			return (0);
		}

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct gc0307_fh *fh = priv;
	struct gc0307_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(gc0307_qctrl); i++)
		if (ctrl->id == gc0307_qctrl[i].id) {
			ctrl->value = dev->qctl_regs[i];
			return 0;
		}

	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct gc0307_fh *fh = priv;
	struct gc0307_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(gc0307_qctrl); i++)
		if (ctrl->id == gc0307_qctrl[i].id) {
			if (ctrl->value < gc0307_qctrl[i].minimum ||
			    ctrl->value > gc0307_qctrl[i].maximum ||
			    gc0307_setting(dev,ctrl->id,ctrl->value)<0) {
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

static int gc0307_open(struct file *file)
{
	struct gc0307_device *dev = video_drvdata(file);
	struct gc0307_fh *fh = NULL;
	int retval = 0;
	gc0307_have_open=1;
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 1);
#endif	
	if(dev->platform_dev_data.device_init) {
		dev->platform_dev_data.device_init();
		printk("+++found a init function, and run it..\n");
	}
	GC0307_init_regs(dev);
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

	videobuf_queue_vmalloc_init(&fh->vb_vidq, &gc0307_video_qops,
			NULL, &dev->slock, fh->type, V4L2_FIELD_INTERLACED,
			sizeof(struct gc0307_buffer), fh,NULL);

	gc0307_start_thread(fh);

	return 0;
}

static ssize_t
gc0307_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct gc0307_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
					file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int
gc0307_poll(struct file *file, struct poll_table_struct *wait)
{
	struct gc0307_fh        *fh = file->private_data;
	struct gc0307_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int gc0307_close(struct file *file)
{
	struct gc0307_fh         *fh = file->private_data;
	struct gc0307_device *dev       = fh->dev;
	struct gc0307_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);
	gc0307_have_open=0;

	gc0307_stop_thread(vidq);
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

	if(dev->platform_dev_data.device_uninit) {
		dev->platform_dev_data.device_uninit();
		printk("+++found a uninit function, and run it..\n");
	}
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 0);
#endif		
	return 0;
}

static int gc0307_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct gc0307_fh  *fh = file->private_data;
	struct gc0307_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations gc0307_fops = {
	.owner		= THIS_MODULE,
	.open           = gc0307_open,
	.release        = gc0307_close,
	.read           = gc0307_read,
	.poll		= gc0307_poll,
	.ioctl          = video_ioctl2, /* V4L2 ioctl handler */
	.mmap           = gc0307_mmap,
};


static const struct v4l2_ioctl_ops gc0307_ioctl_ops = {
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

static struct video_device gc0307_template = {
	.name		= "gc0307_v4l",
	.fops           = &gc0307_fops,
	.ioctl_ops 	= &gc0307_ioctl_ops,
	.release	= video_device_release,

	.tvnorms              = V4L2_STD_525_60,
	.current_norm         = V4L2_STD_NTSC_M,
};

static int gc0307_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SENSOR, 0);
}


/* ----------------------------------------------------------------------- */
static struct i2c_client *this_client;

static const struct v4l2_subdev_core_ops gc0307_core_ops = {
	.g_chip_ident = gc0307_g_chip_ident,
};

static const struct v4l2_subdev_ops gc0307_ops = {
	.core = &gc0307_core_ops,
	//.video = &gc0307_video_ops,
};

static int gc0307_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	aml_plat_cam_data_t* plat_dat;
	int err;
	struct gc0307_device *t;
	struct v4l2_subdev *sd;
	printk("gc0307 probe !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return -ENOMEM;
	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &gc0307_ops);
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
	memcpy(t->vdev, &gc0307_template, sizeof(*t->vdev));

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
			//power_down_gc0307(t);
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

	return 0;
}

static int gc0307_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0307_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
	kfree(t);
	return 0;
}

static int gc0307_suspend(struct i2c_client *client, pm_message_t state)
{
    
}

static int gc0307_resume(struct i2c_client *client)
{
    
}

static const struct i2c_device_id gc0307_id[] = {
	{ "gc0307_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc0307_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "gc0307",
	.probe = gc0307_probe,
	.remove = gc0307_remove,
	.suspend = gc0307_suspend,
	.resume = gc0307_resume,
	.id_table = gc0307_id,
};

