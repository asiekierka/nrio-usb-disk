/**
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2024 Adrian "asie" Siekierka
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

#include <nds.h>
#include <nds/arm9/dldi.h>

#include "tusb.h"
#include "ui.h"

static bool ejected = false;
static bool errata_last_sector = false;

// Initialize DLDI driver.
bool msc_dldi_initialize(void) {
  const DISC_INTERFACE *io = dldiGetInternal();
  if (!io->startup())
    return false;
  if (!io->isInserted())
    return false;

  // Some cartridges crash upon trying to access the final sector.
  if (
    !memcmp(&io->ioType, "SG3D", 4) /* TODO: Verify */
    || !memcmp(&io->ioType, "R4TF", 4)
  ) {
    printf(UI_COLOR_WARNING "Enabled workaround for last sector access bug.\n");
    errata_last_sector = true;
  }

  return true;
}

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
  (void) lun;

  const char vid[] = "USB-NDS";
  const char rev[] = "1.0";

  int name_len = strlen(io_dldi_data->friendlyName);
  if (name_len > 16) name_len = 16;

  memcpy(vendor_id, vid, strlen(vid));
  memcpy(product_id, io_dldi_data->friendlyName, name_len);
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  // RAM disk is ready until ejected
  if (ejected) {
    // Additional Sense 3A-00 is NOT_FOUND
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return false;
  }

  return true;
}

static uint32_t block_count_cached = 0;

uint32_t msc_find_block_count(void) {
  uint8_t sec_buffer[512];
  if (block_count_cached)
    return block_count_cached;

  // Read sector 0
  const DISC_INTERFACE *io = dldiGetInternal();
  io->readSectors(0, 1, sec_buffer);

  uint16_t footer = *((uint16_t*) (sec_buffer + 510));
  if (footer == 0xAA55) {
    // Valid header, but MBR or FAT?
    uint8_t boot_opcode = sec_buffer[0];
    if (boot_opcode == 0xEB || boot_opcode == 0xE9 || boot_opcode == 0xE8) {
      if (!memcmp(sec_buffer + 54, "FAT", 3) || !memcmp(sec_buffer + 82, "FAT32   ", 8)) {
        // Looks like a FAT partition.
        uint32_t total_sectors = *((uint32_t*) (sec_buffer + 32));
        if (total_sectors < 0x10000) {
          total_sectors = sec_buffer[19] | (sec_buffer[20] << 8);
        }
        block_count_cached = total_sectors;
        return total_sectors;
      }
    }

    // Looks like an MBR header.
    block_count_cached = 0;
    for (uint16_t table_entry = 0x1BE; table_entry < 0x1FE; table_entry += 16) {
      uint32_t p_start = *((uint16_t*) (sec_buffer + table_entry + 8))
        | (*((uint16_t*) (sec_buffer + table_entry + 10)) << 16);
      uint32_t p_count = *((uint16_t*) (sec_buffer + table_entry + 12))
        | (*((uint16_t*) (sec_buffer + table_entry + 14)) << 16);
      uint32_t p_end = p_start + p_count;
      if (p_end > block_count_cached)
        block_count_cached = p_end;
    }
  }

  return block_count_cached;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
  (void) lun;

  *block_count = msc_find_block_count() - (errata_last_sector ? 1 : 0);
  *block_size  = 512;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
  (void) lun;
  (void) power_condition;

  if (load_eject) {
    if (start) {
      // load disk storage
    } else {
      // unload disk storage
      if (!ejected) {
        printf(UI_COLOR_INFO "Storage ejected.\n");
        ejected = true;
      }
    }
  }

  return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  const DISC_INTERFACE *io = dldiGetInternal();

  if (!(bufsize & 0x1FF) && !offset && io->readSectors(lba, bufsize >> 9, buffer)) {
    return bufsize;
  } else {
    printf(UI_COLOR_ERROR "Read error [%ld, %ld]\n", lba, bufsize);
    return 0;
  }
}

bool tud_msc_is_writable_cb(uint8_t lun) {
  (void) lun;

  return true;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  const DISC_INTERFACE *io = dldiGetInternal();

  if (!(bufsize & 0x1FF) && !offset && io->writeSectors(lba, bufsize >> 9, buffer)) {
    return bufsize;
  } else {
    printf(UI_COLOR_ERROR "Write error [%ld, %ld]\n", lba, bufsize);
    return 0;
  }
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks (MUST not be handled here)
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
  void const* response = NULL;
  int32_t resplen = 0;

  // most scsi handled is input
  bool in_xfer = true;

  switch (scsi_cmd[0]) {
    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

      // negative means error -> tinyusb could stall and/or response with failed status
      return -1;
  }

  // return resplen must not larger than bufsize
  if (resplen > bufsize) resplen = bufsize;

  if (response && (resplen > 0)) {
    if (in_xfer) {
      memcpy(buffer, response, (size_t) resplen);
    } else {
      // SCSI output
    }
  }

  return resplen;
}
