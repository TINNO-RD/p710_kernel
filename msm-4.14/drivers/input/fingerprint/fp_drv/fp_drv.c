/*
 * Simple synchronous userspace interface to SPI devices
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
 * Copyright (C) 2007 David Brownell (simplification, cleanup)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/leds.h>
#include <linux/timer.h>
#include <linux/timex.h>

#include <linux/wakelock.h>
#include <linux/poll.h>

#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/of.h>


#include "fp_drv.h"
/******************************************************************/
#define __POLL_MSG__  (0)
#define __FP_DRV_FB_NOTIFIER__ (1)

#define FP_DEV_NAME "fp_drv"
#define CHRD_DRIVER_NAME	"fp_drv_chrd"
#define FP_CLASS_NAME "fp_drv"

///add for tz log, by wenguangyu, start
#define  FP_IOC_MAGIC    't'
#define  TZ_LOG_IOC_DISABLE_OUTPUT	_IO(FP_IOC_MAGIC, 1)
#define  TZ_LOG_IOC_ENABLE_OUTPUT	_IO(FP_IOC_MAGIC, 2)
// fp quick wakeup.
#define  FP_IOC_BL_CONTROL    _IO(FP_IOC_MAGIC, 10)
#define  FP_IOC_GET_EVENT    _IO(FP_IOC_MAGIC, 11)
#define  FP_IOC_RESET_EVENT    _IO(FP_IOC_MAGIC, 12)
#define  FP_IOC_QWK_ENABLE    _IO(FP_IOC_MAGIC, 13)

#define FP_BL_LOCK_RESET (0) 
#define FP_BL_LOCK (1) 
#define FP_BL_UNLOCK_AND_TRIGGER (2)


#if __FP_DRV_FB_NOTIFIER__
#define HW_EVENT_FB_BLANK (10)
#define HW_EVENT_FB_UNBLANK (11)
#endif

#define M_TIME_OUT (HZ*1) // 1000ms

#define ESCAPE() ((long)(timeval_to_ns(&t_end) - timeval_to_ns(&t_start)) / 1000)
#define ESCAPE_MS() (ESCAPE() / 1000) + 10
#define ESCAPE_US() (ESCAPE() % 1000)
#define is_valid(x) ((long)timeval_to_ns(&x)/ 1000)


#if __POLL_MSG__
#define KILL_MSG(x) \
    do { \
        klog("[guomingyi], event: %d, %s:%d\n",x,__func__,__LINE__); \
        fp_report_event = x; \
        wake_up(&fp_poll_wq); \
    } while(0);
#else
#define KILL_MSG(x) \
    do { \
        if (fasync_queue) { \
            klog("[guomingyi], event: %d, %s:%d\n",x,__func__,__LINE__); \
            fp_report_event = x; \
            kill_fasync(&fasync_queue, SIGIO, POLL_IN); \
        } \
    } while(0);
#endif


static int isFpQuickWakeUpSupport = 0;
static int fp_report_event = 0;
volatile int fp_backlight_control = 0;
static struct fasync_struct *fasync_queue = NULL;
static struct timeval t_start, t_end;
extern struct rw_semaphore leds_list_lock;
extern struct list_head leds_list;

static int irq_report_disable_flag = 0;
static int disable_by_user = 0;           
/** static struct timer_list m_timer; */
static bool fpid_tristate = false;
#if __POLL_MSG__
static DECLARE_WAIT_QUEUE_HEAD(fp_poll_wq);
#endif
/******************************************************************/
static int fp_probe(struct platform_device *pdev);
static int fp_remove(struct platform_device *pdev);

#if __FP_DRV_FB_NOTIFIER__
static int fp_notifier_block_callback(struct notifier_block *nb, unsigned long val, void *data);
#endif
/******************************************************************/

static struct platform_driver fp_driver = {
    .probe = fp_probe,
    .remove = fp_remove,
    .driver = {
        .name = "fp_drv",

    },
};

struct platform_device fp_device = {
    .name   	= "fp_drv",
    .id        	= -1,
};

// IC info.
static char m_dev_name[64];
static int has_exist = 0;

// so & TA info.
static char m_dev_info[64];
static int all_info_exist = 0;
static int fp_id_pin_value = -1;

struct device *fp_dev_tz;  
struct class *fp_class;  

