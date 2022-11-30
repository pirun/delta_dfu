/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <zephyr/dfu/mcuboot.h>
//#include "delta/delta.h"

#define SLEEP_TIME_MS	1000

#define FW_VERSION		"1.1.9"


/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE	DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

/* SoC embedded NVM */
#define FLASH_NODEID 	DT_CHOSEN(zephyr_flash_controller)


static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,{0});
static struct gpio_callback button_cb_data;

volatile bool btnFlag = false;

/*
 * The led0 devicetree alias is optional. If present, we'll use it
 * to turn on the LED whenever the button is pressed.
 */
static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led2), gpios,{0});

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	btnFlag = true;
	printk("Version:%s	Button pressed at %" PRIu32 "\n", FW_VERSION,k_uptime_get_32());
	gpio_pin_toggle_dt(&led);
}

void main(void)
{
	int ret;

	//printk("Congratulations!! Delta DFU test successful!!!!!!\r\n");

	if (!device_is_ready(button.port)) {
		printk("Error: button device %s is not ready\n", button.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n", ret, button.port->name, button.pin);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n", ret, button.port->name, button.pin);
		return;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	printk("Set up button at %s pin %d\n", button.port->name, button.pin);

	if (led.port && !device_is_ready(led.port)) {
		printk("Error %d: LED device %s is not ready; ignoring it\n", ret, led.port->name);
		led.port = NULL;
	}
	if (led.port) {
		ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
		if (ret != 0) {
			printk("Error %d: failed to configure LED device %s pin %d\n", ret, led.port->name, led.pin);
			led.port = NULL;
		} else {
			printk("Set up LED at %s pin %d\n", led.port->name, led.pin);
		}
	}


	printk("Press the button\n");
	while (1) 
	{
		//printk("btnPressFlag = %d\r\n",btnFlag);
		if (btnFlag) 
		{
			printk("start delta upgrade to version %s!!!device will enter mcuboot, please wait...... \r\n", FW_VERSION);
			if (boot_request_upgrade(BOOT_UPGRADE_PERMANENT)) 
			{
				printk("flash image_ok flag failed!!!\r\n");
			}
	
			btnFlag = false;
		}
		//k_msleep(SLEEP_TIME_MS);
	}
}
