/*
 * board.c
 *
 * Board functions for TI AM3352 EISSBox3 board
 *
 * Copyright (C) 2018, IPKeys Technologies, LLC - http://www.ipkeys.com/
 *
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <spl.h>
#include <serial.h>
#include <asm/arch/cpu.h>
#include <asm/arch/hardware.h>
#include <asm/arch/omap.h>
#include <asm/arch/ddr_defs.h>
#include <asm/arch/clock.h>
#include <asm/arch/clk_synthesizer.h>
#include <asm/arch/gpio.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mem.h>
#include <asm/io.h>
#include <asm/emif.h>
#include <asm/gpio.h>
#include <asm/omap_common.h>
#include <asm/omap_sec_common.h>
#include <asm/omap_mmc.h>
#include <i2c.h>
#include <miiphy.h>
#include <cpsw.h>
#include <power/tps65217.h>
#include <environment.h>
#include <watchdog.h>
#include "board.h"

DECLARE_GLOBAL_DATA_PTR;

/* GPIO that controls power to DDR */
#define GPIO_TO_PIN(bank, gpio)		(32 * (bank) + (gpio))

#define GPIO_DDR_VTT_EN			GPIO_TO_PIN(0, 7)

#define GPIO_PR1_MII_CTRL		GPIO_TO_PIN(3, 4)
#define GPIO_MUX_MII_CTRL		GPIO_TO_PIN(3, 10)
#define GPIO_FET_SWITCH_CTRL	GPIO_TO_PIN(0, 7)
#define GPIO_PHY_RESET			GPIO_TO_PIN(2, 5)
#define GPIO_ETH0_MODE			GPIO_TO_PIN(0, 11)
#define GPIO_ETH1_MODE			GPIO_TO_PIN(1, 26)

static struct ctrl_dev *cdev = (struct ctrl_dev *)CTRL_DEVICE_BASE;

#define GPIO0_RISINGDETECT	(AM33XX_GPIO0_BASE + OMAP_GPIO_RISINGDETECT)
#define GPIO1_RISINGDETECT	(AM33XX_GPIO1_BASE + OMAP_GPIO_RISINGDETECT)

#define GPIO0_IRQSTATUS1	(AM33XX_GPIO0_BASE + OMAP_GPIO_IRQSTATUS1)
#define GPIO1_IRQSTATUS1	(AM33XX_GPIO1_BASE + OMAP_GPIO_IRQSTATUS1)

#define GPIO0_IRQSTATUSRAW	(AM33XX_GPIO0_BASE + 0x024)
#define GPIO1_IRQSTATUSRAW	(AM33XX_GPIO1_BASE + 0x024)

#ifndef CONFIG_DM_SERIAL
struct serial_device *default_serial_console(void)
{
	return &eserial1_device;
}
#endif

#ifndef CONFIG_SKIP_LOWLEVEL_INIT

static const struct ddr_data ddr3_eissbox3_data = {
	.datardsratio0 = MT41K256M16HA125E_RD_DQS,
	.datawdsratio0 = MT41K256M16HA125E_WR_DQS,
	.datafwsratio0 = MT41K256M16HA125E_PHY_FIFO_WE,
	.datawrsratio0 = MT41K256M16HA125E_PHY_WR_DATA,
};

static const struct cmd_control ddr3_eissbox3_cmd_ctrl_data = {
	.cmd0csratio = MT41K256M16HA125E_RATIO,
	.cmd0iclkout = MT41K256M16HA125E_INVERT_CLKOUT,

	.cmd1csratio = MT41K256M16HA125E_RATIO,
	.cmd1iclkout = MT41K256M16HA125E_INVERT_CLKOUT,

	.cmd2csratio = MT41K256M16HA125E_RATIO,
	.cmd2iclkout = MT41K256M16HA125E_INVERT_CLKOUT,
};

static struct emif_regs ddr3_eissbox3_emif_reg_data = {
	.sdram_config = MT41K256M16HA125E_EMIF_SDCFG,
	.ref_ctrl = MT41K256M16HA125E_EMIF_SDREF,
	.sdram_tim1 = MT41K256M16HA125E_EMIF_TIM1,
	.sdram_tim2 = MT41K256M16HA125E_EMIF_TIM2,
	.sdram_tim3 = MT41K256M16HA125E_EMIF_TIM3,
	.ocp_config = EMIF_OCP_CONFIG_EISSBOX3,
	.zq_config = MT41K256M16HA125E_ZQ_CFG,
	.emif_ddr_phy_ctlr_1 = MT41K256M16HA125E_EMIF_READ_LATENCY,
};

