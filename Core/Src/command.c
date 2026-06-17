#include "command.h"
#include "photo.h"
#include "status.h"
#include "sram.h"
#include "comms.h"
#include "fram.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "stm32f2xx_hal.h"

extern IWDG_HandleTypeDef hiwdg;
extern cam_params_t cam_params;

// Command table — add new entries here, matching the extern declaration in command.h, for variable return size, change here!
const command_t command_table[] = {
    { "CMD_TakePicture",        CMD_TAKE_PICTURE_ID,          CMD_TakePicture,         HAS_OPCODE, 	AIRMAC_SIZE - 2},
    { "CMD_TakePictureDelayed", CMD_TAKE_PICTURE_DELAYED_ID,  CMD_TakePictureDelayed,  HAS_OPCODE, 	AIRMAC_SIZE - 2},
    { "CMD_ChangeCamParams",    CMD_CHANGE_CAM_PARAMS_ID,     CMD_ChangeCamParams,     HAS_OPCODE, 	AIRMAC_SIZE - 2},
    { "CMD_CompressRawPhoto",   CMD_COMPRESS_PHOTO_ID,     	  CMD_CompressRawPhoto,    HAS_OPCODE, 	AIRMAC_SIZE - 2},
    { "CMD_GetStatus",          CMD_GET_STATUS_ID,            CMD_GetStatus,           NO_OPCODE , 	AIRMAC_SIZE - 2},
    { "CMD_DumpRaw",   			CMD_DUMP_RAW_ID,     		  CMD_DumpRaw,    		   HAS_OPCODE, 	AIRMAC_SIZE - 2},
    { "CMD_EraseFRAM",   		CMD_ERASE_FRAM_ID,     		  CMD_EraseFRAM,    	   NO_OPCODE , 	AIRMAC_SIZE - 2},
	{ "CMD_DumpCompressed",   	CMD_DUMP_COMPRESSED_ID,       CMD_DumpCompressed,      NO_OPCODE , 	AIRMAC_SIZE - 2},
	{ "CMD_ForceReset",   		CMD_FORCE_RESET_ID,       	  CMD_ForceReset,          NO_OPCODE , 	AIRMAC_SIZE - 2},
	{ "CMD_SendRawFrame",   	CMD_SEND_RAW_FRAME_ID,        CMD_SendRawFrame,        HAS_OPCODE, 	AIRMAC_SIZE - 2},
	{ "CMD_SendCompFrame",   	CMD_SEND_COMP_FRAME_ID,       CMD_SendCompFrame,       HAS_OPCODE, 	AIRMAC_SIZE - 2},
	{ "CMD_SendRawHeader",   	CMD_SEND_RAW_HEADER_ID,       CMD_SendRawHeader,       HAS_OPCODE, 	AIRMAC_SIZE - 2},
	{ "CMD_SendCompHeader",   	CMD_SEND_COMP_HEADER_ID,      CMD_SendCompHeader,      HAS_OPCODE, 	AIRMAC_SIZE - 2},
    // ... add more here
};

const uint16_t COMMAND_COUNT = sizeof(command_table) / sizeof(command_table[0]);

const command_t* current_command_pointer = NULL;
CMD_ReturnStatus cmd_ret;

uint32_t picture_delay_start = 0;				// Moment the delayed photo instruction was executed
uint8_t picture_delay_mins = 0;					// Amount of N-minute intervals to take a delayed photo
uint8_t delayed_flag = 0;						// Flag used to transmit scheduled command buffer for CMD_TakeDelayedPicture() only once

uint8_t ignore_flag = 0;						// Used to only transmit "Waiting for reset" in debug UART the first time you enter STATE_IGNORE


