/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "../include/delta.h"
#include <stdlib.h>

uint32_t patch_size; 
static uint8_t to_flash_buf[ERASE_PAGE_SIZE + MAX_WRITE_UNIT];

uint8_t opFlag = 0;	
/** variable used to indicate source image should be moved up how many pages before aplly */
uint8_t move_up_pages = 8;			
struct detools_apply_patch_t apply_patch;


struct
{
	int addr[IMAGE_ARRAY_SIZE];
	uint16_t size[IMAGE_ARRAY_SIZE];
	uint16_t count;
} image_position_adjust;


static int delta_init_flash_mem(struct flash_mem *flash)
{
	if (!flash) {
		return -DELTA_NO_FLASH_FOUND;
	}

	/* From. */
    flash->ffrom_p = fopen("../", "rb");
	 if (flash->ffrom_p == NULL) {
        return -1;
    }

	 /* To. */
    flash->fto_p = fopen("../", "wb");
    if (flash->fto_p == NULL) {
        return -1;
    }

	 /* Patch. */
    flash->fpatch_p = fopen("../", "rb");
    if (flash->fpatch_p == NULL) {
        return -1;
    }

	move_up_pages = (((patch_size/2)/PAGE_SIZE > 0) ? ((patch_size/2)/PAGE_SIZE) : 1) ;

	flash->from_current = PRIMARY_OFFSET + PAGE_SIZE * move_up_pages;
	flash->from_end = flash->from_current + PRIMARY_SIZE - PAGE_SIZE;

	flash->to_current = PRIMARY_OFFSET;
	flash->to_end = flash->to_current + PRIMARY_SIZE - PAGE_SIZE;

	flash->erased_addr = PRIMARY_OFFSET;

	flash->patch_current = SECONDARY_OFFSET + MCUBOOT_PAD_SIZE + HEADER_SIZE;
	flash->patch_end = flash->patch_current + SECONDARY_SIZE - HEADER_SIZE - MCUBOOT_PAD_SIZE - PAGE_SIZE;

	flash->write_size = 0;

	flash->backup_addr = 0;

	image_position_adjust.count = 0;

	// printf("\nInit: mcuboot_pad=0X%X from_current=0X%lX to_current=0X%lX patch_current=0X%lX STATUS_ADDRESS=0X%lX backup_addr=0x%lX\t write_size=%d\n",
	// 		MCUBOOT_PAD_SIZE,flash->from_current, flash->to_current, flash->patch_current,status_address,flash->backup_addr, flash->write_size);

	return DELTA_OK;
}



static int save_backup_image(void *arg_p)
{
	uint16_t i;
	uint32_t total_size = 0;
	uint8_t data[MAX_WRITE_UNIT + DATA_HEADER + 2];
	uint32_t addr;
	uint32_t magic = 0;
	uint32_t opFlag = DELTA_OP_TRAVERSE;

	struct flash_mem *flash = (struct flash_mem *)arg_p;	

	for (i = 0; i < image_position_adjust.count; i++)
	{
		total_size += (DATA_HEADER +image_position_adjust.size[i]);  //addr->4bytes len->2bytes				
	}
	printk("==== total_count=%d\t totat_size=%d\r\n", image_position_adjust.count,total_size);

	// return DELTA_OK;	
	return total_size;	
}



static int traverse_flash_write(void *arg_p,
					const uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash = (struct flash_mem *)arg_p;	
	if (!flash) {
		return -DELTA_CASTING_ERROR;
	}
#ifdef DELTA_ENABLE_LOG
	printk("to_flash write size 0x%x\r", size);
#endif
	flash->write_size += size;
	if (flash->write_size >= ERASE_PAGE_SIZE) {
		flash->erased_addr =  flash->to_current + ERASE_PAGE_SIZE;
#ifdef DELTA_ENABLE_LOG
		printk("==== erased_addr 0x%x\r", flash->erased_addr);
#endif
		flash->to_current += ERASE_PAGE_SIZE;
		flash->write_size = flash->write_size - ERASE_PAGE_SIZE;
	}

	return DELTA_OK;		
}



int write_last_buffer(void *arg_p)
{
	return save_backup_image(arg_p);
}


