/*
 * Common power driver for Amlogic Devices with one or two external
 * power supplies (AC/USB) connected to main and backup batteries,
 * and optional builtin charger.
 *
 * Copyright © 2011-4-27 alex.deng <alex.deng@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/usb/otg.h>

#include <linux/rfkill.h>

#include <linux/aml_modem.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend modem_early_suspend;
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static int aml_modem_suspend(struct early_suspend *handler);
static int aml_modem_resume(struct early_suspend *handler);
#else

#define aml_modem_suspend NULL
#define aml_modem_resume NULL

#endif

#define POWER_OFF (0)
#define POWER_ON  (1)

static struct device *dev;
static struct aml_modem_pdata *g_pData;
static int g_modemPowerMode = 0x0001 ;  // bit 1 controlls usb 0,bit 2 controlls usb 1.
static int g_modemPowerState = POWER_OFF ;
static int g_modemEnableFlag = 0 ;

#ifdef CONFIG_HAS_EARLYSUSPEND
static int aml_modem_suspend(struct early_suspend *handler)
{
    //printk("aml_modem_suspend !!!!!!!!!!!!!###################\n");
    int ret = -1;

    #ifdef  CONFIG_AMLOGIC_MODEM_PM
    if(handler && 2<=g_modemPowerMode ) 
    {
        printk("do aml_modem_suspend \n");
        g_modemPowerState = POWER_OFF;
        g_pData->power_off();
        g_pData->disable();
        ret = 0;
    }
    #endif
    return 0 ;
}

static int aml_modem_resume(struct early_suspend *handler)
{
    //printk("aml_modem_resume !!!!!!!!!!!!!###################\n");
    int ret = -1;

    #ifdef  CONFIG_AMLOGIC_MODEM_PM
    if(handler && 2<=g_modemPowerMode )
    {
        printk("do aml_modem_resume \n");
        g_modemPowerState = POWER_ON;
        g_pData->power_on();
        g_pData->enable();
        // need to reset ??
        g_pData->reset();
        ret = 0 ;
    }
    #endif
    return 0 ;
}
#endif

static int modem_set_block(void *data, bool blocked)
{
    pr_info("modem_RADIO going: %s\n", blocked ? "off" : "on");

    if( NULL == data)
    {
        return -1 ;
    }

    struct aml_modem_pdata *pData = (struct aml_modem_pdata *)data;

    if (!blocked)
    {
        pr_info("amlogic modem: going ON\n");
        if ( pData->power_on && pData->enable && pData->reset) 
        {
            pData->power_on();
            pData->enable();
            //pData->reset();
        }
    } 
    else 
    {
        pr_info("amlogic modem: going OFF\n");
        if (NULL != pData->power_off) 
        {
            pData->power_off();
        }
    }
    return 0;
}

static const struct rfkill_ops modem_rfkill_ops = {
	.set_block = modem_set_block,
};

int GetModemPowerMode(void)
{
    return g_modemPowerMode ;
}
EXPORT_SYMBOL(GetModemPowerMode);

static ssize_t get_modemPowerMode(struct class *cla, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", g_modemPowerMode ); 
}

static ssize_t set_modemPowerMode(struct class *cla, struct class_attribute *attr, char *buf, size_t count)
{
    if(!strlen(buf)){
        printk("%s parameter is required!\n",__FUNCTION__);
        return 0;
    }
    g_modemPowerMode = (int)(buf[0]-'0');
    if(g_modemPowerMode <0 || g_modemPowerMode>3){
        printk("%s parameter is invalid! %s \n",__FUNCTION__);
        g_modemPowerMode = 3 ;
    }
        
    return count;
}

static ssize_t get_modemEnableFlag(struct class *cla, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", g_modemEnableFlag ); 
}

static ssize_t set_modemEnableFlag(struct class *cla, struct class_attribute *attr, char *buf, size_t count)
{
    if(!strlen(buf)){
        printk("%s parameter is required!\n",__FUNCTION__);
        return 0;
    }
    g_modemEnableFlag = (int)(buf[0]-'0');
    g_modemEnableFlag = !!g_modemEnableFlag ;
    if(g_modemEnableFlag){
        g_pData->enable();
    }
    else{
        g_pData->disable() ;
    }
        
    return count;
}


static ssize_t get_modemPowerState(struct class *cla, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", g_modemPowerState ); 
}

static ssize_t set_modemPowerState(struct class *cla, struct class_attribute *attr, char *buf, size_t count)
{
    int old_state = g_modemPowerState ;
    if(!strlen(buf)){
        printk("%s parameter is required!\n",__FUNCTION__);
        return 0;
    }
    g_modemPowerState = (int)(buf[0]-'0');
    g_modemPowerState = !!g_modemPowerState ;

    if(old_state == g_modemPowerState){
        printk("modemPowerState is already %s now \n", g_modemPowerState?"ON":"OFF");
        return count ;
    }
    else
        printk("set_modemPowerState %s \n", g_modemPowerState?"ON":"OFF");

    if(POWER_ON == g_modemPowerState){
        g_pData->power_on();
        g_pData->enable();
        //g_pData->reset();
    }
    else{
        g_pData->power_off();
        g_pData->disable();
    }
        
    return count;
}


static ssize_t set_modemReset(struct class *cla, struct class_attribute *attr, char *buf, size_t count)
{
    int ret = 0 ;
    if(!strlen(buf)){
        printk("%s parameter is required!\n",__FUNCTION__);
        return 0;
    }
    ret = (int)(buf[0]-'0');
    ret = !!ret ;
    if(ret){
        printk("set_modemReset \n");
        g_pData->reset();
    }

    return count ;
}

static struct class_attribute AmlModem_class_attrs[] = {
    __ATTR(mode,S_IRUGO|S_IWUGO,get_modemPowerMode,set_modemPowerMode),
    __ATTR(enable,S_IRUGO|S_IWUGO,get_modemEnableFlag,set_modemEnableFlag),
    __ATTR(state,S_IRUGO|S_IWUGO,get_modemPowerState,set_modemPowerState),
    __ATTR(reset,S_IWUGO,NULL,set_modemReset),
    __ATTR_NULL
};
static struct class AmlModem_class = {
    .name = "aml_modem",
    .class_attrs = AmlModem_class_attrs,
};

static int __init aml_modem_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct rfkill *rfk = NULL;

    printk(" aml_modem_probe  \n");

    dev = &pdev->dev;

    if (pdev->id != -1) {
    	dev_err(dev, "it's meaningless to register several "
    		"pda_powers; use id = -1\n");
    	ret = -EINVAL;
    	goto exit;
    }

    g_pData = pdev->dev.platform_data;

#if 0
    rfk = rfkill_alloc("modem-dev", &pdev->dev, RFKILL_TYPE_WWAN,
			&modem_rfkill_ops, g_pData);
						   
    if (!rfk)
    {
        printk("aml_modem_probe, rfk alloc fail\n");
        ret = -ENOMEM;
        goto exit;
    }

    ret = rfkill_register(rfk);
    if (ret)
    {
        printk("aml_modem_probe, rfkill_register fail\n");
        goto err_rfkill;
    }

    platform_set_drvdata(pdev, rfk);
#else
    if(g_pData){
        g_modemPowerState = POWER_ON ;
        g_pData->power_on();
        g_pData->enable();
        //g_pData->reset();
    }
#endif

    #ifdef CONFIG_HAS_EARLYSUSPEND
        modem_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
        modem_early_suspend.suspend = aml_modem_suspend;
        modem_early_suspend.resume = aml_modem_resume;
        modem_early_suspend.param = g_pData;
        register_early_suspend(&modem_early_suspend);
    #endif
    
    return 0 ;
    
err_rfkill:
	rfkill_destroy(rfk);

exit:;	
    return ret;
}

static int aml_modem_remove(struct platform_device *pdev)
{
    int ret = 0;

    printk(" aml_modem_remove \n");

    dev = &pdev->dev;

    if (pdev->id != -1) {
        dev_err(dev, "it's meaningless to register several "
        	"pda_powers; use id = -1\n");
        ret = -EINVAL;
        goto exit;
    }

    g_pData = pdev->dev.platform_data;

    if (g_pData->power_off) {
        ret = g_pData->power_off();

    }

    struct rfkill *rfk = platform_get_drvdata(pdev);

    platform_set_drvdata(pdev, NULL);

    if (rfk) {
    	rfkill_unregister(rfk);
    	rfkill_destroy(rfk);
    }
    rfk = NULL;

    #ifdef CONFIG_HAS_EARLYSUSPEND
        unregister_early_suspend(&modem_early_suspend);
    #endif
    
exit:;	
	return ret;
}


MODULE_ALIAS("platform:aml-modem");

static struct platform_driver aml_modem_pdrv = {
    .driver = {
        .name = "aml-modem",
        .owner = THIS_MODULE,	
    },
    .probe = aml_modem_probe,
    .remove = aml_modem_remove,
    .suspend = aml_modem_suspend,
    .resume = aml_modem_resume,
};

static int __init aml_modem_init(void)
{
	printk("amlogic 3G supply init \n");
       class_register(&AmlModem_class);
	return platform_driver_register(&aml_modem_pdrv);
}

static void __exit aml_modem_exit(void)
{
      platform_driver_unregister(&aml_modem_pdrv);
      class_unregister(&AmlModem_class);
}

module_init(aml_modem_init);
module_exit(aml_modem_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Deng");
