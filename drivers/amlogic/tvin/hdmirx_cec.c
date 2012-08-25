
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>

#include <asm/uaccess.h>
#include <asm/delay.h>
#include <mach/am_regs.h>
#include <mach/power_gate.h>
#include <linux/tvin/tvin.h>

#include <mach/gpio.h>


#include "hdmirx.h"
#include "hdmi_regs.h"
#include "hdmirx_cec.h"

#define _RX_CEC_DBG_ON_

#ifdef _RX_CEC_DBG_ON_
#define hdmirx_cec_dbg_print hdmirx_print
#else
#define hdmirx_cec_dbg_print
#endif

#define _RX_DATA_BUF_SIZE_ 6

/* global variables */
static	unsigned char    gbl_msg[MAX_MSG];
static cec_global_info_t cec_global_info;

static struct semaphore  tv_cec_sema;

static DEFINE_SPINLOCK(p_tx_list_lock);
static DEFINE_SPINLOCK(cec_tx_lock);

static unsigned long cec_tx_list_flags;
static unsigned long cec_tx_flags;
static unsigned int tx_msg_cnt = 0;

static struct list_head cec_tx_msg_phead = LIST_HEAD_INIT(cec_tx_msg_phead);

static tv_cec_pending_e cec_pending_flag = TV_CEC_PENDING_OFF;
static tv_cec_polling_state_e cec_polling_state = TV_CEC_POLLING_OFF;

unsigned int menu_lang_array[] = {(((unsigned int)'c')<<16)|(((unsigned int)'h')<<8)|((unsigned int)'i'),
                                  (((unsigned int)'e')<<16)|(((unsigned int)'n')<<8)|((unsigned int)'g'),
                                  (((unsigned int)'j')<<16)|(((unsigned int)'p')<<8)|((unsigned int)'n'),
                                  (((unsigned int)'k')<<16)|(((unsigned int)'o')<<8)|((unsigned int)'r'),
                                  (((unsigned int)'f')<<16)|(((unsigned int)'r')<<8)|((unsigned int)'a'),
                                  (((unsigned int)'g')<<16)|(((unsigned int)'e')<<8)|((unsigned int)'r')
                                 };

static unsigned char * default_osd_name[16] = {
    "tv",
    "recording 1",
    "recording 2",
    "tuner 1",
    "playback 1",
    "audio system",
    "tuner 2",
    "tuner 3",
    "playback 2",
    "recording 3",
    "tunre 4",
    "playback 3",
    "reserved 1",
    "reserved 2",
    "free use",
    "unregistered"
};

static struct {
    cec_rx_message_t cec_rx_message[_RX_DATA_BUF_SIZE_];
    unsigned char rx_write_pos;
    unsigned char rx_read_pos;
    unsigned char rx_buf_size;
} cec_rx_msg_buf;

static unsigned char * osd_name_uninit = "\0\0\0\0\0\0\0\0";
static irqreturn_t cec_handler(int irq, void *dev_instance);

static unsigned int hdmi_rd_reg(unsigned long addr);
static void hdmi_wr_reg(unsigned long addr, unsigned long data);

void cec_test_function(unsigned char* arg, unsigned char arg_cnt)
{
    int i;
    char buf[1024];
    printk("arg_cnt = %d\n", arg_cnt);
    for (i=0; i<arg_cnt; i++)
        printk("arg[%x]:%d, ", i, arg[i]);
    printk("\n");

    switch (arg[0]) {
    case 0:
        cec_usrcmd_parse_all_dev_online();
        break;
    case 1:
        cec_usrcmd_get_cec_version(arg[1]);
        break;
    case 2:
        cec_usrcmd_get_audio_status(arg[1]);
        break;
    case 3:
        cec_usrcmd_get_deck_status(arg[1]);
        break;
    case 4:
        cec_usrcmd_get_device_power_status(arg[1]);
        break;
    case 5:
        cec_usrcmd_get_device_vendor_id(arg[1]);
        break;
    case 6:
        cec_usrcmd_get_osd_name(arg[1]);
        break;
    case 7:
        cec_usrcmd_get_physical_address(arg[1]);
        break;
    case 8:
        cec_usrcmd_get_system_audio_mode_status(arg[1]);
        break;
    case 9:
        cec_usrcmd_get_tuner_device_status(arg[1]);
        break;
    case 10:
        cec_usrcmd_set_deck_cnt_mode(arg[1], arg[2]);
        break;
    case 11:
        cec_usrcmd_set_standby(arg[1]);
        break;
    case 12:
        cec_usrcmd_set_imageview_on(arg[1]);
        break;
    case 13:
        cec_usrcmd_set_play_mode(arg[1], arg[2]);
        break;
    case 14:
        cec_usrcmd_get_menu_state(arg[1]);
        break;
    case 15:
        cec_usrcmd_set_menu_state(arg[1], arg[2]);
        break;
    case 16:
        cec_usrcmd_get_global_info(buf);
        break;
    case 17:
        cec_usrcmd_get_menu_language(arg[1]);
        break;
    case 18:
        cec_usrcmd_set_menu_language(arg[1], arg[2]);
        break;
    case 19:
        cec_usrcmd_get_active_source();
        break;
    case 20:
        cec_usrcmd_set_active_source(arg[1]);
        break;
    case 21:
        cec_usrcmd_set_deactive_source(arg[1]);
        break;
    case 22:
        cec_usrcmd_set_stream_path(arg[1]);
        break;
    default:
        break;
    }
}

/* cec low level code */

static unsigned int hdmi_rd_reg(unsigned long addr)
{
    unsigned long data;
    WRITE_APB_REG(HDMI_ADDR_PORT, addr);
    WRITE_APB_REG(HDMI_ADDR_PORT, addr);

    data = READ_APB_REG(HDMI_DATA_PORT);

    return (data);
}

static void hdmi_wr_reg(unsigned long addr, unsigned long data)
{
    unsigned long rd_data;

    WRITE_APB_REG(HDMI_ADDR_PORT, addr);
    WRITE_APB_REG(HDMI_ADDR_PORT, addr);

    WRITE_APB_REG(HDMI_DATA_PORT, data);
    rd_data = hdmi_rd_reg (addr);
    if (rd_data != data) {
        //while(1){};
    }
}

static unsigned int cec_get_ms_tick(void)
{
    unsigned int ret = 0;
    struct timeval cec_tick;
    do_gettimeofday(&cec_tick);
    ret = cec_tick.tv_sec * 1000 + cec_tick.tv_usec / 1000;

    return ret;
}

static unsigned int cec_get_ms_tick_interval(unsigned int last_tick)
{
    unsigned int ret = 0;
    unsigned int tick = 0;
    struct timeval cec_tick;
    do_gettimeofday(&cec_tick);
    tick = cec_tick.tv_sec * 1000 + cec_tick.tv_usec / 1000;

    if (last_tick < tick) ret = tick - last_tick;
    else ret = ((unsigned int)(-1) - last_tick) + tick;
    return ret;
}