void ReturnCode(CMD_ReturnStatus st)
{
	if (st == CMD_OK)
	{
		char log_buf[128];
		int offset = sprintf(log_buf, "Command executed: %s | opcode: ", current_command_pointer->name);		// Log to tell with which opcode command was executed with
		for (int i = 0; i < OPCODE_SIZE; i++)
		{
			offset += sprintf(log_buf + offset, "%02X ", instr_opcode[i]);
		}
		sprintf(log_buf + offset, "\r\n");
		Log(log_buf);
		tx_buffer[1] = COMMAND_SUCCESS;						// Command executing successfully
	}
	else if (st == CMD_ERROR) {
	  tx_buffer[1] = COMMAND_ERROR;							// Command return generic error!
	}
	else if (st == CMD_BUFFER_UNOCCUPIED) {
		  tx_buffer[1] = COMMAND_BUFFER_UNOCCUPIED;			// Command return generic error!
	}
	else if (st == CMD_BUFFER_OOB) {
		tx_buffer[1] = COMMAND_BUFFER_OUT_OF_BOUNDS;
	}
	else if (st == CMD_CAM_BOOT_ERROR) {
		tx_buffer[1] = COMMAND_CAM_BOOT_ERROR;
	}
	else if (st == CMD_CAM_DCMI_ERROR) {
		tx_buffer[1] = COMMAND_CAM_DCMI_ERROR;
	}
	else if (st == CMD_COMPRESS_ERROR) {
			tx_buffer[1] = COMMAND_COMPRESS_ERROR;
	}
	else if (st == CMD_FRAM_FULL) {
			tx_buffer[1] = COMMAND_FRAM_FULL;
	}
	else if (st == CMD_BUFFER_INVALID) {
			tx_buffer[1] = COMMAND_BUFFER_INVALID;
	}
	else if (st == CMD_INDEX_FULL) {
			tx_buffer[1] = COMMAND_INDEX_FULL;
	}
	// TODO: when implementing Commands, different runtime errors will be handled here! Add accordingly.
}


// ===== Lookup Function =====
const command_t* GetCommand(uint8_t instruction_number)
{
    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        if (command_table[i].instruction_number == instruction_number) {
            return &command_table[i];
        }
    }
    Log("Invalid command received, not executed\r\n");
    return NULL;
}

// ===== Execute Function =====
CMD_ReturnStatus ExecuteCommand(const command_t *command, uint8_t *opcode)
{
    CMD_ReturnStatus st = CMD_OK;
    if (!command) {
        tx_buffer[1] = COMMAND_NOT_FOUND_FAILURE;
        board_status.last_cmd_status = CMD_NOT_FOUND;
        return CMD_NOT_FOUND;
    }

    st = command->handler(opcode);
    ReturnCode(st);																	// writes the correct return code to tx_buffer[1]

    if (command->instruction_number != CMD_GET_STATUS_ID){
    	board_status.last_instruction = instr_number;
    	board_status.last_cmd_status = st;											// Logs last command return executed
    	memcpy(board_status.last_opcode, opcode, OPCODE_SIZE);		// Logs last command opcode
    	// TODO: Maybe write the command executed and opcode for some commands that have unused bytes?
    }
    return st;
}

/*
 * opcode[0] --> buffer number (4 MSb), CAM number (4 lsb)
 * opcode[1] --> Use black filtering? 0 if no, 1 if yes
 * opcode[2] --> photo tries if black filtering enabled. Otherwise unused
 * opcode[3] --> black threshold for filtering if enabled. Otherwise unised
 * opcode[4] --> Unused for CMD_TakePicture
 *
 * Take a single pic with CAM N (0 for A, 1 for B) and save in BUFFER 0 with opcode: 0N 00 00 00 00
 */
