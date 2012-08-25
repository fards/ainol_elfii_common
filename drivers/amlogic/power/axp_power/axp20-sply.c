/*
 * Battery charger driver for Dialog Semiconductor DA9030
 *
 * Copyright (C) 2008 Compulab, Ltd.
 *  Mike Rapoport <mike@compulab.co.il>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/input.h>
#include <mach/am_regs.h>
#include <mach/gpio.h>
//#include <linux/mfd/axp-mfd.h>
#include "axp-mfd.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

//#include "axp-cfg.h"
#include "axp-sply.h"
#include "axp-gpio.h"

#define DBG_PSY_MSG(format,args...)   if(axp_debug) printk(KERN_ERR "[AXP]"format,##args)

#define   TIMER5  15
#define   TIMER2  10
#define   TIMER4  10
#define   BATCAPCORRATE  5

#define ABS(x)				((x) >0 ? (x) : -(x) )

static int pmu_used2 = 0;
static int gpio_adp_hdle = 0;
static int Cap_Index2 = 0;
static int count = 0;
#ifdef CONFIG_HAS_EARLYSUSPEND
static int pmu_earlysuspend_chgcur = 0;
static struct early_suspend axp_early_suspend;
int early_suspend_flag = 0;
#endif

int ADC_Freq_Get(struct axp_charger *charger)
{	
	uint8_t  temp;
	int  rValue = 25;

	axp_read(charger->master, AXP20_ADC_CONTROL3,&temp);
	temp &= 0xc0;
	switch(temp >> 6)
	{
		case 0:
			rValue = 25;
			break;
		case 1:
			rValue = 50;
			break;
		case 2:
			rValue = 100;
			break;
		case 3:
			rValue = 200;
			break;
		default:
			break;
	}
	return rValue;
}

static inline int axp20_vbat_to_mV(uint16_t reg)
{
  return ((int)((( reg >> 8) << 4 ) | (reg & 0x000F))) * 1100 / 1000;
}

static inline int axp20_vdc_to_mV(uint16_t reg)
{
  return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) * 1700 / 1000;
}


static inline int axp20_ibat_to_mA(uint16_t reg)
{
    return ((int)(((reg >> 8) << 5 ) | (reg & 0x001F))) * 500 / 1000;
}

static inline int axp20_icharge_to_mA(uint16_t reg)
{
    return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) * 500 / 1000;
}

static inline int axp20_iac_to_mA(uint16_t reg)
{
    return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) * 625 / 1000;
}

static inline int axp20_iusb_to_mA(uint16_t reg)
{
    return ((int)(((reg >> 8) << 4 ) | (reg & 0x000F))) * 375 / 1000;
}


static inline void axp_read_adc(struct axp_charger *charger,
  struct axp_adc_res *adc);


static void axp_charger_update_state(struct axp_charger *charger);

static void axp_charger_update(struct axp_charger *charger);


#if defined  (CONFIG_AXP_CHARGEINIT)
static void axp_set_charge(struct axp_charger *charger)
{
  uint8_t val=0x00;
  uint8_t tmp=0x00;
    if(charger->chgvol < 4150000)
      val &= ~(3 << 5);
    else if (charger->chgvol<4200000){
      val &= ~(3 << 5);
      val |= 1 << 5;
      }
    else if (charger->chgvol<4360000){
      val &= ~(3 << 5);
      val |= 1 << 6;
      }
    else
      val |= 3 << 5;

    if(charger->chgcur< 300000)
      charger->chgcur = 300000;
    else if(charger->chgcur > 1800000)
     charger->chgcur = 1800000;

    val |= (charger->chgcur - 200001) / 100000 ;
    if(charger ->chgend == 10){
      val &= ~(1 << 4);
    }
    else {
      val |= 1 << 4;
    }
    val &= 0x7F;
    val |= charger->chgen << 7;
      if(charger->chgpretime < 30)
      charger->chgpretime = 30;
    if(charger->chgcsttime < 360)
      charger->chgcsttime = 360;

    tmp = ((((charger->chgpretime - 40) / 10) << 6)  \
      | ((charger->chgcsttime - 360) / 120));
	axp_write(charger->master, AXP20_CHARGE_CONTROL1,val);
	axp_update(charger->master, AXP20_CHARGE_CONTROL2,tmp,0xC2);
}
#else
static void axp_set_charge(struct axp_charger *charger)
{

}
#endif

static enum power_supply_property axp_battery_props[] = {
  POWER_SUPPLY_PROP_MODEL_NAME,
  POWER_SUPPLY_PROP_STATUS,
  POWER_SUPPLY_PROP_PRESENT,
  POWER_SUPPLY_PROP_ONLINE,
  POWER_SUPPLY_PROP_HEALTH,
  POWER_SUPPLY_PROP_TECHNOLOGY,
  POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
  POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
  POWER_SUPPLY_PROP_VOLTAGE_NOW,
  POWER_SUPPLY_PROP_CURRENT_NOW,
  //POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
  //POWER_SUPPLY_PROP_CHARGE_FULL,
  POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
  POWER_SUPPLY_PROP_CAPACITY,
  //POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
  //POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
  POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property axp_ac_props[] = {
  POWER_SUPPLY_PROP_MODEL_NAME,
  POWER_SUPPLY_PROP_PRESENT,
  POWER_SUPPLY_PROP_ONLINE,
  POWER_SUPPLY_PROP_VOLTAGE_NOW,
  POWER_SUPPLY_PROP_CURRENT_NOW,
};

static enum power_supply_property axp_usb_props[] = {
  POWER_SUPPLY_PROP_MODEL_NAME,
  POWER_SUPPLY_PROP_PRESENT,
  POWER_SUPPLY_PROP_ONLINE,
  POWER_SUPPLY_PROP_VOLTAGE_NOW,
  POWER_SUPPLY_PROP_CURRENT_NOW,
};

static void axp_battery_check_status(struct axp_charger *charger,
            union power_supply_propval *val)
{
  if (charger->bat_det) {
    if (charger->ext_valid){
    	if( charger->rest_vol == 100)
        {
            val->intval = POWER_SUPPLY_STATUS_FULL;
            if (charger->led_control)
            {
    	        axp_clr_bits(charger->master,0x32,0x38);
    	        axp_set_bits(charger->master,0x32,0x08);
                charger->led_control(1);
            }
        }
    	else
        {
    		val->intval = POWER_SUPPLY_STATUS_CHARGING;
            if (charger->led_control)
            {
    	        axp_clr_bits(charger->master,0x32,0x38);
                charger->led_control(0);
            }
        }
    }
    else
    {
      val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
        if (charger->led_control)
        {
    	    axp_clr_bits(charger->master,0x32,0x38);
            charger->led_control(0);
        }
    }
  }
  else
    {
    val->intval = POWER_SUPPLY_STATUS_FULL;
    }
}

static void axp_battery_check_health(struct axp_charger *charger,
            union power_supply_propval *val)
{
    if (charger->fault & AXP20_FAULT_LOG_BATINACT)
    val->intval = POWER_SUPPLY_HEALTH_DEAD;
  else if (charger->fault & AXP20_FAULT_LOG_OVER_TEMP)
    val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
  else if (charger->fault & AXP20_FAULT_LOG_COLD)
    val->intval = POWER_SUPPLY_HEALTH_COLD;
  else
    val->intval = POWER_SUPPLY_HEALTH_GOOD;
}

static int axp_battery_get_property(struct power_supply *psy,
           enum power_supply_property psp,
           union power_supply_propval *val)
{
  struct axp_charger *charger;
  int ret = 0;
  charger = container_of(psy, struct axp_charger, batt);

  switch (psp) {
  case POWER_SUPPLY_PROP_STATUS:
    axp_battery_check_status(charger, val);
    break;
  case POWER_SUPPLY_PROP_HEALTH:
    axp_battery_check_health(charger, val);
    break;
  case POWER_SUPPLY_PROP_TECHNOLOGY:
    val->intval = charger->battery_info->technology;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
    val->intval = charger->battery_info->voltage_max_design;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
    val->intval = charger->battery_info->voltage_min_design;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_NOW:
    val->intval = charger->ocv * 1000;
    break;
  case POWER_SUPPLY_PROP_CURRENT_NOW:
    val->intval = charger->ibat * 1000;
    break;
  case POWER_SUPPLY_PROP_MODEL_NAME:
    val->strval = charger->batt.name;
    break;
/*  case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
  case POWER_SUPPLY_PROP_CHARGE_FULL:
    val->intval = charger->battery_info->charge_full_design;
        break;
*/
  case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
    val->intval = charger->battery_info->energy_full_design;
  //  DBG_PSY_MSG("POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:%d\n",val->intval);
       break;
  case POWER_SUPPLY_PROP_CAPACITY:
    val->intval = charger->rest_vol;
    break;
/*  case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
    if(charger->bat_det && !(charger->is_on) && !(charger->ext_valid))
      val->intval = charger->rest_time;
    else
      val->intval = 0;
    break;
  case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
    if(charger->bat_det && charger->is_on)
      val->intval = charger->rest_time;
    else
      val->intval = 0;
    break;
*/
  case POWER_SUPPLY_PROP_ONLINE:
    val->intval = (!charger->is_on)&&(charger->bat_det) && (! charger->ext_valid);
    DBG_PSY_MSG("POWER_SUPPLY_PROP_ONLINE axp bat:%d\n",val->intval);
    break;
  case POWER_SUPPLY_PROP_PRESENT:
    val->intval = charger->bat_det;
    break;
  case POWER_SUPPLY_PROP_TEMP:
    //val->intval = charger->ic_temp - 200;
    val->intval =  300;
    break;
  default:
    ret = -EINVAL;
    break;
  }

  return ret;
}

static int axp_ac_get_property(struct power_supply *psy,
           enum power_supply_property psp,
           union power_supply_propval *val)
{
  struct axp_charger *charger;
  int ret = 0;
  charger = container_of(psy, struct axp_charger, ac);

  switch(psp){
  case POWER_SUPPLY_PROP_MODEL_NAME:
    val->strval = charger->ac.name;break;
  case POWER_SUPPLY_PROP_PRESENT:
    val->intval = charger->ac_det;
    break;
  case POWER_SUPPLY_PROP_ONLINE:
	val->intval = charger->ac_valid;
    DBG_PSY_MSG("POWER_SUPPLY_PROP_ONLINE axp ac :%d\n",val->intval);
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_NOW:
    val->intval = charger->vac * 1000;
    break;
  case POWER_SUPPLY_PROP_CURRENT_NOW:
    val->intval = charger->iac * 1000;
    break;
  default:
    ret = -EINVAL;
    break;
  }
   return ret;
}

