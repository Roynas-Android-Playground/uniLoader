/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 */

#include <board.h>
#include <util.h>
#include <stdint.h>
#include <string.h>
#include <drivers/framework.h>
#include <lib/console.h>
#include <lib/simplefb.h>
#include <lib/debug.h>

/* -------------------------------------------------------------------------- */
/* Display                                                                    */
/* -------------------------------------------------------------------------- */

#define DECON_F_BASE                            0x13960000
#define HW_SW_TRIG_CONTROL                     0x70
#define HW_SW_TRIG_CONTROL_TRIG_AUTO_MASK_TRIG (1 << 12)
#define HW_SW_TRIG_CONTROL_HW_TRIG_EN          (1 << 4)

#define HW_TRIG_EN \
	(HW_SW_TRIG_CONTROL_TRIG_AUTO_MASK_TRIG | \
	 HW_SW_TRIG_CONTROL_HW_TRIG_EN)

/* UART4 is the AP debug UART routed through the MAX77854 MUIC. */
#define UART_BASE		0x14c50000UL
#define UART_UFCON		0x08
#define UART_UTRSTAT		0x10
#define UART_UFSTAT		0x18
#define UART_UTXH		0x20
#define UART_UFCON_FIFOMODE	(1U << 0)
#define UART_UTRSTAT_TX_EMPTY	(1U << 1)
#define UART_UFSTAT_TX_FULL	(1U << 24)
#define UART_POLL_LIMIT		1000000U

#ifdef CONFIG_EARLYCON
static int uart_failed;

void uart_putc(char ch)
{
	unsigned int timeout = UART_POLL_LIMIT;

	if (uart_failed)
		return;
	if (readl((void *)(UART_BASE + UART_UFCON)) & UART_UFCON_FIFOMODE) {
		while ((readl((void *)(UART_BASE + UART_UFSTAT)) &
			UART_UFSTAT_TX_FULL) && --timeout)
			;
	} else {
		while (!(readl((void *)(UART_BASE + UART_UTRSTAT)) &
			 UART_UTRSTAT_TX_EMPTY) && --timeout)
			;
	}
	if (!timeout) {
		uart_failed = 1;
		return;
	}
	writel((uint32_t)(uint8_t)ch, (void *)(UART_BASE + UART_UTXH));
}

void uart_puts(const char *s)
{
	while (*s)
		uart_putc(*s++);
}

void uart_flush(void)
{
	unsigned int timeout = UART_POLL_LIMIT;

	if (uart_failed)
		return;
	while (!(readl((void *)(UART_BASE + UART_UTRSTAT)) &
		 UART_UTRSTAT_TX_EMPTY) && --timeout)
		;
	if (!timeout)
		uart_failed = 1;
}
#endif

/* -------------------------------------------------------------------------- */
/* MAX77854 EXT_I2C                                                          */
/* -------------------------------------------------------------------------- */

/*
 * Recovered from this G930L S-Boot:
 *
 * EXT_I2C bus 0:
 *   SCL GPIO ID = 0x1c3
 *   SDA GPIO ID = 0x1c2
 *
 * S-Boot GPIO encoding:
 *   bank = id >> 5
 *   pin  = id & 0x1f
 *
 * Both are bank 14. S-Boot's GPIO bank table maps bank 14 to
 * 0x136d0040, which is the Exynos8890 GPD1 bank.
 *
 * Therefore:
 *   SCL = GPD1-3
 *   SDA = GPD1-2
 */

#define GPD1_BASE		0x136d0040UL
#define GPIO_CON		0x00
#define GPIO_DAT		0x04

#define EXT_I2C_SCL_PIN		3
#define EXT_I2C_SDA_PIN		2

#define GPIO_FUNC_INPUT		0
#define GPIO_FUNC_OUTPUT	1

/*
 * MAX77854 MUIC transaction recovered from S-Boot:
 *
 *   address byte = 0x4a
 *   register     = 0x0c (CONTROL1)
 *   value        = 0x1b (UART -> AP)
 *
 * S-Boot writes 0x00 here in ifconn_com_to_open().
 */
