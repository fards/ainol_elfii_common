/*
 * TVHDMI char device driver.
 *
 * Copyright (c) 2010 Bo Yang <bo.yang@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/mutex.h>

/* Amlogic headers */
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>
#include <linux/tvin/tvin.h>

#include <linux/amports/canvas.h>
#include <mach/am_regs.h>
#include <linux/amports/vframe.h>
/* Local include */
#include "tvin_frontend.h"
#include "hdmirx_cec.h"
#include "hdmirx.h"           /* For user used */

#define TVHDMI_NAME               "hdmirx"
#define TVHDMI_DRIVER_NAME        "hdmirx"
#define TVHDMI_MODULE_NAME        "hdmirx"
#define TVHDMI_DEVICE_NAME        "hdmirx"
#define TVHDMI_CLASS_NAME         "hdmirx"

#define TVHDMI_COUNT              1

#define TIMER_STATE_CHECK              (1*HZ/HDMI_STATE_CHECK_FREQ)            //50ms timer for hdmirx main loop (HDMI_STATE_CHECK_FREQ is 20)

static unsigned char init_flag=0;
#define INIT_FLAG_NOT_LOAD 0x80

static dev_t                     hdmirx_devno;
static struct class              *hdmirx_clsp;


typedef struct hdmirx_dev_s {
    int                         index;
    dev_t                       devt;
    struct cdev                 cdev;
    struct device               *dev;

    struct tvin_parm_s          param;
    struct timer_list           timer;
    tvin_frontend_t             frontend;
} hdmirx_dev_t;


static struct hdmirx_dev_s *hdmirx_devp[TVHDMI_COUNT];

void hdmirx_timer_handler(unsigned long arg)
{
    struct hdmirx_dev_s *devp = (struct hdmirx_dev_s *)arg;

    hdmirx_hw_monitor();

    devp->timer.expires = jiffies + TIMER_STATE_CHECK;
    add_timer(&devp->timer);
}

int hdmirx_dec_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
    if ((port >= TVIN_PORT_HDMI0) && (port <= TVIN_PORT_HDMI7)) {
        pr_info("%s port:%x supported \n", __FUNCTION__, port);
        return 0;
    }
    else
    {
        pr_info("%s port:%x not supported \n", __FUNCTION__, port);
        return -1;
    }
}

void hdmirx_dec_open(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
    struct hdmirx_dev_s *devp = container_of(fe, struct hdmirx_dev_s, frontend);
    devp->param.port = port;
    hdmirx_hw_enable();
    hdmirx_hw_init(port);

    /* timer */
    init_timer(&devp->timer);
    devp->timer.data = (ulong)devp;
    devp->timer.function = hdmirx_timer_handler;
    devp->timer.expires = jiffies + TIMER_STATE_CHECK;
    add_timer(&devp->timer);
    pr_info("%s port:%x ok\n",__FUNCTION__, port);
}

