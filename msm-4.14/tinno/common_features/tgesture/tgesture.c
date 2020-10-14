/* 
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/uaccess.h>
#include <linux/input.h>
#include <asm/atomic.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>

#include "tgesture.h"
#include "tgesture_global.h"

/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[TGesture] "
#define APS_FUN(f)               printk(KERN_INFO APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    printk(KERN_ERR  APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(KERN_ERR APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)         
#define TGESTURE_DEBUG_FUNC(fmt, args...)	\
	do{\
		printk("<<-GTP-FUNC->> Func:%s@Line:%d\n",__func__,__LINE__);\
	}while(0)

//static struct input_dev *TGesture_key_dev;
extern int gBackLightLevel;
u8 gTGesture = 0;

static int enable_key = 1;
static s32 tgesture_state = 1; //open status
int bEnTGesture = 0;
EXPORT_SYMBOL(bEnTGesture);

unsigned int is_TDDI_solution = 0;
EXPORT_SYMBOL(is_TDDI_solution);

unsigned int Need_Module_Reset_Keep_High = 0;
EXPORT_SYMBOL(Need_Module_Reset_Keep_High);

char Tg_buf[16]={"-1"};
static int TGesture_probe(struct platform_device *pdev);
static int TGesture_remove(struct platform_device *pdev);
static ssize_t TGesture_config_read_proc(struct file *file, char __user *page, size_t size, loff_t *ppos);
static ssize_t TGesture_config_write_proc(struct file *filp, const char __user *buffer, size_t count, loff_t *off);

struct input_dev *TGesture_dev;
bool get_and_init_TGesture_dev(void)
{
	if (!TGesture_dev) {
		TGesture_dev = input_allocate_device();
		if (!TGesture_dev) {
			printk("[TGesture] alloc mem failed!\n");
			return false;
		}
	}

	__set_bit(EV_KEY, TGesture_dev->evbit);
	__set_bit(KEY_TGESTURE, TGesture_dev->keybit);

	TGesture_dev->id.bustype = BUS_HOST;
	TGesture_dev->name = "TP_GESTURE";
	if(input_register_device(TGesture_dev)) {
		printk("[TGesture] TGesture_dev register: failed!\n");
		return false;
	} else {
		printk("[TGesture] TGesture_dev register: succeed!\n");
	}
	return true;
}

void TGesture_dev_report(char value)
{
        gTGesture = value;
	printk("[TGesture] TGesture_dev_report value = %c!\n", gTGesture);
        input_report_key(TGesture_dev, KEY_TGESTURE, 1);
        input_sync(TGesture_dev);
        input_report_key(TGesture_dev, KEY_TGESTURE, 0);
        input_sync(TGesture_dev);
}

atomic_t tp_write_flag=ATOMIC_INIT(1);
struct platform_device TGesture_sensor = {
	.name	       = "tgeseture",
	.id            = -1,
};
static struct platform_driver TGesture_driver = {
	.probe      = TGesture_probe,
	.remove     = TGesture_remove,    
	.driver     = {
		.name  = "tgeseture",
	}
};
static const struct file_operations config_proc_ops = {
    .owner = THIS_MODULE,
    .read = TGesture_config_read_proc,
    .write = TGesture_config_write_proc,
};

#define TGesture_CONFIG_PROC_FILE	"tgeseture_config"
#define TGesture_CONFIG_MAX_LENGTH	240

static struct proc_dir_entry *tgesture_config_proc = NULL;
static char config[TGesture_CONFIG_MAX_LENGTH] = {0};
/*-------------------------------attribute file for debugging----------------------------------*/

/******************************************************************************
 * Sysfs attributes
*******************************************************************************/
/*----------------------------------------------------------------------------*/
static ssize_t tgesture_state_show(struct device_driver *ddri, char *buf)
{
	APS_LOG("tgesture_state = %d\n", tgesture_state);
	return snprintf(buf, PAGE_SIZE, "%d\n", tgesture_state);
}

/*----------------------------------------------------------------------------*/
static ssize_t key_tgesture_enable_show(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", enable_key);
}

