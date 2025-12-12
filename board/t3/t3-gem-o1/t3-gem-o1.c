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
#include <command.h>
#include <linux/libfdt.h>


#if IS_ENABLED(CONFIG_SET_DFU_ALT_INFO)
void set_dfu_alt_info(char *interface, char *devstr)
{
    if (IS_ENABLED(CONFIG_EFI_HAVE_CAPSULE_SUPPORT))
        env_set("dfu_alt_info", update_info.dfu_string);
    else {
        /* DFU için rawemmc - tam disk erişimi */
        env_set("dfu_alt_info", "rawemmc raw 0 0x40000000");
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

    /* Boot device'ı al ve kaydet */
    bootdev = spl_boot_device();

    /* Device tree'ye ekle */
    void *fdt = (void *)(uintptr_t)spl_image->fdt_addr;
    if (fdt) {
        int chosen = fdt_path_offset(fdt, "/chosen");
        if (chosen < 0)
            chosen = fdt_add_subnode(fdt, 0, "chosen");
        if (chosen >= 0)
            fdt_setprop_u32(fdt, chosen, "u-boot,spl-boot-device", bootdev);
    }
}
#endif

#if IS_ENABLED(CONFIG_BOARD_LATE_INIT)
int board_late_init(void)
{
    char fdtfile[50];
    u32 boot_device = BOOT_DEVICE_MMC1; /* default */
    int nodeoffset;
    const u32 *boot_dev_prop;

    /* Device tree ayarla */
    snprintf(fdtfile, sizeof(fdtfile), "%s.dtb", CONFIG_DEFAULT_DEVICE_TREE);
    env_set("fdtfile", fdtfile);

    /* Boot device'ı device tree'den oku */
    nodeoffset = fdt_path_offset(gd->fdt_blob, "/chosen");
    if (nodeoffset >= 0) {
        boot_dev_prop = fdt_getprop(gd->fdt_blob, nodeoffset, 
                                     "u-boot,spl-boot-device", NULL);
        if (boot_dev_prop)
            boot_device = fdt32_to_cpu(*boot_dev_prop);
    }

    /* Debug: boot device'ı göster */
    printf("T3 GEM O1: Boot device = 0x%x\n", boot_device);

    /* Eğer USB/DFU modunda boot ettiyse */
    if (boot_device == BOOT_DEVICE_DFU || boot_device == BOOT_DEVICE_USB) {
        printf("T3 GEM O1: USB Peripheral mode detected\n");
        
        /* ÖNEMLİ: eMMC'yi initialize et - DFU için gerekli */
        printf("T3 GEM O1: Initializing eMMC for DFU...\n");
        run_command("mmc dev 0", 0);
        run_command("mmc info", 0);
        
        printf("T3 GEM O1: Entering automatic DFU mode for full disk flashing\n");
        printf("T3 GEM O1: Host can now flash with: sudo dfu-util -d 0451:6165 -a rawemmc -D image.img\n");
        
        /* DFU alt info ayarla - tam disk (rawemmc) */
        env_set("dfu_alt_info_emmc", "rawemmc raw 0 0x40000000");
        env_set("dfu_alt_info", "rawemmc raw 0 0x40000000");
        
        /* DFU exit handler ayarla - flash tamamlandığında çalışacak */
        env_set("dfu_alt_info", "rawemmc raw 0 0x40000000");
        
        /* Otomatik DFU modunu başlat + Flash sonrası reboot */
        env_set("bootcmd", 
            "echo 'T3 GEM O1 - DFU Mode (waiting for host...)';"
            "echo 'Use: sudo dfu-util -d 0451:6165 -a rawemmc -D full-disk.img';"
            "mmc dev 0;"
            "dfu 0 mmc 0;"
            "echo '';"
            "echo 'DFU completed! Rebooting from eMMC in 3 seconds...';"
            "sleep 3;"
            "reset");
        
        /* Boot delay 0 - hemen başla */
        env_set("bootdelay", "0");
    } else {
        /* Normal boot mode - eMMC/SD */
        printf("T3 GEM O1: Normal boot mode\n");
        
        /* Normal boot ayarları */
        env_set("mmcdev", "0");
        env_set("bootdev", "mmc");
        
        /* Normal bootcmd - .env dosyasındaki bootcmd kullanılacak */
        /* bootdelay default değerde kalacak */
    }

    return 0;
}
#endif