#define TX_TIME_OUT_CNT 300

int cec_ll_tx(unsigned char *msg, unsigned char len, unsigned char *stat_header)
{
    int i;
    int ret;
    int tick = 0;
    int cnt = 0;

    spin_lock_irqsave(&cec_tx_lock, cec_tx_flags);

    for (i = 0; i < len; i++) {
        hdmi_wr_reg(CEC0_BASE_ADDR+CEC_TX_MSG_0_HEADER + i, msg[i]);
    }
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_TX_MSG_LENGTH, len-1);
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_TX_MSG_CMD, TX_REQ_CURRENT);

    if (stat_header == NULL) {
        tick = cec_get_ms_tick();

        while (hdmi_rd_reg(CEC0_BASE_ADDR+CEC_TX_MSG_STATUS) == TX_BUSY) {
            msleep(50);
            cnt = cec_get_ms_tick_interval(tick);
            if (cnt >= TX_TIME_OUT_CNT)
                break;
        }
    } else if (*stat_header == 1) { // ping
        tick = cec_get_ms_tick();

        while (hdmi_rd_reg(CEC0_BASE_ADDR+CEC_TX_MSG_STATUS) == TX_BUSY) {
            msleep(50);
            cnt = cec_get_ms_tick_interval(tick);
            if (cnt >= (TX_TIME_OUT_CNT / 2))
                break;
        }
    }

    ret = hdmi_rd_reg(CEC0_BASE_ADDR+CEC_TX_MSG_STATUS);
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_TX_MSG_CMD, TX_NO_OP);

    //if (cnt >= TX_TIME_OUT_CNT)
    //    hdmirx_cec_dbg_print("tx time out: cnt = %x\n", cnt);

    spin_unlock_irqrestore(&cec_tx_lock, cec_tx_flags);

    return ret;
}

#define RX_TIME_OUT_CNT 0x10

int cec_ll_rx( unsigned char *msg, unsigned char *len)
{
    unsigned char rx_status = hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_STATUS);
    int i;
    int tick = 0;
    int cnt = 0;
    unsigned char data;

    int rx_msg_length = hdmi_rd_reg(CEC0_BASE_ADDR + CEC_RX_MSG_LENGTH) + 1;

    for (i = 0; i < rx_msg_length; i++) {
        data = hdmi_rd_reg(CEC0_BASE_ADDR + CEC_RX_MSG_0_HEADER +i);
        *msg = data;
        msg++;
        //hdmirx_cec_dbg_print("cec rx message %x = %x\n", i, data);
    }
    *len = rx_msg_length;
    hdmi_wr_reg(CEC0_BASE_ADDR + CEC_RX_MSG_CMD,  RX_ACK_CURRENT);

    tick = cec_get_ms_tick();
    while (hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_STATUS) == RX_BUSY) {
        cnt = cec_get_ms_tick_interval(tick);
        if (cnt++ >= RX_TIME_OUT_CNT)
            break;
    }
    hdmi_wr_reg(CEC0_BASE_ADDR + CEC_RX_MSG_CMD,  RX_NO_OP);

    //if (cnt >= RX_TIME_OUT_CNT)
    //    hdmirx_cec_dbg_print("rx time out cnt = %x\n", cnt);

    return rx_status;
}

void cec_isr_post_process(void)
{
    /* isr post process */
    while(cec_rx_msg_buf.rx_read_pos != cec_rx_msg_buf.rx_write_pos) {
        cec_handle_message(&(cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_read_pos]));
        if (cec_rx_msg_buf.rx_read_pos == cec_rx_msg_buf.rx_buf_size - 1) {
            cec_rx_msg_buf.rx_read_pos = 0;
        } else {
            cec_rx_msg_buf.rx_read_pos++;
        }
    }

    //printk("[TV CEC RX]: rx_read_pos %x, rx_write_pos %x\n", cec_rx_msg_buf.rx_read_pos, cec_rx_msg_buf.rx_write_pos);
}

void cec_usr_cmd_post_process(void)
{
    cec_tx_message_list_t *p, *ptmp;

    /* usr command post process */
    spin_lock_irqsave(&p_tx_list_lock, cec_tx_list_flags);

    list_for_each_entry_safe(p, ptmp, &cec_tx_msg_phead, list) {
        cec_ll_tx(p->msg, p->length, NULL);
        unregister_cec_tx_msg(p);
    }

    spin_unlock_irqrestore(&p_tx_list_lock, cec_tx_list_flags);

    //printk("[TV CEC TX]: tx_msg_cnt = %x\n", tx_msg_cnt);
}

void cec_timer_post_process(void)
{
    /* timer post process*/
    if (cec_polling_state == TV_CEC_POLLING_ON) {
        cec_tv_polling_online_dev();
        cec_polling_state = TV_CEC_POLLING_OFF;
    }
}

static int tv_cec_task_handle(void *data)
{
    while (1) {
        down_interruptible(&tv_cec_sema);
        cec_isr_post_process();
        cec_usr_cmd_post_process();
        cec_timer_post_process();
    }

    return 0;
}

/* cec low level code end */

/* cec middle level code */

void tv_cec_timer_func(unsigned long arg)
{
    struct timer_list *timer = (struct timer_list *)arg;
    timer->expires = jiffies + TV_CEC_INTERVAL;

    if (cec_pending_flag == TV_CEC_PENDING_OFF) {
        cec_polling_state = TV_CEC_POLLING_ON;
    }

    add_timer(timer);

    up(&tv_cec_sema);
}

void register_cec_rx_msg(unsigned char *msg, unsigned char len )
{
    memset((void*)(&(cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos])), 0, sizeof(cec_rx_message_t));
    memcpy(cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos].content.buffer, msg, len);

    cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos].operand_num = len >= 2 ? len - 2 : 0;
    cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos].msg_length = len;

    if (cec_rx_msg_buf.rx_write_pos == cec_rx_msg_buf.rx_buf_size - 1) {
        cec_rx_msg_buf.rx_write_pos = 0;
    } else {
        cec_rx_msg_buf.rx_write_pos++;
    }

    up(&tv_cec_sema);    
}

void register_cec_tx_msg(unsigned char *msg, unsigned char len )
{
    cec_tx_message_list_t* cec_usr_message_list = kmalloc(sizeof(cec_tx_message_list_t), GFP_ATOMIC);

    if (cec_usr_message_list != NULL) {
        memset(cec_usr_message_list, 0, sizeof(cec_tx_message_list_t));
        memcpy(cec_usr_message_list->msg, msg, len);
        cec_usr_message_list->length = len;

        spin_lock_irqsave(&p_tx_list_lock, cec_tx_list_flags);
        list_add_tail(&cec_usr_message_list->list, &cec_tx_msg_phead);
        spin_unlock_irqrestore(&p_tx_list_lock, cec_tx_list_flags);

        tx_msg_cnt++;

        up(&tv_cec_sema);
    }
}