static int axp_usb_get_property(struct power_supply *psy,
           enum power_supply_property psp,
           union power_supply_propval *val)
{
  struct axp_charger *charger;
  int ret = 0;
  charger = container_of(psy, struct axp_charger, usb);

  switch(psp){
  case POWER_SUPPLY_PROP_MODEL_NAME:
    val->strval = charger->usb.name;break;
  case POWER_SUPPLY_PROP_PRESENT:
    val->intval = charger->usb_det;
    break;
  case POWER_SUPPLY_PROP_ONLINE:
    val->intval = charger->usb_valid;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_NOW:
    val->intval = charger->vusb * 1000;
    break;
  case POWER_SUPPLY_PROP_CURRENT_NOW:
    val->intval = charger->iusb * 1000;
    break;
  default:
    ret = -EINVAL;
    break;
  }
   return ret;
}


#if	1
int axp_charger_set_chgcur(struct axp_charger *charger, int chgcur)
{
    int tmp;
    if(chgcur >= 300000 && chgcur <= 1800000){
        tmp = (chgcur -200001)/100000;
        charger->chgcur = tmp *100000 + 300000;
        axp_set_bits(charger->master,AXP20_CHARGE_CONTROL1, 0x80);
        axp_update(charger->master,AXP20_CHARGE_CONTROL1, tmp,0x0F);
    }
    else if(chgcur == 0){
        axp_clr_bits(charger->master,AXP20_CHARGE_CONTROL1, 0x80);
    }
    return 0;
}
#else	//需要完善
int axp_charger_set_chgcur(struct axp_charger *charger, int chgcur)
{
	char reg_val = 0;
	axp_read(POWER20_CHARGE1, &reg_val);
	if(current == 0)
	{
		reg_val &= 0x7f;
		axp_write(POWER20_CHARGE1, reg_val);
		DBG_PSY_MSG("%s: set charge current to %d Reg value %x!\n",__FUNCTION__, current, reg_val);		
	}
	else if((current<300)||(current>1800))
	{
		DBG_PSY_MSG("%s: value(%dmA) is outside the allowable range of 300-1800mA!\n",
			__FUNCTION__, current);
		return -1;
	}
	else
	{
		reg_val &= 0xf0;
		reg_val |= ((current-300)/100);
		axp_write(POWER20_CHARGE1, reg_val);
		DBG_PSY_MSG("%s: set charge current to %d Reg value %x!\n",__FUNCTION__, current, reg_val);		
	}
	return 0;
}
#endif


static void axp_change(struct axp_charger *charger)
{
  DBG_PSY_MSG("battery state change\n");
  axp_charger_update_state(charger);
  axp_charger_update(charger);
  flag_state_change = 1;
  power_supply_changed(&charger->batt);
}

static void axp_presslong(struct axp_charger *charger)
{
	DBG_PSY_MSG("press long\n");
	input_report_key(powerkeydev, KEY_POWER, 1);
	input_sync(powerkeydev);
	ssleep(2);
	DBG_PSY_MSG("press long up\n");
	input_report_key(powerkeydev, KEY_POWER, 0);
	input_sync(powerkeydev);
}

static void axp_pressshort(struct axp_charger *charger)
{
	DBG_PSY_MSG("press short\n");
  input_report_key(powerkeydev, KEY_POWER, 1);
  input_sync(powerkeydev);
  msleep(100);
  input_report_key(powerkeydev, KEY_POWER, 0);
  input_sync(powerkeydev);
}

static void axp_capchange(struct axp_charger *charger)
{
	uint8_t val;
	int k;
	
	DBG_PSY_MSG("battery change\n");
	ssleep(2);
  axp_charger_update_state(charger);
  axp_charger_update(charger);
  axp_read(charger->master, AXP20_CAP,&val);
  charger->rest_vol = (int) (val & 0x7F);
  
  if((charger->bat_det == 0) || (charger->rest_vol == 127)){
  	charger->rest_vol = 100;
  }
  
  DBG_PSY_MSG("rest_vol = %d\n",charger->rest_vol);
  memset(Bat_Cap_Buffer, 0, sizeof(Bat_Cap_Buffer));
  for(k = 0;k < AXP20_VOL_MAX; k++){
    Bat_Cap_Buffer[k] = charger->rest_vol;
  }
  Total_Cap = charger->rest_vol * AXP20_VOL_MAX;
  power_supply_changed(&charger->batt);
}

static int axp_battery_event(struct notifier_block *nb, unsigned long event,
        void *data)
{
    struct axp_charger *charger =
    container_of(nb, struct axp_charger, nb);

    uint8_t w[9];

    w[0] = (uint8_t) ((event) & 0xFF);
    w[1] = POWER20_INTSTS2;
    w[2] = (uint8_t) ((event >> 8) & 0xFF);
    w[3] = POWER20_INTSTS3;
    w[4] = (uint8_t) ((event >> 16) & 0xFF);
    w[5] = POWER20_INTSTS4;
    w[6] = (uint8_t) ((event >> 24) & 0xFF);
    w[7] = POWER20_INTSTS5;
    w[8] = (uint8_t) (((uint64_t) event >> 32) & 0xFF);

    if(event & (AXP20_IRQ_BATIN|AXP20_IRQ_BATRE)) {
    	axp_capchange(charger);
    }

    if(event & (AXP20_IRQ_ACIN|AXP20_IRQ_USBIN|AXP20_IRQ_ACOV|AXP20_IRQ_USBOV|AXP20_IRQ_CHAOV
               |AXP20_IRQ_CHAST|AXP20_IRQ_TEMOV|AXP20_IRQ_TEMLO)) {
        axp_change(charger);
    }

    if(event & (AXP20_IRQ_ACRE|AXP20_IRQ_USBRE)) {
    	axp_change(charger);
    	axp_clr_bits(charger->master,0x32,0x38);
    }

    if(event & AXP20_IRQ_PEKLO) {
    	axp_presslong(charger);
    }

    if(event & AXP20_IRQ_PEKSH) {
    	axp_pressshort(charger);
    }

    DBG_PSY_MSG("event = 0x%x\n",(int) event);
    axp_writes(charger->master,POWER20_INTSTS1,9,w);

    return 0;
}

static char *supply_list[] = {
  "battery",
};



static void axp_battery_setup_psy(struct axp_charger *charger)
{
  struct power_supply *batt = &charger->batt;
  struct power_supply *ac = &charger->ac;
  struct power_supply *usb = &charger->usb;
  struct power_supply_info *info = charger->battery_info;

  batt->name = "battery";
  batt->use_for_apm = info->use_for_apm;
  batt->type = POWER_SUPPLY_TYPE_BATTERY;
  batt->get_property = axp_battery_get_property;

  batt->properties = axp_battery_props;
  batt->num_properties = ARRAY_SIZE(axp_battery_props);

  ac->name = "ac";
  ac->type = POWER_SUPPLY_TYPE_MAINS;
  ac->get_property = axp_ac_get_property;

  ac->supplied_to = supply_list,
  ac->num_supplicants = ARRAY_SIZE(supply_list),

  ac->properties = axp_ac_props;
  ac->num_properties = ARRAY_SIZE(axp_ac_props);

  usb->name = "usb";
  usb->type = POWER_SUPPLY_TYPE_USB;
  usb->get_property = axp_usb_get_property;

  usb->supplied_to = supply_list,
  usb->num_supplicants = ARRAY_SIZE(supply_list),

  usb->properties = axp_usb_props;
  usb->num_properties = ARRAY_SIZE(axp_usb_props);
};

#if defined  (CONFIG_AXP_CHARGEINIT)
static int axp_battery_adc_set(struct axp_charger *charger)
{
   int ret ;
   uint8_t val;

  /*enable adc and set adc */
  val= AXP20_ADC_BATVOL_ENABLE | AXP20_ADC_BATCUR_ENABLE
  | AXP20_ADC_DCINCUR_ENABLE | AXP20_ADC_DCINVOL_ENABLE
  | AXP20_ADC_USBVOL_ENABLE | AXP20_ADC_USBCUR_ENABLE;

	ret = axp_update(charger->master, AXP20_ADC_CONTROL1, val , val);
  if (ret)
    return ret;
    ret = axp_read(charger->master, AXP20_ADC_CONTROL3, &val);
  switch (charger->sample_time/25){
  case 1: val &= ~(3 << 6);break;
  case 2: val &= ~(3 << 6);val |= 1 << 6;break;
  case 4: val &= ~(3 << 6);val |= 2 << 6;break;
  case 8: val |= 3 << 6;break;
  default: break;
  }
  ret = axp_write(charger->master, AXP20_ADC_CONTROL3, val);
  if (ret)
    return ret;

  return 0;
}
#else
static int axp_battery_adc_set(struct axp_charger *charger)
{
  return 0;
}
#endif

static int axp_battery_first_init(struct axp_charger *charger)
{
   int ret;
   uint8_t val;
   axp_set_charge(charger);
   ret = axp_battery_adc_set(charger);
   if(ret)
    return ret;

   ret = axp_read(charger->master, AXP20_ADC_CONTROL3, &val);
   switch ((val >> 6) & 0x03){
  case 0: charger->sample_time = 25;break;
  case 1: charger->sample_time = 50;break;
  case 2: charger->sample_time = 100;break;
  case 3: charger->sample_time = 200;break;
  default:break;
  }
  return ret;
}

//加上配置
static int axp_get_rdc(struct axp_charger *charger)
{
	int temp;
	int rdc;
	uint8_t v[2];
	axp_reads(charger->master,0xba,2,v);
 	rdc = (((v[0] & 0x1F) << 8) | v[1]) * 10742 / 10000;
    DBG_PSY_MSG("===========================calculate rdc \n");
	//ssleep(30);
	if(!charger->bat_det){
		charger->disvbat = 0;
		charger->disibat = 0;
		return rdc;
  }
  if( charger->ext_valid){
    axp_charger_update(charger);
    if( axp20_ibat_to_mA(charger->adc.idischar_res) == 0 && axp20_icharge_to_mA(charger->adc.ichar_res) > 200){
    }
    else{
    	DBG_PSY_MSG("%s->%d\n",__FUNCTION__,__LINE__);
    	charger->disvbat = 0;
			charger->disibat = 0;
    	return rdc;
    }
    DBG_PSY_MSG("CHARGING:      charger->vbat = %d,   charger->ibat = %d\n",charger->vbat,charger->ibat);
    DBG_PSY_MSG("DISCHARGING:charger->disvbat = %d,charger->disibat = %d\n",charger->disvbat,charger->disibat);
    axp_charger_update_state(charger);
    if(!charger->bat_current_direction){
    	charger->disvbat = 0;
			charger->disibat = 0;
      return rdc;
    }
		if(charger->disvbat == 0){
		  charger->disvbat = 0;
			charger->disibat = 0;
			return rdc;
		}
    else{
    	temp = 1000 * ABS(charger->vbat - charger->disvbat) / ABS(charger->ibat + charger->disibat);
    	DBG_PSY_MSG("CALRDC:temp = %d\n",temp);
    	charger->disvbat = 0;
			charger->disibat = 0;
    	if((temp < 75) || (temp > 1000))
    		return rdc;
   		else {
    		axp_set_bits(charger->master,0x04,0x08);
    		return temp;
    	}
    }
  }
  else{
    charger->disvbat = 0;
		charger->disibat = 0;
    return rdc;
  }
}

