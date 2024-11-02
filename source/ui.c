// SPDX-License-Identifier: MIT
//
// SPDX-FileContributor: Adrian "asie" Siekierka, 2024

#include <stdio.h>
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <fat.h>
#include "ui.h"
#include "nds/system.h"

static PrintConsole bottomConsole, topConsole;

#define NRIO_REG(index) GBA_BUS[(index) * 0x10000]
#define NRIO_CHIP_ID   NRIO_REG(0x70)
#define NRIO_CHIP_REV  NRIO_REG(0x72)

void ui_toggle_blink_activity(void) {
    topConsole.fontBgMap[(23 * 32) + 30] ^= 0xA000;
}

void ui_toggle_blink_write_activity(void) {
    topConsole.fontBgMap[(23 * 32) + 29] ^= 0x9000;
}

void ui_init(void) {
    powerOn(POWER_ALL_2D);

    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);

    vramSetPrimaryBanks(VRAM_A_LCD, VRAM_B_LCD, VRAM_C_SUB_BG, VRAM_D_MAIN_BG_0x06000000);
    setBrightness(3, 0);

    consoleInit(&bottomConsole,
        0, BgType_Text4bpp, BgSize_T_256x256, 22, 3, false, true);
    consoleInit(&topConsole,
        0, BgType_Text4bpp, BgSize_T_256x256, 22, 3, true, true);

    consoleSelect(&topConsole);

    printf("\x1b[2J");
    printf("\x1b[4;0H");
    printf("\x1b[37;1m");
    printf("                  _\n");
    printf("       _ __  _ __(_) __\n");
    printf("      | '_ \\| '__| |'_ \\\n");
    printf("      | | | | |  | |(_) )\n");
    printf("      |_| |_|_|  |_|.__/\n");
    printf("\x1b[37;0m");
    printf("       _   _   __| |__\n");
    printf("      | | | | / _| '_ \\\n");
    printf("      | |_| |_\\_ \\ |_) )\n");
    printf("       \\____|____/____/\n");
    printf("\x1b[30;1m");
    printf("        __| (_)__| | __\n");
    printf("       / _' | / _| |/ /\n");
    printf("      ( (_| | \\_ \\   <\n");
    printf("       \\__,_|_|__/_|\\_\\ ");
    printf("\x1b[37;0m");
    printf("0.1");

    printf("\x1b[21;0H");
    printf("\x1b[37;1mUSB controller %04X, rev %02X\n", NRIO_CHIP_ID, NRIO_CHIP_REV & 0xFF);
    printf("\x1b[37;0m%s", io_dldi_data->friendlyName);

    printf("\x1b[23;27H");
    printf("\x1b[30;0moooo");

    consoleSelect(&bottomConsole);
    printf("\x1b[2J");
    printf("\x1b[23;0H");
}

void ui_select_top(void) {
    consoleSelect(&topConsole);
}

void ui_select_bottom(void) {
    consoleSelect(&bottomConsole);
}
