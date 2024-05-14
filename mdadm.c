#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"
#include "net.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

static int is_mounted = 0;
static int is_permitted = 0;

// *updated assignment 3 code to pass all trace files*

//  build and store op parameter:
//  updated order of operations since last assignment to reserved, command, blockid, diskid
int op_format(int reserved, jbod_cmd_t command, int blockID, int diskID)
{
  // calculate number of bits to shift by using given operation table and bit operations
  return diskID | blockID << 4 | command << 12 | reserved << 20;
}

// mount JBOD
int mdadm_mount(void)
{

  if (is_mounted == 1)
  {
    return -1;
  }
  if (jbod_client_operation(op_format(0, JBOD_MOUNT, 0, 0), NULL) == 0)
  {
    // update flag variable is_mounted to 1 if calling mount returns 0
    is_mounted = 1;
    return 1;
  }
  return -1;
}
// Unmount Jbod
int mdadm_unmount(void)
{
  if (is_mounted == 0)
  {
    return -1;
  }
  if (jbod_client_operation(op_format(0, JBOD_UNMOUNT, 0, 0), NULL) == 0)
  {
    // update flag variable is_mounted to 0 if calling unmount returns 0
    is_mounted = 0;
    return 1;
  }
  return -1;
}

// Define maximum read len of 1024 bytes
#define MAX_READ_LEN 1024

int mdadm_write_permission(void)
{
  if (jbod_client_operation(op_format(0, JBOD_WRITE_PERMISSION, 0, 0), NULL) == 0)
  {
    // update flag var is_permitted to 1 if write permission is granted
    is_permitted = 1;
    return 0;
  }
  return -1;
}
int mdadm_revoke_write_permission(void)
{
  if (jbod_client_operation(op_format(0, JBOD_REVOKE_WRITE_PERMISSION, 0, 0), NULL) == -1)
  {
    return -1;
  }
  // update flag var is_permitted to 1 if write permission is granted
  is_permitted = 0;
  return 0;
}
// same mdadm_read code from assigniment 3
int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)
{
  if (read_len == 0)
  {
    return 0;
  }

  // If any of these fail we return -1 and exit
  if (!is_mounted || !read_buf || read_len > MAX_READ_LEN || start_addr + read_len > JBOD_DISK_SIZE * JBOD_NUM_DISKS)
  {
    return -1;
  }
  int current_addr = start_addr;

  while (start_addr + read_len > current_addr)
  {
    // calculate variables based on jbod.h values within while loop
    int current_disk = current_addr / JBOD_DISK_SIZE;
    int current_block = (current_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    int offset = current_addr % JBOD_BLOCK_SIZE;
    uint8_t block_buffer[JBOD_BLOCK_SIZE];

    if (current_disk >= JBOD_NUM_DISKS)
    {
      return -1;
    }
    // seek to disk and seek to block at current block
    jbod_operation(op_format(0, JBOD_SEEK_TO_DISK, 0, current_disk), NULL);
    jbod_operation(op_format(0, JBOD_SEEK_TO_BLOCK, current_block, 0), NULL);
    // Read from block
    jbod_operation(op_format(0, JBOD_READ_BLOCK, 0, 0), block_buffer);
    int copy_bytes = (read_len > (JBOD_BLOCK_SIZE - offset) ? JBOD_BLOCK_SIZE - offset : read_len); // calculate bytes that will be copied

    if (start_addr + read_len - current_addr > JBOD_NUM_BLOCKS_PER_DISK)
    {
      // copying source readbuf + bytes read to destination block buffer for the copy_bytes which will be 16
      memcpy(read_buf + current_addr - start_addr, block_buffer + offset, copy_bytes);
    }
    // copying block buffer to destination read buf
    int bytes_to_copy = start_addr + read_len - current_addr;
    memcpy(read_buf + current_addr - start_addr, block_buffer + offset, bytes_to_copy);

    current_addr += (JBOD_NUM_BLOCKS_PER_DISK - offset); // update current address after exiting while
    offset = 0;                                          // reset offset variable
  }
  // return read_len which should be 16 bytes
  return read_len;
}

// Helper function to find the minimum for bytes_to_write calculation in mdadm_write
static inline int min(int x, int y)
{
  return (x < y) ? x : y;
}
// second helper function for mdadm_write calls seek to block and seek to disk, make code more robust
void seek_to_disk_then_block(int current_disk, int current_block)
{
  jbod_client_operation(op_format(0, JBOD_SEEK_TO_DISK, 0, current_disk), NULL);
  jbod_client_operation(op_format(0, JBOD_SEEK_TO_BLOCK, current_block, 0), NULL);
}

// updated mdadm_write code from lab 3 to pass trace files
int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf)
{
  // check if write length is 0, return 0 if true, ensuring there are no null pointers
  if (write_len == 0 || !is_mounted || !write_buf ||
      write_len > MAX_READ_LEN || start_addr + write_len > JBOD_DISK_SIZE * JBOD_NUM_DISKS ||
      !is_permitted)
  {
    return -1;
  }
  int bytes_written = 0;                 // track bytes written
  uint8_t block_buffer[JBOD_BLOCK_SIZE]; // create buffer

  // loop as long as bytes_written stays less than the write_len parameter
  while (bytes_written < write_len)
  {
    // calculate current disk and blocks using jbod.h values
    int current_addr = start_addr + bytes_written;
    int current_disk = current_addr / JBOD_DISK_SIZE;
    int current_block = (current_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    int offset = current_addr % JBOD_BLOCK_SIZE;

    // seek to current disk/block
    seek_to_disk_then_block(current_disk, current_block);

    // read current block to buffer
    jbod_client_operation(op_format(0, JBOD_READ_BLOCK, 0, 0), block_buffer);

    // calculate number of bytes that need to be written using min helper, will be used in memcpy
    int bytes_to_write = min(JBOD_BLOCK_SIZE - offset, write_len - bytes_written);

    // copy data to block_buffer, bytes_to_write is amount being copied here
    memcpy(&block_buffer[offset], &write_buf[bytes_written], bytes_to_write);

    // seek to final disk/block
    seek_to_disk_then_block(current_disk, current_block);
    jbod_client_operation(op_format(0, JBOD_WRITE_BLOCK, 0, 0), block_buffer); // write to buffer again

    bytes_written += bytes_to_write; // update bytes_written value using bytes that were copied
  }

  return write_len;
}
