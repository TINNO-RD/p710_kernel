/*
 * platform indepent driver interface
 *
 * Coypritht (c) 2017 Goodix
 */
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/err.h>

#include "gf_spi.h"

#if defined(USE_SPI_BUS)
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#elif defined(USE_PLATFORM_BUS)
#include <linux/platform_device.h>
#endif
//lijian add 
#define GF_VDD_MIN_UV      3300000
#define GF_VDD_MAX_UV	   3300000
int gf_power_init(struct gf_dev *gf_dev)
{
	int rc = 0;
	struct device *dev = &gf_dev->spi->dev;
	struct device_node *np = dev->of_node;

	gf_dev->ldo_en_gpio = of_get_named_gpio(np, "fp-gpio-powen", 0);
	rc = devm_gpio_request(dev, gf_dev->ldo_en_gpio, "goodix_power");
	if (rc) {
		pr_err("failed to request lod en gpio, rc = %d\n", rc);
		goto err_ldo_en;
	}
	gpio_direction_output(gf_dev->ldo_en_gpio, 1);
	gpio_set_value(gf_dev->ldo_en_gpio, 0);
	pr_err("gf  power enable gpio %d\n", gf_dev->ldo_en_gpio);

	gf_dev->vdd = regulator_get(dev, "vdd");
	if (IS_ERR(gf_dev->vdd)) {
		rc = PTR_ERR(gf_dev->vdd);
		pr_err("gf:Regulator get failed vdd ret=%d\n", rc);
		return rc;
	}
	//if (regulator_count_voltages(gf_dev->vdd) > 0) {
		rc = regulator_set_voltage(gf_dev->vdd, GF_VDD_MIN_UV,
					   GF_VDD_MAX_UV);
		if (rc) {
			pr_err("gf:Regulator set_vtg failed vdd ret=%d\n", rc);
			goto reg_vdd_put;
		}
	//}

	pr_err("gf  power init ok!! \n");
/*
	rc = regulator_enable(gf_dev->vdd);
	if (rc) {
		pr_err("gf:Regulator vdd enable failed ret=%d\n", rc);
		return rc;
		}
	pr_err("gf  power on ok!! \n");
*/
	return 0;

err_ldo_en:
	return rc;
reg_vdd_put:
	regulator_put(gf_dev->vdd);
	return rc;
}

int gf_power_deinit(struct gf_dev* gf_dev)
{
    int ret = 0;

    if (gf_dev->vdd)
    {
        if (regulator_count_voltages(gf_dev->vdd) > 0)
            regulator_set_voltage(gf_dev->vdd, 0, GF_VDD_MAX_UV);

        regulator_disable(gf_dev->vdd);
        regulator_put(gf_dev->vdd);
    }

    return ret;
}
//end by lijian

int gf_parse_dts(struct gf_dev *gf_dev)
{
	int rc = 0;
	struct device *dev = &gf_dev->spi->dev;
	struct device_node *np = dev->of_node;

	gf_dev->reset_gpio = of_get_named_gpio(np, "fp-gpio-reset", 0);
	if (gf_dev->reset_gpio < 0) {
		pr_err("falied to get reset gpio!\n");
		return gf_dev->reset_gpio;
	}

	rc = devm_gpio_request(dev, gf_dev->reset_gpio, "goodix_reset");
	if (rc) {
		pr_err("failed to request reset gpio, rc = %d\n", rc);
		goto err_reset;
	}
	gpio_direction_output(gf_dev->reset_gpio, 1);
	pr_err("gf  request reset gpio %d\n", gf_dev->reset_gpio);

	gf_dev->irq_gpio = of_get_named_gpio(np, "fp-gpio-irq", 0);
	if (gf_dev->irq_gpio < 0) {
		pr_err("falied to get irq gpio!\n");
		return gf_dev->irq_gpio;
	}

	rc = devm_gpio_request(dev, gf_dev->irq_gpio, "goodix_irq");
	if (rc) {
		pr_err("failed to request irq gpio, rc = %d\n", rc);
		goto err_irq;
	}
	gpio_direction_input(gf_dev->irq_gpio);
	pr_err("gf  request irq gpio %d\n", gf_dev->irq_gpio);
	
err_irq:
	devm_gpio_free(dev, gf_dev->reset_gpio);
err_reset:
	return rc;
}

void gf_cleanup(struct gf_dev *gf_dev)
{
	pr_err("[info] %s\n", __func__);

	if (gpio_is_valid(gf_dev->irq_gpio)) {
		gpio_free(gf_dev->irq_gpio);
		pr_err("remove irq_gpio success\n");
	}
	if (gpio_is_valid(gf_dev->reset_gpio)) {
		gpio_free(gf_dev->reset_gpio);
		pr_err("remove reset_gpio success\n");
	}
	if (gpio_is_valid(gf_dev->ldo_en_gpio)) {//lijian add
		gpio_set_value(gf_dev->ldo_en_gpio, 0);
		gpio_direction_input(gf_dev->ldo_en_gpio);
		gpio_free(gf_dev->ldo_en_gpio);
		pr_err("remove ldo_en_gpio success\n");
	}
}

int gf_power_on(struct gf_dev *gf_dev)
{
	int rc = 0;

	/* TODO: add your power control here */
	//gpio_set_value(gf_dev->ldo_en_gpio, 1);//lijian add
	rc = regulator_enable(gf_dev->vdd);
	if (rc) {
		pr_err("gf:Regulator vdd enable failed ret=%d\n", rc);
		return rc;
	}
	pr_err("gf  power enable \n");
	return rc;
}

int gf_power_off(struct gf_dev *gf_dev)
{
	int rc = 0;

	/* TODO: add your power control here */
	//gpio_set_value(gf_dev->ldo_en_gpio, 0);////lijian add
	rc = regulator_disable(gf_dev->vdd);
	if (rc) {
		pr_err("gf:Regulator vdd disable failed ret=%d\n", rc);
		return rc;
	}
	pr_err("gf  power disable \n");

	return rc;
}

int gf_hw_reset(struct gf_dev *gf_dev, unsigned int delay_ms)
{
	if (gf_dev == NULL) {
		pr_err("Input buff is NULL.\n");
		return -1;
	}
	gpio_direction_output(gf_dev->reset_gpio, 1);
	gpio_set_value(gf_dev->reset_gpio, 0);
	mdelay(3);
	gpio_set_value(gf_dev->reset_gpio, 1);
	mdelay(delay_ms);
	return 0;
}

int gf_irq_num(struct gf_dev *gf_dev)
{
	if (gf_dev == NULL) {
		pr_err("Input buff is NULL.\n");
		return -1;
	} else {
		return gpio_to_irq(gf_dev->irq_gpio);
	}
}
