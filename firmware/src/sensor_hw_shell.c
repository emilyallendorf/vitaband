/*
 * Shell commands for isolated hardware sensor checks (UART console).
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include "tmp117.h"
#include "sht3x-dis.h"
#include "max86140.h"

LOG_MODULE_REGISTER(sensor_hw_shell, LOG_LEVEL_INF);

static int cmd_sensor_tmp117(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "TMP117 (body): init + read…");

	int ret = tmp117_init();
	if (ret != 0) {
		shell_error(sh, "tmp117_init failed: %d", ret);
		return ret;
	}

	k_msleep(15);

	float t = tmp117_read_temperature();
	if (t <= -90.0f) {
		shell_error(sh, "read failed or invalid (got %.2f C)", (double)t);
		return -EIO;
	}

	shell_print(sh, "TMP117 temperature: %.3f C", (double)t);
	return 0;
}

static int cmd_sensor_sht3(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "SHT3x (ambient): init + read…");

	int ret = sht3xdis_init();
	if (ret != 0) {
		shell_error(sh, "sht3xdis_init failed: %d", ret);
		return ret;
	}

	float t, rh;
	ret = sht3xdis_read_all(&t, &rh);
	if (ret != 0) {
		shell_error(sh, "sht3xdis_read_all failed: %d", ret);
		return ret;
	}

	shell_print(sh, "SHT3x temperature: %.3f C", (double)t);
	shell_print(sh, "SHT3x humidity:    %.2f %%RH", (double)rh);
	return 0;
}

static int cmd_sensor_ppg(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "MAX86140 (PPG): init — place finger on LED/photodiode…");

	int ret = max86140_init();
	if (ret != 0) {
		shell_error(sh, "max86140_init failed: %d", ret);
		return ret;
	}

	/* Allow FIFO to fill; HR may stay 0 until peaks are detected */
	for (int i = 0; i < 5; i++) {
		k_msleep(200);
		uint8_t bpm = max86140_read_heartrate();
		shell_print(sh, "  sample %d: HR = %u BPM", i + 1, bpm);
	}

	shell_print(sh, "MAX86140: last line is best estimate (0 = no finger / settling).");
	return 0;
}

static int cmd_sensor_all(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "=== sensor all ===");
	(void)cmd_sensor_tmp117(sh, 1, NULL);
	(void)cmd_sensor_sht3(sh, 1, NULL);
	(void)cmd_sensor_ppg(sh, 1, NULL);
	shell_print(sh, "=== done ===");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sensor_cmds,
	SHELL_CMD(tmp117, NULL, "TMP117 body temperature (C)", cmd_sensor_tmp117),
	SHELL_CMD(sht3, NULL, "SHT3x ambient temp + humidity", cmd_sensor_sht3),
	SHELL_CMD(ppg, NULL, "MAX86140 heart rate (BPM), ~1s sampling", cmd_sensor_ppg),
	SHELL_CMD(all, NULL, "Run tmp117, sht3, ppg in order", cmd_sensor_all),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(sensor, &sensor_cmds, "Hardware sensor readings", NULL);