static ssize_t chgen_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master, AXP20_CHARGE_CONTROL1, &val);
  charger->chgen  = val >> 7;
  return sprintf(buf, "%d\n",charger->chgen);
}

static ssize_t chgen_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var){
    charger->chgen = 1;
    axp_set_bits(charger->master,AXP20_CHARGE_CONTROL1,0x80);
  }
  else{
    charger->chgen = 0;
    axp_clr_bits(charger->master,AXP20_CHARGE_CONTROL1,0x80);
  }
  return count;
}

static ssize_t chgmicrovol_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master, AXP20_CHARGE_CONTROL1, &val);
  switch ((val >> 5) & 0x03){
    case 0: charger->chgvol = 4100000;break;
    case 1: charger->chgvol = 4150000;break;
    case 2: charger->chgvol = 4200000;break;
    case 3: charger->chgvol = 4360000;break;
  }
  return sprintf(buf, "%d\n",charger->chgvol);
}

static ssize_t chgmicrovol_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t tmp, val;
  var = simple_strtoul(buf, NULL, 10);
  switch(var){
    case 4100000:tmp = 0;break;
    case 4150000:tmp = 1;break;
    case 4200000:tmp = 2;break;
    case 4360000:tmp = 3;break;
    default:  tmp = 4;break;
  }
  if(tmp < 4){
    charger->chgvol = var;
    axp_read(charger->master, AXP20_CHARGE_CONTROL1, &val);
    val &= 0x9F;
    val |= tmp << 5;
    axp_write(charger->master, AXP20_CHARGE_CONTROL1, val);
  }
  return count;
}

static ssize_t chgintmicrocur_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master, AXP20_CHARGE_CONTROL1, &val);
  charger->chgcur = (val & 0x0F) * 100000 +300000;
  return sprintf(buf, "%d\n",charger->chgcur);
}

static ssize_t chgintmicrocur_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t val,tmp;
  var = simple_strtoul(buf, NULL, 10);
  if(var >= 300000 && var <= 1800000){
    tmp = (var -200001)/100000;
    charger->chgcur = tmp *100000 + 300000;
    axp_read(charger->master, AXP20_CHARGE_CONTROL1, &val);
    val &= 0xF0;
    val |= tmp;
    axp_write(charger->master, AXP20_CHARGE_CONTROL1, val);
  }
  return count;
}

static ssize_t chgendcur_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master, AXP20_CHARGE_CONTROL1, &val);
  charger->chgend = ((val >> 4)& 0x01)? 15 : 10;
  return sprintf(buf, "%d\n",charger->chgend);
}

static ssize_t chgendcur_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var == 10 ){
    charger->chgend = var;
    axp_clr_bits(charger->master ,AXP20_CHARGE_CONTROL1,0x10);
  }
  else if (var == 15){
    charger->chgend = var;
    axp_set_bits(charger->master ,AXP20_CHARGE_CONTROL1,0x10);

  }
  return count;
}

static ssize_t chgpretimemin_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master,AXP20_CHARGE_CONTROL2, &val);
  charger->chgpretime = (val >> 6) * 10 +40;
  return sprintf(buf, "%d\n",charger->chgpretime);
}

static ssize_t chgpretimemin_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t tmp,val;
  var = simple_strtoul(buf, NULL, 10);
  if(var >= 40 && var <= 70){
    tmp = (var - 40)/10;
    charger->chgpretime = tmp * 10 + 40;
    axp_read(charger->master,AXP20_CHARGE_CONTROL2,&val);
    val &= 0x3F;
    val |= (tmp << 6);
    axp_write(charger->master,AXP20_CHARGE_CONTROL2,val);
  }
  return count;
}

static ssize_t chgcsttimemin_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master,AXP20_CHARGE_CONTROL2, &val);
  charger->chgcsttime = (val & 0x03) *120 + 360;
  return sprintf(buf, "%d\n",charger->chgcsttime);
}

static ssize_t chgcsttimemin_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t tmp,val;
  var = simple_strtoul(buf, NULL, 10);
  if(var >= 360 && var <= 720){
    tmp = (var - 360)/120;
    charger->chgcsttime = tmp * 120 + 360;
    axp_read(charger->master,AXP20_CHARGE_CONTROL2,&val);
    val &= 0xFC;
    val |= tmp;
    axp_write(charger->master,AXP20_CHARGE_CONTROL2,val);
  }
  return count;
}

static ssize_t adcfreq_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master, AXP20_ADC_CONTROL3, &val);
  switch ((val >> 6) & 0x03){
     case 0: charger->sample_time = 25;break;
     case 1: charger->sample_time = 50;break;
     case 2: charger->sample_time = 100;break;
     case 3: charger->sample_time = 200;break;
     default:break;
  }
  return sprintf(buf, "%d\n",charger->sample_time);
}

static ssize_t adcfreq_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t val;
  var = simple_strtoul(buf, NULL, 10);
  axp_read(charger->master, AXP20_ADC_CONTROL3, &val);
  switch (var/25){
    case 1: val &= ~(3 << 6);charger->sample_time = 25;break;
    case 2: val &= ~(3 << 6);val |= 1 << 6;charger->sample_time = 50;break;
    case 4: val &= ~(3 << 6);val |= 2 << 6;charger->sample_time = 100;break;
    case 8: val |= 3 << 6;charger->sample_time = 200;break;
    default: break;
    }
  axp_write(charger->master, AXP20_ADC_CONTROL3, val);
  return count;
}


static ssize_t vholden_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master,AXP20_CHARGE_VBUS, &val);
  val = (val>>6) & 0x01;
  return sprintf(buf, "%d\n",val);
}

static ssize_t vholden_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var)
    axp_set_bits(charger->master, AXP20_CHARGE_VBUS, 0x40);
  else
    axp_clr_bits(charger->master, AXP20_CHARGE_VBUS, 0x40);

  return count;
}

static ssize_t vhold_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  int vhold;
  axp_read(charger->master,AXP20_CHARGE_VBUS, &val);
  vhold = ((val >> 3) & 0x07) * 100000 + 4000000;
  return sprintf(buf, "%d\n",vhold);
}

static ssize_t vhold_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  uint8_t val,tmp;
  var = simple_strtoul(buf, NULL, 10);
  if(var >= 4000000 && var <=4700000){
    tmp = (var - 4000000)/100000;
    //printk("tmp = 0x%x\n",tmp);
    axp_read(charger->master, AXP20_CHARGE_VBUS,&val);
    val &= 0xC7;
    val |= tmp << 3;
    //printk("val = 0x%x\n",val);
    axp_write(charger->master, AXP20_CHARGE_VBUS,val);
  }
  return count;
}

static ssize_t iholden_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val;
  axp_read(charger->master,AXP20_CHARGE_VBUS, &val);
  return sprintf(buf, "%d\n",((val & 0x03) == 0x03)?0:1);
}

static ssize_t iholden_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var)
    axp_clr_bits(charger->master, AXP20_CHARGE_VBUS, 0x01);
  else
    axp_set_bits(charger->master, AXP20_CHARGE_VBUS, 0x03);

  return count;
}

static ssize_t ihold_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  uint8_t val,tmp;
  int ihold;
  axp_read(charger->master,AXP20_CHARGE_VBUS, &val);
  tmp = (val) & 0x03;
  switch(tmp){
    case 0: ihold = 900000;break;
    case 1: ihold = 500000;break;
    case 2: ihold = 100000;break;
    default: ihold = 0;break;
  }
  return sprintf(buf, "%d\n",ihold);
}

static ssize_t ihold_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
  struct axp_charger *charger = dev_get_drvdata(dev);
  int var;
  var = simple_strtoul(buf, NULL, 10);
  if(var == 900000)
    axp_clr_bits(charger->master, AXP20_CHARGE_VBUS, 0x03);
  else if (var == 500000){
    axp_clr_bits(charger->master, AXP20_CHARGE_VBUS, 0x02);
    axp_set_bits(charger->master, AXP20_CHARGE_VBUS, 0x01);
  }
  else if (var == 100000){
    axp_clr_bits(charger->master, AXP20_CHARGE_VBUS, 0x01);
    axp_set_bits(charger->master, AXP20_CHARGE_VBUS, 0x02);
  }

  return count;
}

static ssize_t debug_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", axp_debug);
}

static ssize_t debug_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
	struct axp_charger *charger = dev_get_drvdata(dev);
	int var;
	var = simple_strtoul(buf, NULL, 10);
	axp_debug = var;
	return count;
}


static struct device_attribute axp_charger_attrs[] = {
  AXP_CHG_ATTR(chgen),
  AXP_CHG_ATTR(chgmicrovol),
  AXP_CHG_ATTR(chgintmicrocur),
  AXP_CHG_ATTR(chgendcur),
  AXP_CHG_ATTR(chgpretimemin),
  AXP_CHG_ATTR(chgcsttimemin),
  AXP_CHG_ATTR(adcfreq),
  AXP_CHG_ATTR(vholden),
  AXP_CHG_ATTR(vhold),
  AXP_CHG_ATTR(iholden),
  AXP_CHG_ATTR(ihold),
  AXP_CHG_ATTR(debug),
};

