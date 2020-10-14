/**
 * plat-msm8916.c
 *
**/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_platform.h>
#include "ff_log.h"
#include "ff_ctl.h"

# undef LOG_TAG
#define LOG_TAG "msm8916"

#define FF_COMPATIBLE_NODE "focaltech,fingerprint"

/*
 * Driver configuration. See ff_ctl.c
 */
extern ff_driver_config_t *g_config;
//lijian add
static struct regulator *ff_vdd = NULL;
int ff_power_init(struct device *dev)
{
	int rc = 0;
	
	ff_vdd = regulator_get(dev, "vdd");
	if (IS_ERR(ff_vdd)) {
		rc = PTR_ERR(ff_vdd);
		FF_LOGE("Regulator get failed vdd ret=%d\n", rc);
		return rc;
	}
		rc = regulator_set_voltage(ff_vdd, 3300000,3300000);
		if (rc) {
			FF_LOGE("Regulator set_vtg failed vdd ret=%d\n", rc);
			goto reg_vdd_put;
		}

	FF_LOGE("focal power init ok!! \n");
	return 0;

reg_vdd_put:
	regulator_put(ff_vdd);
	return rc;
}

int ff_power_on(void)
{
	int rc = 0;
	rc = regulator_enable(ff_vdd);
	if (rc) {
		FF_LOGE("ff:Regulator vdd enable failed ret=%d\n", rc);
		return rc;
	}
	FF_LOGE("ff  power enable \n");
	return rc;
}