void unregister_cec_tx_msg(cec_tx_message_list_t* cec_tx_message_list)
{
    if (cec_tx_message_list != NULL) {
        list_del(&cec_tx_message_list->list);
        kfree(cec_tx_message_list);
        cec_tx_message_list = NULL;

        if (tx_msg_cnt > 0) tx_msg_cnt--;
    }
}

static void cec_recover_reset(void)
{
    WRITE_APB_REG(HDMI_CTRL_PORT, READ_APB_REG(HDMI_CTRL_PORT)|(1<<16));
    hdmi_wr_reg(OTHER_BASE_ADDR+HDMI_OTHER_CTRL0, 0xC);
    mdelay(10);
    hdmi_wr_reg(OTHER_BASE_ADDR+HDMI_OTHER_CTRL0, 0x0);
    WRITE_APB_REG(HDMI_CTRL_PORT, READ_APB_REG(HDMI_CTRL_PORT)&(~(1<<16)));

#ifdef _SUPPORT_CEC_TV_MASTER_
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_H, 0x00 );
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_L, 0xF9 );
#else
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_H, 0x07 );
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_L, 0x52 );
#endif

    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_LOGICAL_ADDR0, (0x1 << 4) | CEC0_LOG_ADDR);
}

static unsigned char check_cec_msg_valid(cec_rx_message_t* pcec_message)
{
    unsigned char rt = 0;
    unsigned char opcode;
    unsigned char opernum;
    if (!pcec_message)
        return rt;

    opcode = pcec_message->content.msg.opcode;
    opernum = pcec_message->operand_num;

    switch (opcode) {
    case CEC_OC_VENDOR_REMOTE_BUTTON_UP:
    case CEC_OC_STANDBY:
        if ( opernum == 0)  rt = 1;
        break;
    case CEC_OC_SET_SYSTEM_AUDIO_MODE:
        if ( opernum == 1)  rt = 1;
        break;
    case CEC_OC_VENDOR_COMMAND_WITH_ID:
        if ((opernum > 3)&&(opernum < 15))  rt = 1;
        break;
    case CEC_OC_VENDOR_REMOTE_BUTTON_DOWN:
        if (opernum < 15)  rt = 1;
        break;
    case CEC_OC_RECORD_OFF:
    case CEC_OC_RECORD_TV_SCREEN:
    case CEC_OC_TUNER_STEP_DECREMENT:
    case CEC_OC_TUNER_STEP_INCREMENT:
    case CEC_OC_GIVE_AUDIO_STATUS:
    case CEC_OC_GIVE_SYSTEM_AUDIO_MODE_STATUS:
    case CEC_OC_USER_CONTROL_RELEASED:
    case CEC_OC_GIVE_OSD_NAME:
    case CEC_OC_GIVE_PHYSICAL_ADDRESS:
    case CEC_OC_GET_CEC_VERSION:
    case CEC_OC_GET_MENU_LANGUAGE:
    case CEC_OC_GIVE_DEVICE_VENDOR_ID:
    case CEC_OC_GIVE_DEVICE_POWER_STATUS:
    case CEC_OC_TEXT_VIEW_ON:
    case CEC_OC_IMAGE_VIEW_ON:
    case CEC_OC_ABORT_MESSAGE:
        if (opernum == 0) rt = 1;
        break;
    case CEC_OC_RECORD_STATUS:
    case CEC_OC_DECK_CONTROL:
    case CEC_OC_DECK_STATUS:
    case CEC_OC_GIVE_DECK_STATUS:
    case CEC_OC_GIVE_TUNER_DEVICE_STATUS:
    case CEC_OC_PLAY:
    case CEC_OC_MENU_REQUEST:
    case CEC_OC_MENU_STATUS:
    case CEC_OC_REPORT_AUDIO_STATUS:
    case CEC_OC_TIMER_CLEARED_STATUS:
    case CEC_OC_SYSTEM_AUDIO_MODE_STATUS:
    case CEC_OC_USER_CONTROL_PRESSED:
    case CEC_OC_CEC_VERSION:
    case CEC_OC_REPORT_POWER_STATUS:
    case CEC_OC_SET_AUDIO_RATE:
        if (opernum == 1) rt = 1;
        break;
    case CEC_OC_INACTIVE_SOURCE:
    case CEC_OC_SYSTEM_AUDIO_MODE_REQUEST:
    case CEC_OC_FEATURE_ABORT:
        if (opernum == 2) rt = 1;
        break;
    case CEC_OC_SELECT_ANALOGUE_SERVICE:
        if (opernum == 4) rt = 1;
        break;
    case CEC_OC_SELECT_DIGITAL_SERVICE:
        if (opernum == 7) rt = 1;
        break;
    case CEC_OC_SET_ANALOGUE_TIMER:
    case CEC_OC_CLEAR_ANALOGUE_TIMER:
        if (opernum == 11) rt = 1;
        break;
    case CEC_OC_CLEAR_DIGITAL_TIMER:
    case CEC_OC_SET_DIGITAL_TIMER:
        if (opernum == 14) rt = 1;
        break;
    case CEC_OC_TIMER_STATUS:
        if ((opernum == 1 || opernum == 3)) rt = 1;
        break;
    case CEC_OC_TUNER_DEVICE_STATUS:
        if ((opernum == 5 || opernum == 8)) rt = 1;
        break;
    case CEC_OC_RECORD_ON:
        if (opernum > 0 && opernum < 9)  rt = 1;
        break;
    case CEC_OC_CLEAR_EXTERNAL_TIMER:
    case CEC_OC_SET_EXTERNAL_TIMER:
        if ((opernum == 9 || opernum == 10)) rt = 1;
        break;
    case CEC_OC_SET_TIMER_PROGRAM_TITLE:
    case CEC_OC_SET_OSD_NAME:
        if (opernum > 0 && opernum < 15) rt = 1;
        break;
    case CEC_OC_SET_OSD_STRING:
        if (opernum > 1 && opernum < 15) rt = 1;
        break;
    case CEC_OC_VENDOR_COMMAND:
        if (opernum < 15)   rt = 1;
        break;
    case CEC_OC_REQUEST_ACTIVE_SOURCE:
        if (opernum == 0) rt = 1;
        break;
    case CEC_OC_REPORT_PHYSICAL_ADDRESS:
    case CEC_OC_SET_MENU_LANGUAGE:
    case CEC_OC_DEVICE_VENDOR_ID:
        if (opernum == 3) rt = 1;
        break;
    case CEC_OC_ROUTING_CHANGE:
        if (opernum == 4) rt = 1;
        break;
    case CEC_OC_ACTIVE_SOURCE:
    case CEC_OC_ROUTING_INFORMATION:
    case CEC_OC_SET_STREAM_PATH:
        if (opernum == 2) rt = 1;
        break;
    default:
        rt = 1;
        break;
    }

    if (rt == 0)
        hdmirx_cec_dbg_print("opcode and opernum not match: %x, %x\n", opcode, opernum);

    rt = 1; // temporal
    return rt;
}

