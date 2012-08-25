/*
 * Amlogic BOOT MONITOR SYSFS(M6)
 *
 * Copyright (C) 2010 Amlogic Corporation
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
 
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>

#include <mach/am_regs.h>
#include <linux/reboot.h>
#include <linux/fs.h>

static struct platform_device *pdev;

/* Sysfs Files */

static struct timer_list boot_timer;
void boot_timer_func(unsigned long arg);

#define BOOT_TIMER_INTERVAL     (HZ*5)

static void disable_watchdog(void)
{
    printk(KERN_INFO "** disable watchdog\n");
    aml_write_reg32(P_WATCHDOG_RESET, 0);
    aml_clr_reg32_mask(P_WATCHDOG_TC,(1 << WATCHDOG_ENABLE_BIT));
}
static void enable_watchdog(void)
{
	printk(KERN_INFO "** enable watchdog\n");
    aml_write_reg32(P_WATCHDOG_RESET, 0);
    aml_write_reg32(P_WATCHDOG_TC, 1 << WATCHDOG_ENABLE_BIT | 0x1FFFFF);//about 20sec
}
static void reset_watchdog(void)
{
	printk(KERN_INFO "** reset watchdog\n");
    aml_write_reg32(P_WATCHDOG_RESET, 0);	
}

void boot_timer_func(unsigned long arg)
{
    printk("boot_timer_func: <%s>\n", "timer expires, reset watchdog"); 
    reset_watchdog();
    mod_timer(&boot_timer, jiffies + BOOT_TIMER_INTERVAL); 
}

static ssize_t boot_timer_set(struct class *cla,struct class_attribute *attr,const char *buf, size_t count)
{    
    bool enable = (strncmp(buf, "1", 1) == 0);   
       
    if (enable) {
        printk("boot_timer_set: <%s>\n", "start!");
        init_timer(&boot_timer);
        boot_timer.data = (ulong) & boot_timer;
        boot_timer.function = boot_timer_func;
        boot_timer.expires = jiffies + BOOT_TIMER_INTERVAL;
        add_timer(&boot_timer); 
        
        enable_watchdog();
        aml_write_reg32(P_AO_RTI_STATUS_REG1, MESON_NORMAL_BOOT);
        
    } else {
        printk("boot_timer_set: <%s>\n", "stop!");        
        del_timer_sync(&boot_timer);
        disable_watchdog();
    }
    return count;
}
    
static struct class_attribute boot_monitor_class_attrs[] = {
    __ATTR(boot_timer,
           S_IRUGO | S_IWUSR,
           NULL,
           boot_timer_set),
    __ATTR_NULL
};

static struct class boot_monitor_class = {    
	.name = "boot_monitor",    
	.class_attrs = boot_monitor_class_attrs,

};

/* Device model stuff */
static int boot_monitor_probe(struct platform_device *dev)
{
	printk(KERN_INFO "boot_monitor: device successfully initialized.\n");
	return 0;
}

static struct platform_driver boot_monitor_driver = {
	.probe = boot_monitor_probe,
	.driver	= {
		.name = "boot_monitor",
		.owner = THIS_MODULE,
	},
};

/* Module stuff */
static int __init boot_monitor_init(void)
{
	int ret;

	ret = platform_driver_register(&boot_monitor_driver);
	if (ret)
		goto out;

	pdev = platform_device_register_simple("boot_monitor", -1, NULL, 0);
	if (IS_ERR(pdev)) {
		ret = PTR_ERR(pdev);
		goto out_driver;
	}

	ret = class_register(&boot_monitor_class);
	if (ret)
		goto out_device;

	printk(KERN_INFO "boot_monitor: driver successfully loaded.\n");
	return 0;

out_device:
	platform_device_unregister(pdev);
out_driver:
	platform_driver_unregister(&boot_monitor_driver);
out:
	printk(KERN_WARNING "boot_monitor: driver init failed (ret=%d)!\n", ret);
	return ret;
}

static void __exit boot_monitor_exit(void)
{
	class_unregister(&boot_monitor_class);
	platform_device_unregister(pdev);
	platform_driver_unregister(&boot_monitor_driver);	

	printk(KERN_INFO "boot_monitor: driver unloaded.\n");
}

module_init(boot_monitor_init);
module_exit(boot_monitor_exit);

MODULE_DESCRIPTION("Amlogic BOOT MONITOR SYSFS");
MODULE_LICENSE("GPL");
