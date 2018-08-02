/*
 * am3352_eissbox3.h
 *
 * Copyright (C) 2018 IPKeys Technologies - http://www.ipkeys.com/
 *
 */

#ifndef __CONFIG_AM3352_EISSBOX3_H
#define __CONFIG_AM3352_EISSBOX3_H

#include <configs/ti_am335x_common.h>
#include <linux/sizes.h>

#ifndef CONFIG_SPL_BUILD
# define CONFIG_TIMESTAMP
#endif

#define CONFIG_SYS_BOOTM_LEN		SZ_16M

#define CONFIG_MACH_TYPE		MACH_TYPE_AM335XEVM

/* Clock Defines */
#define V_OSCK				24000000  /* Clock output from T2 */
#define V_SCLK				(V_OSCK)

/* Custom script for NOR */
#define CONFIG_SYS_LDSCRIPT		"board/ti/am3352_eissbox3/u-boot.lds"

/* Always 128 KiB env size */
#define CONFIG_ENV_SIZE			SZ_128K

#include <config_distro_bootcmd.h>

#ifndef CONFIG_SPL_BUILD

#define CONFIG_EXTRA_ENV_SETTINGS \
	"loadaddr=0x82000000\0" \
	"fdtaddr=0x88000000\0" \
	"bootenvfile=uEnv.txt\0" \
	"bootdir=/boot\0" \
	"bootfile=zImage\0" \
	"fdtfile=dtBlob\0" \
	"console=ttyS0,115200n8\0" \
	"mmcdev=0\0" \
	"bootpart=0:2\0" \
	"optargs=\0" \
	"loadbootenv=fatload mmc ${mmcdev} ${loadaddr} ${bootenvfile}\0" \
	"importbootenv=echo Importing environment from mmc${mmcdev} ...; " \
		"env import -t ${loadaddr} ${filesize}\0" \
	"envboot=mmc dev ${mmcdev}; " \
		"if mmc rescan; " \
		"then " \
			"echo uSD card found; setenv mmcdev 0; " \
		"else " \
			"echo No uSD card, switch to eMMC; setenv mmcdev 1; " \
		"fi; " \
		"mmc dev ${mmcdev}; " \
		"setenv bootpart ${mmcdev}:2; " \
		"echo Loading boot enviroment from ${bootenvfile} on mmc${mmcdev} ...; " \
		"if run loadbootenv; " \
		"then " \
			"run importbootenv; " \
		"fi; " \
		"if test -n $startcmd; " \
		"then " \
			"echo Running startcmd ...; " \
			"run startcmd;" \
		"fi;\0" \
	"cmdline=init=/lib/systemd/systemd\0" \
	"mmcloadimage=load mmc ${bootpart} ${loadaddr} ${bootdir}/${bootfile}\0" \
	"mmcloadfdt=load mmc ${bootpart} ${fdtaddr} ${bootdir}/${fdtfile}\0" \
	"mmcargs=setenv bootargs console=${console} " \
		"${optargs} " \
		"root=/dev/mmcblk0p2 " \
		"rootfstype=ext4 rootwait ${cmdline}\0" \
	"mmcboot=echo Booting from mmc${mmcdev} ...; " \
		"run mmcloadimage; " \
		"run mmcloadfdt; " \
		"run mmcargs; " \
		"bootz ${loadaddr} - ${fdtaddr}\0" \
	"nfsopts=nolock\0" \
	"rootpath=/export/rootfs\0" \
	"netloadimage=tftp ${loadaddr} ${bootfile}\0" \
	"netloadfdt=tftp ${fdtaddr} ${fdtfile}\0" \
	"netargs=setenv bootargs console=${console} " \
		"${optargs} " \
		"root=/dev/nfs " \
		"nfsroot=${serverip}:${rootpath},${nfsopts}\0" \
	"netboot=echo Booting from network ...; " \
		"setenv autoload no; " \
		"dhcp; " \
		"run netloadimage; " \
		"run netloadfdt; " \
		"run netargs; " \
		"bootz ${loadaddr} - ${fdtaddr}\0"
#endif


/* NS16550 Configuration */
#define CONFIG_SYS_NS16550_COM1		0x44e09000	/* UART0 */

/* PMIC support */
#define CONFIG_POWER_TPS65217

/* SPL */
/* Bootcount using the RTC block */
#define CONFIG_SYS_BOOTCOUNT_BE

/*
 * USB configuration.  We enable MUSB support, both for host and for
 * gadget.  We set USB0 as peripheral and USB1 as host, based on the
 * board schematic and physical port wired to each.  Then for host we
 * add mass storage support and for gadget we add both RNDIS ethernet
 * and DFU.
 */
#define CONFIG_USB_MUSB_DISABLE_BULK_COMBINE_SPLIT
#define CONFIG_AM335X_USB0
#define CONFIG_AM335X_USB0_MODE	MUSB_PERIPHERAL
#define CONFIG_AM335X_USB1
#define CONFIG_AM335X_USB1_MODE MUSB_HOST

/*
 * Disable MMC DM for SPL build and can be re-enabled after adding
 * DM support in SPL
 */
#ifdef CONFIG_SPL_BUILD
#undef CONFIG_DM_MMC
#undef CONFIG_TIMER
#undef CONFIG_DM_USB
#endif

#if defined(CONFIG_SPL_BUILD) && defined(CONFIG_SPL_USB_ETHER)
/* Remove other SPL modes. */
/* disable host part of MUSB in SPL */
/* disable EFI partitions and partition UUID support */
#endif

#if defined(CONFIG_EMMC_BOOT)
#define CONFIG_SYS_MMC_ENV_DEV		1
#define CONFIG_SYS_MMC_ENV_PART		0
#define CONFIG_ENV_OFFSET			0x260000
#define CONFIG_ENV_OFFSET_REDUND	(CONFIG_ENV_OFFSET + CONFIG_ENV_SIZE)
#define CONFIG_SYS_REDUNDAND_ENVIRONMENT
#define CONFIG_SYS_MMC_MAX_DEVICE	2
#endif

/* Network. */
#define CONFIG_PHY_SMSC

#ifdef CONFIG_DRIVER_TI_CPSW
#define CONFIG_CLOCK_SYNTHESIZER
#define CLK_SYNTHESIZER_I2C_ADDR 0x65
#endif

#endif	/* ! __CONFIG_AM3352_EISSBOX3_H */