#if defined CONFIG_HAS_EARLYSUSPEND
static void axp_earlysuspend(struct early_suspend *h)
{
	uint8_t tmp;
	DBG_PSY_MSG("======early suspend=======\n");
	
#if defined (CONFIG_AXP_CHGCHANGE)
  	early_suspend_flag = 1;
    if(pmu_earlysuspend_chgcur >= 300000 && pmu_earlysuspend_chgcur <= 1800000){
    	tmp = (pmu_earlysuspend_chgcur -200001)/100000;
    	axp_update(axp_charger->master, AXP20_CHARGE_CONTROL1, tmp,0x0F);
    }
    if (axp_charger->led_control)
    {
        axp_charger->led_control(3);
	}
#endif
	
}
static void axp_lateresume(struct early_suspend *h)
{
	uint8_t tmp;
	DBG_PSY_MSG("======late resume=======\n");
	
#if defined (CONFIG_AXP_CHGCHANGE)
	early_suspend_flag = 0;
    if(axp_cfg_board->pmu_resume_chgcur >= 300000 && axp_cfg_board->pmu_resume_chgcur <= 1800000){
        tmp = (axp_cfg_board->pmu_resume_chgcur -200001)/100000;
        axp_update(axp_charger->master, AXP20_CHARGE_CONTROL1, tmp,0x0F);
    }
    
    if ((axp_charger->ext_valid && ( axp_charger->rest_vol != 100)) || !( axp_charger->ext_valid )){
		if (axp_charger->led_control)
		axp_charger->led_control(4);
    }
#endif

}
#endif

int axp_charger_create_attrs(struct power_supply *psy)
{
  int j,ret;
  for (j = 0; j < ARRAY_SIZE(axp_charger_attrs); j++) {
    ret = device_create_file(psy->dev,
          &axp_charger_attrs[j]);
    if (ret)
      goto sysfs_failed;
  }
    goto succeed;

sysfs_failed:
  while (j--)
    device_remove_file(psy->dev,
         &axp_charger_attrs[j]);
succeed:
  return ret;
}

int Get_Bat_Coulomb_Count(struct axp_charger *charger)
{
	uint8_t  temp[8];
	int  rValue1,rValue2;
	int Cur_CoulombCounter_tmp;

	axp_reads(charger->master, AXP20_CCHAR3_RES,8,temp);
	rValue1 = ((temp[0] << 24) + (temp[1] << 16) + (temp[2] << 8) + temp[3]);
	rValue2 = ((temp[4] << 24) + (temp[5] << 16) + (temp[6] << 8) + temp[7]);
	DBG_PSY_MSG("Get_Bat_Coulomb_Count -     CHARGINGOULB:[0]=0x%x,[1]=0x%x,[2]=0x%x,[3]=0x%x\n",temp[0],temp[1],temp[2],temp[3]);
	DBG_PSY_MSG("Get_Bat_Coulomb_Count - DISCHARGINGCLOUB:[4]=0x%x,[5]=0x%x,[6]=0x%x,[7]=0x%x\n",temp[4],temp[5],temp[6],temp[7]);
	
	Cur_CoulombCounter_tmp = (4369 * (rValue1 - rValue2) / ADC_Freq_Get(charger) / 240 / 2);

	return Cur_CoulombCounter_tmp;				//unit mAh
}

static void axp_charging_monitor(struct work_struct *work);

static inline int axp_vbat_to_mV(uint16_t reg)
{
	return (reg) * 1100 / 1000;
}

static inline int axp_vdc_to_mV(uint16_t reg)
{
	return (reg) * 1700 / 1000;
}

static inline int axp_ibat_to_mA(uint16_t reg)
{
	return (reg) * 500 / 1000;
}

static inline int axp_iac_to_mA(uint16_t reg)
{
	return (reg) * 625 / 1000;
}

static inline int axp_iusb_to_mA(uint16_t reg)
{
	return (reg) * 375 / 1000;
}


/*  AXP  */
#define AXP_STATUS_SOURCE					(1<< 0)
#define AXP_STATUS_ACUSBSH					(1<< 1)
#define AXP_STATUS_BATCURDIR				(1<< 2)
#define AXP_STATUS_USBLAVHO					(1<< 3)
#define AXP_STATUS_USBVA					(1<< 4)
#define AXP_STATUS_USBEN					(1<< 5)
#define AXP_STATUS_ACVA						(1<< 6)
#define AXP_STATUS_ACEN						(1<< 7)

#define AXP_STATUS_CHACURLOEXP				(1<<10)
#define AXP_STATUS_BATINACT					(1<<11)

#define AXP_STATUS_BATEN					(1<<13)
#define AXP_STATUS_INCHAR					(1<<14)
#define AXP_STATUS_ICTEMOV					(1<<15)



#define AXP_CHARGE_STATUS				POWER20_STATUS

#define AXP_CHARGE_CONTROL1				POWER20_CHARGE1
#define AXP_CHARGE_CONTROL2				POWER20_CHARGE2

#define AXP_ADC_CONTROL1				POWER20_ADC_EN1
#define AXP_ADC_BATVOL_ENABLE			(1 << 7)
#define AXP_ADC_BATCUR_ENABLE			(1 << 6)
#define AXP_ADC_DCINVOL_ENABLE			(1 << 5)
#define AXP_ADC_DCINCUR_ENABLE			(1 << 4)
#define AXP_ADC_USBVOL_ENABLE			(1 << 3)
#define AXP_ADC_USBCUR_ENABLE			(1 << 2)
#define AXP_ADC_APSVOL_ENABLE			(1 << 1)
#define AXP_ADC_TSVOL_ENABLE			(1 << 0)
#define AXP_ADC_CONTROL3				POWER20_ADC_SPEED

#define AXP_VACH_RES					POWER20_ACIN_VOL_H8
#define AXP_VACL_RES					POWER20_ACIN_VOL_L4
#define AXP_IACH_RES					POWER20_ACIN_CUR_H8
#define AXP_IACL_RES					POWER20_ACIN_CUR_L4
#define AXP_VUSBH_RES					POWER20_VBUS_VOL_H8
#define AXP_VUSBL_RES					POWER20_VBUS_VOL_L4
#define AXP_IUSBH_RES					POWER20_VBUS_CUR_H8
#define AXP_IUSBL_RES					POWER20_VBUS_CUR_L4

#define AXP_VBATH_RES					POWER20_BAT_AVERVOL_H8
#define AXP_VBATL_RES					POWER20_BAT_AVERVOL_L4
#define AXP_ICHARH_RES					POWER20_BAT_AVERCHGCUR_H8
#define AXP_ICHARL_RES					POWER20_BAT_AVERCHGCUR_L5
#define AXP_IDISCHARH_RES				POWER20_BAT_AVERDISCHGCUR_H8
#define AXP_IDISCHARL_RES				POWER20_BAT_AVERDISCHGCUR_L5			//Elvis fool

#define AXP_COULOMB_CONTROL				POWER20_COULOMB_CTL

#define AXP_CCHAR3_RES					POWER20_BAT_CHGCOULOMB3
#define AXP_CCHAR2_RES					POWER20_BAT_CHGCOULOMB2
#define AXP_CCHAR1_RES					POWER20_BAT_CHGCOULOMB1
#define AXP_CCHAR0_RES					POWER20_BAT_CHGCOULOMB0
#define AXP_CDISCHAR3_RES				POWER20_BAT_DISCHGCOULOMB3
#define AXP_CDISCHAR2_RES				POWER20_BAT_DISCHGCOULOMB2
#define AXP_CDISCHAR1_RES				POWER20_BAT_DISCHGCOULOMB1
#define AXP_CDISCHAR0_RES				POWER20_BAT_DISCHGCOULOMB0

#define AXP_DATA_BUFFER0				POWER20_DATA_BUFFER1
#define AXP_DATA_BUFFER1				POWER20_DATA_BUFFER2
#define AXP_DATA_BUFFER2				POWER20_DATA_BUFFER3
#define AXP_DATA_BUFFER3				POWER20_DATA_BUFFER4
#define AXP_DATA_BUFFER4				POWER20_DATA_BUFFER5
#define AXP_DATA_BUFFER5				POWER20_DATA_BUFFER6
#define AXP_DATA_BUFFER6				POWER20_DATA_BUFFER7
#define AXP_DATA_BUFFER7				POWER20_DATA_BUFFER8
#define AXP_DATA_BUFFER8				POWER20_DATA_BUFFER9
#define AXP_DATA_BUFFER9				POWER20_DATA_BUFFERA
#define AXP_DATA_BUFFERA				POWER20_DATA_BUFFERB
#define AXP_DATA_BUFFERB				POWER20_DATA_BUFFERC
#define AXP_IC_TYPE						POWER20_IC_TYPE

#define AXP_CAP							(0xB9)
#define AXP_RDC_BUFFER0					(0xBA)
#define AXP_RDC_BUFFER1					(0xBB)
#define AXP_OCV_BUFFER0					(0xBC)
#define AXP_OCV_BUFFER1					(0xBD)

#define AXP_HOTOVER_CTL					POWER20_HOTOVER_CTL
#define AXP_CHARGE_VBUS					POWER20_IPS_SET
#define AXP_APS_WARNING1				POWER20_APS_WARNING1
#define AXP_APS_WARNING2				POWER20_APS_WARNING2
#define AXP_TIMER_CTL					POWER20_TIMER_CTL
#define AXP_PEK_SET						POWER20_PEK_SET
#define AXP_DATA_NUM					12

struct axp_charger *axpcharger;






static int axp_get_freq(void)
{
	int  ret = 25;
	uint8_t  temp;
	axp_read(axpcharger->master, AXP_ADC_CONTROL3,&temp);
	temp &= 0xc0;
	switch(temp >> 6){
		case 0:	ret = 25; break;
		case 1:	ret = 50; break;
		case 2:	ret = 100;break;
		case 3:	ret = 200;break;
		default:break;
	}
	return ret;
}






static int axp_get_coulomb(struct axp_charger *charger)
{
	uint8_t  temp[8];
	int64_t  rValue1,rValue2,rValue;
	int Cur_CoulombCounter_tmp,m;

	axp_reads(charger->master, AXP_CCHAR3_RES,8,temp);
	rValue1 = ((temp[0] << 24) + (temp[1] << 16) + (temp[2] << 8) + temp[3]);
	rValue2 = ((temp[4] << 24) + (temp[5] << 16) + (temp[6] << 8) + temp[7]);
	if(axp_debug){
		DBG_PSY_MSG("%s->%d -     CHARGINGOULB:[0]=0x%x,[1]=0x%x,[2]=0x%x,[3]=0x%x\n",__FUNCTION__,__LINE__,temp[0],temp[1],temp[2],temp[3]);
		DBG_PSY_MSG("%s->%d - DISCHARGINGCLOUB:[4]=0x%x,[5]=0x%x,[6]=0x%x,[7]=0x%x\n",__FUNCTION__,__LINE__,temp[4],temp[5],temp[6],temp[7]);
	}
	rValue = (ABS(rValue1 - rValue2)) * 4369;
	m = axp_get_freq() * 480;
	do_div(rValue,m);
	if(rValue1 >= rValue2)
		Cur_CoulombCounter_tmp = (int)rValue;
	else
		Cur_CoulombCounter_tmp = (int)(0 - rValue);
	if(axp_debug)
		DBG_PSY_MSG("Cur_CoulombCounter_tmp = %d\n",Cur_CoulombCounter_tmp);
	return Cur_CoulombCounter_tmp;	//unit mAh
}