const struct dpll_params *get_dpll_ddr_params(void)
{
	int ind = get_sys_clk_index();

	return &dpll_ddr3_400MHz[ind];
}

static u8 bone_not_connected_to_ac_power(void)
{
	uchar pmic_status_reg;
	if (tps65217_reg_read(TPS65217_STATUS, &pmic_status_reg))
		return 1;
	if (!(pmic_status_reg & TPS65217_PWR_SRC_AC_BITMASK)) {
		puts("No AC power, switching to default OPP\n");
		return 1;
	}

	return 0;
}

const struct dpll_params *get_dpll_mpu_params(void)
{
	int ind = get_sys_clk_index();
	int freq = am335x_get_efuse_mpu_max_freq(cdev);

	if (bone_not_connected_to_ac_power())
		freq = MPUPLL_M_600;
	else
		freq = MPUPLL_M_1000;

	switch (freq) {
	case MPUPLL_M_1000:
		return &dpll_mpu_opp[ind][5];
	case MPUPLL_M_800:
		return &dpll_mpu_opp[ind][4];
	case MPUPLL_M_720:
		return &dpll_mpu_opp[ind][3];
	case MPUPLL_M_600:
		return &dpll_mpu_opp[ind][2];
	case MPUPLL_M_500:
		return &dpll_mpu_opp100;
	case MPUPLL_M_300:
		return &dpll_mpu_opp[ind][0];
	}

	return &dpll_mpu_opp[ind][0];
}

static void scale_vcores_eissbox3(int freq)
{
	int usb_cur_lim, mpu_vdd;

	/*
	 * Perform PMIC configurations
	 */
	if (i2c_probe(TPS65217_CHIP_PM))
		return;

	/*
	 * Ensure we have AC power
	 * before increasing the frequency.
	 */
	if (bone_not_connected_to_ac_power())
		freq = MPUPLL_M_600;
	else
		freq = MPUPLL_M_1000;

	switch (freq) {
		case MPUPLL_M_1000:
			mpu_vdd = TPS65217_DCDC_VOLT_SEL_1325MV;
			usb_cur_lim = TPS65217_USB_INPUT_CUR_LIMIT_1800MA;
			break;
		case MPUPLL_M_800:
			mpu_vdd = TPS65217_DCDC_VOLT_SEL_1275MV;
			usb_cur_lim = TPS65217_USB_INPUT_CUR_LIMIT_1300MA;
			break;
		case MPUPLL_M_720:
			mpu_vdd = TPS65217_DCDC_VOLT_SEL_1200MV;
			usb_cur_lim = TPS65217_USB_INPUT_CUR_LIMIT_1300MA;
			break;
		case MPUPLL_M_600:
		case MPUPLL_M_500:
		case MPUPLL_M_300:
		default:
			mpu_vdd = TPS65217_DCDC_VOLT_SEL_1125MV;
			usb_cur_lim = TPS65217_USB_INPUT_CUR_LIMIT_1300MA;
			break;
	}

	if (tps65217_reg_write(TPS65217_PROT_LEVEL_NONE,
			       TPS65217_POWER_PATH,
			       usb_cur_lim,
			       TPS65217_USB_INPUT_CUR_LIMIT_MASK))
		puts("tps65217_reg_write failure\n");

	/* Set DCDC3 (CORE) voltage to 1.125V */
	if (tps65217_voltage_update(TPS65217_DEFDCDC3,
				    TPS65217_DCDC_VOLT_SEL_1125MV)) {
		puts("tps65217_voltage_update failure\n");
		return;
	}

	/* Set DCDC2 (MPU) voltage */
	if (tps65217_voltage_update(TPS65217_DEFDCDC2, mpu_vdd)) {
		puts("tps65217_voltage_update failure\n");
		return;
	}

	/* Set LDO3 to 1.8V and LDO4 to 3.3V for EISSbox3 */
	if (tps65217_reg_write(TPS65217_PROT_LEVEL_2,
					TPS65217_DEFLS1,
					TPS65217_LDO_VOLTAGE_OUT_1_8,
					TPS65217_LDO_MASK))
		puts("tps65217_reg_write failure\n");

	if (tps65217_reg_write(TPS65217_PROT_LEVEL_2,
			       TPS65217_DEFLS2,
			       TPS65217_LDO_VOLTAGE_OUT_3_3,
			       TPS65217_LDO_MASK))
		puts("tps65217_reg_write failure\n");
}

