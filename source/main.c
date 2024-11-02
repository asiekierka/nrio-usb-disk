/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <nds.h>
#include <nds/arm9/dldi.h>

#include "tusb.h"

#include "msc.h"
#include "ui.h"

static void wait_for_key(uint16_t mask) {
  while (1) {
    swiWaitForVBlank();
    scanKeys();

    if (keysDown() & mask)
        break;
  }
}

static void exit_to_loader(void) {
  printf("\n" UI_COLOR_INFO "Press any button to exit...\n");
  wait_for_key(0xFFFF);
  exit(0);
}

int main(void) {
  defaultExceptionHandler();
  powerOff(POWER_3D_CORE | POWER_MATRIX);

  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  bool usb_init_status = !isDSiMode() && tusb_init(BOARD_TUD_RHPORT, &dev_init);

  ui_init();

  if (isDSiMode()) {
    printf(UI_COLOR_ERROR "This program is not compatible with DSi/3DS consoles.\n");
    exit_to_loader();
  }

  if (!usb_init_status) {
    printf(UI_COLOR_ERROR "Could not initialize USB!\n");
    exit_to_loader();
  }

  if (!msc_dldi_initialize()) {
    printf(UI_COLOR_ERROR "Could not initialize DLDI!\n");
    exit_to_loader();
  }

  printf(UI_COLOR_INFO "Ready. Press START to exit.\n");

  bool last_key_lid = false;

  while (1) {
    tud_task();

    scanKeys();
    if (keysDown() & KEY_START)
        break;

    bool curr_key_lid = keysHeld() & KEY_LID;
    if (last_key_lid != curr_key_lid) {
      if (curr_key_lid)
        powerOff(POWER_ALL_2D);
      else
        powerOn(POWER_ALL_2D);
      swiWaitForVBlank();
      last_key_lid = curr_key_lid;
    }
  }

  tud_deinit(0);
  powerOn(POWER_ALL);
}

// Invoked when device is mounted
void tud_mount_cb(void) {
  fifoSendValue32(FIFO_PM, PM_REQ_SLEEP_DISABLE);
  printf(UI_COLOR_INFO "Device connected.\n");
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  printf(UI_COLOR_INFO "Device disconnected.\n");
  fifoSendValue32(FIFO_PM, PM_REQ_SLEEP_ENABLE);
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
  (void) remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
}