int fp_major;
static struct task_struct *fp_tlog_output_task;
bool fp_drv_is_tz_log_enable = false;
bool fp_drv_is_need_interrupt_oneshot = false;

static DEFINE_MUTEX(tz_log_lock);

int (*fp_disp_qsee_log_stats)(size_t count);
long create_fp_tlog_thread(void);

int fp_tlog_worker(void *data){
    klog("[%s]#########", __FUNCTION__);	
    while (!kthread_should_stop()) {
        if (fp_disp_qsee_log_stats && fp_drv_is_tz_log_enable) {
            mutex_lock(&tz_log_lock);
            fp_disp_qsee_log_stats(4096); 
            mutex_unlock(&tz_log_lock);		 
        } else {
            break;
        }		
    }
    return 0;
}

static void start_fp_tz_log_output_task(void)
{
    if (fp_tlog_output_task == NULL) {
        klog("[%s]", __FUNCTION__);
        /* fp_tlog_output_task = kthread_run(fp_tlog_output_thread, NULL, "tz_log_output_thread");
           if(IS_ERR(fp_tlog_output_task)){
           klog("Unable to start tz log output kernel thread. [TZ_LOG_IOC_ENABLE_OUTPUT][%s]", __FUNCTION__);
           fp_tlog_output_task = NULL;
           return;
           }*/
        if (create_fp_tlog_thread() >= 0)        
            fp_drv_is_tz_log_enable = true;		
    }
}


long create_fp_tlog_thread(void)
{
    int ret = 0;

    struct sched_param param = { .sched_priority = 1 };

    fp_tlog_output_task = kthread_create(fp_tlog_worker, NULL, "fp_tlog");

    if (IS_ERR(fp_tlog_output_task)) {
        pr_err("[%s][%d] fail to create tlog output thread !\n", __func__, __LINE__);
        return -1;
    }

    ret = sched_setscheduler(fp_tlog_output_task, SCHED_IDLE, &param);

    if (ret == -1) {
        pr_err("[%s][%d] fail to setscheduler tlog thread !\n", __func__, __LINE__);
        return -1;
    }

    wake_up_process(fp_tlog_output_task);
    return 0;
}

static void stop_fp_tz_log_output_task(void)
{
    if(fp_tlog_output_task){
        klog("[%s] before", __FUNCTION__);	
        fp_drv_is_tz_log_enable = false;		
        fp_drv_is_need_interrupt_oneshot = true;			
        kthread_stop(fp_tlog_output_task);	
        fp_tlog_output_task = NULL;				
    }
    klog("[%s] after", __FUNCTION__);	
}


static int backlight_trigger(int value) {
    struct led_classdev *led_cdev;

    down_read(&leds_list_lock);
    list_for_each_entry(led_cdev, &leds_list, node) {
        down_write(&led_cdev->trigger_lock);
        if (led_cdev && led_cdev->name != NULL) {
            if (strcmp(led_cdev->name, "lcd-backlight") == 0) {
                led_cdev->brightness_set(led_cdev, value);
                klog("[guomingyi], set lcd-backlight %s:%d-%d\n",__func__,__LINE__,value);
            }
        }
        up_write(&led_cdev->trigger_lock);
    }
    up_read(&leds_list_lock);
    return 0;
}

// This function call by :drivers/leds/leds.h @__led_set_brightness
int get_bl_ctr_flag(struct led_classdev *led_cdev, int value) {
    if (led_cdev && led_cdev->name != NULL) {
        if (strcmp(led_cdev->name, "lcd-backlight") == 0) {
            if (value == 0) {
                goto out;
            }
            if (fp_backlight_control == 1) {
                klog("[guomingyi], lcd-backlight block by %s:%d\n",__func__,__LINE__);
                return 1;
            }
        }
    }

out:
    return 0;
}

// This function called by :drivers/video/msm/mdss/mdss_fb.c @mdss_fb_set_bl_brightness
int fp_drv_bl_cb(int value) {
    if (value > 100 && !isFpQuickWakeUpSupport) {
        if (is_valid(t_start)) {
            do_gettimeofday(&t_end);
            klog("[guomingyi],(%s:%d) total-escape2: %ld.%ld (ms)\n", __func__, __LINE__, ESCAPE_MS(), ESCAPE_US());
            memset(&t_start, 0, sizeof(struct timeval));
        }
    }
    return 0;
}