static irqreturn_t cec_handler(int irq, void *dev_instance)
{
    unsigned int data;

    if (cec_pending_flag == TV_CEC_PENDING_ON) {
        WRITE_MPEG_REG(A9_0_IRQ_IN1_INTR_STAT_CLR, 1 << 23);             // Clear the interrupt
        return;
    }

    data = hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_STATUS);
    if (data) {
        //hdmirx_cec_dbg_print("CEC Irq Rx Status %x\n", data);
        if ((data & 0x3) == RX_DONE) {
            data = hdmi_rd_reg(CEC0_BASE_ADDR + CEC_RX_NUM_MSG);
            if (data == 1) {
                unsigned char rx_msg[MAX_MSG], rx_len;
                cec_ll_rx(rx_msg, &rx_len);
                register_cec_rx_msg(rx_msg, rx_len);
            } else {
                hdmi_wr_reg(CEC0_BASE_ADDR + CEC_RX_CLEAR_BUF,  0x01);
                hdmi_wr_reg(CEC0_BASE_ADDR + CEC_RX_MSG_CMD,  RX_NO_OP);
                cec_recover_reset();
                hdmirx_cec_dbg_print("Error: CEC1->CEC0 transmit data fail, rx_num_msg = %x  !", data);
            }
        } else {
            hdmirx_cec_dbg_print("Error: CEC1->CEC0 transmit data fail, msg_status = %x!", data);
            hdmi_wr_reg(CEC0_BASE_ADDR + CEC_RX_CLEAR_BUF,  0x01);
            hdmi_wr_reg(CEC0_BASE_ADDR + CEC_RX_MSG_CMD,  RX_NO_OP);
            cec_recover_reset();

        }
    }

    data = hdmi_rd_reg(CEC0_BASE_ADDR+CEC_TX_MSG_STATUS);
    if (data) {
        //hdmirx_cec_dbg_print("CEC Irq Tx Status %x\n", data);
    }

    WRITE_MPEG_REG(A9_0_IRQ_IN1_INTR_STAT_CLR, 1 << 23);             // Clear the interrupt

    return IRQ_HANDLED;
}

static unsigned short cec_log_addr_to_dev_type(unsigned char log_addr)
{
    unsigned short us = CEC_UNREGISTERED_DEVICE_TYPE;
    if ((1 << log_addr) & CEC_DISPLAY_DEVICE) {
        us = CEC_DISPLAY_DEVICE_TYPE;
    } else if ((1 << log_addr) & CEC_RECORDING_DEVICE) {
        us = CEC_RECORDING_DEVICE_TYPE;
    } else if ((1 << log_addr) & CEC_PLAYBACK_DEVICE) {
        us = CEC_PLAYBACK_DEVICE_TYPE;
    } else if ((1 << log_addr) & CEC_TUNER_DEVICE) {
        us = CEC_TUNER_DEVICE_TYPE;
    } else if ((1 << log_addr) & CEC_AUDIO_SYSTEM_DEVICE) {
        us = CEC_AUDIO_SYSTEM_DEVICE_TYPE;
    }

    return us;
}

static cec_hdmi_port_e cec_find_hdmi_port(unsigned char log_addr)
{
    cec_hdmi_port_e rt = CEC_HDMI_PORT_UKNOWN;

    if ((cec_global_info.dev_mask & (1 << log_addr)) &&
            (cec_global_info.cec_node_info[log_addr].phy_addr != 0) &&
            (cec_global_info.cec_node_info[log_addr].hdmi_port == CEC_HDMI_PORT_UKNOWN)) {
        if ((cec_global_info.cec_node_info[log_addr].phy_addr & 0xF000) == 0x1000) {
            cec_global_info.cec_node_info[log_addr].hdmi_port = CEC_HDMI_PORT_1;
        } else if ((cec_global_info.cec_node_info[log_addr].phy_addr & 0xF000) == 0x2000) {
            cec_global_info.cec_node_info[log_addr].hdmi_port = CEC_HDMI_PORT_2;
        } else if ((cec_global_info.cec_node_info[log_addr].phy_addr & 0xF000) == 0x3000) {
            cec_global_info.cec_node_info[log_addr].hdmi_port = CEC_HDMI_PORT_3;
        }
    }

    rt = cec_global_info.cec_node_info[log_addr].hdmi_port;

    return rt;
}

// -------------- command from cec devices ---------------------

void cec_tv_polling_online_dev(void)
{
    int log_addr = 0;
    int r;
    unsigned short dev_mask_tmp = 0;
    unsigned char msg[1];
    unsigned char ping = 1;

    for (log_addr = 1; log_addr < CEC_UNREGISTERED_ADDR; log_addr++) {
        msg[0] = log_addr;
        r = cec_ll_tx(msg, 1, &ping);
        if (r != TX_DONE) {
            dev_mask_tmp &= ~(1 << log_addr);
            memset(&(cec_global_info.cec_node_info[log_addr]), 0, sizeof(cec_node_info_t));
        }
        if (r == TX_DONE) {
            dev_mask_tmp |= 1 << log_addr;
            cec_global_info.cec_node_info[log_addr].log_addr = log_addr;
            cec_global_info.cec_node_info[log_addr].dev_type = cec_log_addr_to_dev_type(log_addr);
            cec_find_hdmi_port(log_addr);
        }
    }

    if (cec_global_info.dev_mask != dev_mask_tmp) {
        cec_global_info.dev_mask = dev_mask_tmp;
    }

    //hdmirx_cec_dbg_print("cec log device exist: %x\n", dev_mask_tmp);
}

void cec_report_phy_addr(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    cec_global_info.dev_mask |= 1 << log_addr;
    cec_global_info.cec_node_info[log_addr].dev_type = cec_log_addr_to_dev_type(log_addr);
    cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_DEVICE_TYPE;
    memcpy(cec_global_info.cec_node_info[log_addr].osd_name_def, default_osd_name[log_addr], 16);
    if ((cec_global_info.cec_node_info[log_addr].real_info_mask & INFO_MASK_OSD_NAME) == 0) {
        memcpy(cec_global_info.cec_node_info[log_addr].osd_name, osd_name_uninit, 16);
    }
    cec_global_info.cec_node_info[log_addr].log_addr = log_addr;
    cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_LOGIC_ADDRESS;
    cec_global_info.cec_node_info[log_addr].phy_addr = (pcec_message->content.msg.operands[0] << 8) | pcec_message->content.msg.operands[1];
    cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_PHYSICAL_ADDRESS;

    hdmirx_cec_dbg_print("cec_report_phy_addr: %s\n", cec_global_info.cec_node_info[log_addr].osd_name_def);
}

