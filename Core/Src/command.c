/**
  ******************************************************************************
  * @file           : command.c
  * @brief          : Command dispatch table and execution handlers
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#include "command.h"
#include "photo.h"
#include "status.h"
#include "sram.h"
#include "fram.h"
#include "comms.h"
#include "fram.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "stm32f2xx_hal.h"

extern IWDG_HandleTypeDef hiwdg;
extern UART_HandleTypeDef huart4;

// Command table — add new entries here, matching the extern declaration in command.h, for variable return size, change here!
const command_t command_table[] = {
    { "CMD_TakePicture",        CMD_TAKE_PICTURE_ID,          CMD_TakePicture,         HAS_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},
    { "CMD_TakePictureDelayed", CMD_TAKE_PICTURE_DELAYED_ID,  CMD_TakePictureDelayed,  HAS_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},
    { "CMD_ChangeCamParams",    CMD_CHANGE_CAM_PARAMS_ID,     CMD_ChangeCamParams,     HAS_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},
    { "CMD_CompressRawPhoto",   CMD_COMPRESS_PHOTO_ID,     	  CMD_CompressRawPhoto,    HAS_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},
    { "CMD_GetStatus",          CMD_GET_STATUS_ID,            CMD_GetStatus,           NO_OPCODE , 	AIRMAC_SIZE - HEADER_SIZE},
    { "CMD_DumpRaw",   			CMD_DUMP_RAW_ID,     		  CMD_DumpRaw,    		   HAS_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},
    { "CMD_EraseFRAM",   		CMD_ERASE_FRAM_ID,     		  CMD_EraseFRAM,    	   HAS_OPCODE,  AIRMAC_SIZE - HEADER_SIZE},		// confirm needed
	{ "CMD_DumpCompressed",   	CMD_DUMP_COMPRESSED_ID,       CMD_DumpCompressed,      NO_OPCODE , 	AIRMAC_SIZE - HEADER_SIZE},
	{ "CMD_ForceReset",   		CMD_FORCE_RESET_ID,       	  CMD_ForceReset,          NO_OPCODE , 	AIRMAC_SIZE - HEADER_SIZE},
	{ "CMD_SendRawFrame",   	CMD_SEND_RAW_FRAME_ID,        CMD_SendRawFrame,        HAS_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},
	{ "CMD_SendCompFrame",   	CMD_SEND_COMP_FRAME_ID,       CMD_SendCompFrame,       HAS_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},
	{ "CMD_SendRawHeader",   	CMD_SEND_RAW_HEADER_ID,       CMD_SendRawHeader,       HAS_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},
	{ "CMD_SendCompHeader",   	CMD_SEND_COMP_HEADER_ID,      CMD_SendCompHeader,      HAS_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},
	{ "CMD_EraseCompressions",  CMD_ERASE_COMP_ID,      	  CMD_EraseCompressions,   HAS_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},		// confirm needed
	{ "CMD_DumpAllSRAM",   		CMD_DUMP_SRAM_BIN_ID,         CMD_DumpAllSRAM,         NO_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},
	{ "CMD_DumpAllFRAM",   		CMD_DUMP_FRAM_BIN_ID,         CMD_DumpAllFRAM,         NO_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},
	{ "CMD_BackupFirmware",   	CMD_BACKUP_FIRMWARE_ID,       CMD_BackupFirmware,      HAS_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},		// confirm needed
	{ "CMD_ChangeBurstParams",  CMD_CHANGE_BURST_PARAMS_ID,   CMD_ChangeBurstParams,   HAS_OPCODE, 	AIRMAC_SIZE - HEADER_SIZE},
    // ... add more here
};


const uint16_t COMMAND_COUNT = sizeof(command_table) / sizeof(command_table[0]);

const command_t* current_command_pointer = NULL;
CMD_ReturnStatus cmd_ret;

uint32_t picture_delay_start = 0;				// Moment the delayed photo instruction was executed
uint8_t picture_delay_mins = 0;					// Amount of N-minute intervals to take a delayed photo
uint8_t delayed_flag = 0;						// Flag used to transmit scheduled command buffer for CMD_TakeDelayedPicture() only once
uint8_t buffer_burst_start = 0;					// Buffer slot to save first photo of delayed burst captures

extern fw_backup_info_t fw_backup_info;

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
	else if (st == CMD_CONFIRM_FAILED) {
		tx_buffer[1] = COMMAND_CONFIRM_FAILED;
	}
	else if (st == CMD_PARAM_INVALID) {
		tx_buffer[1] = COMMAND_PARAM_INVALID;
	}
	// when implementing Commands, different runtime errors are handled here! Add accordingly.
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
    }
    return st;
}

/*
 * opcode[0] --> buffer number (4 MSb), CAM number (4 lsb)
 * opcode[1] --> Use black filtering? 0 if no, 1 if yes
 * opcode[2] --> photo tries if black filtering enabled. Otherwise unused
 * opcode[3] --> black fraction for filtering if enabled. Otherwise unused: Values possible are 0-200 (each is 0.5% of total pixels)
 * opcode[4] --> Unused for CMD_TakePicture
 *
 * Take a single pic with CAM N (0 for A, 1 for B) and save in BUFFER 0 with opcode: 0N 00 00 00 00
 */