static int go_go_go(int key) {
    switch (key) {
        case HW_EVENT_DOWN:
        case HW_EVENT_UP: 

#if __FP_DRV_FB_NOTIFIER__
        case HW_EVENT_FB_UNBLANK:
        case HW_EVENT_FB_BLANK:
#endif
            KILL_MSG(key);
            break;
        case HW_EVENT_WAKEUP: 
            if (irq_report_disable_flag == 0 && disable_by_user == 0) {
                irq_report_disable_flag = 1;
                do_gettimeofday(&t_start);
                KILL_MSG(key);
            }
            break;
    }
    return 0;
}

// This function call by external fingerprint driver
int fp_event_record(int key) {
    if (isFpQuickWakeUpSupport) {
        return go_go_go(key);
    }
    // for record irq time.
    do_gettimeofday(&t_start);
    return -1;
}

static void fp_bl_control(int data) {
    int cmd = (data & 0xffff0000) >> 16;
    int level = (data & 0x0000ffff);

    klog("[guomingyi] %s:%d, cmd:0x%x,level:0x%x, data:0x%08x\n",__func__,__LINE__, cmd, level, data);
    switch (cmd) {
        case FP_BL_LOCK:
            if (fp_backlight_control == 0) {
                fp_backlight_control = 1;
            }
            break;

        case FP_BL_LOCK_RESET:
            if (fp_backlight_control) {
                fp_backlight_control = 0;
            }
            break;

        case FP_BL_UNLOCK_AND_TRIGGER:
            if (fp_backlight_control) {
                fp_backlight_control = 0;
                backlight_trigger(level);
                if (is_valid(t_start)) {
                    do_gettimeofday(&t_end);
                    klog("[guomingyi],(%s:%d) total-escape: %ld.%ld (ms)\n", __func__, __LINE__, ESCAPE_MS(), ESCAPE_US());
                    memset(&t_start, 0, sizeof(struct timeval)); 
                }
            }
            break;

        default:
            klog("[guomingyi], set level err! %s:%d\n",__func__,__LINE__);
            break;
    }
}

static ssize_t store_cmd(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    int level = 0;

    if (buf[0] == '0')
        level = 0;
    else
        level = 1;

    klog("%s:%d,%s\n",__func__,level,buf);
    if (level == 1) {
        fp_backlight_control = 0;
        backlight_trigger(200);
    } else {
        backlight_trigger(0);
        fp_backlight_control = 1;
    }
    return count;
}

static ssize_t show_cmd(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d", fp_backlight_control); 
}

static long fp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int val = 0;
    int ret = -1;

    /** klog("guomingyi :ioctl cmd = %d, [%s]", cmd, __FUNCTION__); */
    switch(cmd) {
        case TZ_LOG_IOC_ENABLE_OUTPUT:
            klog("[TZ_LOG_IOC_ENABLE_OUTPUT][%s]", __FUNCTION__);			
            start_fp_tz_log_output_task();
            break;
        case TZ_LOG_IOC_DISABLE_OUTPUT:
            klog("[TZ_LOG_IOC_DISABLE_OUTPUT][%s]", __FUNCTION__);
            stop_fp_tz_log_output_task();
            break;
        case FP_IOC_BL_CONTROL:
            ret = copy_from_user(&val, (void __user *)arg, sizeof(int));
            if (ret) {
                klog("copy from user err:%d,%s:%d\n",ret, __func__, __LINE__);
            }
            fp_bl_control(val);
            break;
        case FP_IOC_GET_EVENT:
            ret = copy_to_user((char *)arg, &fp_report_event, sizeof(int));
            if (ret) {
                klog("FP_IOC_GET_EVENT:cp to user err:%d,%s:%d\n",ret, __func__, __LINE__);
                return -1;
            }
            fp_report_event = 0;
            break;
        case FP_IOC_RESET_EVENT:
            ret = copy_from_user(&val, (void __user *)arg, sizeof(int));
            if (ret) {
                klog("FP_IOC_RESET_EVENT:cp from user err:%d,%s:%d\n",ret, __func__, __LINE__);
                return -1;
            }
            if (val) {
                disable_by_user = 1;
            }
            else {
                disable_by_user = 0;
                irq_report_disable_flag = 0;
            }
            klog("[guomingyi] set disable_by_user = %d,irq_report_disable_flag:%d,val:%d,%s:%d\n",
                    disable_by_user,irq_report_disable_flag, val, __func__, __LINE__);
            break;
        case FP_IOC_QWK_ENABLE:
            ret = copy_from_user(&val, (void __user *)arg, sizeof(int));
            if (ret) {
                klog("FP_IOC_QWK_ENABLE:cp from user err:%d,%s:%d\n",ret, __func__, __LINE__);
                return -1;
            }
            isFpQuickWakeUpSupport = val; 
            klog("[guomingyi] %s - FP_IOC_QWK_ENABLE:isFpQuickWakeUpSupport:%d\n",__func__,isFpQuickWakeUpSupport);
            break;
    } 
    return 0;
}