static int traverse_flash_from_read(void *arg_p,
					uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash;
	static int fatal_err = 0;

	flash = (struct flash_mem *)arg_p;
#ifdef DELTA_ENABLE_LOG
	printk("from_flash read size: 0x%x off: 0x%x\r", size, flash->from_current);
#endif
	if (!flash) {
		return -DELTA_CASTING_ERROR;
	}
	if (size <= 0) {
		return -DELTA_INVALID_BUF_SIZE;
	}

	if (fatal_err)
	{
		return -DELTA_CASTING_ERROR;
	}

	if (flash->from_current < flash->erased_addr)
	{
	#ifdef DELTA_ENABLE_LOG
		printk("=== adjust pos %d\r", image_position_adjust.count);
	#endif
		image_position_adjust.addr[image_position_adjust.count] = flash->from_current;
		image_position_adjust.size[image_position_adjust.count] = size;
		image_position_adjust.count++;
		if (image_position_adjust.count > IMAGE_ARRAY_SIZE)
		{
			fatal_err = -DELTA_WRITING_ERROR;
			return -DELTA_WRITING_ERROR;				
		}	
	}

	flash->from_current += size;
	if (flash->from_current >= flash->from_end) {
		return -DELTA_READING_SOURCE_ERROR;
	}

	return DELTA_OK;
}


static int delta_flash_patch_read(void *arg_p,
					uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash;

	flash = (struct flash_mem *)arg_p;
#ifdef DELTA_ENABLE_LOG
	printk("patch_flash read size 0x%x from %p\r\n\n", size,flash->patch_current);
#endif

	if (size <= 0) {
		return -DELTA_INVALID_BUF_SIZE;
	}

	// if (flash_read(flash_device, flash->patch_current, buf_p, size))
	if (fread(buf_p, size, 1, flash->fpatch_p) != 1)  {
		return -DELTA_READING_PATCH_ERROR;
	}
	
	return DELTA_OK;
}

int increase_patch_offset(void *arg_p,uint32_t size)
{
	struct flash_mem *flash = (struct flash_mem *)arg_p;
	flash->patch_current += (uint32_t) size;
	if (flash->patch_current >= flash->patch_end) {
		return -DELTA_READING_PATCH_ERROR;
	}
	return DELTA_OK;
}

static int delta_flash_seek(void *arg_p, int offset)
{
	struct flash_mem *flash;

	flash = (struct flash_mem *)arg_p;
#ifdef DELTA_ENABLE_LOG
	printk("from_flash seek offset %d\r", offset);
#endif
	if (!flash) {
		return -DELTA_CASTING_ERROR;
	}

	flash->from_current += offset;
	if (flash->from_current >= flash->from_end) {
		return -DELTA_SEEKING_ERROR;
	}

	fseek(flash->ffrom_p, offset, SEEK_CUR);

	return DELTA_OK;
}



static int delta_traverse_init(struct flash_mem *flash,uint32_t patch_size,struct detools_apply_patch_t *apply_patch)
{
	int ret;
	ret = delta_init_flash_mem(flash);
	ret += detools_apply_patch_init(apply_patch,
                                   traverse_flash_from_read,
                                   delta_flash_seek,
                                   patch_size,
                                   traverse_flash_write,
                                   flash);

	
	if (ret) {
		return ret;
	}

	return DELTA_OK;
}


int traverse_delta_file(struct flash_mem *flash, struct detools_apply_patch_t *apply_patch)
{
	int ret;
	
	ret = delta_traverse_init(flash,patch_size,apply_patch);
	if (ret) {
		return ret;
	}
#ifndef DELTA_ENABLE_LOG
	printf("\nTraverse: mcuboot_pad=0X%X\t from_current=0X%lX\t size=0x%X\t to_current=0X%lX\t size=0x%X\t patch_current=0X%lX\t patch_end=0X%lX\t backup_addr=0X%lX\n",
		MCUBOOT_PAD_SIZE,flash->from_current,PRIMARY_SIZE,flash->to_current,SECONDARY_SIZE,flash->patch_current,flash->patch_end, flash->backup_addr);
#endif
	ret = apply_patch_process(apply_patch, delta_flash_patch_read, patch_size, 0, flash);
	
	return ret;
}


const char *delta_error_as_string(int error)
{
	if (error < 28) {
		return detools_error_as_string(error);
	}

	if (error < 0) {
		error *= -1;
	}

	switch (error) {
	case DELTA_SLOT1_OUT_OF_MEMORY:
		return "Slot 1 out of memory.";
	case DELTA_READING_PATCH_ERROR:
		return "Error reading patch.";
	case DELTA_READING_SOURCE_ERROR:
		return "Error reading source image.";
	case DELTA_WRITING_ERROR:
		return "Error writing to slot 1.";
	case DELTA_SEEKING_ERROR:
		return "Seek error.";
	case DELTA_CASTING_ERROR:
		return "Error casting to flash_mem.";
	case DELTA_INVALID_BUF_SIZE:
		return "Read/write buffer less or equal to 0.";
	case DELTA_CLEARING_ERROR:
		return "Could not clear slot 1.";
	case DELTA_NO_FLASH_FOUND:
		return "No flash found.";
	case DELTA_PATCH_HEADER_ERROR:
		return "Error reading patch header.";
	default:
		return "Unknown error.";
	}
}
