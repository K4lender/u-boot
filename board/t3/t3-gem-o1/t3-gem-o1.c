// SPDX-License-Identifier: GPL-2.0+
/*
 * https://docs.t3gemstone.org/tr/boards/obsidian/introduction
 *
 * Copyright (C) 2024 Texas Instruments Incorporated
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

/* ----------------------------------------------------------- */
/* DFU ALT INFO */
/* ----------------------------------------------------------- */
#if IS_ENABLED(CONFIG_SET_DFU_ALT_INFO)
void set_dfu_alt_info(char *interface, char *devstr)
{
    /* Güvenli varsayılan: SADECE rootfs (.img) */
    if (!env_get("dfu_alt_info"))
        env_set("dfu_alt_info", "rootfs raw 0 0x100000");
}
#endif

int board_init(void)
{
    return 0;
}

/* ----------------------------------------------------------- */
/* FIT CONFIG */
/* ----------------------------------------------------------- */
#if defined(CONFIG_SPL_LOAD_FIT)
int board_fit_config_name_match(const char *name)
{
    if (!strcmp(name, "k3-am67a-t3-gem-o1"))
        return 0;
    return -1;
}
#endif

/* ----------------------------------------------------------- */
/* DRAM */
/* ----------------------------------------------------------- */
int dram_init(void)
{
    return fdtdec_setup_mem_size_base();
}

int dram_init_banksize(void)
{
    return fdtdec_setup_memory_banksize();
}

/* ----------------------------------------------------------- */
/* SPL FIXUPS */
/* ----------------------------------------------------------- */
#if defined(CONFIG_XPL_BUILD)
void spl_perform_fixups(struct spl_image_info *spl_image)
{
    u32 bootdev = spl_boot_device();
    void *fdt = (void *)(uintptr_t)spl_image->fdt_addr;

    if (!fdt)
        return;

    int chosen = fdt_path_offset(fdt, "/chosen");
    if (chosen < 0)
        chosen = fdt_add_subnode(fdt, 0, "chosen");

    if (chosen >= 0)
        fdt_setprop_u32(fdt, chosen, "u-boot,spl-boot-device", bootdev);
}
#endif

/* ----------------------------------------------------------- */
/* BOARD LATE INIT */
/* ----------------------------------------------------------- */
#if IS_ENABLED(CONFIG_BOARD_LATE_INIT)
int board_late_init(void)
{
    char fdtfile[64];
    u32 boot_device = BOOT_DEVICE_MMC1;
    int nodeoffset;
    const u32 *boot_dev_prop;

    /* DTB */
    snprintf(fdtfile, sizeof(fdtfile), "%s.dtb", CONFIG_DEFAULT_DEVICE_TREE);
    env_set("fdtfile", fdtfile);

    /* SPL boot device */
    nodeoffset = fdt_path_offset(gd->fdt_blob, "/chosen");
    if (nodeoffset >= 0) {
        boot_dev_prop = fdt_getprop(gd->fdt_blob, nodeoffset,
                                    "u-boot,spl-boot-device", NULL);
        if (boot_dev_prop)
            boot_device = fdt32_to_cpu(*boot_dev_prop);
    }

    printf("T3 GEM O1: Boot device = 0x%x\n", boot_device);

    /* ------------------------------------------------------- */
    /* DFU MODE */
    /* ------------------------------------------------------- */
    if (boot_device == BOOT_DEVICE_DFU || boot_device == BOOT_DEVICE_USB) {
        printf("T3 GEM O1: USB DFU mode detected\n");

        /* eMMC hazırla */
        run_command("mmc dev 0", 0);
        run_command("mmc rescan", 0);
        run_command("mmc info", 0);

        /* SADECE ROOTFS (.img) */
        env_set("dfu_alt_info", "rootfs raw 0 0x100000");

        printf("DFU MODE: rootfs (.img)\n");
        printf("Host command:\n");
        printf("sudo dfu-util -d 0451:6165 -a rootfs -D gemstone-minimal.img -e\n");

        /* DFU loop:
         * - Host -e (detach) gönderirse dfu döner
         * - reset atılır
         */
        env_set("bootcmd",
            "echo 'T3 GEM O1 - DFU (rootfs only)';"
            "mmc dev 0; mmc rescan;"
            "while true; do "
                "if dfu 0 mmc 0; then "
                    "echo 'DFU done, rebooting...'; sleep 1; reset; "
                "else "
                    "echo 'DFU aborted, waiting...'; sleep 1; "
                "fi; "
            "done"
        );

        env_set("bootdelay", "0");
    }
    /* ------------------------------------------------------- */
    /* NORMAL BOOT */
    /* ------------------------------------------------------- */
    else {
        printf("T3 GEM O1: Normal boot\n");
        env_set("mmcdev", "0");
        env_set("bootdev", "mmc");
    }

    return 0;
}
#endif