static int fp_open(struct inode *inode, struct file *file)
{
    klog("fp_open \n");

    return 0;
}

static int fp_fasync(int fd, struct file * filp, int on)
{
	return fasync_helper(fd, filp, on, &fasync_queue);
}

#if __POLL_MSG__
static unsigned int fp_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;

    poll_wait(filp, &fp_poll_wq, wait);
    klog("[guomingyi], %s:%d,fp_report_event:%d\n",__func__,__LINE__,fp_report_event);

    if (fp_report_event > 0) {
        mask |= POLLIN;
        mask |= POLLRDNORM;
        /** klog("guomingyi,%s,%d, poll result:%d\n",__func__,__LINE__,fp_report_event); */
        return mask;
    }
    return 0;
}
#endif

static const struct file_operations fp_fops = {
    .owner = THIS_MODULE,
    .open = fp_open,
    .unlocked_ioctl = fp_ioctl,
    .fasync = fp_fasync,
    
#if __POLL_MSG__
    .poll = fp_poll,
#endif
};

//add for tz log, by wenguangyu, end


static DECLARE_WAIT_QUEUE_HEAD(waiter);

struct fp_gpio_data  {
    struct pinctrl *pinctrl1;
    struct pinctrl_state *id_default;
};
struct fp_gpio_data *fp_gpio_id = NULL;