#define MAX77854_MUIC_WRITE_ADDR	0x4a
#define MAX77854_MUIC_CONTROL1		0x0c
#define MAX77854_MUIC_UART_AP		0x1b

static int muic_restore_status;

/*
 * S-Boot uses a tiny delay between each GPIO transition.
 *
 * Exact timing is not important for this transaction; intentionally
 * run the software I2C very slowly for bring-up.
 */
static inline void ext_i2c_delay(void)
{
	for (volatile unsigned int i = 0; i < 5000; i++)
		__asm__ volatile("nop");
}

static inline uint32_t gpd1_read(uint32_t offset)
{
	return readl((void *)(GPD1_BASE + offset));
}

static inline void gpd1_write(uint32_t offset, uint32_t value)
{
	writel(value, (void *)(GPD1_BASE + offset));
}

/*
 * These two functions reproduce S-Boot's GPIO helpers at
 * image offsets 0x7ff20 and 0x7ffc4.
 */
static void gpio_set_func(unsigned int pin, unsigned int func)
{
	uint32_t val;
	unsigned int shift = pin * 4;

	val = gpd1_read(GPIO_CON);
	val &= ~(0xfu << shift);
	val |= (func & 0xfu) << shift;
	gpd1_write(GPIO_CON, val);
}

static void gpio_set_value(unsigned int pin, int value)
{
	uint32_t val;
	uint32_t mask = 1u << pin;

	val = gpd1_read(GPIO_DAT);

	if (value)
		val |= mask;
	else
		val &= ~mask;

	gpd1_write(GPIO_DAT, val);
}

static int gpio_get_value(unsigned int pin)
{
	return !!(gpd1_read(GPIO_DAT) & (1u << pin));
}

static inline void scl(int value)
{
	gpio_set_value(EXT_I2C_SCL_PIN, value);
}

static inline void sda(int value)
{
	gpio_set_value(EXT_I2C_SDA_PIN, value);
}

/*
 * S-Boot drives the lines push-pull for the transfer and switches SDA
 * to input only while receiving ACK.
 *
 * Reproduce that behavior first instead of trying to "improve" the
 * implementation during bring-up.
 */
static void ext_i2c_gpio_init(void)
{
	/*
	 * Preload high before switching to output to avoid an unnecessary
	 * low pulse.
	 */
	scl(1);
	sda(1);

	gpio_set_func(EXT_I2C_SCL_PIN, GPIO_FUNC_OUTPUT);
	gpio_set_func(EXT_I2C_SDA_PIN, GPIO_FUNC_OUTPUT);

	ext_i2c_delay();
}

static void ext_i2c_start(void)
{
	/* bus idle */
	scl(1);
	sda(1);
	ext_i2c_delay();

	/* START: SDA high -> low while SCL is high */
	scl(1);
	sda(0);
	ext_i2c_delay();
	ext_i2c_delay();

	/* enter data phase */
	scl(0);
	sda(0);
	ext_i2c_delay();
}

static void ext_i2c_stop(void)
{
	/* Start from 00, as S-Boot does. */
	scl(0);
	sda(0);
	ext_i2c_delay();

	/* SCL rises while SDA stays low. */
	scl(1);
	sda(0);
	ext_i2c_delay();
	ext_i2c_delay();

	/* STOP: SDA low -> high while SCL is high. */
	scl(1);
	sda(1);
	ext_i2c_delay();
}

static void ext_i2c_write_bit(int bit)
{
	scl(0);
	sda(bit ? 1 : 0);
	ext_i2c_delay();

	scl(1);
	ext_i2c_delay();
	ext_i2c_delay();

	scl(0);
	ext_i2c_delay();
}

/*
 * Return:
 *   0  = ACK
 *  -1  = NACK
 */