void hdmirx_dec_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt)
{
    struct hdmirx_dev_s *devp = container_of(fe, struct hdmirx_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->param;

    parm->info.fmt = fmt;
    parm->info.status = TVIN_SIG_STATUS_STABLE;
    pr_info("%s fmt:%d ok\n",__FUNCTION__, fmt);
}

void hdmirx_dec_stop(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
    struct hdmirx_dev_s *devp = container_of(fe, struct hdmirx_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->param;

    parm->info.fmt = TVIN_SIG_FMT_NULL;
    parm->info.status = TVIN_SIG_STATUS_NULL;
    //hdmirx_reset(); /*remove it to fix bug of sony BDP-S300, audio status channel is 0 after 2D<->3D, the right value is 48K, this problem can be recovered after change port register*/
    pr_info("%s ok\n",__FUNCTION__);
}

void hdmirx_dec_close(struct tvin_frontend_s *fe)
{
    struct hdmirx_dev_s *devp = container_of(fe, struct hdmirx_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->param;

    del_timer_sync(&devp->timer);
    hdmirx_hw_uninit();
    hdmirx_hw_disable(0);
    parm->info.fmt = TVIN_SIG_FMT_NULL;
    parm->info.status = TVIN_SIG_STATUS_NULL;
    pr_info("%s ok\n",__FUNCTION__);
}

// interrupt handler
int hdmirx_dec_isr(struct tvin_frontend_s *fe, unsigned int hcnt64)
{
    struct hdmirx_dev_s *devp = container_of(fe, struct hdmirx_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->param;

    /* if there is any error or overflow, do some reset, then rerurn -1;*/
    if ((parm->info.status != TVIN_SIG_STATUS_STABLE) ||
        (parm->info.fmt == TVIN_SIG_FMT_NULL)) {
        return -1;
    }

    return 0;
}

static struct tvin_decoder_ops_s hdmirx_dec_ops = {
    .support    = hdmirx_dec_support,
    .open       = hdmirx_dec_open,
    .start      = hdmirx_dec_start,
    .stop       = hdmirx_dec_stop,
    .close      = hdmirx_dec_close,
    .decode_isr = hdmirx_dec_isr,
};

bool hdmirx_is_nosig(struct tvin_frontend_s *fe)
{
    bool ret = 0;
    /* Get the per-device structure that contains this frontend */
    //struct hdmirx_dev_s *devp = container_of(fe, struct hdmirx_dev_s, frontend);
    ret = hdmirx_hw_is_nosig();

    return ret;
}

bool hdmirx_fmt_chg(struct tvin_frontend_s *fe)
{
    enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
    /* Get the per-device structure that contains this frontend */
    struct hdmirx_dev_s *devp = container_of(fe, struct hdmirx_dev_s, frontend);
    struct tvin_parm_s *parm = &devp->param;

    if(!hdmirx_hw_pll_lock()){
        return true;
    }
    else{
        fmt = hdmirx_hw_get_fmt();
        if(fmt != parm->info.fmt)
        {
            pr_info("hdmirx %d --> %d\n", parm->info.fmt, fmt);
            parm->info.fmt = fmt;
            return true;
        }
        else
            return false;
    }
}

bool hdmirx_pll_lock(struct tvin_frontend_s *fe)
{
    bool ret = true;
    /* Get the per-device structure that contains this frontend */
    //struct hdmirx_dev_s *devp = container_of(fe, struct hdmirx_dev_s, frontend);
    ret = hdmirx_hw_pll_lock();
    return (ret);
}

enum tvin_sig_fmt_e hdmirx_get_fmt(struct tvin_frontend_s *fe)
{
    enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
    /* Get the per-device structure that contains this frontend */
    //struct hdmirx_dev_s *devp = container_of(fe, struct hdmirx_dev_s, frontend);

    fmt = hdmirx_hw_get_fmt();

    return fmt;
}

enum tvin_color_fmt_e hdmirx_get_color_fmt(struct tvin_frontend_s *fe)
{
    enum tvin_color_fmt_e color_fmt = TVIN_RGB444;
    /* Get the per-device structure that contains this frontend */
    //struct hdmirx_dev_s *devp = container_of(fe, struct hdmirx_dev_s, frontend);
    switch(hdmirx_hw_get_color_fmt()){
        case 1:
            color_fmt = TVIN_YUV444;
            break;
        case 3:
            color_fmt = TVIN_YUV422;
            break;
        case 0:
        default:
            color_fmt = TVIN_RGB444;
            break;
    }

    return color_fmt;
}

void hdmirx_get_sig_propery(struct tvin_frontend_s *fe, struct tvin_sig_property_s *prop)
{
    //struct hdmirx_dev_s *devp = container_of(fe, struct hdmirx_dev_s, frontend);
    //struct tvin_parm_s *parm = &devp->param;
    unsigned char _3d_structure, _3d_ext_data;
    switch(hdmirx_hw_get_color_fmt()){
        case 1:
            prop->color_format = TVIN_YUV444;
            break;
        case 3:
            prop->color_format = TVIN_YUV422;
            break;
        case 0:
        default:
            prop->color_format = TVIN_RGB444;
            break;
    }

    prop->trans_fmt = TVIN_TFMT_2D;
    if(hdmirx_hw_get_3d_structure(&_3d_structure, &_3d_ext_data)>=0){
        if(_3d_structure==0x0){ // frame packing
            prop->trans_fmt = TVIN_TFMT_3D_FP; // Primary: Frame Packing
        }
        else if(_3d_structure==0x1){   //field alternative
            prop->trans_fmt = TVIN_TFMT_3D_FA;   // Secondary: Field Alternative
        }
        else if(_3d_structure==0x2){   //line alternative
            prop->trans_fmt = TVIN_TFMT_3D_LA;   // Secondary: Line Alternative
        }
        else if(_3d_structure==0x3){   // side-by-side full
            prop->trans_fmt = TVIN_TFMT_3D_LRF;  // Secondary: Side-by-Side(Full)
        }
        else if(_3d_structure==0x4){   // L + depth
            prop->trans_fmt = TVIN_TFMT_3D_LD;   // Secondary: L+depth
        }
        else if(_3d_structure==0x5){   // L + depth + graphics + graphics-depth
            prop->trans_fmt = TVIN_TFMT_3D_LDGD; // Secondary: L+depth+Graphics+Graphics-depth
        }
        else if(_3d_structure==0x6){  //top-and-bot
            prop->trans_fmt = TVIN_TFMT_3D_TB; // Primary: Top-and-Bottom
        }
        else if(_3d_structure==0x8){ // Side-by-Side half
            switch(_3d_ext_data){
                case 0x5:
                    prop->trans_fmt = TVIN_TFMT_3D_LRH_OLER;  // Odd/Left picture, Even/Right picture
                    break;
                case 0x6:
                    prop->trans_fmt = TVIN_TFMT_3D_LRH_ELOR;  // Even/Left picture, Odd/Right picture
                    break;
                case 0x7:
                    prop->trans_fmt = TVIN_TFMT_3D_LRH_ELER;  // Even/Left picture, Even/Right picture
                    break;
                case 0x4:
                default:
                    prop->trans_fmt = TVIN_TFMT_3D_LRH_OLOR;  // Odd/Left picture, Odd/Right picture
                    break;
            }
        }
    }

    prop->pixel_repeat = hdmirx_hw_get_pixel_repeat(); /* 1: no repeat; 2: repeat 1 times; 3: repeat two; ... */
}

bool hdmirx_check_frame_skip(struct tvin_frontend_s *fe)
{
    return hdmirx_hw_check_frame_skip();
}

static struct tvin_state_machine_ops_s hdmirx_sm_ops = {
    .nosig            = hdmirx_is_nosig,
    .fmt_changed      = hdmirx_fmt_chg,
    .get_fmt          = hdmirx_get_fmt,
    .fmt_config       = NULL,
    .adc_cal          = NULL,
    .pll_lock         = hdmirx_pll_lock,
    .get_sig_propery  = hdmirx_get_sig_propery,
#ifdef TVAFE_SET_CVBS_MANUAL_FMT_POS
    .set_cvbs_fmt_pos = NULL,
#endif
    .vga_set_param    = NULL,
	.vga_get_param    = NULL,
	.check_frame_skip = hdmirx_check_frame_skip,
};



/*
hdmirx device driver
*/
static int hdmirx_open(struct inode *inode, struct file *file)
{
    hdmirx_dev_t *devp;

    /* Get the per-device structure that contains this cdev */
    devp = container_of(inode->i_cdev, hdmirx_dev_t, cdev);
    file->private_data = devp;

    return 0;
}

static int hdmirx_release(struct inode *inode, struct file *file)
{
    //hdmirx_dev_t *devp = file->private_data;

    file->private_data = NULL;

    return 0;
}


static int hdmirx_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    return ret;
}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations hdmirx_fops = {
    .owner   = THIS_MODULE,         /* Owner */
    .open    = hdmirx_open,          /* Open method */
    .release = hdmirx_release,       /* Release method */
    .ioctl   = hdmirx_ioctl,         /* Ioctl method */
    //.fasync  = hdmirx_fasync,
    //.poll    = hdmirx_poll,                    // poll function
    /* ... */
};

/* attr */
static unsigned char* hdmirx_log_buf=NULL;
static unsigned int hdmirx_log_wr_pos=0;
static unsigned int hdmirx_log_rd_pos=0;
static unsigned int hdmirx_log_buf_size=0;
static DEFINE_SPINLOCK(hdmirx_print_lock);
#define DEF_LOG_BUF_SIZE (1024*128)

#define PRINT_TEMP_BUF_SIZE 128

int hdmirx_print_buf(char* buf, int len)
{
    unsigned long flags;
    int pos;
    int hdmirx_log_rd_pos_;
    if(hdmirx_log_buf_size==0)
        return 0;

    spin_lock_irqsave(&hdmirx_print_lock, flags);
    hdmirx_log_rd_pos_=hdmirx_log_rd_pos;
    if(hdmirx_log_wr_pos>=hdmirx_log_rd_pos)
        hdmirx_log_rd_pos_+=hdmirx_log_buf_size;

    for(pos=0;pos<len && hdmirx_log_wr_pos<(hdmirx_log_rd_pos_-1);pos++,hdmirx_log_wr_pos++){
        if(hdmirx_log_wr_pos>=hdmirx_log_buf_size)
            hdmirx_log_buf[hdmirx_log_wr_pos-hdmirx_log_buf_size]=buf[pos];
        else
            hdmirx_log_buf[hdmirx_log_wr_pos]=buf[pos];
    }
    if(hdmirx_log_wr_pos>=hdmirx_log_buf_size)
        hdmirx_log_wr_pos-=hdmirx_log_buf_size;
    spin_unlock_irqrestore(&hdmirx_print_lock, flags);
    return pos;

}

int hdmirx_print(const char *fmt, ...)
{
    va_list args;
    int avail = PRINT_TEMP_BUF_SIZE;
    char buf[PRINT_TEMP_BUF_SIZE];
    int pos,len=0;

    if(hdmirx_log_flag&1){
        va_start(args, fmt);
        vprintk(fmt, args);
        va_end(args);
        return 0;
    }

    if(hdmirx_log_buf_size==0)
        return 0;

    //len += snprintf(buf+len, avail-len, "%d:",log_seq++);
    len += snprintf(buf+len, avail-len, "[%u] ", (unsigned int)jiffies);
    va_start(args, fmt);
    len += vsnprintf(buf+len, avail-len, fmt, args);
    va_end(args);

    if ((avail-len) <= 0) {
        buf[PRINT_TEMP_BUF_SIZE - 1] = '\0';
    }
    pos = hdmirx_print_buf(buf, len);
    //printk("hdmirx_print:%d %d\n", hdmirx_log_wr_pos, hdmirx_log_rd_pos);
	  return pos;
}

static int log_init(int bufsize)
{
    if(bufsize==0){
        if(hdmirx_log_buf){
            kfree(hdmirx_log_buf);
            hdmirx_log_buf=NULL;
            hdmirx_log_buf_size=0;
            hdmirx_log_rd_pos=0;
            hdmirx_log_wr_pos=0;
        }
    }
    if((bufsize>=1024)&&(hdmirx_log_buf==NULL)){
        hdmirx_log_buf_size=0;
        hdmirx_log_rd_pos=0;
        hdmirx_log_wr_pos=0;
        hdmirx_log_buf=kmalloc(bufsize, GFP_KERNEL);
        if(hdmirx_log_buf){
            hdmirx_log_buf_size=bufsize;
        }
    }
    return 0;
}

static ssize_t show_log(struct device * dev, struct device_attribute *attr, char * buf)
{
    unsigned long flags;
    ssize_t read_size=0;
    if(hdmirx_log_buf_size==0)
        return 0;
    //printk("show_log:%d %d\n", hdmirx_log_wr_pos, hdmirx_log_rd_pos);
    spin_lock_irqsave(&hdmirx_print_lock, flags);
    if(hdmirx_log_rd_pos<hdmirx_log_wr_pos){
        read_size = hdmirx_log_wr_pos-hdmirx_log_rd_pos;
    }
    else if(hdmirx_log_rd_pos>hdmirx_log_wr_pos){
        read_size = hdmirx_log_buf_size-hdmirx_log_rd_pos;
    }
    if(read_size>PAGE_SIZE)
        read_size=PAGE_SIZE;
    if(read_size>0)
        memcpy(buf, hdmirx_log_buf+hdmirx_log_rd_pos, read_size);

    hdmirx_log_rd_pos += read_size;
    if(hdmirx_log_rd_pos>=hdmirx_log_buf_size)
        hdmirx_log_rd_pos = 0;
    spin_unlock_irqrestore(&hdmirx_print_lock, flags);
    return read_size;
}

static ssize_t store_log(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    int tmp;
    unsigned long flags;
    if(strncmp(buf, "bufsize", 7)==0){
        tmp = simple_strtoul(buf+7,NULL,10);
        spin_lock_irqsave(&hdmirx_print_lock, flags);
        log_init(tmp);
        spin_unlock_irqrestore(&hdmirx_print_lock, flags);
        printk("hdmirx_store:set bufsize tmp %d %d\n",tmp, hdmirx_log_buf_size);
    }
    else{
        hdmirx_print(0, "%s", buf);
    }
    return 16;
}


static ssize_t hdmirx_debug_show(struct device * dev, struct device_attribute *attr, char * buf)
{
    return 0;
}

static ssize_t hdmirx_debug_store(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    hdmirx_debug(buf, count);
    return count;
}

static ssize_t hdmirx_edid_show(struct device * dev, struct device_attribute *attr, char * buf)
{
    return hdmirx_read_edid_buf(buf, PAGE_SIZE);
}

static ssize_t hdmirx_edid_store(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    hdmirx_fill_edid_buf(buf, count);
    return count;
}

static ssize_t show_reg(struct device * dev, struct device_attribute *attr, char * buf)
{
    return hdmirx_hw_dump_reg(buf, PAGE_SIZE);
}

static ssize_t cec_get_state(struct device * dev, struct device_attribute *attr, char * buf)
{
    ssize_t t = cec_usrcmd_get_global_info(buf);    
    return t;
}

static ssize_t cec_set_state(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    cec_usrcmd_set_dispatch(buf, count);
    return count;
}

static DEVICE_ATTR(debug, S_IWUSR | S_IWUGO | S_IWOTH , hdmirx_debug_show, hdmirx_debug_store);
static DEVICE_ATTR(edid, S_IWUSR | S_IRUGO, hdmirx_edid_show, hdmirx_edid_store);
static DEVICE_ATTR(log, S_IWUGO | S_IRUGO, show_log, store_log);
static DEVICE_ATTR(reg, S_IWUGO | S_IRUGO, show_reg, NULL);
static DEVICE_ATTR(cec, S_IWUGO | S_IRUGO, cec_get_state, cec_set_state);


static int hdmirx_probe(struct platform_device *pdev)
{
    int ret;
    int i;

    ret = alloc_chrdev_region(&hdmirx_devno, 0, TVHDMI_COUNT, TVHDMI_NAME);
	if (ret < 0) {
		pr_info("hdmirx: failed to allocate major number\n");
		return 0;
	}
    log_init(DEF_LOG_BUF_SIZE);

    hdmirx_clsp = class_create(THIS_MODULE, TVHDMI_NAME);
    if (IS_ERR(hdmirx_clsp))
    {
        pr_info(KERN_ERR "hdmirx: can't get hdmirx_clsp\n");
        unregister_chrdev_region(hdmirx_devno, TVHDMI_COUNT);
        return PTR_ERR(hdmirx_clsp);
	}

    for (i = 0; i < TVHDMI_COUNT; ++i)
    {
        /* allocate memory for the per-device structure */
        hdmirx_devp[i] = kmalloc(sizeof(struct hdmirx_dev_s), GFP_KERNEL);
        if (!hdmirx_devp[i])
        {
            pr_info("hdmirx: failed to allocate memory for hdmirx device\n");
            return -ENOMEM;
        }
        memset(hdmirx_devp[i], 0, sizeof(struct hdmirx_dev_s));
        hdmirx_devp[i]->index = i;

        /* connect the file operations with cdev */
        cdev_init(&hdmirx_devp[i]->cdev, &hdmirx_fops);
        hdmirx_devp[i]->cdev.owner = THIS_MODULE;
        /* connect the major/minor number to the cdev */
        ret = cdev_add(&hdmirx_devp[i]->cdev, (hdmirx_devno + i), 1);
	    if (ret) {
    		pr_info( "hdmirx: failed to add device\n");
            /* @todo do with error */
    		return ret;
    	}
        /* create /dev nodes */
        hdmirx_devp[i]->devt = MKDEV(MAJOR(hdmirx_devno), i);
        hdmirx_devp[i]->dev = device_create(hdmirx_clsp, &pdev->dev, hdmirx_devp[i]->devt,
                            hdmirx_devp[i], "hdmirx%d", i);
         if (IS_ERR(hdmirx_devp[i]->dev)) {
             pr_info("hdmirx: failed to create device node\n");
             cdev_del(&hdmirx_devp[i]->cdev);
			 kfree(hdmirx_devp[i]);
             return PTR_ERR(hdmirx_devp[i]->dev);;
        }
        device_create_file(hdmirx_devp[i]->dev, &dev_attr_debug);
        device_create_file(hdmirx_devp[i]->dev, &dev_attr_edid);
        device_create_file(hdmirx_devp[i]->dev, &dev_attr_log);
        device_create_file(hdmirx_devp[i]->dev, &dev_attr_reg);
        device_create_file(hdmirx_devp[i]->dev, &dev_attr_cec);

        /* frontend */
        tvin_frontend_init(&hdmirx_devp[i]->frontend, &hdmirx_dec_ops, &hdmirx_sm_ops, i);
        sprintf(hdmirx_devp[i]->frontend.name, "%s%d", TVHDMI_NAME, i);
        tvin_reg_frontend(&hdmirx_devp[i]->frontend);
    }

    hdmirx_hw_enable();
    pr_info("hdmirx: driver probe ok\n");
    return 0;
}

static int hdmirx_remove(struct platform_device *pdev)
{
    int i = 0;

    for (i = 0; i < TVHDMI_COUNT; ++i)
    {
        device_remove_file(hdmirx_devp[i]->dev, &dev_attr_debug);
        device_remove_file(hdmirx_devp[i]->dev, &dev_attr_edid);
        device_remove_file(hdmirx_devp[i]->dev, &dev_attr_log);
        device_remove_file(hdmirx_devp[i]->dev, &dev_attr_reg);
        device_remove_file(hdmirx_devp[i]->dev, &dev_attr_cec);
        tvin_unreg_frontend(&hdmirx_devp[i]->frontend);
        device_destroy(hdmirx_clsp, MKDEV(MAJOR(hdmirx_devno), i));
        cdev_del(&hdmirx_devp[i]->cdev);
        kfree(hdmirx_devp[i]);
    }
    class_destroy(hdmirx_clsp);
    unregister_chrdev_region(hdmirx_devno, TVHDMI_COUNT);

    pr_info("hdmirx: driver removed ok.\n");
    return 0;
}

#ifdef CONFIG_PM
static int hdmirx_suspend(struct platform_device *pdev,pm_message_t state)
{
    pr_info("hdmirx: hdmirx_suspend\n");
    hdmirx_hw_disable(1);
    pr_info("hdmirx: suspend module\n");
    return 0;
}

static int hdmirx_resume(struct platform_device *pdev)
{
    hdmirx_hw_enable();
    pr_info("hdmirx: resume module\n");
    return 0;
}
#endif

static struct platform_driver hdmirx_driver = {
    .probe      = hdmirx_probe,
    .remove     = hdmirx_remove,
#ifdef CONFIG_PM
    .suspend    = hdmirx_suspend,
    .resume     = hdmirx_resume,
#endif
    .driver     = {
        .name   = TVHDMI_DRIVER_NAME,
    }
};

static int __init hdmirx_init(void)
{
    int ret = 0;
    if(init_flag&INIT_FLAG_NOT_LOAD)
        return 0;

    ret = platform_driver_register(&hdmirx_driver);
    if (ret != 0) {
        pr_info("failed to register hdmirx module, error %d\n", ret);
        return -ENODEV;
    }
    pr_info("hdmirx: hdmirx_init.\n");
    return ret;
}

static void __exit hdmirx_exit(void)
{
    platform_driver_unregister(&hdmirx_driver);

    pr_info("hdmirx: hdmirx_exit.\n");
}


static char* next_token_ex(char* seperator, char *buf, unsigned size, unsigned offset, unsigned *token_len, unsigned *token_offset)
{ /* besides characters defined in seperator, '\"' are used as seperator; and any characters in '\"' will not act as seperator */
	char *pToken = NULL;
    char last_seperator = 0;
    char trans_char_flag = 0;
    if(buf){
    	for (;offset<size;offset++){
    	    int ii=0;
    	    char ch;
            if (buf[offset] == '\\'){
                trans_char_flag = 1;
                continue;
            }
    	    while(((ch=seperator[ii++])!=buf[offset])&&(ch)){
    	    }
    	    if (ch){
                if (!pToken){
    	            continue;
                }
    	        else {
                    if (last_seperator != '"'){
    	                *token_len = (unsigned)(buf + offset - pToken);
    	                *token_offset = offset;
    	                return pToken;
    	            }
    	        }
            }
    	    else if (!pToken)
            {
                if (trans_char_flag&&(buf[offset] == '"'))
                    last_seperator = buf[offset];
    	        pToken = &buf[offset];
            }
            else if ((trans_char_flag&&(buf[offset] == '"'))&&(last_seperator == '"')){
                *token_len = (unsigned)(buf + offset - pToken - 2);
                *token_offset = offset + 1;
                return pToken + 1;
            }
            trans_char_flag = 0;
        }
        if (pToken) {
            *token_len = (unsigned)(buf + offset - pToken);
            *token_offset = offset;
        }
    }
	return pToken;
}

#if 0
static  int __init hdmirx_boot_para_setup(char *s)
{
    char separator[]={' ',',',';',0x0};
    char *token;
    unsigned token_len, token_offset, offset=0;
    int size=strlen(s);
    do{
        token=next_token_ex(separator, s, size, offset, &token_len, &token_offset);
        if(token){
            if((token_len==3) && (strncmp(token, "off", token_len)==0)){
                init_flag|=INIT_FLAG_NOT_LOAD;
            }
        }
        offset=token_offset;
    }while(token);
    return 0;
}

__setup("hdmirx=",hdmirx_boot_para_setup);
#endif

module_init(hdmirx_init);
module_exit(hdmirx_exit);

MODULE_DESCRIPTION("AMLOGIC HDMIRX driver");
MODULE_LICENSE("GPL");