void cec_report_power_status(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        cec_global_info.cec_node_info[log_addr].power_status = pcec_message->content.msg.operands[0];
        cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_POWER_STATUS;
        hdmirx_cec_dbg_print("cec_report_power_status: %x\n", cec_global_info.cec_node_info[log_addr].power_status);
    }
}

void cec_feature_abort(cec_rx_message_t* pcec_message)
{
    hdmirx_cec_dbg_print("cec_feature_abort: opcode %x\n", pcec_message->content.msg.opcode);
}

void cec_report_version(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        cec_global_info.cec_node_info[log_addr].cec_version = pcec_message->content.msg.operands[0];
        cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_CEC_VERSION;
        hdmirx_cec_dbg_print("cec_report_version: %x\n", cec_global_info.cec_node_info[log_addr].cec_version);
    }
}

void cec_active_source(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        cec_global_info.active_log_dev = log_addr;
        hdmirx_cec_dbg_print("cec_active_source: %x\n", log_addr);
    }
}

void cec_deactive_source(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        if (cec_global_info.active_log_dev == log_addr) {
        cec_global_info.active_log_dev = 0;
        }
        hdmirx_cec_dbg_print("cec_deactive_source: %x\n", log_addr);
    }
}

void cec_get_version(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        unsigned char msg[3];
        msg[0] = log_addr;
        msg[1] = CEC_OC_CEC_VERSION;
        msg[2] = CEC_VERSION_13A;
        cec_ll_tx(msg, 3, NULL);
    }
}

void cec_give_deck_status(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {

    }
}

void cec_menu_status(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        cec_global_info.cec_node_info[log_addr].menu_state = pcec_message->content.msg.operands[0];
        cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_MENU_STATE;
        hdmirx_cec_dbg_print("cec_menu_status: %x\n", cec_global_info.cec_node_info[log_addr].menu_state);
    }
}

void cec_deck_status(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        cec_global_info.cec_node_info[log_addr].specific_info.playback.deck_info = pcec_message->content.msg.operands[0];
        cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_DECK_INfO;
        hdmirx_cec_dbg_print("cec_deck_status: %x\n", cec_global_info.cec_node_info[log_addr].specific_info.playback.deck_info);
    }
}

void cec_device_vendor_id(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        int i, tmp = 0;
        for (i = 0; i < pcec_message->operand_num; i++) {
            tmp |= (pcec_message->content.msg.operands[i] << ((pcec_message->operand_num - i - 1)*8));
        }
        cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id= tmp;
        cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id_byte_num = pcec_message->operand_num;
        cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_VENDOR_ID;
        hdmirx_cec_dbg_print("cec_device_vendor_id: %x\n", cec_global_info.cec_node_info[log_addr].vendor_id);
    }
}

void cec_set_osd_name(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        memcpy(cec_global_info.cec_node_info[log_addr].osd_name,  pcec_message->content.msg.operands, 14);
        cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_OSD_NAME;
        hdmirx_cec_dbg_print("cec_set_osd_name: %s\n", cec_global_info.cec_node_info[log_addr].osd_name);
    }
}

void cec_vendor_cmd_with_id(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        if (cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id_byte_num != 0) {
            int i = cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id_byte_num;
            int tmp = 0;
            for ( ; i < pcec_message->operand_num; i++) {
                tmp |= (pcec_message->content.msg.operands[i] << ((cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id_byte_num - i - 1)*8));
            }
            hdmirx_cec_dbg_print("cec_vendor_cmd_with_id: %x, %x\n", cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id, tmp);
        }
    }
}

void cec_set_menu_language(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        if (pcec_message->operand_num == 3) {
            int i;
            unsigned int tmp = ((pcec_message->content.msg.operands[0] << 16)  |
                                (pcec_message->content.msg.operands[1] << 8) |
                                (pcec_message->content.msg.operands[2]));

            hdmirx_cec_dbg_print("%c, %c, %c\n", pcec_message->content.msg.operands[0],
                                 pcec_message->content.msg.operands[1],
                                 pcec_message->content.msg.operands[2]);

            for (i = 0; i < (sizeof(menu_lang_array)/sizeof(menu_lang_array[0])); i++) {
                if (menu_lang_array[i] == tmp)
                    break;
            }

            cec_global_info.cec_node_info[log_addr].menu_lang = i;

            cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_MENU_LANGUAGE;

            hdmirx_cec_dbg_print("cec_set_menu_language: %x\n", cec_global_info.cec_node_info[log_addr].menu_lang);
        }
    }
}