static inline void axp_read_adc(struct axp_charger *charger,
  struct axp_adc_res *adc)
{
	uint8_t tmp[8];
	axp_reads(charger->master,AXP_VACH_RES,8,tmp);
	adc->vac_res = ((uint16_t) tmp[0] << 4 )| (tmp[1] & 0x0f);
	adc->iac_res = ((uint16_t) tmp[2] << 4 )| (tmp[3] & 0x0f);
	adc->vusb_res = ((uint16_t) tmp[4] << 4 )| (tmp[5] & 0x0f);
	adc->iusb_res = ((uint16_t) tmp[6] << 4 )| (tmp[7] & 0x0f);
	axp_reads(charger->master,AXP_VBATH_RES,6,tmp);
	adc->vbat_res = ((uint16_t) tmp[0] << 4 )| (tmp[1] & 0x0f);

	adc->ichar_res = ((uint16_t) tmp[2] << 4 )| (tmp[3] & 0x0f);

	adc->idischar_res = ((uint16_t) tmp[4] << 5 )| (tmp[5] & 0x1f);
}

static void axp_charger_update_state(struct axp_charger *charger)
{
	uint8_t val[2];
	uint16_t tmp;

	axp_reads(charger->master,AXP_CHARGE_STATUS,2,val);
	tmp = (val[1] << 8 )+ val[0];
	charger->is_on 					= (tmp & AXP_STATUS_INCHAR)?1:0;
	charger->bat_det 				= (tmp & AXP_STATUS_BATEN)?1:0;
	charger->ac_det 				= (tmp & AXP_STATUS_ACEN)?1:0;
	charger->usb_det 				= (tmp & AXP_STATUS_USBEN)?1:0;
	charger->usb_valid 				= (tmp & AXP_STATUS_USBVA)?1:0;
	charger->ac_valid 				= (tmp & AXP_STATUS_ACVA)?1:0;
	charger->ext_valid 				= charger->ac_valid | charger->usb_valid;
	charger->bat_current_direction 	= (tmp & AXP_STATUS_BATCURDIR)?1:0;
	charger->in_short 				= (tmp& AXP_STATUS_ACUSBSH)?1:0;
	charger->batery_active 			= (tmp & AXP_STATUS_BATINACT)?1:0;
	charger->low_charge_current 	= (tmp & AXP_STATUS_CHACURLOEXP)?1:0;
	charger->int_over_temp 			= (tmp & AXP_STATUS_ICTEMOV)?1:0;
	axp_read(charger->master,AXP_CHARGE_CONTROL1,val);
	charger->charge_on 				= ((val[0] & 0x80)?1:0);
}

static void axp_charger_update(struct axp_charger *charger)
{
	uint16_t tmp;	
	axp_read_adc(charger, &charger->adc);
	tmp = charger->adc.vbat_res;
	charger->vbat = axp_vbat_to_mV(tmp);
	charger->ibat = ABS(axp_ibat_to_mA(charger->adc.ichar_res)-axp_ibat_to_mA(charger->adc.idischar_res));
	tmp = charger->adc.vac_res;
	charger->vac = axp_vdc_to_mV(tmp);
	tmp = charger->adc.iac_res;
	charger->iac = axp_iac_to_mA(tmp);
	tmp = charger->adc.vusb_res;
	charger->vusb = axp_vdc_to_mV(tmp);
	tmp = charger->adc.iusb_res;
	charger->iusb = axp_iusb_to_mA(tmp);
	if(!charger->ext_valid){
		charger->disvbat =  charger->vbat;
		charger->disibat =  charger->ibat;
	}
}
/*
static void axp_set_basecap(struct axp_charger *charger, int base_cap)
{
	uint8_t val;
	if(base_cap >= 0)
		val = base_cap & 0x7F;
	else
		val = ABS(base_cap) | 0x80;

	axp_write(charger->master, AXP_DATA_BUFFER4, val);


}
*/
static void axp_set_basecap(struct axp_charger *charger, int base_cap)
{
	uint8_t val;
	if(base_cap >= 0)
		val = base_cap & 0x7F;
	else
		val = ABS(base_cap) | 0x80;
	DBG_PSY_MSG("axp_set_basecap = 0x%x\n",val);
	axp_write(charger->master, AXP_DATA_BUFFER4, val);

}


static int axp_get_basecap(struct axp_charger *charger)
{
	uint8_t val;

	axp_read(charger->master, AXP_DATA_BUFFER4, &val);
	DBG_PSY_MSG("axp_get_basecap = 0x%x\n",val);

	if((val & 0x80) >> 7)
		return (int) (0 - (val & 0x7F));
	else
		return (int) (val & 0x7F);
}

static void axp_set_batcap(struct axp_charger *charger,int cou)
{
	uint8_t temp[3];
	cou |= 0x8000;
	temp[0] = ((cou & 0xff00) >> 8);
	temp[1] = AXP_DATA_BUFFER3;
	temp[2] = (cou & 0x00ff);
	axp_writes(charger->master,AXP_DATA_BUFFER2,3,temp);
}

static void reset_charger(struct axp_charger *charger)
{
	uint8_t val;
	DBG_PSY_MSG("reset_charger\n");
	axp_read(charger->master, AXP20_CHARGE_CONTROL1, &val);
	val &= ~(1<<7);
	axp_write(charger->master, AXP20_CHARGE_CONTROL1, val);
	mdelay(100);
	val |= 1<<7;
	axp_write(charger->master, AXP20_CHARGE_CONTROL1, val);
}

static int axp_get_batcap(struct axp_charger *charger)
{
	uint8_t temp[2];
	int rValue;

	axp_reads(charger->master, AXP_DATA_BUFFER2,2,temp);
	rValue = ((temp[0] << 8) + temp[1]);
	if(rValue & 0x8000){
		return ((rValue & 0x7fff));

	} else {
		axp_set_batcap(charger, axp_cfg_board->pmu_battery_cap);
		return axp_cfg_board->pmu_battery_cap;
	}
}

uint8_t pre_chg_status = 0;

