/*
 * spiffs_integration.c
 *
 *  Created on: Jan 17, 2018
 *      Author: jan
 *
 * Information on the spiffs used on the Mist Bosch XDK port:
 * -Filesystem is created in the "reserved" segment of the MCU flash, starting from addr 0x000B6000
 * -File system size is 64k
 *
 */

#include "spiffs.h"
#include "BCDS_MCU_Flash.h"
#include "spiffs_integration.h"

static spiffs fs;

static s32_t my_spiffs_read(u32_t addr, u32_t size, u8_t *dst) {
	if (MCU_Flash_Read( (uint8_t*) addr, dst, size) != RETCODE_SUCCESS) {
		printf("flash read error, %x", addr);
		return SPIFFS_ERR_INTERNAL;
	}
	return SPIFFS_OK;
}

static s32_t my_spiffs_write(u32_t addr, u32_t size, u8_t *src) {
	if (MCU_Flash_Write((uint8_t*) addr, src, size) != RETCODE_SUCCESS) {
		printf("flash write error %x", addr);
		return SPIFFS_ERR_INTERNAL;
	}
	return SPIFFS_OK;
}

static s32_t my_spiffs_erase(u32_t addr, u32_t size) {
	const uint32_t page_count = size / MCU_Flash_GetPageSize();
	if (MCU_Flash_Erase((uint32_t*) addr, page_count) != RETCODE_SUCCESS) {
		printf("flash erase error %x", addr);
		return SPIFFS_ERR_INTERNAL;
	}

	return SPIFFS_OK;
}

#define LOG_PAGE_SIZE       256

static u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static u8_t spiffs_fds[32*4];
static u8_t spiffs_cache_buf[(LOG_PAGE_SIZE+32)*4];

void my_spiffs_mount() {
	spiffs_config cfg;
	cfg.phys_size = 64 * 1024;
	cfg.phys_addr = 0x000B6000; // start spiffs at start of "Reserved" segment, see XDK110 FreeRTOS guide, chapter 4.1
	cfg.phys_erase_block = MCU_Flash_GetPageSize(); // according to return value of
	cfg.log_block_size = MCU_Flash_GetPageSize(); // let us not complicate things
	cfg.log_page_size = LOG_PAGE_SIZE; // as we said

	cfg.hal_read_f = my_spiffs_read;
	cfg.hal_write_f = my_spiffs_write;
	cfg.hal_erase_f = my_spiffs_erase;

	int res = SPIFFS_mount(&fs, &cfg, spiffs_work_buf, spiffs_fds,
			sizeof(spiffs_fds), spiffs_cache_buf, sizeof(spiffs_cache_buf), 0);
	printf("spiffs mount res: %i\n", res);
}

void my_spiffs_test(void) {
  char buf[12];

  // Surely, I've mounted spiffs before entering here

  spiffs_file fd = SPIFFS_open(&fs, "my_file", SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_RDWR, 0);
  if (fd < 0) {
	  printf("open error %i\n", SPIFFS_errno(&fs));
  }
  if (SPIFFS_write(&fs, fd, (u8_t *)"Hello SPIFFS", 13) < 0) printf("errno %i\n", SPIFFS_errno(&fs));
  SPIFFS_close(&fs, fd);

  fd = SPIFFS_open(&fs, "my_file", SPIFFS_RDWR, 0);
  if (SPIFFS_read(&fs, fd, (u8_t *)buf, 13) < 0) printf("errno %i\n", SPIFFS_errno(&fs));
  SPIFFS_close(&fs, fd);

  printf("--> %s <--\n", buf);
}