void gpi2c_init(void)
{
	/* When needed to be invoked prior to BSS initialization */
	static bool first_time = true;

	if (first_time) {
		enable_i2c0_pin_mux();
		i2c_init(CONFIG_SYS_OMAP24_I2C_SPEED, CONFIG_SYS_OMAP24_I2C_SLAVE);
		first_time = false;
	}
}

void scale_vcores(void)
{
	int freq;

	gpi2c_init();
	freq = am335x_get_efuse_mpu_max_freq(cdev);

	scale_vcores_eissbox3(freq);
}

void set_uart_mux_conf(void)
{
	enable_uart0_pin_mux();
}

void set_mux_conf_regs(void)
{
	enable_board_pin_mux();
}

const struct ctrl_ioregs ioregs_eissbox3 = {
	.cm0ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
	.cm1ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
	.cm2ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
	.dt0ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
	.dt1ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
};

void sdram_init(void)
{
	config_ddr(400, &ioregs_eissbox3,
			&ddr3_eissbox3_data,
			&ddr3_eissbox3_cmd_ctrl_data,
			&ddr3_eissbox3_emif_reg_data, 0);
}
#endif

/*
 * Basic board specific setup.  Pinmux has been handled already.
 */
int board_init(void)
{
	u32 sys_reboot;

	sys_reboot = readl(PRM_RSTST);

	if (sys_reboot & (1 << 0))
		puts("Reset: Power-on reset has occurred.\n");

	if (sys_reboot & (1 << 1))
		puts("Reset: Global internal warm reset has occurred.\n");

	if (sys_reboot & (1 << 4))
		puts("Reset: Watchdog reset has occurred.\n");

	if (sys_reboot & (1 << 5))
		puts("Reset: Global external warm reset has occurred.\n");

#if defined(CONFIG_HW_WATCHDOG)
	hw_watchdog_init();
#endif

	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	return 0;
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
#if !defined(CONFIG_SPL_BUILD)
	uint8_t mac_addr[6];
	uint32_t mac_hi, mac_lo;

	/* try reading mac address from efuse */
	mac_lo = readl(&cdev->macid0l);
	mac_hi = readl(&cdev->macid0h);
	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;

	if (!env_get("ethaddr")) {
		printf("<ethaddr> not set. Validating first E-fuse MAC\n");

		if (is_valid_ethaddr(mac_addr))
			eth_env_set_enetaddr("ethaddr", mac_addr);
	}

	mac_lo = readl(&cdev->macid1l);
	mac_hi = readl(&cdev->macid1h);
	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;

	if (!env_get("eth1addr")) {
		if (is_valid_ethaddr(mac_addr))
			eth_env_set_enetaddr("eth1addr", mac_addr);
	}
#endif

	return 0;
}
#endif

#ifndef CONFIG_DM_ETH

#if (defined(CONFIG_DRIVER_TI_CPSW) && !defined(CONFIG_SPL_BUILD)) || \
	(defined(CONFIG_SPL_ETH_SUPPORT) && defined(CONFIG_SPL_BUILD))
static void cpsw_control(int enabled)
{
	/* VTP can be added here */
	return;
}

static struct cpsw_slave_data cpsw_slaves[] = {
	{
		.slave_reg_ofs	= 0x208,
		.sliver_reg_ofs	= 0xd80,
		.phy_addr		= 0,
	},
	{
		.slave_reg_ofs	= 0x308,
		.sliver_reg_ofs	= 0xdc0,
		.phy_addr		= 1,
	},
};