static void axp_charging_monitor(struct work_struct *work)
{
	struct axp_charger *charger;
	int pre_rest_cap;
	int rdc = 0,Cur_CoulombCounter = 0,base_cap = 0,bat_cap = 0;
	uint8_t val,v[2];
	static int cou_clr_flag = 0;
	bool flag_coulomb,flag_batcap,flag_rdc;
	uint8_t data_mm[AXP_DATA_NUM];
	int mm;


	charger = container_of(work, struct axp_charger, work.work);

	/* 更新电池信息 */
	pre_rest_cap = charger->rest_vol;
	axp_charger_update_state(charger);
	axp_charger_update(charger);

	axp_read(charger->master,AXP_DATA_BUFFERB,&val);
	//flag_coulomb = ((val >> 6) & 0x01);
	//flag_batcap = ((val >> 5) & 0x1);
	//flag_rdc = ((val >> 3) & 0x1);

	/* 更新电池开路电压 */
	axp_reads(charger->master,AXP_OCV_BUFFER0,2,v);
	charger->ocv = ((v[0] << 4) + (v[1] & 0x0f)) * 11 /10 ;
	if(state_count ++){
		if(state_count >= TIMER2){
			state_count = 0;
		}
	} else {
		axp_read(charger->master, AXP_CAP,&val);

		charger->ocv_rest_vol = (int) (val & 0x7F);
		if((charger->bat_det == 0) || (charger->ocv_rest_vol == 127) ){
			charger->ocv_rest_vol = 100;
		}
		/*
		if(ABS(charger->ocv_rest_vol - charger->ocv_rest_vol) > 5){
			printk("correct rdc\n");

			axp_clr_bits(charger->master,AXP_DATA_BUFFERB,0x08);
		}
		*/
	}

	
	Cur_CoulombCounter = axp_get_coulomb(charger);
	
	base_cap = axp_get_basecap(charger);
	DBG_PSY_MSG("base_cap = axp_get_basecap(charger):%d\n",base_cap);

	bat_cap = axp_cfg_board->pmu_battery_cap;
	//charger->rest_vol = 100 * (Cur_CoulombCounter) / axp_cfg_board->pmu_battery_cap + base_cap;
	charger->rest_vol = 100 * (base_cap * axp_cfg_board->pmu_battery_cap / 100 + Cur_CoulombCounter + axp_cfg_board->pmu_battery_cap/200) / axp_cfg_board->pmu_battery_cap;

	if(charger->resume)
	{
		if((charger->ocv >= 4090) && (charger->rest_vol < 100) && (charger->bat_current_direction == 0) && (charger->ext_valid) && charger->charge_on)
		{
			DBG_PSY_MSG("AXP202 resume fix: charger->ocv=%d\n", charger->ocv);
			base_cap = 100 - (charger->rest_vol - base_cap);
			axp_set_basecap(charger,base_cap);
			charger->rest_vol = 100;
			charger->resume = 0;
			if(charger->ocv < 4170)
			{
				DBG_PSY_MSG("if(charger->ocv < 4170) reset_charger()\n");
				reset_charger(charger);
			}
		} 
	} 

	
	/*出错处理，如果初始base_cap估计偏小，导致充满后用库仑计计算的电量低于100%*/
	if((charger->ocv_rest_vol > 98) && (charger->rest_vol < 99) && (charger->bat_current_direction == 1)){
		if(axp_debug)
			DBG_PSY_MSG("%s->%d:(charger->ocv_rest_vol > 98) && (charger->rest_vol < 99)\n",__FUNCTION__,__LINE__);
		if(cap_count1 >= TIMER5) {
			charger->rest_vol ++;
			base_cap ++;
			axp_set_basecap(charger,base_cap);
			cap_count1 = 0;
		} else {
			cap_count1++;
		}
	} else {
		cap_count1 = 0;
	}
	/*出错处理，如果初始base_cap估计偏小，导致充满后用库仑计计算的电量低于100%*/
	if((charger->ocv >= 4100) && (charger->rest_vol < 100) && (charger->bat_current_direction == 0) && (charger->ext_valid) && charger->charge_on){
		if(axp_debug)
			DBG_PSY_MSG("%s->%d:(charger->ocv >= 4100) && (charger->rest_vol < 100) && (charger->bat_current_direction == 0) && (charger->ext_valid)\n",__FUNCTION__,__LINE__);
		if(charger->ocv < 4170)
		{
			reset_charger(charger);
		}
		else
		{
			if(cap_count2 >= TIMER5) {
				charger->rest_vol++;
				base_cap++;
				axp_set_basecap(charger,base_cap);
				cap_count2 = 0;
			} 
			else {
				cap_count2++;
			}
		}
	}


	/*出错处理，如果初始base_cap估计偏小，导致放电后用库仑计计算的电量低于0%*/
	if((charger->ocv_rest_vol > charger->rest_vol) && (charger->rest_vol < BATCAPCORRATE) && charger->bat_current_direction == 0){
		if(axp_debug)
			DBG_PSY_MSG("%s->%d(charger->ocv_rest_vol > %d) && (charger->rest_vol < %d) && charger->bat_current_direction == 0\n",__FUNCTION__,__LINE__,BATCAPCORRATE, BATCAPCORRATE);
		if(pre_rest_cap > charger->rest_vol){
			//base_cap = pre_rest_cap - 100 * (Cur_CoulombCounter) / bat_cap;
			base_cap++;
			axp_set_basecap(charger,base_cap);
		}
		charger->rest_vol =  pre_rest_cap;
	}
	
	/*出错处理，如果初始base_cap估计偏大，导致放电后用库仑计计算的电量过高*/
	if((charger->ocv_rest_vol < BATCAPCORRATE) && (charger->rest_vol > charger->ocv_rest_vol) && (charger->bat_current_direction == 0)){
		if(axp_debug)
			DBG_PSY_MSG("%s->%d(charger->ocv_rest_vol < %d) && (charger->rest_vol > %d) && (charger->bat_current_direction == 0)\n",__FUNCTION__,__LINE__,BATCAPCORRATE,BATCAPCORRATE);
		if(cap_count3 >= TIMER5){
			charger->rest_vol --;
			base_cap --;
			axp_set_basecap(charger,base_cap);
			cap_count3 = 0;
		} else {
			cap_count3++;
		}
	} else {
		cap_count3 = 0;
	}
		

	/*出错处理，如果还在充电，即使返回100%，也置为99%*/
	if((charger->bat_current_direction == 1) && (charger->rest_vol >= 100)){
		if(charger->ibat >=280)
		{
			if(axp_debug)
				DBG_PSY_MSG("%s->%d:[Before fix] charger->rest_vol = %d\n",__FUNCTION__,__LINE__, charger->rest_vol);
			charger->rest_vol = 99;
			base_cap = 99 - 100 * (Cur_CoulombCounter) / bat_cap;
			axp_set_basecap(charger,base_cap);
		}
		else
		{
			if(axp_debug)
				DBG_PSY_MSG("%s->%d:[Before fix] charger->rest_vol = %d\n",__FUNCTION__,__LINE__, charger->rest_vol);
			base_cap = 100 * (bat_cap - Cur_CoulombCounter) / bat_cap;
			charger->rest_vol = 100;
			axp_set_basecap(charger,base_cap);
		}
	}

	if((charger->rest_vol < 100) && (charger->ibat < 280) && (charger->ext_valid) && (charger->charge_on) && (charger->ocv >= 4150))
	{
		DBG_PSY_MSG("(charger->ibat < 280)&&(charger->ocv >= 4150)\n");

		if(cap_count4 >= TIMER5)
		{
			charger->rest_vol++;
			base_cap++;
			axp_set_basecap(charger,base_cap);
			cap_count4 = 0;
		}
		else
		{
			cap_count4++;
		}
	}
	
	if((charger->rest_vol == 0) && ((!cou_clr_flag) || (pre_rest_cap == 1)))
	{
		axp_read(charger->master, AXP_COULOMB_CONTROL, &val);
		val |= 0x20;
		val &= 0xbf;
		axp_write(charger->master, AXP_COULOMB_CONTROL, val);
		val |= 0x80;
		val &= 0xbf;
		axp_write(charger->master, AXP_COULOMB_CONTROL, val);
		axp_set_basecap(charger,0);
		cou_clr_flag = 1;
	}
	/* usb 限流 */
	//axp_usb_curlim(USBCURLIM);	//Elvis fool

	if(charge_index >= TIMER4){
		charger->disvbat = 0;
		charger->disibat = 0;
		charge_index = 0;
	}		
	if(charger->ext_valid == 1)
		charge_index ++;
	else 
		charge_index = 0;

	/* 超过量程校正 */
	if(charger->rest_vol > 100){
		if(axp_debug)
			DBG_PSY_MSG("%s->%d: charger->rest_vol > 100\n",__FUNCTION__,__LINE__);
		charger->rest_vol = 100;
	}
	else if (charger->rest_vol < 0){
		if(axp_debug)
			DBG_PSY_MSG("%s->%d: charger->rest_vol < 0\n",__FUNCTION__,__LINE__);
		charger->rest_vol = 0;
	}

	if((charger->rest_vol - pre_rest_cap) || (pre_chg_status != charger->ext_valid)){
		printk("battery vol change: %d->%d \n", pre_rest_cap, charger->rest_vol);
		pre_rest_cap = charger->rest_vol;

		axp_write(charger->master,AXP_DATA_BUFFER1,charger->rest_vol | 0x80);

		power_supply_changed(&charger->batt);
	}
	pre_chg_status = charger->ext_valid;
	if(axp_debug){
		DBG_PSY_MSG("Cur_CoulombCounter = %d\n", Cur_CoulombCounter);
		DBG_PSY_MSG("charger->ic_temp = %d\n",charger->ic_temp);
		DBG_PSY_MSG("charger->vbat = %d\n",charger->vbat);
		DBG_PSY_MSG("charger->ibat = %d\n",charger->ibat);
		DBG_PSY_MSG("charger->disvbat = %d\n",charger->disvbat);
		DBG_PSY_MSG("charger->disibat = %d\n",charger->disibat);
		DBG_PSY_MSG("charger->vusb = %d\n",charger->vusb);
		DBG_PSY_MSG("charger->iusb = %d\n",charger->iusb);
		DBG_PSY_MSG("charger->vac = %d\n",charger->vac);
		DBG_PSY_MSG("charger->iac = %d\n",charger->iac);
		DBG_PSY_MSG("charger->ocv = %d\n",charger->ocv);
		DBG_PSY_MSG("charger->chgcur = %d\n",charger->chgcur);
		//DBG_PSY_MSG("charger->chgearcur = %d\n",charger->chgearcur);
		//DBG_PSY_MSG("charger->chgsuscur = %d\n",charger->chgsuscur);
		//DBG_PSY_MSG("charger->chgclscur = %d\n",charger->chgclscur);
		DBG_PSY_MSG("charger->ocv_rest_vol = %d\n",charger->ocv_rest_vol);
		DBG_PSY_MSG("bat_cap = %d\n",bat_cap);
		DBG_PSY_MSG("base_cap = %d\n",base_cap);
		//DBG_PSY_MSG("charger->ocv_rest_cap = %d\n",charger->ocv_rest_cap);
		DBG_PSY_MSG("charger->rest_vol = %d\n",charger->rest_vol);
		axp_reads(charger->master,AXP_RDC_BUFFER0,2,v);

		rdc = (((v[0] & 0x1F) << 8) | v[1]) * 10742 / 10000;
		DBG_PSY_MSG("rdc = %d\n",rdc);


		DBG_PSY_MSG("charger->is_on = %d\n",charger->is_on);
		DBG_PSY_MSG("charger->ext_valid = %d\n",charger->ext_valid);
		DBG_PSY_MSG("rdc_count = %d\n",rdc_count);
		DBG_PSY_MSG("state_count = %d\n",state_count);
		DBG_PSY_MSG("cap_count1 = %d,cap_count2 = %d,cap_count3 = %d,cap_count4 = %d\n",cap_count1,cap_count2,cap_count3,cap_count4);
		if(axp_debug)
		{
			axp_reads(charger->master,AXP_DATA_BUFFER0,AXP_DATA_NUM,data_mm);
			for( mm = 0; mm < AXP_DATA_NUM; mm++){
				DBG_PSY_MSG("REG[0x%x] = 0x%x\n",mm+AXP_DATA_BUFFER0,data_mm[mm]);	
			}
		}
	}

    /* reschedule for the next time */
    schedule_delayed_work(&charger->work, charger->interval);
}

