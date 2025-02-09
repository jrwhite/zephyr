/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/ztest.h>

#include <zephyr/zephyr.h>
#include <zephyr/sys/reboot.h>
#include <string.h>

#include <zephyr/settings/settings.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>

static uint32_t val32;

#if defined(CONFIG_SOC_SERIES_STM32L0X) || defined(CONFIG_SOC_SERIES_STM32L0X)
#define ERASED_VAL 0x00
#else
#define ERASED_VAL 0xFF
#endif

/* leverage that this area has to be embedded flash part */
#if FLASH_AREA_LABEL_EXISTS(image_0)
#define FLASH_WRITE_BLOCK_SIZE \
	DT_PROP(DT_CHOSEN(zephyr_flash), write_block_size)
static const volatile __attribute__((section(".rodata")))
__aligned(FLASH_WRITE_BLOCK_SIZE)
uint8_t prepared_mark[FLASH_WRITE_BLOCK_SIZE] = {ERASED_VAL};
#endif

static int c1_set(const char *name, size_t len, settings_read_cb read_cb,
		  void *cb_arg)
{
	int rc;
	const char *next;

	if (settings_name_steq(name, "val32", &next) && !next) {
		rc = read_cb(cb_arg, &val32, sizeof(val32));
		zassert_true(rc >= 0, "SETTINGS_VALUE_SET callback");
		return 0;
	}

	return -ENOENT;
}

static int c1_export(int (*export_func)(const char *name,
					const void *value, size_t val_len))
{
	(void)export_func("hello/val32", &val32, sizeof(val32));

	return 0;
}

static struct settings_handler c1_settings = {
	.name = "hello",
	.h_set = c1_set,
	.h_export = c1_export,
};

void test_init(void)
{
	int err;
	uint32_t prev_int;

	val32++;

	err = settings_save();
	zassert_true(err == 0, "can't save settings");

	prev_int = val32;
	val32 = 0U;
	err = settings_load();
	zassert_true(err == 0, "can't load settings");
	zassert_equal(prev_int, val32,
		      "load value doesn't match to what was saved");
}


void test_prepare_storage(void)
{
#if FLASH_AREA_LABEL_EXISTS(image_0)
/* This procedure uses mark which is stored inside SoC embedded program
 * flash. It will not work on devices on which read/write to them is not
 * possible.
 */
	int err;
	const struct flash_area *fa;
	const struct device *dev;
	uint8_t new_val[FLASH_WRITE_BLOCK_SIZE];

	if (prepared_mark[0] == ERASED_VAL) {
		TC_PRINT("First run: erasing the storage\r\n");
		err = flash_area_open(FLASH_AREA_ID(storage), &fa);
		zassert_true(err == 0, "Can't open storage flash area");

		err = flash_area_erase(fa, 0, fa->fa_size);
		zassert_true(err == 0, "Can't erase storage flash area");

		err = flash_area_open(FLASH_AREA_ID(image_0), &fa);
		zassert_true(err == 0, "Can't open storage flash area");

		dev = flash_area_get_device(fa);

		(void)memset(new_val, (~ERASED_VAL) & 0xFF,
			     FLASH_WRITE_BLOCK_SIZE);
		err = flash_write(dev, (off_t)&prepared_mark, &new_val,
				  sizeof(new_val));
		zassert_true(err == 0, "can't write prepared_mark");
	}
#else
	TC_PRINT("Storage preparation can't be performed\r\n");
	TC_PRINT("Erase storage manually before test flashing\r\n");
#endif
}

void test_init_setup(void)
{
	int err;

	test_prepare_storage();

	err = settings_subsys_init();
	zassert_true(err == 0, "subsys init failed");

	err = settings_register(&c1_settings);
	zassert_true(err == 0, "can't register the settings handler");

	err = settings_load();
	zassert_true(err == 0, "can't load settings");

	if (val32 < 1) {
		val32 = 1U;
		err = settings_save();
		zassert_true(err == 0, "can't save settings");
		k_sleep(K_MSEC(250));
		sys_reboot(SYS_REBOOT_COLD);
	}
}

void test_main(void)
{
	/* Bellow call is not used as a test setup intentionally.    */
	/* It causes device reboot at the first device run after it */
	/* was flashed. */
	test_init_setup();

	ztest_test_suite(test_initialization,
			 ztest_unit_test(test_init)
			);

	ztest_run_test_suite(test_initialization);
}