/*----------------------------------------------------------------------------*/
static ssize_t key_tgesture_enable_store(struct device_driver *ddri, const char *buf, size_t count)
{
	int enable;//, res;
	
	if(1 == sscanf(buf, "%d", &enable))
	{
		enable_key = enable;
	} else {
		APS_ERR("invalid enable content: '%s', length = %ld\n", buf, count);
	}
	return count;    
}
/*---------------------------------------------------------------------------------------*/
//static DRIVER_ATTR(tgesture_state,     S_IWUSR | S_IRUGO, TGesture_show_state, NULL);
//static DRIVER_ATTR(key_tgesture_enable,    0660, TGesture_show_key, TGesture_store_key);
static DRIVER_ATTR_RO(tgesture_state);
static DRIVER_ATTR_RW(key_tgesture_enable);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *TGesture_attr_list[] = {
	&driver_attr_tgesture_state,
	&driver_attr_key_tgesture_enable,
};

static ssize_t TGesture_config_read_proc(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	int ret;
	char buf[256];

	if (*ppos)
		return 0;

	memset(buf, 0, sizeof(buf));
	ret = sprintf(buf, "0x%c,%s ", gTGesture, Tg_buf);
	*ppos += ret;
	if (copy_to_user(page, buf, ret)) {
		return -1;
	}

	return ret;
}

static ssize_t TGesture_config_write_proc(struct file *filp, const char __user *buffer, size_t count, loff_t *off)
{
	TGESTURE_DEBUG_FUNC("====LGC=========TGesture_config_write_procwrite count %d\n", count);

	if (count > TGesture_CONFIG_MAX_LENGTH)
	{
		TGESTURE_DEBUG_FUNC("size not match [%d:%d]\n", TGesture_CONFIG_MAX_LENGTH, count);
		return -EFAULT;
	}

	if (copy_from_user(&config, buffer, count))
	{
		TGESTURE_DEBUG_FUNC("copy from user fail\n");
		return -EFAULT;
	}

	if(atomic_read(&tp_write_flag))
	{
		atomic_set(&tp_write_flag,0);
		bEnTGesture=config[0]-48;
		printk("===== TGesture_config_write_proc%d=====",bEnTGesture);
		atomic_set(&tp_write_flag,1);
	}

	return count;
}

/*----------------------------------------------------------------------------*/
static int TGesture_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(TGesture_attr_list)/sizeof(TGesture_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, TGesture_attr_list[idx])))
		{            
			APS_ERR("driver_create_file (%s) = %d\n", TGesture_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}

/*----------------------------------------------------------------------------*/
	static int TGesture_delete_attr(struct device_driver *driver)
	{
	int idx ,err = 0;
	int num = (int)(sizeof(TGesture_attr_list)/sizeof(TGesture_attr_list[0]));

	if (!driver)
	return -EINVAL;

	for (idx = 0; idx < num; idx++) 
	{
		driver_remove_file(driver, TGesture_attr_list[idx]);
	}
	
	return err;
}

/*----------------------------------------------------------------------------*/
static int TGesture_probe(struct platform_device *pdev) 
{
	int err;
	APS_FUN();  

	printk("==============TGesture==================\n");
	if((err = TGesture_create_attr(&TGesture_driver.driver)))
	{
		printk("create attribute err = %d\n", err);
		return 0;
	}
	// Create proc file system
	tgesture_config_proc = proc_create(TGesture_CONFIG_PROC_FILE, 0666, NULL, &config_proc_ops);
	if (tgesture_config_proc == NULL)
	{
		TGESTURE_DEBUG_FUNC("create_proc_entry %s failed\n", TGesture_CONFIG_PROC_FILE);
	}
	else
	{
		TGESTURE_DEBUG_FUNC("create proc entry %s success", TGesture_CONFIG_PROC_FILE);
	}
	return 0;
}

/*----------------------------------------------------------------------------*/
static int TGesture_remove(struct platform_device *pdev)
{
	int err;	
	APS_FUN(); 
	printk(KERN_ERR "==============TGesture_remove==================\n");		
	if((err =  TGesture_delete_attr(&TGesture_driver.driver)))
	{
		printk("ap3220_delete_attr fail: %d\n", err);
	}
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init TGesture_init(void)
{
	APS_FUN();
	if(platform_device_register(&TGesture_sensor))
	{
		printk("failed to register driver");
		return 0;
	}
	if(platform_driver_register(&TGesture_driver))
	{
		APS_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit TGesture_exit(void)
{
	APS_FUN();
	platform_driver_unregister(&TGesture_driver);
}

/*----------------------------------------------------------------------------*/
module_init(TGesture_init);
module_exit(TGesture_exit);

/*----------------------------------------------------------------------------*/
//MODULE_LICENSE("GPL");
//MODULE_DESCRIPTION("tgesture DRIRVER");
//MODULE_AUTHOR("yaohua.li");