void cec_handle_message(cec_rx_message_t* pcec_message)
{
    unsigned char	brdcst, opcode;
    unsigned char	initiator, follower;
    unsigned char   operand_num;
    unsigned char   msg_length;

    /* parse message */
    if ((!pcec_message) || (check_cec_msg_valid(pcec_message) == 0)) return;

    initiator	= pcec_message->content.msg.header >> 4;
    follower	= pcec_message->content.msg.header & 0x0f;
    opcode		= pcec_message->content.msg.opcode;
    operand_num = pcec_message->operand_num;
    brdcst      = (follower == 0x0f);
    msg_length  = pcec_message->msg_length;

    /* process messages from tv polling and cec devices */

    switch (opcode) {
    case CEC_OC_ACTIVE_SOURCE:
        cec_active_source(pcec_message);
        break;
    case CEC_OC_INACTIVE_SOURCE:
        cec_deactive_source(pcec_message);
        break;
    case CEC_OC_CEC_VERSION:
        cec_report_version(pcec_message);
        break;
    case CEC_OC_DECK_STATUS:
        cec_deck_status(pcec_message);
        break;
    case CEC_OC_DEVICE_VENDOR_ID:
        cec_device_vendor_id(pcec_message);
        break;
    case CEC_OC_FEATURE_ABORT:
        cec_feature_abort(pcec_message);
        break;
    case CEC_OC_GET_CEC_VERSION:
        cec_get_version(pcec_message);
        break;
    case CEC_OC_GIVE_DECK_STATUS:
        cec_give_deck_status(pcec_message);
        break;
    case CEC_OC_MENU_STATUS:
        cec_menu_status(pcec_message);
        break;
    case CEC_OC_REPORT_PHYSICAL_ADDRESS:
        cec_report_phy_addr(pcec_message);
        break;
    case CEC_OC_REPORT_POWER_STATUS:
        cec_report_power_status(pcec_message);
        break;
    case CEC_OC_SET_OSD_NAME:
        cec_set_osd_name(pcec_message);
        break;
    case CEC_OC_VENDOR_COMMAND_WITH_ID:
        cec_vendor_cmd_with_id(pcec_message);
        break;
    case CEC_OC_SET_MENU_LANGUAGE:
        cec_set_menu_language(pcec_message);
        break;
    case CEC_OC_VENDOR_REMOTE_BUTTON_DOWN:
    case CEC_OC_VENDOR_REMOTE_BUTTON_UP:
    case CEC_OC_CLEAR_ANALOGUE_TIMER:
    case CEC_OC_CLEAR_DIGITAL_TIMER:
    case CEC_OC_CLEAR_EXTERNAL_TIMER:
    case CEC_OC_DECK_CONTROL:
    case CEC_OC_GIVE_DEVICE_POWER_STATUS:
    case CEC_OC_GIVE_DEVICE_VENDOR_ID:
    case CEC_OC_GIVE_OSD_NAME:
    case CEC_OC_GIVE_PHYSICAL_ADDRESS:
    case CEC_OC_GIVE_SYSTEM_AUDIO_MODE_STATUS:
    case CEC_OC_GIVE_TUNER_DEVICE_STATUS:
    case CEC_OC_IMAGE_VIEW_ON:
    case CEC_OC_MENU_REQUEST:
    case CEC_OC_SET_OSD_STRING:
    case CEC_OC_SET_STREAM_PATH:
    case CEC_OC_SET_SYSTEM_AUDIO_MODE:
    case CEC_OC_SET_TIMER_PROGRAM_TITLE:
    case CEC_OC_STANDBY:
    case CEC_OC_SYSTEM_AUDIO_MODE_REQUEST:
    case CEC_OC_SYSTEM_AUDIO_MODE_STATUS:
    case CEC_OC_TEXT_VIEW_ON:
    case CEC_OC_TIMER_CLEARED_STATUS:
    case CEC_OC_TIMER_STATUS:
    case CEC_OC_TUNER_DEVICE_STATUS:
    case CEC_OC_TUNER_STEP_DECREMENT:
    case CEC_OC_TUNER_STEP_INCREMENT:
    case CEC_OC_USER_CONTROL_PRESSED:
    case CEC_OC_USER_CONTROL_RELEASED:
    case CEC_OC_VENDOR_COMMAND:
    case CEC_OC_REQUEST_ACTIVE_SOURCE:
    case CEC_OC_ROUTING_CHANGE:
    case CEC_OC_ROUTING_INFORMATION:
    case CEC_OC_SELECT_ANALOGUE_SERVICE:
    case CEC_OC_SELECT_DIGITAL_SERVICE:
    case CEC_OC_SET_ANALOGUE_TIMER :
    case CEC_OC_SET_AUDIO_RATE:
    case CEC_OC_SET_DIGITAL_TIMER:
    case CEC_OC_SET_EXTERNAL_TIMER:
    case CEC_OC_PLAY:
    case CEC_OC_RECORD_OFF:
    case CEC_OC_RECORD_ON:
    case CEC_OC_RECORD_STATUS:
    case CEC_OC_RECORD_TV_SCREEN:
    case CEC_OC_REPORT_AUDIO_STATUS:
    case CEC_OC_GET_MENU_LANGUAGE:
    case CEC_OC_GIVE_AUDIO_STATUS:
    case CEC_OC_ABORT_MESSAGE:
        printk("\n=========================\n");
        printk("Get not support cec command: %x\n", opcode);
        printk("\n=========================\n");
        break;
    default:
        break;
    }
}


// --------------- cec command from user application --------------------

void cec_usrcmd_parse_all_dev_online(void)
{
    int i;
    unsigned short tmp_mask;

    hdmirx_cec_dbg_print("cec online: ###############################################\n");
    hdmirx_cec_dbg_print("active_log_dev %x\n", cec_global_info.active_log_dev);
    for (i = 0; i < MAX_NUM_OF_DEV; i++) {
        tmp_mask = 1 << i;
        if (tmp_mask & cec_global_info.dev_mask) {
            hdmirx_cec_dbg_print("cec online: -------------------------------------------\n");
            hdmirx_cec_dbg_print("hdmi_port:     %x\n", cec_global_info.cec_node_info[i].hdmi_port);
            hdmirx_cec_dbg_print("dev_type:      %x\n", cec_global_info.cec_node_info[i].dev_type);
            hdmirx_cec_dbg_print("power_status:  %x\n", cec_global_info.cec_node_info[i].power_status);
            hdmirx_cec_dbg_print("cec_version:   %x\n", cec_global_info.cec_node_info[i].cec_version);
            hdmirx_cec_dbg_print("vendor_id:     %x\n", cec_global_info.cec_node_info[i].vendor_id.vendor_id);
            hdmirx_cec_dbg_print("phy_addr:      %x\n", cec_global_info.cec_node_info[i].phy_addr);
            hdmirx_cec_dbg_print("log_addr:      %x\n", cec_global_info.cec_node_info[i].log_addr);
            hdmirx_cec_dbg_print("osd_name:      %s\n", cec_global_info.cec_node_info[i].osd_name);
            hdmirx_cec_dbg_print("osd_name_def:  %s\n", cec_global_info.cec_node_info[i].osd_name_def);
            hdmirx_cec_dbg_print("menu_state:    %x\n", cec_global_info.cec_node_info[i].menu_state);

            if (cec_global_info.cec_node_info[i].dev_type == CEC_PLAYBACK_DEVICE_TYPE) {
                hdmirx_cec_dbg_print("deck_cnt_mode: %x\n", cec_global_info.cec_node_info[i].specific_info.playback.deck_cnt_mode);
                hdmirx_cec_dbg_print("deck_info:     %x\n", cec_global_info.cec_node_info[i].specific_info.playback.deck_info);
                hdmirx_cec_dbg_print("play_mode:     %x\n", cec_global_info.cec_node_info[i].specific_info.playback.play_mode);
            }
        }
    }
    hdmirx_cec_dbg_print("##############################################################\n");
}

