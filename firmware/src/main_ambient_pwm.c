/*
 * Ambient SHT3x-DIS via drivers/src/sht3x-dis.c + TMP117 via drivers/src/tmp117.c.
 * I2C layout from app_ambient_pwm.overlay (label sht3xdis + tmp117 on i2c0).
 *
 * Build: prj_ambient_pwm.conf + app_ambient_pwm.overlay
 *
 * Zephyr CONFIG_SHT3XD is off so the stock sensirion,sht3xd driver does not bind;
 * your custom driver talks to the same node using I2C_DT_SPEC_GET(DT_NODELABEL(sht3xdis)).
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <sht3x-dis.h>
#include <tmp117.h>

int main(void)
{
	k_sleep(K_MSEC(2000)); /* wait for RTT viewer to attach */

	printk("\n=== VitaBand ambient SHT3x (custom) + TMP117 ===\n");

	int ret;


	ret = sht3xdis_init();

	if (ret != 0) {
		printk("SHT3x-DIS init failed 3(%d)\n", ret);
		return ret;
	}
	printk("SHT3x-DIS initialized successfully\n");

	// ret = tmp117_init();
	// /* #region agent log */
	// printk("{\"sessionId\":\"75362d\",\"hypothesisId\":\"H4\",\"location\":\"main_ambient_pwm.c\","
	//        "\"message\":\"tmp117_init\",\"data\":{\"ret\":%d},\"timestamp\":%u}\n",
	//        ret, (unsigned)k_uptime_get_32());
	// /* #endregion */
	// if (ret != 0) {
	// 	printk("TMP117 init failed (%d)\n", ret);
	// 	return ret;
	// }
	// printk("TMP117 initialized successfully\n");

	

	printk("Sensors ready, sampling every 500 ms\n");

	for (;;) {
		// float amb_t;
		// float amb_rh;

		// ret = sht3xdis_read_all(&amb_t, &amb_rh);
		// if (ret != 0) {
		// 	printk("SHT3x read failed (%d)\n", ret);
		// } else {
		// 	printk("Ambient (SHT): %.3f °C  RH: %.2f %%  |  ",
		// 	       (double)amb_t, (double)amb_rh);
		// }

		// float body = tmp117_read_temperature();

		// if (body > -90.0f) {
		// 	printk("Body (TMP117): %.3f °C\n", (double)body);
		// } else {
		// 	printk("Body (TMP117): read error\n");
		// }

		k_sleep(K_MSEC(500));
	}
}