int ff_power_off(void)
{
	int rc = 0;
	rc = regulator_disable(ff_vdd);
	if (rc) {
		FF_LOGE("ff:Regulator vdd disable failed ret=%d\n", rc);
		return rc;
	}
	FF_LOGE("ff  power disable \n");

	return rc;
}
//lijian end
int ff_ctl_init_pins(int *irq_num)
{
    int err = 0, gpio;
    struct device_node *dev_node = NULL;
    enum of_gpio_flags flags;
    bool b_config_dirtied = false;
    struct platform_device *pdev = NULL;//lijian add
    FF_LOGV("'%s' enter.", __func__);

    if (unlikely(!g_config)) {
        return (-ENOSYS);
    }

    /* Find device tree node. */
    dev_node = of_find_compatible_node(NULL, NULL, FF_COMPATIBLE_NODE);
    if (!dev_node) {
        FF_LOGE("of_find_compatible_node(.., '%s') failed.", FF_COMPATIBLE_NODE);
        return (-ENODEV);
    }
//lijian add
    /* Convert to platform device. */
    pdev = of_find_device_by_node(dev_node);
    if (!pdev) {
        FF_LOGE("of_find_device_by_node(..) failed.");
        return (-ENODEV);
    }
    ff_power_init(&pdev->dev);
//lijian end
    /* Initialize RST pin. */
    gpio = of_get_named_gpio_flags(dev_node, "fp,reset_gpio", 0, &flags);
    if (gpio > 0) {
        g_config->gpio_rst_pin = gpio;
        b_config_dirtied = true;
    }
    if (!gpio_is_valid(g_config->gpio_rst_pin)) {
        FF_LOGE("g_config->gpio_rst_pin(%d) is invalid.", g_config->gpio_rst_pin);
        return (-ENODEV);
    }
    err = gpio_request(g_config->gpio_rst_pin, "ff_gpio_rst_pin");
    if (err) {
        FF_LOGE("gpio_request(%d) = %d.", g_config->gpio_rst_pin, err);
        return err;
    }
    err = gpio_direction_output(g_config->gpio_rst_pin, 1);
    if (err) {
        FF_LOGE("gpio_direction_output(%d, 1) = %d.", g_config->gpio_rst_pin, err);
        return err;
    }
#if 0
    /* Initialize PWR/VDD pin. */
    gpio = of_get_named_gpio_flags(dev_node, "fp,vdd_gpio", 0, &flags);
    if (gpio > 0) {
        g_config->gpio_power_pin = gpio;
        b_config_dirtied = true;
    }
    if (!gpio_is_valid(g_config->gpio_power_pin)) {
        FF_LOGE("g_config->gpio_power_pin(%d) is invalid.", g_config->gpio_power_pin);
        return (-ENODEV);
    }
    err = gpio_request(g_config->gpio_power_pin, "ff_gpio_power_pin");
    if (err) {
        FF_LOGE("gpio_request(%d) = %d.", g_config->gpio_power_pin, err);
        return err;
    }
    err = gpio_direction_output(g_config->gpio_power_pin, 0); // power off.
    if (err) {
        FF_LOGE("gpio_direction_output(%d, 0) = %d.", g_config->gpio_power_pin, err);
        return err;
    }

    /* Initialize IOVCC pin. */
    gpio = of_get_named_gpio_flags(dev_node, "fp,iovcc_gpio", 0, &flags);
    if (gpio > 0) {
        g_config->gpio_iovcc_pin = gpio;
        b_config_dirtied = true;
    }
    if (!gpio_is_valid(g_config->gpio_iovcc_pin)) {
        FF_LOGE("g_config->gpio_iovcc_pin(%d) is invalid.", g_config->gpio_iovcc_pin);
        return (-ENODEV);
    }
    err = gpio_request(g_config->gpio_iovcc_pin, "ff_gpio_iovcc_pin");
    if (err) {
        FF_LOGE("gpio_request(%d) = %d.", g_config->gpio_iovcc_pin, err);
        return err;
    }
    err = gpio_direction_output(g_config->gpio_iovcc_pin, 0); // power off.
    if (err) {
        FF_LOGE("gpio_direction_output(%d, 0) = %d.", g_config->gpio_iovcc_pin, err);
        return err;
    }
#endif
    /* Initialize INT pin. */
    gpio = of_get_named_gpio_flags(dev_node, "fp,irq-gpio", 0, &flags);
    if (gpio > 0) {
        g_config->gpio_int_pin = gpio;
        b_config_dirtied = true;
    }
    if (!gpio_is_valid(g_config->gpio_int_pin)) {
        FF_LOGE("g_config->gpio_int_pin(%d) is invalid.", g_config->gpio_int_pin);
        return (-ENODEV);
    }
    err = gpio_request(g_config->gpio_int_pin, "ff_gpio_int_pin");
    if (err) {
        FF_LOGE("gpio_request(%d) = %d.", g_config->gpio_int_pin, err);
        return err;
    }
    err = gpio_direction_input(g_config->gpio_int_pin);
    if (err) {
        FF_LOGE("gpio_direction_input(%d) = %d.", g_config->gpio_int_pin, err);
        return err;
    }

    /* Retrieve the IRQ number. */
    *irq_num = gpio_to_irq(g_config->gpio_int_pin);
    if (*irq_num < 0) {
        FF_LOGE("gpio_to_irq(%d) failed.", g_config->gpio_int_pin);
        return (-EIO);
    } else {
        FF_LOGD("gpio_to_irq(%d) = %d.", g_config->gpio_int_pin, *irq_num);
    }

    /* Configuration is dirty, must sync back to HAL. */
    if (!err && b_config_dirtied) {
        err = 1;
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ff_ctl_free_pins(void)
{
    int err = 0;
    FF_LOGV("'%s' enter.", __func__);

    /* Release GPIO resources. */
    gpio_free(g_config->gpio_rst_pin  );
    gpio_free(g_config->gpio_int_pin  );
#if 0
    gpio_free(g_config->gpio_iovcc_pin);
    gpio_free(g_config->gpio_power_pin);
#else
    if(ff_vdd){
	regulator_put(ff_vdd);
	ff_vdd = NULL;
    }
#endif
    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ff_ctl_enable_spiclk(bool on)
{
    int err = 0;
    FF_LOGV("'%s' enter.", __func__);
    FF_LOGD("clock: '%s'.", on ? "enable" : "disabled");

    if (on) {
        // TODO:
    } else {
        // TODO:
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ff_ctl_enable_power(bool on)
{
    int err = 0;
    FF_LOGV("'%s' enter.", __func__);
    FF_LOGD("power: '%s'.", on ? "on" : "off");

    if (unlikely(!g_config)) {
        return (-ENOSYS);
    }
#if 0
    if (on) {
        err = gpio_direction_output(g_config->gpio_power_pin, 1);
        msleep(5);
        err = gpio_direction_output(g_config->gpio_iovcc_pin, 1);
    } else {
        err = gpio_direction_output(g_config->gpio_iovcc_pin, 0);
        msleep(5);
        err = gpio_direction_output(g_config->gpio_power_pin, 0);
    }
#else
    if (on) {
        ff_power_on();
    } else {
        ff_power_off();
    }
#endif
    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ff_ctl_reset_device(void)
{
    int err = 0;
    FF_LOGV("'%s' enter.", __func__);

    if (unlikely(!g_config)) {
        return (-ENOSYS);
    }

    /* 3-1: Pull down RST pin. */
    err = gpio_direction_output(g_config->gpio_rst_pin, 0);

    /* 3-2: Delay for 10ms. */
    mdelay(10);

    /* Pull up RST pin. */
    err = gpio_direction_output(g_config->gpio_rst_pin, 1);

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

const char *ff_ctl_arch_str(void)
{
    return "msm8916";
}

