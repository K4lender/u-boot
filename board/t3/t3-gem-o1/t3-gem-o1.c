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
	else {
        // Tam disk + partition desteği
        env_set("dfu_alt_info", 
            "rawemmc raw 0 0x40000000;"
            "rootfs part 0 1");
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

    // DFU alt info - TAM DISK desteği (0x40000000 = ~1GB)
    env_set("dfu_alt_info_emmc", 
        "rawemmc raw 0 0x40000000;"
        "rootfs part 0 1");
    
    env_set("dfu_alt_info", env_get("dfu_alt_info_emmc"));

    // Otomatik DFU boot - 300 saniye timeout
    if (!env_get("bootcmd_orig")) {
        env_set("bootcmd_orig", env_get("bootcmd"));
    }
    
    env_set("bootcmd", 
        "echo 'T3 GEM O1 - DFU Mode (300s timeout)';"
        "setenv dfu_alt_info ${dfu_alt_info_emmc};"
        "dfu 0 mmc 0 300;"
        "run bootcmd_orig");

    // Boot delay'i 0 yap
    env_set("bootdelay", "0");

    snprintf(fdtfile, sizeof(fdtfile), "%s.dtb", CONFIG_DEFAULT_DEVICE_TREE);
    env_set("fdtfile", fdtfile);

    uint32_t boot_media = k3_get_boot_media();

    switch (boot_media)
    {
    case 0x08: // sdcard
    {
        env_set("mmcdev", "1");
        env_set("bootdev", "mmc");
        break;
    }

    case 0x09: // emmc
    {
        env_set("mmcdev", "0");
        env_set("bootdev", "mmc");
        break;
    }

    case 0x0b: // ethernet
    {
        env_set("bootdev", "eth");
        break;
    }

    default:
        printf("Unsupported boot media: 0x%x\n", boot_media);
        return -1;
    }

    return 0;
}
#endif