CMD_ReturnStatus CMD_TakePicture(uint8_t *opcode)
{
	char log_buf[64];

	uint8_t cam_number 		= opcode[0] & 0x0F;					// 0000_1111 mask,
	uint8_t buffer_number 	= (opcode[0] & 0xF0) >> 4;	    	// 1111_0000 mask, upper nibble
	uint8_t filter_flag 	= opcode[1] & 0x0F;					// 0000_1111 mask,
	uint8_t advanced_flag 	= (opcode[1] & 0xF0) >> 4;			// 1111_0000 mask, upper nibble
	uint8_t tries 		 	= opcode[2];
	uint8_t black_fraction  = opcode[3];
	// opcode 4 unused here

	if (buffer_number >= RAW_PHOTO_COUNT) {
		Log("Invalid buffer number!\r\n");
		CMD_PopulateEcho(opcode);
		return CMD_BUFFER_INVALID;
	}

	if (black_fraction > 200) {
		Log("Allowed values for black fraction are 0-200!\r\n");
		CMD_PopulateEcho(opcode);
		return CMD_PARAM_INVALID;
	}

	sprintf(log_buf, "cam=%d buf=%d filter=%d tries=%d fraction=%d\r\n",
	        cam_number, buffer_number, filter_flag, tries, black_fraction);
	Log(log_buf);

	HAL_StatusTypeDef ret;
	if(cam_number == 0){
		ActivateCAMA();

		if (advanced_flag == 0)	{	// basic mode
			Log("BASIC MODE SELECTED FOR CAM A\r\n");
			ret = CAM_Init(CAM_I2C_ADDR_A);
		}
		else {	// advanced mode
			Log("ADVANCED MODE SELECTED FOR CAM A\r\n");
			ret = CAM_InitAdvanced(CAM_I2C_ADDR_A);
		}

		if(ret != HAL_OK)
		{
			sprintf(log_buf, "Camera A init FAILED, ret=%d\r\n", ret);
			Log(log_buf);
			DeactivateCAMA();
			CMD_PopulateEcho(opcode);
			return CMD_CAM_BOOT_ERROR;
		}
		Log("Camera A init OK\r\n");

		if (filter_flag != 0){
			Log("Black filtering activated\r\n");
			if (Photo_CaptureRawBlack(buffer_number, board_status.photos_taken, opcode, tries, black_fraction) != HAL_OK) {
				Log("CAMA: photo capture FAILED\r\n");
				DeactivateCAMA();
				CMD_PopulateEcho(opcode);
				return CMD_CAM_DCMI_ERROR;
			}
		}

		else {	// no black filtering!
			if (Photo_CaptureRaw(buffer_number, board_status.photos_taken, opcode) != HAL_OK) {
				Log("CAMA: photo capture FAILED\r\n");
				DeactivateCAMA();
				CMD_PopulateEcho(opcode);
				return CMD_CAM_DCMI_ERROR;
			}
		}


		HAL_Delay(10);
		DeactivateCAMA();
	}
	else if(cam_number == 1){
		ActivateCAMB();

		if (advanced_flag == 0)	{	// basic mode
			Log("BASIC MODE SELECTED FOR CAM B\r\n");
			ret = CAM_Init(CAM_I2C_ADDR_B);
		}
		else {	// advanced mode
			Log("ADVANCED MODE SELECTED FOR CAM B\r\n");
			ret = CAM_InitAdvanced(CAM_I2C_ADDR_B);
		}

		if(ret != HAL_OK)
		{
			sprintf(log_buf, "Camera B init FAILED, ret=%d\r\n", ret);
			Log(log_buf);
			DeactivateCAMB();
			CMD_PopulateEcho(opcode);
			return CMD_CAM_BOOT_ERROR;
		}
		Log("Camera B init OK\r\n");

		if (filter_flag != 0){
			Log("Black filtering activated\r\n");
			if (Photo_CaptureRawBlack(buffer_number, board_status.photos_taken, opcode, tries, black_fraction) != HAL_OK) {
				Log("CAMB: photo capture FAILED\r\n");
				DeactivateCAMB();
				CMD_PopulateEcho(opcode);
				return CMD_CAM_DCMI_ERROR;
			}
		}
		else {
			if (Photo_CaptureRaw(buffer_number, board_status.photos_taken, opcode) != HAL_OK) {
				Log("CAMB: photo capture FAILED\r\n");
				DeactivateCAMB();
				CMD_PopulateEcho(opcode);
				return CMD_CAM_DCMI_ERROR;
			}
		}

		HAL_Delay(10);
		DeactivateCAMB();
	}
	else{
		Log("Wrong camera number!\r\n");
		CMD_PopulateEcho(opcode);
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

	CMD_PopulateEcho(opcode);
	return CMD_OK;
}

/*
 * opcode[0:3] --> Same as CMD_TakePicture
 * opcode[4] --> first picture delay (max delay = 255*MIN_INTERVAL mins. If MIN_INTERVAL = 5, max. delay is 21h
 */
CMD_ReturnStatus CMD_TakePictureDelayed(uint8_t *opcode)
{
	picture_delay_mins = opcode[4]; 						// delay is Byte 5 of opcode
	picture_delay_start = HAL_GetTick();
	buffer_burst_start = (opcode[0] & 0xF0) >> 4;	    	// 1111_0000 mask, upper nibble

	// write tx_buffer for USS return
	tx_buffer[1] = COMMAND_SCHEDULED;

	char log_buf[64];
    sprintf(log_buf, "Scheduling delayed photos after: %u x %umins\r\n", picture_delay_mins, MIN_INTERVAL);
    Log(log_buf);

	CMD_PopulateEcho(opcode);
	return CMD_SCHEDULED;
}

/*
 * no opcode
 */
CMD_ReturnStatus CMD_GetStatus(uint8_t *opcode)
{
    board_status.uptime_session = HAL_GetTick();

    _Static_assert(sizeof(board_status_t) + sizeof(fw_backup_info_t) <= AIRMAC_SIZE - DATA_HEADER_SIZE, "board_status_t too large for tx_buffer");	// static assert for airmac_board_status_t size

    memcpy(&tx_buffer[DATA_HEADER_SIZE], &board_status, sizeof(board_status_t));		// outputs the abridged board status to the tx_buffer
    memcpy(&tx_buffer[DATA_HEADER_SIZE + sizeof(board_status_t)], &fw_backup_info, sizeof(fw_backup_info_t));	// outputs the fw backup info to the tx_buffer
    LogBoardStatusFull();		// Outputs a summary of the board status to UART4 (debug) for human viewing

    // No need to log the instr + opcode, as this gives board status with that info
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
		CMD_PopulateEcho(opcode);
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

	CMD_PopulateEcho(opcode);
	return CMD_OK;
}

/*
 * No opcode, can be included in case of many compressed buffers
 */
CMD_ReturnStatus CMD_DumpCompressed(uint8_t *opcode)
{

	if (!board_status.compression_buffer_occupied) {
		Log("No compression in SRAM buffer!\r\n");
		CMD_PopulateEcho(opcode);
		return CMD_BUFFER_INVALID;
	}

	compressed_photo_t *compression = COMPRESSED_BUFFER(0);		// Only one compressed buffer
	char log_buf[64];

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

	CMD_PopulateEcho(opcode);
	return CMD_OK;
}

/*
 * opcode[0] --> index of variable parameter to change
 * opcode[1] --> value MSB
 * opcode[2] --> value LSB
 */
CMD_ReturnStatus CMD_ChangeCamParams(uint8_t *opcode)
{
	uint8_t idx = opcode[0];		// number of parameter to change
	uint16_t value = ( opcode[1] << 8 ) | opcode[2];
	CMD_ReturnStatus ret = CMD_OK;
	switch(idx) {
	case 0:
		board_status.cam_params.black_threshold = BLACK_THRESHOLD_DEFAULT;
		board_status.cam_params.sensor_analog_gain = GAIN_ANALOG_DEFAULT;
		board_status.cam_params.sensor_digital_gain = GAIN_DIGITAL_DEFAULT;
		board_status.cam_params.sensor_coarse_exposure = EXPOSURE_COARSE_DEFAULT;
		board_status.cam_params.sensor_fine_exposure = EXPOSURE_FINE_DEFAULT;
		break;
	case 1:
		board_status.cam_params.black_threshold  = value;
		break;
	case 2:
		board_status.cam_params.sensor_analog_gain  = value;
		break;
	case 3:
		board_status.cam_params.sensor_digital_gain  = value;
		break;
	case 4:
		board_status.cam_params.sensor_coarse_exposure  = value;
		break;
	case 5:
		board_status.cam_params.sensor_fine_exposure  = value;			// add new params here
		break;
	default:
		ret = CMD_ERROR;
		break;
	}

	CMD_PopulateEcho(opcode);
	return ret;
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
		CMD_PopulateEcho(opcode);
		return CMD_BUFFER_INVALID;
	}

	Log("Photo compression initiated.\r\n");
	ret = CompressRawPhoto(buffer, quality);
	if (ret == 0){		// Compression failed
		Log("Compression error!\r\n");
		CMD_PopulateEcho(opcode);
		return CMD_COMPRESS_ERROR;
	}

	// Saves compression in FRAM and advances status pointer
	compressed_photo_t *compression = COMPRESSED_BUFFER(0);
	uint32_t jpeg_size = ((uint32_t)compression->size_MSB << 16) | compression->size_LSB;
	uint32_t header_size = sizeof(compressed_photo_t) - sizeof(compression->data);
	uint32_t total_size  = header_size + jpeg_size;

	if (board_status.compression_ptr_address + total_size > FIRMWARE_BACKUP_START) {		// Checks if FRAM will overflow allowed space (before FW backup start)
		Log("FRAM full — cannot save compression.\r\n");
		CMD_PopulateEcho(opcode);
		return CMD_FRAM_FULL;
	}

	if (board_status.compression_count >= MAX_COMPRESSED_PHOTOS) {
		Log("Compression index full.\r\n");
		CMD_PopulateEcho(opcode);
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
	board_status.compression_buffer_occupied = 1;

	CMD_PopulateEcho(opcode);
	return CMD_OK;
}

/*
 * opcode[0:4] --> opcode to confirm instruction
 */
CMD_ReturnStatus CMD_EraseFRAM(uint8_t *opcode)
{
	static const uint8_t confirm_seq[OPCODE_SIZE] = {0x0A, 0x0F, 0x0A, 0x0F, 0x0A};		// opcode needed to confirm FRAM erase

	if (memcmp(opcode, confirm_seq, OPCODE_SIZE) != 0) {
		Log("FRAM erase confirmation mismatch -- aborting.\r\n");
		CMD_PopulateEcho(opcode);
		return CMD_CONFIRM_FAILED;
	}

	EraseFRAM();
	CMD_PopulateEcho(opcode);
	return CMD_OK;
}

/*
 * no opcode
 */
CMD_ReturnStatus CMD_ForceReset(uint8_t *opcode)
{
    Log("Forced reset requested. Resetting...\r\n");
    board_status.requested_power_downs++;		// Increase by one the number of requested power downs

    // Save board status before reset...
    SaveBufferFRAM((uint8_t *)&board_status, sizeof(board_status), BOARD_STATUS_START);

    tx_buffer[1] = COMMAND_SUCCESS;		// Force to send a successful message to GS before reset
	CMD_PopulateEcho(opcode);			// populates received instr + opcode before transmission
    TransmitBufferRS485();				// Sends the message through RS485

    __disable_irq();

    // Reconfigure IWDG for fastest possible timeout (~125us with prescaler 4, reload 0)
    hiwdg.Init.Prescaler = IWDG_PRESCALER_4;
    hiwdg.Init.Reload    = 0;

    HAL_IWDG_Init(&hiwdg);

    while (1) {
        // IWDG fires almost immediately
    }

    // This will never be reached
    return HAL_OK;
}

/*
 * opcode[0]   --> raw buffer to use
 * opcode[1:4] --> offset from raw buffer start to transfer
 */
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
		Log("Out of bounds address requested!\r\n");	// For first photo, last address is 0x00095FF0
		return CMD_BUFFER_OOB;
	}

	uint32_t chunk_size = AIRMAC_SIZE - DATA_HEADER_SIZE;  // First bytes are HEADER
	uint32_t remaining  = frame_size - offset;
	if (chunk_size > remaining) chunk_size = remaining;  // last chunk: 33 bytes

	// Populates tx_buffer with response frame starting at correct address
	memcpy(&tx_buffer[DATA_HEADER_SIZE], (uint8_t*)raw_buffer->data + offset, chunk_size);

	// Zero-pad if this is the final partial chunk
	if (chunk_size < (AIRMAC_SIZE - DATA_HEADER_SIZE)) {
		memset(&tx_buffer[DATA_HEADER_SIZE + chunk_size], 0, (AIRMAC_SIZE - DATA_HEADER_SIZE) - chunk_size);		// First bytes are HEADER
	}

	// Debug print to UART4
	LogRawFrameDebug(slot, offset, frame_size, chunk_size, remaining);

	// No instruction and opcode header on purpose!
	return CMD_OK;
}

