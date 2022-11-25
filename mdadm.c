#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"
#include "cache.h"

int is_mounted = 0;
int is_written = 0;

const enum { 
	BLOCK_ID_OFF = 0, 
	DISK_ID_OFF = 8,
	COMMAND_OFF = 12,
	RESERVED_OFF = 18
} bitOffsets;

int mdadm_mount(void) {
// Try to mount
	return jbod_operation(( JBOD_MOUNT << COMMAND_OFF), NULL) == 0 ? 1 : -1;
	
}

int mdadm_unmount(void) {
 // Try to unmount 
	return jbod_operation(( JBOD_UNMOUNT << COMMAND_OFF), NULL) == 0 ? 1 : -1;
	
}

int mdadm_write_permission(void){
	// Get write permission
  if(jbod_operation(( JBOD_WRITE_PERMISSION << COMMAND_OFF), NULL) == 0){
    is_written = 1; 
    return 0;
  }
  return -1; 
}


int mdadm_revoke_write_permission(void){
	// Remove write permission
	if(jbod_operation(( JBOD_WRITE_PERMISSION << COMMAND_OFF), NULL) == 0){
		is_written = 0; 
		return 0;
	}
  return -1; 
}


int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)  {
	// Catch invalid paramters
	if(start_addr < 0 || start_addr > 1048576 || read_len > 2048 || read_len + start_addr > 1048576|| (read_buf == NULL && read_len != 0)) {return -1;} // Check bounds and if read is too long
	// Define a count to see if any of our commands fail 
	int n = 0;
	// Define the number of reads
	int j = 0;
	for(int i = 0; i < read_len; i++){
		// Create memory for a block
		uint8_t cur_block[256];
		if(cache_lookup((start_addr) / 65536, start_addr % 256, cur_block) != 1){
			// Seek to the correct disk 
			n += jbod_operation((JBOD_SEEK_TO_DISK << COMMAND_OFF) | ((start_addr + i) / 65536 << DISK_ID_OFF), NULL);	
			// Seek to the correct block
			n += jbod_operation((JBOD_SEEK_TO_BLOCK << COMMAND_OFF) | (((start_addr + i) / 256) % 256 << BLOCK_ID_OFF), NULL);
			// Read current block into our temp buffer
			n += jbod_operation((JBOD_READ_BLOCK << COMMAND_OFF), cur_block);	
		}
		// Write to our return buffer
		read_buf[i] = cur_block[(i + start_addr) % 256];
		j++;
	}
	// If no errors where detected, return the number of reads else it failed. 
	return n == 0 ? j : -1;
}



int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf) {
	// Check for write permission
	if(is_written == 0) return -1;
  	// Catch invalid paramters
	if(start_addr < 0 || start_addr > 1048576 || write_len > 2048 || write_len + start_addr > 1048576|| (write_buf == NULL && write_len != 0)) {return -1;} // Check bounds and if read is too long

  	// Define a count to see if any of our commands fail 
	int n = 0;
	// Define count for number of writes
	int j = 0;

	// Set the last block and last disk looked at 
	int lb = ((start_addr) / 256) % 256; 
	int ld = (start_addr) / 65536; 
	
	// Place to hold data
	uint8_t cur_data[256]; 

	// Seek to the correct disk 
	n += jbod_operation((JBOD_SEEK_TO_DISK << COMMAND_OFF) | ((start_addr) / 65536 << DISK_ID_OFF), NULL);	
	// Seek to the correct block
	n += jbod_operation((JBOD_SEEK_TO_BLOCK << COMMAND_OFF) | (((start_addr) / 256) % 256 << BLOCK_ID_OFF), NULL);
	// Read current block into our temp buffer
	n += jbod_operation((JBOD_READ_BLOCK << COMMAND_OFF), cur_data);

	for(int i = 0; i < write_len; i++){
		// Seek to the correct disk 
		n += jbod_operation((JBOD_SEEK_TO_DISK << COMMAND_OFF) | ((start_addr + i) / 65536 << DISK_ID_OFF), NULL);	
		// Seek to the correct block
		n += jbod_operation((JBOD_SEEK_TO_BLOCK << COMMAND_OFF) | (((start_addr + i) / 256) % 256 << BLOCK_ID_OFF), NULL);
		
		// If the disk or block is different, re-read the block.
		if(ld != (start_addr + i) / 65536 || ((lb != ((start_addr + i) / 256) % 256) && (ld == (start_addr + i) / 65536))) {
			// Seek to the correct disk 
			n += jbod_operation((JBOD_SEEK_TO_DISK << COMMAND_OFF) | ((start_addr + i) / 65536 << DISK_ID_OFF), NULL);	
			// Seek to the correct block
			n += jbod_operation((JBOD_SEEK_TO_BLOCK << COMMAND_OFF) | (((start_addr + i) / 256) % 256 << BLOCK_ID_OFF), NULL);
			// Read current block into our temp buffer
			n += jbod_operation((JBOD_READ_BLOCK << COMMAND_OFF), cur_data);
			// Go back to the correct block
			n += jbod_operation((JBOD_SEEK_TO_BLOCK << COMMAND_OFF) | (((start_addr + i) / 256) % 256 << BLOCK_ID_OFF), NULL);
			// Set new last disk and last block
			ld = (start_addr + i) / 65536;
			lb = ((start_addr + i) / 256) % 256;
		}

		// Write to the correct block position
		cur_data[(start_addr + i) % 256] = write_buf[i];
		cache_insert((start_addr + i) / 65536, ((start_addr + i) / 256) % 256, cur_data);
		// Write the block
		n += jbod_operation((JBOD_WRITE_BLOCK << COMMAND_OFF), cur_data);
		j++;
	}
	// If no errors where detected, return the number of reads else it failed. 
	return n == 0 ? j : -1;
}