CMD_ReturnStatus CMD_TakePicture(uint8_t *opcode)
{
	char log_buf[64];

	uint8_t cam_number 		= opcode[0] & 0x0F;					// 0000_1111 mask,
	uint8_t buffer_number 	= (opcode[0] & 0xF0) >> 4;;	    	// 1111_0000 mask, upper nibble
	uint8_t filter_flag 	= opcode[1];
	uint8_t tries 		 	= opcode[2];
	uint8_t black_threshold = opcode[3];
	// opcode 4 unused here

	if (buffer_number >= RAW_PHOTO_COUNT) {
		Log("Invalid buffer number!\r\n");
		return CMD_BUFFER_INVALID;
	}

	if(cam_number == 0){
		ActivateCAMA();

		HAL_StatusTypeDef ret = CAM_Init(CAM_I2C_ADDR_A);
		if(ret != HAL_OK)
		{
			sprintf(log_buf, "Camera A init FAILED, ret=%d\r\n", ret);
			Log(log_buf);
			DeactivateCAMA();
			return CMD_CAM_BOOT_ERROR;
		}
		Log("Camera A init OK\r\n");

		// TODO: Algorithm to check for black pixels?
		if (Photo_CaptureRaw(buffer_number, board_status.photos_taken, opcode) != HAL_OK) {
			Log("CAMA: photo capture FAILED\r\n");
			DeactivateCAMA();
			return CMD_CAM_DCMI_ERROR;
		}

		HAL_Delay(10);
		DeactivateCAMA();
	}
	else if(cam_number == 1){
		ActivateCAMB();

		HAL_StatusTypeDef ret = CAM_Init(CAM_I2C_ADDR_B);
		if(ret != HAL_OK)
		{
			sprintf(log_buf, "Camera B init FAILED, ret=%d\r\n", ret);
			Log(log_buf);
			DeactivateCAMB();
			return CMD_CAM_BOOT_ERROR;
		}
		Log("Camera B init OK\r\n");

		if (Photo_CaptureRaw(buffer_number, board_status.photos_taken, opcode) != HAL_OK) {
			Log("CAMB: photo capture FAILED\r\n");
			DeactivateCAMB();
			return CMD_CAM_DCMI_ERROR;
		}

		HAL_Delay(10);
		DeactivateCAMB();
	}
	else{
		Log("Wrong camera number!\r\n");
		return CMD_CAM_BOOT_ERROR;
	}


	board_status.photos_taken++; 		// Increment the number of photos taken

	switch(buffer_number){
	case 0:
		board_status.raw_buffer_1_occupied = 1;
		break;
	case 1:
		board_status.raw_buffer_2_occupied = 1;
		break;
	case 2:
		board_status.raw_buffer_3_occupied = 1;
		break;
	case 3:
		board_status.raw_buffer_4_occupied = 1;
		break;
	case 4:
		board_status.raw_buffer_5_occupied = 1;
		break;
	}

	return CMD_OK;
}

/*
 * opcode[0:3] --> Same as CMD_TakePicture
 * opcode[4] --> picture delay (max delay = 255*MIN_INTERVAL mins. If MIN_INTERVAL = 5, max. delay is 21h
 */
CMD_ReturnStatus CMD_TakePictureDelayed(uint8_t *opcode) {
	picture_delay_mins = opcode[4]; 							// delay is Byte 5 of opcode
	picture_delay_start = HAL_GetTick();

	// write tx_buffer for USS return
	tx_buffer[1] = COMMAND_SCHEDULED;
	tx_buffer[2] = picture_delay_mins;

	char log_buf[64];
    sprintf(log_buf, "Scheduling delayed photo: %u x %umin intervals\r\n", picture_delay_mins, MIN_INTERVAL);
    Log(log_buf);

	return CMD_SCHEDULED;
}

/*
 * no opcode
 */
CMD_ReturnStatus CMD_GetStatus(uint8_t *opcode)
{
    board_status.uptime_session = HAL_GetTick();

    _Static_assert(sizeof(board_status_t) <= AIRMAC_SIZE - 1, "board_status_t too large for tx_buffer");	// static assert for airmac_board_status_t size

    memcpy(&tx_buffer[1], &board_status, sizeof(board_status_t));		// outputs the abridged board status to the tx_buffer (which is too large)
    LogBoardStatusFull();		// Outputs a summary of the board status to UART4 (debug) for human viewing

    return CMD_OK;
}

/*
 * opcode[0] --> raw buffer number to dump
 */