/*
 * opcode[0]   --> compressed buffer to use
 * opcode[1:4] --> offset from compressed photo start to transfer
 */
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

	uint32_t chunk_size = AIRMAC_SIZE - DATA_HEADER_SIZE;  // first bytes are HEADER
	uint32_t remaining  = jpeg_size - offset;
	if (chunk_size > remaining) chunk_size = remaining;  // last chunk: partial

	ReadBufferFRAM(&tx_buffer[DATA_HEADER_SIZE], chunk_size, jpeg_start + offset);

	// Zero-pad if this is the final partial chunk
	if (chunk_size < (AIRMAC_SIZE - DATA_HEADER_SIZE)) {
		memset(&tx_buffer[DATA_HEADER_SIZE + chunk_size], 0, (AIRMAC_SIZE - DATA_HEADER_SIZE) - chunk_size);		// First bytes are HEADER
	}

	// debug print
	LogCompFrameDebug(index, offset, header_size, total_size, jpeg_size, jpeg_start, chunk_size, remaining);

	return CMD_OK;
}

/*
 * opcode[0]   --> raw buffer to use
 */
CMD_ReturnStatus CMD_SendRawHeader(uint8_t *opcode)
{
	uint8_t slot = opcode[0];		// buffer selected

	if (slot >= RAW_PHOTO_COUNT) {
		Log("Invalid buffer number!\r\n");
		CMD_PopulateEcho(opcode);
		return CMD_BUFFER_INVALID;
	}

	volatile raw_photo_t *raw_buffer = RAW_BUFFER(slot);

	// Header = everything before data[]: designator, opcode[5], timestamp_MSB, timestamp_LSB
	uint32_t header_size = sizeof(raw_photo_t) - sizeof(raw_buffer->data);  // header only!

	// Header is small and fixed-size — fits in a single response, no chunking needed
	memcpy(&tx_buffer[HEADER_SIZE], (uint8_t*)raw_buffer, header_size);		// First two bytes are start byte and CMD_RETURN

	// Debug print
	LogRawHeaderDebug(slot, raw_buffer, header_size);

	CMD_PopulateEcho(opcode);
	return CMD_OK;
}