static struct cpsw_platform_data cpsw_data = {
	.mdio_base			= CPSW_MDIO_BASE,
	.cpsw_base			= CPSW_BASE,
	.mdio_div			= 0xff,
	.channels			= 8,
	.cpdma_reg_ofs		= 0x800,
	.slaves				= 1,
	.slave_data			= cpsw_slaves,
	.ale_reg_ofs		= 0xd00,
	.ale_entries		= 1024,
	.host_port_reg_ofs	= 0x108,
	.hw_stats_reg_ofs	= 0x900,
	.bd_ram_ofs			= 0x2000,
	.mac_control		= (1 << 5),
	.control			= cpsw_control,
	.host_port_num		= 0,
	.version			= CPSW_CTRL_VERSION_2,
};
#endif

#if ((defined(CONFIG_SPL_ETH_SUPPORT) || defined(CONFIG_SPL_USBETH_SUPPORT)) &&\
	defined(CONFIG_SPL_BUILD)) || \
	((defined(CONFIG_DRIVER_TI_CPSW) || \
	  defined(CONFIG_USB_ETHER) && defined(CONFIG_MUSB_GADGET)) && \
	 !defined(CONFIG_SPL_BUILD))

/*
 * This function will:
 * Read the eFuse for MAC addresses, and set ethaddr/eth1addr/usbnet_devaddr
 * in the environment
 * Perform fixups to the PHY present on certain boards.  We only need this
 * function in:
 * - SPL with either CPSW or USB ethernet support
 * - Full U-Boot, with either CPSW or USB ethernet
 * Build in only these cases to avoid warnings about unused variables
 * when we build an SPL that has neither option but full U-Boot will.
 */
int board_eth_init(bd_t *bis)
{
	int rv, n = 0;
#if defined(CONFIG_USB_ETHER) && \
	(!defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_USBETH_SUPPORT))
	uint8_t mac_addr[6];
	uint32_t mac_hi, mac_lo;

	/*
	 * use efuse mac address for USB ethernet as we know that
	 * both CPSW and USB ethernet will never be active at the same time
	 */
	mac_lo = readl(&cdev->macid0l);
	mac_hi = readl(&cdev->macid0h);
	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;
#endif


#if (defined(CONFIG_DRIVER_TI_CPSW) && !defined(CONFIG_SPL_BUILD)) || \
	(defined(CONFIG_SPL_ETH_SUPPORT) && defined(CONFIG_SPL_BUILD))

#ifdef CONFIG_DRIVER_TI_CPSW
	puts("eth0: MII MODE\n");
	writel(MII_MODE_ENABLE, &cdev->miisel);
	cpsw_slaves[0].phy_if = cpsw_slaves[1].phy_if =
			PHY_INTERFACE_MODE_MII;

	rv = cpsw_register(&cpsw_data);
	if (rv < 0)
		printf("Error %d registering CPSW switch\n", rv);
	else
		n += rv;
#endif

#endif
#if defined(CONFIG_USB_ETHER) && \
	(!defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_USBETH_SUPPORT))
	if (is_valid_ethaddr(mac_addr))
		eth_env_set_enetaddr("usbnet_devaddr", mac_addr);

	rv = usb_eth_initialize(bis);
	if (rv < 0)
		printf("Error %d registering USB_ETHER\n", rv);
	else
		n += rv;
#endif
	return n;
}
#endif

#endif /* CONFIG_DM_ETH */

#if !CONFIG_IS_ENABLED(OF_CONTROL)
static const struct omap_hsmmc_plat am335x_mmc0_platdata = {
	.base_addr = (struct hsmmc *)OMAP_HSMMC1_BASE,
	.cfg.host_caps = MMC_MODE_HS_52MHz | MMC_MODE_HS | MMC_MODE_4BIT,
	.cfg.f_min = 400000,
	.cfg.f_max = 52000000,
	.cfg.voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195,
	.cfg.b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT,
};

U_BOOT_DEVICE(am335x_mmc0) = {
	.name = "omap_hsmmc",
	.platdata = &am335x_mmc0_platdata,
};

static const struct omap_hsmmc_plat am335x_mmc1_platdata = {
	.base_addr = (struct hsmmc *)OMAP_HSMMC2_BASE,
	.cfg.host_caps = MMC_MODE_HS_52MHz | MMC_MODE_HS | MMC_MODE_8BIT,
	.cfg.f_min = 400000,
	.cfg.f_max = 52000000,
	.cfg.voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195,
	.cfg.b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT,
};

U_BOOT_DEVICE(am335x_mmc1) = {
	.name = "omap_hsmmc",
	.platdata = &am335x_mmc1_platdata,
};
#endif