static int ext_i2c_get_ack(void)
{
	int nack;

	/*
	 * This mirrors S-Boot:
	 *   SCL low
	 *   SDA -> input
	 *   SCL high
	 *   sample SDA
	 *   restore SDA as output-low
	 */
	scl(0);
	ext_i2c_delay();

	gpio_set_func(EXT_I2C_SDA_PIN, GPIO_FUNC_INPUT);

	scl(1);
	ext_i2c_delay();

	nack = gpio_get_value(EXT_I2C_SDA_PIN);

	ext_i2c_delay();

	/*
	 * Preload the DAT latch with zero before switching SDA back to
	 * output mode, matching S-Boot's sequence.
	 */
	sda(0);
	gpio_set_func(EXT_I2C_SDA_PIN, GPIO_FUNC_OUTPUT);

	scl(0);
	ext_i2c_delay();

	return nack ? -1 : 0;
}

static int ext_i2c_write_byte(uint8_t value)
{
	for (int bit = 7; bit >= 0; bit--)
		ext_i2c_write_bit((value >> bit) & 1);

	return ext_i2c_get_ack();
}

/*
 * Write exactly the same three bytes as S-Boot's EXT_I2C write helper:
 *
 *     START
 *     0x4a  ACK
 *     0x0c  ACK
 *     0x1b  ACK
 *     STOP
 *
 * Return values make it obvious which byte was NACKed.
 */
static int herolte_restore_uart(void)
{
	int ret;

	ext_i2c_gpio_init();
	ext_i2c_start();

	ret = ext_i2c_write_byte(MAX77854_MUIC_WRITE_ADDR);
	if (ret) {
		ext_i2c_stop();
		return -1;	/* address NACK */
	}

	ret = ext_i2c_write_byte(MAX77854_MUIC_CONTROL1);
	if (ret) {
		ext_i2c_stop();
		return -2;	/* register NACK */
	}

	ret = ext_i2c_write_byte(MAX77854_MUIC_UART_AP);
	if (ret) {
		ext_i2c_stop();
		return -3;	/* data NACK */
	}

	ext_i2c_stop();

	return 0;
}

#ifdef CONFIG_EARLYCON
void early_console_init(void)
{
	muic_restore_status = herolte_restore_uart();
	earlycon_register();
	printk(KERN_INFO, "herolte: UART4 console enabled after MUIC restore: %d\n",
	       muic_restore_status);
}
#endif

/* -------------------------------------------------------------------------- */
/* Board init                                                                 */
/* -------------------------------------------------------------------------- */

int herolte_init(void)
{
#ifndef CONFIG_EARLYCON
	muic_restore_status = herolte_restore_uart();
#endif
	/* allow framebuffer to be written to */
	writel(HW_TRIG_EN, (void *)(DECON_F_BASE + HW_SW_TRIG_CONTROL));

	return 0;
}

/*
 * simplefb has been registered by the time late_init runs, so this message
 * is visible even if restoring UART failed.
 */
static int herolte_late_init(void)
{
	if (muic_restore_status == 0)
		printk(KERN_INFO,
		       "herolte: MAX77854 UART/AP route restored\n");
	else
		printk(KERN_ERR,
		       "herolte: MAX77854 UART restore failed: %d\n",
		       muic_restore_status);

	printk(KERN_INFO,
	       "herolte: EXT_I2C0 = GPD1-3(SCL), GPD1-2(SDA)\n");

	return 0;
}

static struct video_info herolte_fb = {
	.format = FB_FORMAT_ARGB8888,
	.width = 1440,
	.height = 2560,
	.stride = 4,
	.address = (void *)0xe2a00000
};

static const struct device herolte_devices[] = {
	{ "simplefb", &herolte_fb, "fb" },
};

struct board_data board_ops = {
	.name = "samsung-herolte",
	.ops = {
		.early_init = herolte_init,
		.late_init = herolte_late_init,
	},
	.devices = herolte_devices,
	.num_devices = ARRAY_SIZE(herolte_devices),
	.quirks = 0
};