int fp_id_parse_dt(struct device *dev, struct fp_gpio_data *pdata)
{
    struct device_node *node;
    //int ret;
    int dt_error = 1;
    int rc = 0;
    node = dev->of_node;
    if (node) {		
        klog("[fp_id_parse_dt] +++++++++++++++++\n");

        if (fp_gpio_id == NULL) {
            fp_gpio_id = kzalloc(sizeof(struct fp_gpio_data), GFP_KERNEL);
        }

        if(!fp_gpio_id) {
            klog("alloc fp_gpio_data fail.\n");
            return dt_error;
        }

        fp_gpio_id->pinctrl1 = devm_pinctrl_get(dev);
        if (IS_ERR_OR_NULL(fp_gpio_id->pinctrl1)) {
            rc = PTR_ERR(fp_gpio_id->pinctrl1);
            klog("Target does not use pinctrl %d\n", rc);
            kfree(fp_gpio_id);
            return rc;
        }


        fp_gpio_id->id_default
            = pinctrl_lookup_state(fp_gpio_id->pinctrl1,
                    "tlmm_gpio_fpid_active");
        if (IS_ERR_OR_NULL(fp_gpio_id->id_default)) {
            rc = PTR_ERR(fp_gpio_id->id_default);
            klog("default state err: %d\n", rc);
            kfree(fp_gpio_id);
            return rc;
        }

        rc = pinctrl_select_state(fp_gpio_id->pinctrl1,fp_gpio_id->id_default);
        if (rc){
            klog("set state err: %d\n", rc);
            kfree(fp_gpio_id);
            return rc;
        }

        kfree(fp_gpio_id);		
    } else {
        klog("[fp_id_parse_dt] Can't find node qcom,fingerprint----------\n");
        return dt_error;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////
int read_fpId_pin_value(struct device *dev, char *label) {
    struct device_node *np = dev->of_node;
    int fp_pin = 0;
    int ret = -1;
    int val1 = -1, val2 = -1;
    int i = 0;

    if (fp_id_pin_value > 0) {
        klog("%s: fingerprint id pin value :%d\n", __func__, fp_id_pin_value);
        return fp_id_pin_value;
    }

    fp_pin = of_get_named_gpio(np, label, 0);
    if (fp_pin <= 0) {
        klog("%s:fp pin gpio config err!\n", __func__);
        return -1;
    }
//lijian add for check fpid state , compatible old HW:tristate;new HW:dub-state
    fpid_tristate = of_property_read_bool(np,"qcom,fpid-tristate");
    klog("fpid_tristate=%d\n",fpid_tristate);
    if(fpid_tristate){//use fpid tristate
	klog("%s:use fpid tristate...\n", __func__);
	// Give a HIGH
	gpio_direction_input(fp_pin);
	gpio_direction_output(fp_pin, 1);

	for (i = 0; i < 5; i++) {
	val1 = gpio_get_value(fp_pin);
	klog("%s: val1(%d)\n", __func__, val1);
	mdelay(10);
	}

	mdelay(20);

	// Give a LOW
	gpio_direction_input(fp_pin);
	gpio_direction_output(fp_pin, 0);

	for (i = 0; i < 5; i++) {
	val2 = gpio_get_value(fp_pin);
	klog("%s: val2(%d)\n", __func__, val2);
	mdelay(10);
	}

	klog("%s: (%d, %d, %d)\n", __func__, fp_pin, val1, val2);

	if (val1 == 1 && val2 == 0) {
	klog("#~~~~ High impedance!~~~~ \n");
	ret = __HIGH_IMPEDANCE;
	}
	else if (val1 == 0 && val2 == 0) {
	klog("#~~~~ LOW~~~~ \n");
	ret = __LOW;
	}
	else if (val1 == 1 && val2 == 1) {
	klog("~~~~ HIGH ~~~~ \n");
	ret = __HIGH;
	gpio_direction_input(fp_pin);
	gpio_direction_output(fp_pin, 1);
	}
	else {
	klog("Err -what the fuck ??? \n");
	return -1;
	}
	gpio_direction_input(fp_pin);
    }else{//dub-state
	klog("%s:use fpid dub-state...\n", __func__);

	gpio_direction_input(fp_pin);
	for (i = 0; i < 5; i++) {
	val1 = gpio_get_value(fp_pin);
	klog("%s: val1(%d)\n", __func__, val1);
	mdelay(10);
	}

	mdelay(20);
	for (i = 0; i < 5; i++) {
	val2 = gpio_get_value(fp_pin);
	klog("%s: val2(%d)\n", __func__, val2);
	mdelay(10);
	}

	klog("%s: (%d, %d, %d)\n", __func__, fp_pin, val1, val2);

	if (val1 == 1 && val2 == 0) {
	klog("#~~~~ High impedance!~~~~ \n");
	ret = __HIGH_IMPEDANCE;
	}
	else if (val1 == 0 && val2 == 0) {
	klog("#~~~~ LOW~~~~ \n");
	ret = __LOW;
	}
	else if (val1 == 1 && val2 == 1) {
	klog("~~~~ HIGH ~~~~ \n");
	ret = __HIGH;
	}
	else {
	klog("Err -what the fuck ??? \n");
	return -1;
	}
    }
    fp_id_pin_value = ret;

    //<BEGIN>set fp_id to high impedance status .add by yinglong.tang
    fp_id_parse_dt(dev, NULL);
    //<END>set fp_id to high impedance status .add by yinglong.tang

    return ret;
}


int full_fp_chip_name(const char *name)
{
    __FUN();

    if((name == NULL) || (has_exist == 1))
    {
        klog("----(name == NULL) || (has_exist == 1)--err!---\n");
        return -1;
    }

    memset(m_dev_name, 0, sizeof(m_dev_name));
    strcpy(m_dev_name, name);
    has_exist = 1;	
    klog("---has_exist---:[%s]\n",  m_dev_name);
    return 0;
}

int full_fp_chip_info(const char *info)
{
    __FUN();

    if((info == NULL) || (all_info_exist == 1))
    {
        klog("----(info == NULL) || (all_info_exist == 1)--err!---\n");
        return -1;
    }

    memset(m_dev_info, 0, sizeof(m_dev_info));
    strcpy(m_dev_info, info);
    all_info_exist = 1;	
    klog("---m_dev_info---:[%s]\n",  m_dev_info);
    return 0;
}

static ssize_t info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    klog("fp_name=%s\n",m_dev_name);
    if(has_exist) 
    {
        return sprintf(buf, "%s", m_dev_name);    
    }
    return sprintf(buf, "%s", "unknow"); 
}

static ssize_t all_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    klog("fp_dev_info=%s\n",m_dev_info);
    return sprintf(buf, "%s", m_dev_info);   
}