CMD_ReturnStatus CMD_DumpRaw(uint8_t *opcode)
{
	uint8_t slot = opcode[0];

	if (slot >= RAW_PHOTO_COUNT) {
		Log("Invalid buffer number!\r\n");
		return CMD_BUFFER_INVALID;
	}

	raw_photo_t *raw = (raw_photo_t *)RAW_BUFFER(slot);
	char log_buf[64];

	sprintf(log_buf, "  opcode:    %02X %02X %02X %02X %02X\r\n",
			raw->opcode[0],
			raw->opcode[1],
			raw->opcode[2],
			raw->opcode[3],
			raw->opcode[4]);
	Log(log_buf);

	uint32_t timestamp = ((uint32_t)raw->timestamp_MSB << 16) | raw->timestamp_LSB;
	sprintf(log_buf, "  timestamp: 0x%08lX (%lu)\r\n", timestamp, timestamp);
	Log(log_buf);


	DumpRawBuffer(slot, L * H * sizeof(uint16_t));		// Takes a while!
	return CMD_OK;
}

/*
 * No opcode, can include it in case of many compressed buffers
 */
CMD_ReturnStatus CMD_DumpCompressed(uint8_t *opcode)
{
	compressed_photo_t *compression = COMPRESSED_BUFFER(0);		// Only one compressed buffer
	char log_buf[64];

	// TODO: Check if there is no compression and throw an error if there isn't (if you try this function without a compression, board will enter bad state)

	sprintf(log_buf, "  opcode:    %02X %02X %02X %02X %02X\r\n",
	        compression->opcode[0],
	        compression->opcode[1],
	        compression->opcode[2],
	        compression->opcode[3],
	        compression->opcode[4]);
	Log(log_buf);

	uint32_t timestamp = ((uint32_t)compression->timestamp_MSB << 16) | compression->timestamp_LSB;
	sprintf(log_buf, "  timestamp: 0x%08lX (%lu)\r\n", timestamp, timestamp);
	Log(log_buf);

	sprintf(log_buf, "  quality: %d\r\n", compression->quality);
	Log(log_buf);

	uint32_t size = ((uint32_t)compression->size_MSB << 16) | compression->size_LSB;
	DumpCompressedBuffer(0, size);			// Only one compressed buffer
	return CMD_OK;
}

/*
 * opcode[0] --> index of variable parameter to change
 * opcode[1] --> value MSB
 * opcode[2] --> value LSB
 */
// TODO: Untested, for now AE is not used in taking pictures
CMD_ReturnStatus CMD_ChangeCamParams(uint8_t *opcode)
{
	uint8_t idx = opcode[0];		// number of parameter to change
	uint16_t value = ( opcode[1] << 8 ) | opcode[2];
	switch(idx) {
	case 0:
		cam_params.ae_rule_algo_val = value;		// TODO: other parameters?
		break;
	default:
		break;
	}

	return CMD_OK;
}

/*
 * opcode[0] --> buffer to compress
 * opcode[1] --> quality, 1, 2 or 3 (from worse to best)
 */
CMD_ReturnStatus CMD_CompressRawPhoto(uint8_t *opcode)
{
	uint8_t buffer = opcode[0];
	int quality = opcode[1];
	uint8_t ret = 0;

	if (buffer >= RAW_PHOTO_COUNT) {
		Log("Invalid buffer number!\r\n");
		return CMD_BUFFER_INVALID;
	}

	Log("Photo compression initiated.\r\n");
	ret = CompressRawPhoto(buffer, quality);
	if (ret == 0){		// Compression failed
		Log("Compression error!\r\n");
		return CMD_COMPRESS_ERROR;
	}

	// Saves compression in FRAM and advances status pointer
	compressed_photo_t *compression = COMPRESSED_BUFFER(0);
	uint32_t jpeg_size = ((uint32_t)compression->size_MSB << 16) | compression->size_LSB;
	uint32_t header_size = sizeof(compressed_photo_t) - sizeof(compression->data);
	uint32_t total_size  = header_size + jpeg_size;

	if (board_status.compression_ptr_address + total_size > FIRMWARE_BACKUP_START) {		// Checks if FRAM will overflow allowed space (before FW backup start)
		Log("FRAM full — cannot save compression.\r\n");
		return CMD_FRAM_FULL;
	}

	if (board_status.compression_count >= MAX_COMPRESSED_PHOTOS) {
		Log("Compression index full.\r\n");
		return CMD_INDEX_FULL;
	}

	uint16_t idx = board_status.compression_count;
	compression_table[idx].fram_address = board_status.compression_ptr_address;
	compression_table[idx].total_size   = total_size;
	compression_table[idx].valid        = 1;

	char log_buf[96];
	sprintf(log_buf, "Index[%u].fram_address = 0x%06lX\r\n", idx, compression_table[idx].fram_address);
	Log(log_buf);

	// Save header (everything before data[])
	SaveBufferFRAM((uint8_t *)compression, header_size, board_status.compression_ptr_address);
	board_status.compression_ptr_address += header_size;

	// Save JPEG data
	SaveBufferFRAM(compression->data, jpeg_size, board_status.compression_ptr_address);
	board_status.compression_ptr_address += jpeg_size;

	if (board_status.compression_ptr_address <= FIRMWARE_BACKUP_START) {
		board_status.fram_bytes_left = FIRMWARE_BACKUP_START - board_status.compression_ptr_address;
	} else {
		board_status.fram_bytes_left = 0;   // shouldn't happen, but avoids unsigned underflow if it does
		Log("WARNING: compression_ptr_address exceeds FIRMWARE_BACKUP_START!\r\n");
	}

	// Increment number of compressions in memory by one
	board_status.compression_count++;		// compressions in memory currently
	board_status.compressions_done++;		// total compressions done

	return CMD_OK;
}