void cec_usrcmd_get_cec_version(unsigned char log_addr)
{
    MSG_P0(cec_global_info.tv_log_addr, log_addr, CEC_OC_CEC_VERSION);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_audio_status(unsigned char log_addr)
{
    MSG_P0(cec_global_info.tv_log_addr, log_addr, CEC_OC_GIVE_AUDIO_STATUS);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_deck_status(unsigned char log_addr)
{
    MSG_P1(cec_global_info.tv_log_addr, log_addr, CEC_OC_GIVE_DECK_STATUS, STATUS_REQ_ON);

    register_cec_tx_msg(gbl_msg, 3);
}

void cec_usrcmd_set_deck_cnt_mode(unsigned char log_addr, deck_cnt_mode_e deck_cnt_mode)
{
    MSG_P1(cec_global_info.tv_log_addr, log_addr, CEC_OC_DECK_CONTROL, deck_cnt_mode);

    register_cec_tx_msg(gbl_msg, 3);
}

void cec_usrcmd_get_device_power_status(unsigned char log_addr)
{
    MSG_P0(cec_global_info.tv_log_addr, log_addr, CEC_OC_GIVE_DEVICE_POWER_STATUS);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_device_vendor_id(unsigned char log_addr)
{
    MSG_P0(cec_global_info.tv_log_addr, log_addr, CEC_OC_GIVE_DEVICE_VENDOR_ID);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_osd_name(unsigned char log_addr)
{
    MSG_P0(cec_global_info.tv_log_addr, log_addr, CEC_OC_GIVE_OSD_NAME);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_physical_address(unsigned char log_addr)
{
    MSG_P0(cec_global_info.tv_log_addr, log_addr, CEC_OC_GIVE_PHYSICAL_ADDRESS);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_system_audio_mode_status(unsigned char log_addr)
{
    MSG_P0(cec_global_info.tv_log_addr, log_addr, CEC_OC_GIVE_SYSTEM_AUDIO_MODE_STATUS);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_set_standby(unsigned char log_addr)
{
    MSG_P0(cec_global_info.tv_log_addr, log_addr, CEC_OC_STANDBY);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_set_imageview_on(unsigned char log_addr)
{
    MSG_P0(cec_global_info.tv_log_addr, log_addr, CEC_OC_IMAGE_VIEW_ON);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_tuner_device_status(unsigned char log_addr)
{
    MSG_P0(cec_global_info.tv_log_addr, log_addr, CEC_OC_GIVE_TUNER_DEVICE_STATUS);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_set_play_mode(unsigned char log_addr, play_mode_e play_mode)
{
    MSG_P1(cec_global_info.tv_log_addr, log_addr, CEC_OC_PLAY, play_mode);

    register_cec_tx_msg(gbl_msg, 3);
}

void cec_usrcmd_get_menu_state(unsigned char log_addr)
{
    MSG_P1(cec_global_info.tv_log_addr, log_addr, CEC_OC_MENU_REQUEST, MENU_REQ_QUERY);

    register_cec_tx_msg(gbl_msg, 3);
}

void cec_usrcmd_set_menu_state(unsigned char log_addr, menu_req_type_e menu_req_type)
{
    MSG_P1(cec_global_info.tv_log_addr, log_addr, CEC_OC_MENU_REQUEST, menu_req_type);

    register_cec_tx_msg(gbl_msg, 3);
}

void cec_usrcmd_get_menu_language(unsigned char log_addr)
{
    MSG_P0(cec_global_info.tv_log_addr, log_addr, CEC_OC_GET_MENU_LANGUAGE);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_set_menu_language(unsigned char log_addr, cec_menu_lang_e menu_lang)
{
    MSG_P3(cec_global_info.tv_log_addr, log_addr, CEC_OC_SET_MENU_LANGUAGE, (menu_lang_array[menu_lang]>>16)&0xFF,
           (menu_lang_array[menu_lang]>>8)&0xFF,
           (menu_lang_array[menu_lang])&0xFF);
    register_cec_tx_msg(gbl_msg, 5);
}

void cec_usrcmd_get_active_source(void)
{
    MSG_P0(cec_global_info.tv_log_addr, 0xF, CEC_OC_REQUEST_ACTIVE_SOURCE);
        
    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_set_active_source(unsigned char log_addr)
{
    MSG_P2(cec_global_info.tv_log_addr, log_addr, CEC_OC_ACTIVE_SOURCE,
                                                  (unsigned char)(cec_global_info.cec_node_info[log_addr].phy_addr >> 8),
                                                  (unsigned char)(cec_global_info.cec_node_info[log_addr].phy_addr & 0xFF));

    register_cec_tx_msg(gbl_msg, 4);
}

void cec_usrcmd_set_deactive_source(unsigned char log_addr)
{
    MSG_P2(cec_global_info.tv_log_addr, log_addr, CEC_OC_INACTIVE_SOURCE, 
                                                  (unsigned char)(cec_global_info.cec_node_info[log_addr].phy_addr >> 8),
                                                  (unsigned char)(cec_global_info.cec_node_info[log_addr].phy_addr & 0xFF));

    register_cec_tx_msg(gbl_msg, 4);
}

void cec_usrcmd_clear_node_dev_real_info_mask(unsigned char log_addr, cec_info_mask mask)
{
    cec_global_info.cec_node_info[log_addr].real_info_mask &= ~mask;
}

void cec_usrcmd_set_stream_path(unsigned char log_addr)
{
    MSG_P2(cec_global_info.tv_log_addr, log_addr, CEC_OC_SET_STREAM_PATH, 
                                                  (unsigned char)(cec_global_info.cec_node_info[log_addr].phy_addr >> 8),
                                                  (unsigned char)(cec_global_info.cec_node_info[log_addr].phy_addr & 0xFF));

    register_cec_tx_msg(gbl_msg, 4);
}

/* cec middle level code end */

/* cec high level code */

static struct timer_list tv_cec_timer;

static unsigned char dev = 0;
static unsigned char cec_init_flag = 0;
static unsigned char cec_mutex_flag = 0;

void cec_init(void)
{
    // cec Clock
#ifdef _SUPPORT_CEC_TV_MASTER_
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_H, 0x00 );
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_L, 0xF9 );
#else
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_H, 0x07 );
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_L, 0x52 );
#endif
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_LOGICAL_ADDR0, (0x1 << 4) | CEC0_LOG_ADDR);

    if (cec_init_flag == 1) return;

    cec_rx_msg_buf.rx_write_pos = 0;
    cec_rx_msg_buf.rx_read_pos = 0;
    cec_rx_msg_buf.rx_buf_size = sizeof(cec_rx_msg_buf.cec_rx_message)/sizeof(cec_rx_msg_buf.cec_rx_message[0]);
    memset(cec_rx_msg_buf.cec_rx_message, 0, sizeof(cec_rx_msg_buf.cec_rx_message));

    memset(&cec_global_info, 0, sizeof(cec_global_info_t));

    if (cec_mutex_flag == 0) {
        init_MUTEX(&tv_cec_sema);
        cec_mutex_flag = 1;
    }
    
    kthread_run(tv_cec_task_handle, &dev, "kthread_cec");

    request_irq(INT_HDMI_CEC, &cec_handler,
                IRQF_SHARED, "amhdmirx",
                (void *)"amhdmirx");

    WRITE_MPEG_REG(A9_0_IRQ_IN1_INTR_STAT_CLR, READ_MPEG_REG(A9_0_IRQ_IN1_INTR_STAT_CLR) | (1 << 23));    // Clear the interrupt
    WRITE_MPEG_REG(A9_0_IRQ_IN1_INTR_MASK, READ_MPEG_REG(A9_0_IRQ_IN1_INTR_MASK) | (1 << 23));            // Enable the hdmi cec interrupt

    init_timer(&tv_cec_timer);
    tv_cec_timer.data = (ulong) & tv_cec_timer;
    tv_cec_timer.function = tv_cec_timer_func;
    tv_cec_timer.expires = jiffies + TV_CEC_INTERVAL;
    add_timer(&tv_cec_timer);

    cec_init_flag = 1;

    return;
}

void cec_uninit(void)
{
    if (cec_init_flag == 1) {
        WRITE_MPEG_REG(A9_0_IRQ_IN1_INTR_MASK, READ_MPEG_REG(A9_0_IRQ_IN1_INTR_MASK) & ~(1 << 23));            // Disable the hdmi cec interrupt
        free_irq(INT_HDMI_CEC, (void *)"amhdmirx");

        del_timer_sync(&tv_cec_timer);

        cec_init_flag = 0;
    }
}

void cec_set_pending(tv_cec_pending_e on_off)
{
    cec_pending_flag = on_off;
}

size_t cec_usrcmd_get_global_info(char * buf)
{
    int i = 0;
    int dev_num = 0;

    cec_node_info_t * buf_node_addr = (cec_node_info_t *)(buf + (unsigned int)(((cec_global_info_to_usr_t*)0)->cec_node_info_online));

    for (i = 0; i < MAX_NUM_OF_DEV; i++) {
        if (cec_global_info.dev_mask & (1 << i)) {
            memcpy(&(buf_node_addr[dev_num]), &(cec_global_info.cec_node_info[i]), sizeof(cec_node_info_t));
            dev_num++;
        }
    }

    buf[0] = dev_num;
    buf[1] = cec_global_info.active_log_dev;
#if 0
    printk("\n");
    printk("%x\n",(unsigned int)(((cec_global_info_to_usr_t*)0)->cec_node_info_online));
    printk("%x\n", ((cec_global_info_to_usr_t*)buf)->dev_number);
    printk("%x\n", ((cec_global_info_to_usr_t*)buf)->active_log_dev);
    printk("%x\n", ((cec_global_info_to_usr_t*)buf)->cec_node_info_online[0].hdmi_port);
    for (i=0; i < (sizeof(cec_node_info_t) * dev_num) + 2; i++) {
        printk("%x,",buf[i]);
    }
    printk("\n");
#endif
    return (sizeof(cec_node_info_t) * dev_num) + (unsigned int)(((cec_global_info_to_usr_t*)0)->cec_node_info_online);
}

void cec_usrcmd_set_dispatch(const char * buf, size_t count)
{
    int i;
    usr_cmd_type_e usr_cmd_type = 0;
    int param1 = 0;
    int param2 = 0;
    int param3 = 0;
    int param4 = 0;
    int param5 = 0;

    hdmirx_cec_dbg_print("cec_usrcmd_set_dispatch: \n");
    for (i = 0; i < count; i++) {
        hdmirx_cec_dbg_print("%x,", buf[i]);
    }
    hdmirx_cec_dbg_print("\n");

    if (count < 2) return;

    usr_cmd_type = buf[0];

    if (count == 2) {
        param1 = buf[1];
    } else if (count == 3) {
        param1 = buf[1];
        param2 = buf[2];
    } else if (count == 4) {
        param1 = buf[1];
        param2 = buf[2];
        param3 = buf[3];
    } else if (count == 5) {
        param1 = buf[1];
        param2 = buf[2];
        param3 = buf[3];
        param4 = buf[4];
    } else if (count == 6) {
        param1 = buf[1];
        param2 = buf[2];
        param3 = buf[3];
        param4 = buf[4];
        param5 = buf[5];
    }

    switch (usr_cmd_type) {
    case GET_CEC_VERSION:
        cec_usrcmd_get_cec_version(param1);
        break;
    case GET_DEV_POWER_STATUS:
        cec_usrcmd_get_device_power_status(param1);
        break;
    case GET_DEV_VENDOR_ID:
        cec_usrcmd_get_device_vendor_id(param1);
        break;
    case GET_OSD_NAME:
        cec_usrcmd_get_osd_name(param1);
        break;
    case GET_PHYSICAL_ADDR:
        cec_usrcmd_get_physical_address(param1);
        break;
    case SET_STANDBY:
        cec_usrcmd_set_standby(param1);
        break;
    case SET_IMAGEVIEW_ON:
        cec_usrcmd_set_standby(param1);
        break;
    case GIVE_DECK_STATUS:
        cec_usrcmd_get_deck_status(param1);
        break;
    case SET_DECK_CONTROL_MODE:
        cec_usrcmd_set_deck_cnt_mode(param1, param2);
        break;
    case SET_PLAY_MODE:
        cec_usrcmd_set_play_mode(param1, param2);
        break;
    case GET_SYSTEM_AUDIO_MODE:
        cec_usrcmd_get_system_audio_mode_status(param1);
        break;
    case GET_TUNER_DEV_STATUS:
        cec_usrcmd_get_tuner_device_status(param1);
        break;
    case GET_AUDIO_STATUS:
        cec_usrcmd_get_audio_status(param1);
        break;
    case GET_OSD_STRING:
        break;
    case GET_MENU_STATE:
        cec_usrcmd_get_menu_state(param1);
        break;
    case SET_MENU_STATE:
        cec_usrcmd_set_menu_state(param1, param2);
        break;
    case SET_MENU_LANGAGE:
        cec_usrcmd_set_menu_language(param1, param2);
        break;
    case GET_MENU_LANGUAGE:
        cec_usrcmd_get_menu_language(param1);
        break;
    case GET_ACTIVE_SOURCE:
        cec_usrcmd_get_active_source();
        break;
    case SET_ACTIVE_SOURCE:
        cec_usrcmd_set_active_source(param1);
        break;
    case SET_DEACTIVE_SOURCE:
        cec_usrcmd_set_deactive_source(param1);
        break;
    case CLR_NODE_DEV_REAL_INFO_MASK:
        cec_usrcmd_clear_node_dev_real_info_mask(param1, (((cec_info_mask)param2) << 24) |
                                                         (((cec_info_mask)param3) << 16) |
                                                         (((cec_info_mask)param4) << 8)  |
                                                         ((cec_info_mask)param5));
        break;
    case SET_STREAM_PATH:
        cec_usrcmd_set_stream_path(param1);
        break;
    default:
        break;
    }
}

/* cec high level code end */

