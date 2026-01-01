// SPDX-License-Identifier: GPL-2.0+
/*
 * https://docs.t3gemstone.org/tr/boards/obsidian/introduction
 *
 * Copyright (C) 2024 Texas Instruments Incorporated - https://www.ti.com/
 */

#include <asm/arch/hardware.h>
#include <asm/io.h>
#include <dm/uclass.h>
#include <env.h>
#include <fdt_support.h>
#include <spl.h>
#include <asm/arch/k3-ddr.h>
#include <asm/arch/am62a_spl.h>

#if IS_ENABLED(CONFIG_SET_DFU_ALT_INFO)
void set_dfu_alt_info(char *interface, char *devstr)
{
    if (IS_ENABLED(CONFIG_EFI_HAVE_CAPSULE_SUPPORT)) {
        env_set("dfu_alt_info", update_info.dfu_string);
    } else if (!strcmp(interface, "mmc") && !strcmp(devstr, "0")) {
        // Raw eMMC access for complete image flashing (WIC file)
        // 0x1D1C000 blocks = ~3.7 GB (sufficient for AM67 eMMC)
        env_set("dfu_alt_info", "rawemmc raw 0 0 0x1D1C000");
    }
}
#endif

int board_init(void)
{
	return 0;
}

#if defined(CONFIG_SPL_LOAD_FIT)
int board_fit_config_name_match(const char *name)
{
	if (!strcmp(name, "k3-am67a-t3-gem-o1"))
		return 0;

	return -1;
}
#endif

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

#if defined(CONFIG_XPL_BUILD)
void spl_perform_fixups(struct spl_image_info *spl_image)
{
	u32 bootdev;

	if (IS_ENABLED(CONFIG_K3_DDRSS)) {
		if (IS_ENABLED(CONFIG_K3_INLINE_ECC))
			fixup_ddr_driver_for_ecc(spl_image);
	} else {
		fixup_memory_node(spl_image);
	}

	bootdev = spl_boot_device();

	void *fdt = (void *)(uintptr_t)spl_image->fdt_addr;
	if (!fdt)
		return;

	int chosen = fdt_path_offset(fdt, "/chosen");
	if (chosen < 0)
		chosen = fdt_add_subnode(fdt, 0, "chosen");
	if (chosen < 0)
		return;

	fdt_setprop_u32(fdt,    chosen, "u-boot,spl-boot-media", bootdev);
}
#endif

#if IS_ENABLED(CONFIG_BOARD_LATE_INIT)
int board_late_init(void)
{
	char fdtfile[50];

	snprintf(fdtfile, sizeof(fdtfile), "%s.dtb", CONFIG_DEFAULT_DEVICE_TREE);

	env_set("fdtfile", fdtfile);

	const void *fdt = gd->fdt_blob;
	if (!fdt)
		return 0;

	int chosen = fdt_path_offset(fdt, "/chosen");
	if (chosen < 0)
		return 0;

	int len;
	const fdt32_t *pinst = fdt_getprop(fdt, chosen, "u-boot,spl-boot-media", &len);

	if (pinst && len == sizeof(fdt32_t)) {
		u32 boot_media = fdt32_to_cpu(*pinst);
		
		printf("Boot mode detected: 0x%x\n", boot_media);

		switch (boot_media)
		{
		case BOOT_DEVICE_ETHERNET_RGMII:
			env_set("mmcdev", "0");
			env_set("bootdev", "eth");
			break;

		case BOOT_DEVICE_MMC:
			// sdcard
			env_set("mmcdev", "1");
			env_set("bootdev", "mmc");
			break;

		case BOOT_DEVICE_EMMC:
			// emmc
			env_set("mmcdev", "0");
			env_set("bootdev", "mmc");
			break;

		case BOOT_DEVICE_DFU:
			printf("DFU mode activated!\n");

            env_set("mmcdev", "0");
            env_set("bootdev", "dfu");
            
            // Set raw eMMC target with correct syntax
            // Format: "name raw <start> <size> mmcpart <dev>"
            env_set("dfu_alt_info", "rawemmc raw 0x0 0x1D1C000 mmcpart 0");
            
            // DFU sonrası boot komutu
            env_set("bootcmd_dfu_post",
                "echo DFU complete - Attempting boot...; "
                "mmc dev 0; "
                "if load mmc 0:1 ${loadaddr} Image; then "
                    "echo Kernel found, booting...; "
                    "load mmc 0:1 ${fdtaddr} k3-am67a-t3-gem-o1.dtb; "
                    "load mmc 0:1 ${initrdaddr} gemstone-image-rd-t3-gem-o1.cpio.gz; "
                    "setenv bootargs console=ttyS2,115200n8 root=/dev/mmcblk0p2 rw rootfstype=btrfs; "
                    "booti ${loadaddr} ${initrdaddr}:${filesize} ${fdtaddr}; "
                "else "
                    "echo No kernel found - Reset required; "
                "fi");
            
            // Auto-start DFU ve sonrasında boot
            env_set("bootcmd", 
                "echo === DFU Mode: Raw eMMC Flash ===; "
                "echo Waiting for USB host...; "
                "echo 'Use: dfu-util -a rawemmc -D image.wic'; "
                "dfu 0 mmc 0; "
                "run bootcmd_dfu_post");
            
            // Disable distro boot
            env_set("distro_bootcmd", "");
            break;

		case BOOT_DEVICE_USB:
			// Primary USB mode (0x2A)
			printf("USB mode activated!\n");
			env_set("mmcdev", "0");
			env_set("bootdev", "mmc");
			env_set("dfu_mmcdev", "0");
			env_set("dfu_autoboot", "1");
			env_set("dfu_alt_info_emmc", "boot part 0 1; rootfs part 0 2");
			env_set("dfu_alt_info", "boot part 0 1; rootfs part 0 2");
			break;

		default:
			printf("Unknown boot method: 0x%x\n", boot_media);
			break;
		}
	}

	return 0;
}
#endif