/*
 * No opcode
 */
CMD_ReturnStatus CMD_EraseFRAM(uint8_t *opcode)
{
	EraseFRAM();
	return CMD_OK;
}

CMD_ReturnStatus CMD_ForceReset(uint8_t *opcode)
{
    Log("Forced reset requested. Resetting...\r\n");
    board_status.requested_power_downs++;		// Increase by one the number of requested power downs

    // Save board status before reset...
    SaveBufferFRAM((uint8_t *)&board_status, sizeof(board_status), BOARD_STATUS_START);

    tx_buffer[1] = COMMAND_SUCCESS;		// Force to send a successful message to GS before reset
    TransmitBufferRS485();				// Sends the message through RS485

    __disable_irq();

    // Reconfigure IWDG for fastest possible timeout (~125us with prescaler 4, reload 0)
    hiwdg.Init.Prescaler = IWDG_PRESCALER_4;
    hiwdg.Init.Reload    = 0;

    HAL_IWDG_Init(&hiwdg);

    while (1) {
        // IWDG fires almost immediately
    }
    return HAL_OK;	// This will never be reached
}

CMD_ReturnStatus CMD_SendRawFrame(uint8_t *opcode)
{
	uint8_t slot = opcode[0];		// buffer selected
	uint32_t offset = ((uint32_t)opcode[1] << 24) | ((uint32_t)opcode[2] << 16) |
	                   ((uint32_t)opcode[3] <<  8) |  (uint32_t)opcode[4];				// start address

	if (slot >= RAW_PHOTO_COUNT) {
		Log("Invalid buffer number!\r\n");
		return CMD_BUFFER_INVALID;
	}

	volatile raw_photo_t *raw_buffer = RAW_BUFFER(slot);

	// 117 bytes of payload per chunk (AIRMAC_SIZE = 119, first 2 bytes reserved)
	uint32_t frame_size = sizeof(raw_buffer->data);

	if (offset >= frame_size) {
		Log("Out of bounds address requested!\r\n");
		return CMD_BUFFER_OOB;
	}

	uint32_t chunk_size = AIRMAC_SIZE - 2;  // First two bytes are start byte and CMD_RETURN
	uint32_t remaining  = frame_size - offset;
	if (chunk_size > remaining) chunk_size = remaining;  // last chunk: 33 bytes

	// Populates tx_buffer with response frame starting at correct address
	memcpy(&tx_buffer[2], (uint8_t*)raw_buffer->data + offset, chunk_size);

	// Zero-pad if this is the final partial chunk
	if (chunk_size < (AIRMAC_SIZE - 2)) {
		memset(&tx_buffer[2 + chunk_size], 0, (AIRMAC_SIZE - 2) - chunk_size);		// First two bytes are start byte and CMD_RETURN
	}

	return CMD_OK;
}