static DEVICE_ATTR(fp_drv_info, 0444, info_show, NULL);
static DEVICE_ATTR(fp_drv_all_info, 0444, all_info_show, NULL);
static DEVICE_ATTR(fpcmd, 0664, show_cmd, store_cmd);



#if __FP_DRV_FB_NOTIFIER__
static struct notifier_block fp_drv_notifier;
static struct notifier_block fp_notifier_block = {
	.notifier_call = fp_notifier_block_callback,
};

static int fp_notifier_block_callback(struct notifier_block *nb, unsigned long val, void *data) {
    struct fb_event *evdata = data;
    unsigned int blank;

    if (isFpQuickWakeUpSupport != 2/*version_2*/) {
        return 0;
    }

    if (evdata && evdata->data && (val == FB_EARLY_EVENT_BLANK)) {
        blank = *(int *)(evdata->data);

        switch (blank) {
            case FB_BLANK_POWERDOWN:
                klog("guomingyi, %s:%d FB_BLANK_POWERDOWN\n", __func__,__LINE__);  
                FP_EVT_REPORT(HW_EVENT_FB_BLANK);
                break;

            case FB_BLANK_UNBLANK:
                klog("guomingyi, %s:%d FB_BLANK_UNBLANK\n", __func__,__LINE__);  
                FP_EVT_REPORT(HW_EVENT_FB_UNBLANK);
                break;

            default:
                klog("guomingyi, fp_notifier_block_callback ?\n");  
                break;
        }
    }
    return NOTIFY_OK;
}
#endif

static int fp_probe(struct platform_device *pdev)
{	
    __FUN();

    device_create_file(&pdev->dev, &dev_attr_fp_drv_info);
    device_create_file(&pdev->dev, &dev_attr_fp_drv_all_info);
    device_create_file(&pdev->dev, &dev_attr_fpcmd);
    //add for tz log, by wenguangyu, start
    fp_dev_tz = device_create(fp_class, NULL, MKDEV(fp_major, 0), NULL, FP_DEV_NAME);  
    if(!fp_dev_tz)  
    {  
        klog("device_create faile \n");  
    }
    //add for tz log, by wenguangyu, end

#if __FP_DRV_FB_NOTIFIER__
    fp_drv_notifier = fp_notifier_block;
    fb_register_client(&fp_drv_notifier);
#endif
    
    return 0;
}

static int fp_remove(struct platform_device *pdev)
{
    __FUN();
    device_remove_file(&pdev->dev, &dev_attr_fp_drv_info);
    device_remove_file(&pdev->dev, &dev_attr_fp_drv_all_info);
    return 0;
}


char g_val[20];  

static int __init fp_drv_init(void)
{
    __FUN();

    //add for tz log, by wenguangyu, start
    fp_major = register_chrdev(0/*FPDEV_MAJOR*/, FP_CLASS_NAME, &fp_fops);
    klog("register_chrdev fp_major = %d \n", fp_major);        	   
    if(fp_major < 0)  
    {  
        klog("register_chrdev fail \n"); 
        return fp_major;
    }
    fp_class = class_create(THIS_MODULE, FP_CLASS_NAME);
    if (IS_ERR(fp_class)) {
        unregister_chrdev(fp_major, fp_driver.driver.name);
        klog( "class_create fail!.\n");
        return PTR_ERR(fp_class);
    }	
    //add for tz log, by wenguangyu, end

    if (platform_device_register(&fp_device) != 0) {
        klog( "device_register fail!.\n");
        return -1;

    }

    if (platform_driver_register(&fp_driver) != 0) {
        klog( "driver_register fail!.\n");
        return -1;
    }	
    return 0;
}

static void __exit fp_drv_exit(void)
{
    __FUN();
    unregister_chrdev(fp_major, FP_DEV_NAME);  
    device_unregister(fp_dev_tz);	
    platform_driver_unregister(&fp_driver);
    //add for tz log, by wenguangyu, start
    stop_fp_tz_log_output_task();
    //add for tz log, by wenguangyu, end
}

///////////////////////////////////////////////////////////////////
late_initcall(fp_drv_init);
module_exit(fp_drv_exit);

//MODULE_LICENSE("GPL");
//MODULE_DESCRIPTION("fp-drv");
//MODULE_AUTHOR("<mingyi.guo@tinno.com>");



