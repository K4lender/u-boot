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

#if IS_ENABLED(CONFIG_SET_DFU_ALT_INFO)
void set_dfu_alt_info(char *interface, char *devstr)
{
	if (IS_ENABLED(CONFIG_EFI_HAVE_CAPSULE_SUPPORT))
		env_set("dfu_alt_info", update_info.dfu_string);
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

		switch (boot_media)
		{
			case 0xFF:
			{
				env_set("mmcdev", "0");
				env_set("bootdev", "eth");
				break;
			}

			case 0x08:
			{
				// sdcard
				env_set("mmcdev", "1");
				env_set("bootdev", "mmc");
				break;
			}

			case 0x09:
			{
				// emmc
				env_set("mmcdev", "0");
				env_set("bootdev", "mmc");
				break;
			}

			case 0x0A:
			{
				// dfu
				env_set("mmcdev", "0");
				env_set("bootdev", "dfu");
				break;
			}

			default:
			{
				printf("Unknown boot method: %u\n", boot_media);
				break;
			}
		}
	}

	return 0;
}
#endif