CMD_ReturnStatus CMD_SendCompFrame(uint8_t *opcode)
{
	uint8_t  index  = opcode[0];
	uint32_t offset = ((uint32_t)opcode[1] << 24) | ((uint32_t)opcode[2] << 16) |
	                   ((uint32_t)opcode[3] <<  8) |  (uint32_t)opcode[4];				// start address

	if (index >= board_status.compression_count) {
		Log("Invalid compression number!\r\n");
		return CMD_BUFFER_INVALID;
	}
	if (!compression_table[index].valid) {
		Log("Compression not valid!\r\n");
		return CMD_BUFFER_INVALID;
	}

	uint32_t header_size = sizeof(compressed_photo_t) - (2*L*H);  // size of compressed photo header
	uint32_t total_size  = compression_table[index].total_size;
	uint32_t jpeg_size   = total_size - header_size;
	uint32_t jpeg_start  = compression_table[index].fram_address + header_size;

	if (offset >= jpeg_size) {
		Log("Out of bounds address requested!\r\n");
		return CMD_BUFFER_OOB;
	}

	uint32_t chunk_size = AIRMAC_SIZE - 2;  // first two bytes are start byte and CMD_RETURN
	uint32_t remaining  = jpeg_size - offset;
	if (chunk_size > remaining) chunk_size = remaining;  // last chunk: partial

	ReadBufferFRAM(&tx_buffer[2], chunk_size, jpeg_start + offset);

	// Zero-pad if this is the final partial chunk
	if (chunk_size < (AIRMAC_SIZE - 2)) {
		memset(&tx_buffer[2 + chunk_size], 0, (AIRMAC_SIZE - 2) - chunk_size);		// First two bytes are start byte and CMD_RETURN
	}

	return CMD_OK;
}

CMD_ReturnStatus CMD_SendRawHeader(uint8_t *opcode)
{
	uint8_t slot = opcode[0];		// buffer selected

	if (slot >= RAW_PHOTO_COUNT) {
		Log("Invalid buffer number!\r\n");
		return CMD_BUFFER_INVALID;
	}

	volatile raw_photo_t *raw_buffer = RAW_BUFFER(slot);

	// Header = everything before data[]: designator, opcode[5], timestamp_MSB, timestamp_LSB
	uint32_t header_size = sizeof(raw_photo_t) - sizeof(raw_buffer->data);  // header only!

	// Header is small and fixed-size — fits in a single response, no chunking needed
	memcpy(&tx_buffer[2], (uint8_t*)raw_buffer, header_size);		// First two bytes are start byte and CMD_RETURN

	return CMD_OK;
}

CMD_ReturnStatus CMD_SendCompHeader(uint8_t *opcode)
{
	uint8_t index = opcode[0];
	char verify_buf[160];

	if (index >= board_status.compression_count) {
		Log("Invalid buffer number!\r\n");
		return CMD_BUFFER_INVALID;
	}
	if (!compression_table[index].valid) {
		Log("Compression not valid!\r\n");
		return CMD_BUFFER_INVALID;
	}

	uint32_t fram_address = compression_table[index].fram_address;

	// Header = everything before data[]: index, designator, opcode[5],
	// quality, size_MSB, size_LSB, timestamp_MSB, timestamp_LSB
	uint32_t header_size = sizeof(compressed_photo_t) - (2*L*H);  // header size

	sprintf(verify_buf, "Reading index=%u, fram_address=0x%06lX, valid=%u, compression_count=%u\r\n",
	        index,
	        compression_table[index].fram_address,
	        compression_table[index].valid,
	        board_status.compression_count);
	Log(verify_buf);

	// Header is small and fixed-size — fits in a single response, no chunking needed. Write directly to tx_buffer
	ReadBufferFRAM(&tx_buffer[2], header_size, fram_address);			// First two bytes are start byte and CMD_RETURN

	// Print what was actually read into tx_buffer
	int pos = sprintf(verify_buf, "Header at 0x%06lX: ", fram_address);
	for (int i = 0; i < (int)header_size; i++) {
		pos += sprintf(verify_buf + pos, "%02X ", tx_buffer[2 + i]);	// First two bytes are start byte and CMD_RETURN
	}
	sprintf(verify_buf + pos, "\r\n");
	Log(verify_buf);

	return CMD_OK;
}



