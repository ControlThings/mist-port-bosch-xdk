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

#define SPIFFS_FLASH_ADDR  0x000B7000
#define SPIFFS_SIZE (64 * 1024)

void my_spiffs_mount() {
	spiffs_config cfg;
	cfg.phys_size = SPIFFS_SIZE;
	cfg.phys_addr = SPIFFS_FLASH_ADDR; // start spiffs at start of "Reserved" segment, see XDK110 FreeRTOS guide, chapter 4.1
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
	 // Surely, I've mounted spiffs before entering here
  spiffs_file fd;

  size_t test_len = 1024;
  uint8_t test_data[test_len];
  memset(test_data, 0xa5, test_len);

  uint8_t read_buf[test_len];
  memset(read_buf, 0, test_len);

  int32_t io_len = 0;
  fd = SPIFFS_open(&fs, "my_file", SPIFFS_RDWR, 0);
  io_len = SPIFFS_read(&fs, fd, read_buf, test_len);
  if (io_len < 0) printf("errno %i\n", SPIFFS_errno(&fs));
  SPIFFS_close(&fs, fd);
  printf("Read %i bytes\n", io_len);

  if (memcmp(test_data, read_buf, test_len) != 0) {
	  printf("Buffers differ\n");
  }
  else {
	  printf("Buffers are equal!\n");
  }


  fd = SPIFFS_open(&fs, "my_file", SPIFFS_CREAT | SPIFFS_RDWR, 0);
  if (fd < 0) {
	  printf("open error %i\n", SPIFFS_errno(&fs));
  }
  io_len = SPIFFS_write(&fs, fd, test_data, test_len);
  if (io_len < 0) printf("errno %i\n", SPIFFS_errno(&fs));
  printf("Wrote %i bytes\n", io_len);
  SPIFFS_close(&fs, fd);

  fd = SPIFFS_open(&fs, "my_file", SPIFFS_RDWR, 0);
  io_len = SPIFFS_read(&fs, fd, test_data, test_len);
  if (io_len < 0) printf("errno %i\n", SPIFFS_errno(&fs));
  SPIFFS_close(&fs, fd);
  printf("Read %i bytes\n", io_len);

}

void my_spiffs_erase_area(void) {
	if (my_spiffs_erase(SPIFFS_FLASH_ADDR, SPIFFS_SIZE) != SPIFFS_OK) {
		printf("Erasing fails!\n");
	}
	else {
		printf("Successful erase of %i bytes!\n", SPIFFS_SIZE);
	}
}

/* These are the implementations of the wish_fs I/O operations */

wish_file_t my_fs_open(const char *pathname) {
    spiffs_file fd = 0;
    fd = SPIFFS_open(&fs, pathname, SPIFFS_CREAT |  SPIFFS_RDWR, 0);
    if (fd < 0) {
        printf("Could not open file %s: error %d\n\r", pathname, SPIFFS_errno(&fs));
    }
    return fd;
}

int32_t my_fs_read(wish_file_t fd, void* buf, size_t count) {
    int32_t ret = SPIFFS_read(&fs, fd, buf, count);
    if (ret < 0) {
        if (ret == SPIFFS_ERR_END_OF_OBJECT) {
            //SPIFFS_HAL_DEBUG("EOF encountered?\n\r");
            ret = 0;
        }
        else {
            printf("read errno %d\n\r", SPIFFS_errno(&fs));
        }
    }
    return ret;
}

int32_t my_fs_write(wish_file_t fd, const void *buf, size_t count) {
    int32_t ret = SPIFFS_write(&fs, fd, (void *)buf, count);
    if (ret < 0) {
    	printf("write errno %d\n", SPIFFS_errno(&fs));
    }
    return ret;
}

wish_offset_t my_fs_lseek(wish_file_t fd, wish_offset_t offset, int whence) {
    int32_t ret = SPIFFS_lseek(&fs, fd, offset, whence);
    if (ret < 0) {
    	printf("seek errno %d\n", SPIFFS_errno(&fs));
    }
    return ret;
}

int32_t my_fs_close(wish_file_t fd) {
    int32_t ret = SPIFFS_close(&fs, fd);
    return ret;
}

int32_t my_fs_rename(const char *oldpath, const char *newpath) {
    return SPIFFS_rename(&fs, oldpath, newpath);
}


int32_t my_fs_remove(const char *path) {
    return SPIFFS_remove(&fs, path);
}