static int axp_battery_probe(struct platform_device *pdev)
{
	struct axp_charger *charger;
	struct axp_supply_init_data *pdata = pdev->dev.platform_data;

	int ret,k,var;
	uint8_t val1,val2,tmp,val;
	uint8_t ocv_cap[31];
	int Cur_CoulombCounter;
	int rdc;
	uint16_t tmp2;

	powerkeydev = input_allocate_device();
	if (!powerkeydev) {
		kfree(powerkeydev);
		return -ENODEV;
	}

	powerkeydev->name = pdev->name;
	powerkeydev->phys = "m1kbd/input2";
	powerkeydev->id.bustype = BUS_HOST;
	powerkeydev->id.vendor = 0x0001;
	powerkeydev->id.product = 0x0001;
	powerkeydev->id.version = 0x0100;
	powerkeydev->open = NULL;
	powerkeydev->close = NULL;
	powerkeydev->dev.parent = &pdev->dev;

	set_bit(EV_KEY, powerkeydev->evbit);
	set_bit(EV_REL, powerkeydev->evbit);
	//set_bit(EV_REP, powerkeydev->evbit);
	set_bit(KEY_POWER, powerkeydev->keybit);

	ret = input_register_device(powerkeydev);
	if(ret) {
	DBG_PSY_MSG("Unable to Register the power key\n");
	}

	if (pdata == NULL)
	return -EINVAL;

	if (pdata->chgcur > 1800000 ||
		pdata->chgvol < 4100000 ||
		pdata->chgvol > 4360000)
	{
		DBG_PSY_MSG("charger milliamp is too high or target voltage is over range\n");
		return -EINVAL;
	}

	if (pdata->chgpretime < 40 || pdata->chgpretime >70 ||
		pdata->chgcsttime < 360 || pdata->chgcsttime > 720)
	{
		DBG_PSY_MSG("prechaging time or constant current charging time is over range\n");
	    return -EINVAL;
	}

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (charger == NULL)
	return -ENOMEM;

	charger->master = pdev->dev.parent;

	charger->chgcur      = pdata->chgcur;
	charger->chgvol     = pdata->chgvol;
	charger->chgend           = pdata->chgend;
	charger->sample_time          = pdata->sample_time;
	charger->chgen                   = pdata->chgen;
	charger->chgpretime      = pdata->chgpretime;
	charger->chgcsttime = pdata->chgcsttime;
	charger->battery_info         = pdata->battery_info;
	if (pdata->led_control)
	charger->led_control         = pdata->led_control;
	charger->disvbat = 0;
	charger->disibat = 0;
	charger->resume = 1;
	axpcharger = charger;

  ret = axp_battery_first_init(charger);
  if (ret)
    goto err_charger_init;

/*
  charger->nb.notifier_call = axp_battery_event;
  ret = axp_register_notifier(charger->master, &charger->nb, AXP20_NOTIFIER_ON);
  if (ret)
    goto err_notifier;
*/

  axp_battery_setup_psy(charger);
  ret = power_supply_register(&pdev->dev, &charger->batt);
  if (ret)
    goto err_ps_register;
	
  axp_read(charger->master,AXP20_CHARGE_STATUS,&val);
//	if(!((val >> 1) & 0x01))
  {
    ret = power_supply_register(&pdev->dev, &charger->ac);
    if (ret){
      power_supply_unregister(&charger->batt);
      goto err_ps_register;
    }
  }
  ret = power_supply_register(&pdev->dev, &charger->usb);
  if (ret){
    power_supply_unregister(&charger->ac);
    power_supply_unregister(&charger->batt);
    goto err_ps_register;
  }

  ret = axp_charger_create_attrs(&charger->batt);
  if(ret){
    return ret;
  }

  platform_set_drvdata(pdev, charger);

  /* initial restvol*/

  /* usb current and voltage limit */
  if(axp_cfg_board->pmu_usbvol_limit){
    axp_clr_bits(charger->master, AXP20_CHARGE_VBUS, 0x01);
  	var = axp_cfg_board->pmu_usbvol * 1000;
  	if(var >= 4000000 && var <=4700000){
    	tmp = (var - 4000000)/100000;
    	axp_read(charger->master, AXP20_CHARGE_VBUS,&val);
    	val &= 0xC7;
    	val |= tmp << 3;
    	axp_write(charger->master, AXP20_CHARGE_VBUS,val);
  	}
  }
  else
    axp_set_bits(charger->master, AXP20_CHARGE_VBUS, 0x03);

  if(axp_cfg_board->pmu_usbcur_limit){
    axp_clr_bits(charger->master, AXP20_CHARGE_VBUS, 0x01);
    var = axp_cfg_board->pmu_usbcur * 1000;
  	if(var == 900000)
    	axp_clr_bits(charger->master, AXP20_CHARGE_VBUS, 0x03);
  	else if (var == 500000){
    	axp_clr_bits(charger->master, AXP20_CHARGE_VBUS, 0x02);
    	axp_set_bits(charger->master, AXP20_CHARGE_VBUS, 0x01);
  	}
  	else if (var == 100000){
    	axp_clr_bits(charger->master, AXP20_CHARGE_VBUS, 0x01);
    	axp_set_bits(charger->master, AXP20_CHARGE_VBUS, 0x02);
  	} 
  }
  else
    axp_set_bits(charger->master, AXP20_CHARGE_VBUS, 0x03);


  /* set lowe power warning/shutdown voltage*/

  /* 3.5552V--%5 close*/
  axp_write(charger->master, AXP20_APS_WARNING1,0x7A);	//设置低电警告电压，暂时没有可配置项
  ocv_cap[0]  = axp_cfg_board->pmu_bat_para1;
  ocv_cap[1]  = 0xC1;
  ocv_cap[2]  = axp_cfg_board->pmu_bat_para2;
  ocv_cap[3]  = 0xC2;
  ocv_cap[4]  = axp_cfg_board->pmu_bat_para3;
  ocv_cap[5]  = 0xC3;
  ocv_cap[6]  = axp_cfg_board->pmu_bat_para4;
  ocv_cap[7]  = 0xC4;
  ocv_cap[8]  = axp_cfg_board->pmu_bat_para5;
  ocv_cap[9]  = 0xC5;
  ocv_cap[10] = axp_cfg_board->pmu_bat_para6;
  ocv_cap[11] = 0xC6;
  ocv_cap[12] = axp_cfg_board->pmu_bat_para7;
  ocv_cap[13] = 0xC7;
  ocv_cap[14] = axp_cfg_board->pmu_bat_para8;
  ocv_cap[15] = 0xC8;
  ocv_cap[16] = axp_cfg_board->pmu_bat_para9;
  ocv_cap[17] = 0xC9;
  ocv_cap[18] = axp_cfg_board->pmu_bat_para10;
  ocv_cap[19] = 0xCA;
  ocv_cap[20] = axp_cfg_board->pmu_bat_para11;
  ocv_cap[21] = 0xCB;
  ocv_cap[22] = axp_cfg_board->pmu_bat_para12;
  ocv_cap[23] = 0xCC;
  ocv_cap[24] = axp_cfg_board->pmu_bat_para13;
  ocv_cap[25] = 0xCD;
  ocv_cap[26] = axp_cfg_board->pmu_bat_para14;
  ocv_cap[27] = 0xCE;
  ocv_cap[28] = axp_cfg_board->pmu_bat_para15;
  ocv_cap[29] = 0xCF;
  ocv_cap[30] = axp_cfg_board->pmu_bat_para16;
  axp_writes(charger->master, 0xC0,31,ocv_cap);
  
  /* open/close set */
/*  DBG_PSY_MSG("pmu_pekoff_time = %d\n",pmu_pekoff_time);
  DBG_PSY_MSG("pmu_pekoff_en = %d\n",pmu_pekoff_en);
  DBG_PSY_MSG("pmu_peklong_time = %d\n",pmu_peklong_time);
  DBG_PSY_MSG("pmu_pekon_time = %d\n",pmu_pekon_time);
  DBG_PSY_MSG("pmu_pwrok_time = %d\n",pmu_pwrok_time);
  DBG_PSY_MSG("pmu_pwrnoe_time = %d\n",pmu_pwrnoe_time);
  DBG_PSY_MSG("pmu_intotp_en = %d\n",pmu_intotp_en);	*/
  
  /* n_oe delay time set */
	if (axp_cfg_board->pmu_pwrnoe_time < 1000)
		axp_cfg_board->pmu_pwrnoe_time = 128;
	if (axp_cfg_board->pmu_pwrnoe_time > 3000)
		axp_cfg_board->pmu_pwrnoe_time = 3000;
	axp_read(charger->master,POWER20_OFF_CTL,&val);
	val &= 0xfc;
	val |= ((axp_cfg_board->pmu_pwrnoe_time) / 1000);
	axp_write(charger->master,POWER20_OFF_CTL,val);
	DBG_PSY_MSG("%d-->0x%x\n",__LINE__,val);
	
	/* pek open time set */
	axp_read(charger->master,POWER20_PEK_SET,&val);
	if (axp_cfg_board->pmu_pekon_time < 1000)
		val &= 0x3f;
	else if(axp_cfg_board->pmu_pekon_time < 2000){
		val &= 0x3f;
		val |= 0x80;
	}
	else if(axp_cfg_board->pmu_pekon_time < 3000){
		val &= 0x3f;
		val |= 0xc0;
	}
	else {
		val &= 0x3f;
		val |= 0x40;
	}
	axp_write(charger->master,POWER20_PEK_SET,val);
	DBG_PSY_MSG("%d-->0x%x\n",__LINE__,val);
	
	/* pek long time set*/
	if(axp_cfg_board->pmu_peklong_time < 1000)
		axp_cfg_board->pmu_peklong_time = 1000;
	if(axp_cfg_board->pmu_peklong_time > 2500)
		axp_cfg_board->pmu_peklong_time = 2500;
	axp_read(charger->master,POWER20_PEK_SET,&val);
	val &= 0xcf;
	val |= (((axp_cfg_board->pmu_peklong_time - 1000) / 500) << 4);
	axp_write(charger->master,POWER20_PEK_SET,val);
	DBG_PSY_MSG("%d-->0x%x\n",__LINE__,val);
	
	/* pek en set*/
	if(axp_cfg_board->pmu_pekoff_en)
		axp_cfg_board->pmu_pekoff_en = 1;
	axp_read(charger->master,POWER20_PEK_SET,&val);
	val &= 0xf7;
	val |= (axp_cfg_board->pmu_pekoff_en << 3);
	axp_write(charger->master,POWER20_PEK_SET,val);
	DBG_PSY_MSG("%d-->0x%x\n",__LINE__,val);
	
	/* pek delay set */
	if(axp_cfg_board->pmu_pwrok_time <= 8)
		axp_cfg_board->pmu_pwrok_time = 0;
	else
		axp_cfg_board->pmu_pwrok_time = 1;
	axp_read(charger->master,POWER20_PEK_SET,&val);
	val &= 0xfb;
	val |= axp_cfg_board->pmu_pwrok_time << 2;
	axp_write(charger->master,POWER20_PEK_SET,val);
	DBG_PSY_MSG("%d-->0x%x\n",__LINE__,val);
	
	/* pek off time set */
	if(axp_cfg_board->pmu_pekoff_time < 4000)
		axp_cfg_board->pmu_pekoff_time = 4000;
	if(axp_cfg_board->pmu_pekoff_time > 10000)
		axp_cfg_board->pmu_pekoff_time =10000;
	axp_cfg_board->pmu_pekoff_time = (axp_cfg_board->pmu_pekoff_time - 4000) / 2000 ;
	axp_read(charger->master,POWER20_PEK_SET,&val);
	val &= 0xfc;
	val |= axp_cfg_board->pmu_pekoff_time ;
	axp_write(charger->master,POWER20_PEK_SET,val);
	DBG_PSY_MSG("%d-->0x%x\n",__LINE__,val);
	
	/* enable overtemperture off */
	if(axp_cfg_board->pmu_intotp_en)
		axp_cfg_board->pmu_intotp_en = 1;
	axp_read(charger->master,POWER20_HOTOVER_CTL,&val);
	val &= 0xfb;
	val |= axp_cfg_board->pmu_intotp_en << 2;
	axp_write(charger->master,POWER20_HOTOVER_CTL,val);
	DBG_PSY_MSG("%d-->0x%x\n",__LINE__,val);

	axp_charger_update_state(charger);

	axp_charger_set_chgcur(charger, axp_cfg_board->pmu_init_chgcur);	//set charging current

	axp_read(charger->master,AXP20_DATA_BUFFER1,&val1);
	charger->rest_vol = (int) (val1 & 0x7F);

	axp_read(charger->master, AXP20_CAP,&val2);
	charger->rest_vol = (int) (val2 & 0x7F);

	Cur_CoulombCounter = ABS(Get_Bat_Coulomb_Count(charger));
	DBG_PSY_MSG("Cur_CoulombCounter = %d\n",Cur_CoulombCounter);
	//if(ABS(charger->rest_vol-(val2 & 0x7F)) >= 3 && (val1 >> 7)){
	/*
	if(ABS(charger->rest_vol-(val2 & 0x7F)) >= 10 || Cur_CoulombCounter > 50){
	charger->rest_vol = (int) (val2 & 0x7F);
	}
	*/
	if((charger->bat_det == 0) || (charger->rest_vol == 127)){
		charger->rest_vol = 100;
	}

	DBG_PSY_MSG("last_rest_vol = %d, now_rest_vol = %d\n",(val1 & 0x7F),(val2 & 0x7F));
	memset(Bat_Cap_Buffer, 0, sizeof(Bat_Cap_Buffer));
	for(k = 0;k < AXP20_VOL_MAX; k++){
	Bat_Cap_Buffer[k] = charger->rest_vol;
	}
	Total_Cap = charger->rest_vol * AXP20_VOL_MAX;
	/*enable coulomb*/
	axp_set_bits(charger->master,0xB8,0x80);
	/* disable */
	axp_set_bits(charger->master,AXP20_CAP,0x80);
	axp_clr_bits(charger->master,0xBA,0x80);

#if 0	//配置一下
	axp_read(charger->master,AXP20_DATA_BUFFER0,&val1);
	if(((val1 >> 7) & 0x1) == 0){
		rdc = (axp_cfg_board->pmu_battery_rdc * 10000 + 5371) / 10742;	//找李鑫确认
		tmp2 = (uint16_t) rdc;
		axp_write(charger->master,0xBB,tmp2 & 0x00FF);
		axp_update(charger->master, 0xBA, (tmp2 >> 8), 0x1F);
	}
#else
  	tmp2 = (uint16_t) axp_cfg_board->pmu_battery_rdc;
  	axp_write(charger->master,0xBB,tmp2 & 0x00FF);
  	axp_update(charger->master, 0xBA, (tmp2 >> 8), 0x1F);
#endif
	axp_clr_bits(charger->master,AXP20_CAP,0x80);
	mdelay(500);

	
	axp_read(charger->master, AXP_DATA_BUFFERB, &val);
	if((val & 0x80) == 0)
	{
		axp_read(charger->master, AXP20_CAP,&val2);
		charger->rest_vol = (int) (val2 & 0x7F);

		if((charger->bat_det == 0) || (charger->rest_vol == 127)){
		  charger->rest_vol = 100;
		}
		axp_set_basecap(charger,charger->rest_vol);	
		DBG_PSY_MSG("axp_set_basecap = %d\n",charger->rest_vol);
		axp_read(charger->master, AXP_COULOMB_CONTROL, &val);
		val |= 0x20;
		val &= 0xbf;
		axp_write(charger->master, AXP_COULOMB_CONTROL, val);
		val |= 0x80;
		val &= 0xbf;
		axp_write(charger->master, AXP_COULOMB_CONTROL, val);
		axp_set_bits(charger->master, AXP_DATA_BUFFERB, 1<<7);
	}//Offset cap
	DBG_PSY_MSG("axp_set_basecap(charger, charger->ocv_rest_vol) = %d\n",charger->rest_vol);

	//set charging end voltage
	axp_read(charger->master, AXP_CHARGE_CONTROL1, &val);
	val &= ~(0x3<<5);
	switch(axp_cfg_board->pmu_init_chgvol)
	{
		case 4100:
			val |= 0x0<<5;
			break;
		case 4150:
			val |= 0x1<<5;
			break;
		case 4200:
			val |= 0x2<<5;
			break;
		case 4360:
			val |= 0x3<<5;
			break;
		default:
			printk(KERN_WARNING "axp_cfg_board->pmu_init_chgvol=%dmV not right! use default 4200mV\n", axp_cfg_board->pmu_init_chgvol);
			val |= 0x2<<5;
			break;
	}
	//set charging end current
	if(axp_cfg_board->pmu_init_chgend_rate == 15)
	{
		val |= 1<<4;
		
	}
	else
	{
		val &= ~(1<<4);
		if(axp_cfg_board->pmu_init_chgend_rate != 10)
		{
			printk(KERN_WARNING "axp_cfg_board->pmu_init_chgend_rate=%d% not right! use default 10%\n", axp_cfg_board->pmu_init_chgend_rate);
		}
	}
	axp_write(charger->master, AXP_CHARGE_CONTROL1, val);
	
	charger->interval = msecs_to_jiffies(2 * 1000);
	INIT_DELAYED_WORK(&charger->work, axp_charging_monitor);
	schedule_delayed_work(&charger->work, charger->interval);

#ifdef CONFIG_HAS_EARLYSUSPEND
	pmu_earlysuspend_chgcur = axp_cfg_board->pmu_suspend_chgcur;
    axp_charger = charger;
    axp_early_suspend.suspend = axp_earlysuspend;
    axp_early_suspend.resume = axp_lateresume;
    axp_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 2;
    register_early_suspend(&axp_early_suspend);
#endif


	/* axp202 ntc battery low/high temperature alarm function */
	if(axp_cfg_board->pmu_ntc_enable)
	{			
#define TS_CURRENT_REG 		0x84
		axp_read(charger->master,TS_CURRENT_REG, &val);
		tmp = axp_cfg_board->pmu_ntc_ts_current << 4;
		val &= ~(3<<4);	//val &= 0xcf;
		val |= tmp;		
		val &= ~(3<<0);
		val |= 0x1;		//TS pin:charge output current	
		axp_write(charger->master,TS_CURRENT_REG,val); 	//set TS pin output current

		if(axp_cfg_board->pmu_ntc_lowtempvol < 0)
			axp_cfg_board->pmu_ntc_lowtempvol = 0;
		else if(axp_cfg_board->pmu_ntc_lowtempvol > 3264)
			axp_cfg_board->pmu_ntc_lowtempvol = 3264;
		val = (axp_cfg_board->pmu_ntc_lowtempvol*10)/(16*8);
		axp_write(charger->master,POWER20_VLTF_CHGSET,val);	//set battery low temperature threshold

		if(axp_cfg_board->pmu_ntc_hightempvol < 0)
			axp_cfg_board->pmu_ntc_hightempvol = 0;
		else if(axp_cfg_board->pmu_ntc_hightempvol > 3264)
			axp_cfg_board->pmu_ntc_hightempvol = 3264;
		val = (axp_cfg_board->pmu_ntc_hightempvol*10)/(16*8);
		axp_write(charger->master,POWER20_VHTF_CHGSET,val);	//set battery high temperature threshold
		
	}

  return ret;

err_ps_register:
  axp_unregister_notifier(charger->master, &charger->nb, AXP20_NOTIFIER_ON);

err_notifier:
  cancel_delayed_work_sync(&charger->work);

err_charger_init:
  kfree(charger);
  input_unregister_device(powerkeydev);
  kfree(powerkeydev);

  return ret;
}