/*
 * opcode[0]   --> compressed photo to use
 */
CMD_ReturnStatus CMD_SendCompHeader(uint8_t *opcode)
{
	uint8_t index = opcode[0];
	char verify_buf[160];

	if (index >= board_status.compression_count) {
		Log("Invalid buffer number!\r\n");
		CMD_PopulateEcho(opcode);
		return CMD_BUFFER_INVALID;
	}
	if (!compression_table[index].valid) {
		Log("Compression not valid!\r\n");
		CMD_PopulateEcho(opcode);
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
	ReadBufferFRAM(&tx_buffer[HEADER_SIZE], header_size, fram_address);			// First bytes are HEADER

	// Print what was actually read into tx_buffer
	int pos = sprintf(verify_buf, "Header at 0x%06lX: ", fram_address);
	for (int i = 0; i < (int)header_size; i++) {
		pos += sprintf(verify_buf + pos, "%02X ", tx_buffer[HEADER_SIZE + i]);	// First bytes are HEADER
	}
	sprintf(verify_buf + pos, "\r\n");
	Log(verify_buf);

	// Debug print
	LogCompHeaderDebug(index, fram_address, header_size);

	CMD_PopulateEcho(opcode);
	return CMD_OK;
}

/*
 * no opcode
 */
CMD_ReturnStatus CMD_EraseCompressions(uint8_t *opcode)
{
	static const uint8_t confirm_seq[OPCODE_SIZE] = {0xBA, 0xBF, 0x0A, 0x0F, 0x0A};		// opcode needed to confirm FRAM erase

	if (memcmp(opcode, confirm_seq, OPCODE_SIZE) != 0) {
		Log("FRAM erase confirmation mismatch -- aborting.\r\n");
		CMD_PopulateEcho(opcode);
		return CMD_CONFIRM_FAILED;
	}

	EraseCompressions();

	CMD_PopulateEcho(opcode);
	return CMD_OK;
}

/*
 * no opcode
 */
CMD_ReturnStatus CMD_DumpAllSRAM(uint8_t *opcode)
{
    uint8_t *start = (uint8_t *)RAW_PHOTO_BASE_ADDRESS;
    uint8_t *end   = (uint8_t *)END_OF_BUFFERS;
    uint32_t total = (uint32_t)(end - start);

    uint32_t offset = 0;
    while (offset < total)
    {
        uint32_t chunk = ((total - offset) < 256) ? (total - offset) : 256;
        HAL_UART_Transmit(&huart4, start + offset, chunk, HAL_MAX_DELAY);
        HAL_IWDG_Refresh(&hiwdg);
        offset += chunk;
    }

    CMD_PopulateEcho(opcode);
    return CMD_OK;
}

/*
 * no opcode
 */
CMD_ReturnStatus CMD_DumpAllFRAM(uint8_t *opcode)
{
    uint32_t total = END_OF_FRAM - BOARD_STATUS_START;
    uint8_t  staging[256];

    uint32_t offset = 0;
    while (offset < total)
    {
        uint32_t chunk = ((total - offset) < 256) ? (total - offset) : 256;

        ReadBufferFRAM(staging, chunk, BOARD_STATUS_START + offset);

        HAL_UART_Transmit(&huart4, staging, chunk, HAL_MAX_DELAY);
        HAL_IWDG_Refresh(&hiwdg);
        offset += chunk;
    }

    CMD_PopulateEcho(opcode);
    return CMD_OK;
}


/*
 * opcode[0:4] --> confirmation sequence (must match exactly, see confirm_seq below)
 */
CMD_ReturnStatus CMD_BackupFirmware(uint8_t *opcode)
{
	static const uint8_t confirm_seq[OPCODE_SIZE] = {0xB4, 0xC4, 0xB4, 0xC4, 0xB4};		// something deliberate

	if (memcmp(opcode, confirm_seq, OPCODE_SIZE) != 0) {
		Log("Firmware backup confirmation mismatch -- aborting.\r\n");
		CMD_PopulateEcho(opcode);
		return CMD_CONFIRM_FAILED;
	}

	extern uint32_t _app_flash_start;
	extern uint32_t _app_flash_end;
	uint32_t app_start = (uint32_t)&_app_flash_start;
	uint32_t app_end   = (uint32_t)&_app_flash_end;
	uint32_t app_size  = app_end - app_start;

	char log_buf[96];
	sprintf(log_buf, "Backing up firmware: 0x%08lX - 0x%08lX (%lu bytes)\r\n",
	        app_start, app_end, app_size);
	Log(log_buf);

	if (app_size == 0 || app_size > FIRMWARE_IMAGE_SIZE) {
		Log("Firmware backup aborted: app_size out of range for backup region.\r\n");
		CMD_PopulateEcho(opcode);
		return CMD_ERROR;
	}

	uint8_t  chunk[256];
	uint32_t crc       = 0xFFFFFFFF;
	uint32_t src        = app_start;
	uint32_t dst        = FIRMWARE_IMAGE_START;
	uint32_t remaining  = app_size;

	while (remaining > 0) {
		uint32_t n = (remaining > sizeof(chunk)) ? sizeof(chunk) : remaining;

		memcpy(chunk, (uint8_t *)src, n);    // flash is memory-mapped, read like any pointer
		SaveFRAM_Unlocked(chunk, n, dst);

		for (uint32_t i = 0; i < n; i++) {   // CRC32 update, zlib-compatible (matches bootloader)
			crc ^= chunk[i];
			for (int b = 0; b < 8; b++)
				crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
		}

		src += n; dst += n; remaining -= n;
		HAL_IWDG_Refresh(&hiwdg);
	}
	uint32_t final_crc = ~crc;


	// Read back from FRAM and recompute CRC from what's ACTUALLY stored,
	// to catch real SPI/FRAM write faults rather than trusting the write succeeded.
	uint32_t verify_crc = 0xFFFFFFFF;
	uint32_t verify_src  = FIRMWARE_IMAGE_START;
	remaining = app_size;
	while (remaining > 0) {
		uint32_t n = (remaining > sizeof(chunk)) ? sizeof(chunk) : remaining;
		ReadBufferFRAM(chunk, n, verify_src);
		for (uint32_t i = 0; i < n; i++) {
			verify_crc ^= chunk[i];
			for (int b = 0; b < 8; b++)
				verify_crc = (verify_crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(verify_crc & 1)));
		}
		verify_src += n; remaining -= n;
		HAL_IWDG_Refresh(&hiwdg);
	}
	verify_crc = ~verify_crc;

	if (verify_crc != final_crc) {
		sprintf(log_buf, "FRAM readback mismatch! wrote=0x%08lX read=0x%08lX\r\n", final_crc, verify_crc);
		Log(log_buf);
		CMD_PopulateEcho(opcode);
		return CMD_ERROR;
	}

	/* Only write header after image is verified — prevents bootloader
	 * from seeing a valid header pointing to a corrupt image. */
	fw_backup_info.fw_backup_size  = app_size;
	fw_backup_info.fw_backup_crc32 = final_crc;
	fw_backup_info.fw_backup_version = ((uint32_t)VERSION_MAJOR << 16) | (uint32_t)VERSION_MINOR;

	uint8_t *p = (uint8_t *)&fw_backup_info;
	for (uint8_t i = 0; i < sizeof(fw_backup_info_t); i++) {
		FRAM_WriteByte(FIRMWARE_BACKUP_START + i, p[i]);
	}

	sprintf(log_buf, "Firmware backup complete: size=%lu crc=0x%08lX\r\n", app_size, final_crc);
	Log(log_buf);

	CMD_PopulateEcho(opcode);
	return CMD_OK;
}