static int axp_battery_remove(struct platform_device *dev)
{
    struct axp_charger *charger = platform_get_drvdata(dev);

    if(main_task){
        kthread_stop(main_task);
        main_task = NULL;
    }

    axp_unregister_notifier(charger->master, &charger->nb, AXP20_NOTIFIER_ON);
    cancel_delayed_work_sync(&charger->work);
    power_supply_unregister(&charger->usb);
    power_supply_unregister(&charger->ac);
    power_supply_unregister(&charger->batt);

    kfree(charger);
    input_unregister_device(powerkeydev);
    kfree(powerkeydev);

    return 0;
}


static int axp20_suspend(struct platform_device *dev, pm_message_t state)
{
    uint8_t irq_w[9];
    uint8_t tmp;

    struct axp_charger *charger = platform_get_drvdata(dev);
	cancel_delayed_work_sync(&charger->work);

    /*clear all irqs events*/
    irq_w[0] = 0xff;
    irq_w[1] = POWER20_INTSTS2;
    irq_w[2] = 0xff;
    irq_w[3] = POWER20_INTSTS3;
    irq_w[4] = 0xff;
    irq_w[5] = POWER20_INTSTS4;
    irq_w[6] = 0xff;
    irq_w[7] = POWER20_INTSTS5;
    irq_w[8] = 0xff;
    axp_writes(charger->master, POWER20_INTSTS1, 9, irq_w);
    
    charger->disvbat = 0;
    charger->disibat = 0;

    /* close all irqs*/
    //axp_unregister_notifier(charger->master, &charger->nb, AXP20_NOTIFIER_ON);

	axp_charger_set_chgcur(charger, axp_cfg_board->pmu_suspend_chgcur);	//set charging current
	
//    axp_read(charger->master, AXP20_CHARGE_CONTROL1, &tmp);
//    DBG_PSY_MSG("charger current reg 0x%x\n",tmp);
    return 0;
}

static int axp20_resume(struct platform_device *dev)
{
    struct axp_charger *charger = platform_get_drvdata(dev);
    uint8_t tmp;

	axp_charger_set_chgcur(charger, axp_cfg_board->pmu_resume_chgcur);	//set charging current
	
	charger->resume = 1;
	schedule_work(&charger->work);

    return 0;
}

static void axp20_shutdown(struct platform_device *dev)
{
    uint8_t tmp;
    struct axp_charger *charger = platform_get_drvdata(dev);

    /* not limit usb  voltage*/
  	axp_clr_bits(charger->master, AXP20_CHARGE_VBUS,0x40);

	axp_charger_set_chgcur(charger, axp_cfg_board->pmu_shutdown_chgcur);	//set charging current
}

static struct platform_driver axp_battery_driver = {
  .driver = {
    .name = "axp20-supplyer",
    .owner  = THIS_MODULE,
  },
  .probe = axp_battery_probe,
  .remove = axp_battery_remove,
  .suspend = axp20_suspend,
  .resume = axp20_resume,
  .shutdown = axp20_shutdown,
};

static int axp_battery_init(void)
{
  return platform_driver_register(&axp_battery_driver);
}

static void axp_battery_exit(void)
{
  platform_driver_unregister(&axp_battery_driver);
}

module_init(axp_battery_init);
module_exit(axp_battery_exit);

MODULE_DESCRIPTION("axp20 battery charger driver");
MODULE_AUTHOR("Donglu Zhang, Krosspower");
MODULE_LICENSE("GPL");