/*
 * opcode[0] --> 0x00 to reset defaults, 0x01 to modify burst params
 * opcode[1] --> number of burst photos
 * opcode[2] --> time in seconds between photos
 * opcode[3] --> perform compressions?
 * opcode[4] --> compression quality
 */
CMD_ReturnStatus CMD_ChangeBurstParams(uint8_t *opcode)
{
	uint8_t defaults     = opcode[0];
	uint8_t num_photos   = opcode[1];
	uint8_t time_delay   = opcode[2];
	uint8_t comp_flag  	 = opcode[3];
	uint8_t comp_quality = opcode[4];

	// catches invalid parameter changes
	if (num_photos > RAW_PHOTO_COUNT || comp_quality == 0 || comp_quality > 3) {
		CMD_PopulateEcho(opcode);
		return CMD_PARAM_INVALID;
	}

	if (defaults == 0) {	// reset defaults
		board_status.delayed_params.num_photos = BURST_NUM_PHOTOS;
		board_status.delayed_params.time_between_photos = BURST_INTERVAL;
		board_status.delayed_params.perform_compressions = BURST_COMPRESSION;
		board_status.delayed_params.compression_quality = BURST_COMPR_QUALITY;
	}
	else {
		board_status.delayed_params.num_photos = num_photos;
		board_status.delayed_params.time_between_photos = time_delay;
		board_status.delayed_params.perform_compressions = comp_flag;
		board_status.delayed_params.compression_quality = comp_quality;
	}

	CMD_PopulateEcho(opcode);
	return CMD_OK;
}





