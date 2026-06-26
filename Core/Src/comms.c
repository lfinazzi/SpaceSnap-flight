/**
  ******************************************************************************
  * @file           : comms.c
  * @brief          : RS-485 communication driver — frame parsing and transmission
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#include "main.h"

volatile uint8_t rx_buffer[AIRMAC_SIZE+1] = {0}; 		// EnduroSat RS-485 incoming buffer
uint8_t tx_buffer[AIRMAC_SIZE] = {0};					// EnduroSat RS-485 outgoing buffer
uint8_t instr_number = 0;								// Current instruction number
uint8_t instr_opcode[OPCODE_SIZE] = {0};				// Current instruction opcode

volatile uint8_t rx_flag = 0;							// Flag for new incoming message
volatile uint16_t rx_size = 0;							// size of incoming message	through RS-485

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart4;

void Log(char *message)
{
	char timestamp_string[15] = {0};

	uint32_t timestamp = HAL_GetTick();
	uint32_t total_seconds = timestamp / 1000;
	uint32_t seconds = total_seconds % 60;
	uint32_t minutes = (total_seconds % 3600) / 60;
	uint32_t hours = total_seconds / 3600;
	sprintf(timestamp_string, "[%05lu:%02lu:%02lu] ", hours, minutes, seconds);		// Generates timestamp string

	HAL_UART_Transmit(&huart4, (uint8_t *) timestamp_string, 15, LOG_UART_TIMEOUT);
	HAL_UART_Transmit(&huart4, (uint8_t *) message, strlen(message), LOG_UART_TIMEOUT);
	return;
}

void ClearTxBuffer(void)
{
	for(int i = 0; i < AIRMAC_SIZE; i++){
		tx_buffer[i] = 0;
	}
	return;
}

void TransmitBufferUART(void)
{
	tx_buffer[0] = RESPONSE_INIT_BYTE; 		// An init byte to identify USS responses
	HAL_UART_Transmit(&huart1, (uint8_t*) tx_buffer, AIRMAC_SIZE, 200);
	ClearTxBuffer();
}

void TransmitBufferRS485(void)
{
	tx_buffer[0] = RESPONSE_INIT_BYTE; 																	// An init byte to identify USS responses
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);													// RE high (disabled)
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);													// DE high (enabled)

	// Return tailored for each command, header is fixed size
	HAL_UART_Transmit(&huart1, (uint8_t*) tx_buffer, current_command_pointer->return_size + HEADER_SIZE, 200);

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);												// RE low (enabled)
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);												// DE low (disabled)

	ClearTxBuffer();
	ResetLS02();
}

void ResetLS02(void)
{
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);				// Sets LS-02 reset GPIO to low
	HAL_Delay(10);                                           		// Hold high for 10ms
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);    		// Pull low
}

CMD_ReturnStatus LoadInstructionBuffer(void)
{
	instr_number = rx_buffer[RX_HEADER_SIZE];

	if (instr_number != CMD_GET_STATUS_ID)							// Saves last command ID
	        board_status.last_instruction = instr_number;
	const command_t* cmd = GetCommand(instr_number);
	if (cmd == NULL){
		tx_buffer[1] = COMMAND_NOT_FOUND_FAILURE;
		board_status.last_cmd_status = CMD_NOT_FOUND;
		return CMD_NOT_FOUND;
	}

	// This block is used to block and command you want to send with the incorrect number of opcode bytes
	if (rx_size == RX_HEADER_SIZE+1 && cmd->takes_opcode == NO_OPCODE){			// instruction with no opcode
		memset(instr_opcode, 0, sizeof(instr_opcode));							// fills opcode with zeroes, unused here
	}
	else if (rx_size == OPCODE_SIZE+RX_HEADER_SIZE+1 && cmd->takes_opcode == HAS_OPCODE){		// instruction with opcode
		for(int i = RX_HEADER_SIZE+1; i < OPCODE_SIZE+RX_HEADER_SIZE+1; i++){		// First two bytes here are: USS_ID designator, instruction to execute
			instr_opcode[i-RX_HEADER_SIZE-1] = rx_buffer[i];
		}
	}
	else{
		Log("Wrong number of parameters in requested instruction!\r\n");
		tx_buffer[1] = COMMAND_INCORRECT_PARAMETER_FAILURE;
		if (cmd->instruction_number != CMD_GET_STATUS_ID)
			board_status.last_cmd_status = CMD_INCORRECT_PARS;
		return CMD_INCORRECT_PARS;
	}

	// This parses instruction number and instruction received
	char log_buf[128];
	int offset = sprintf(log_buf, "Instruction received: %02X opcode: ", instr_number);
	for (int i = 0; i < OPCODE_SIZE; i++)
	{
	    offset += sprintf(log_buf + offset, "%02X ", instr_opcode[i]);
	}
	offset += sprintf(log_buf + offset, "\r\n");
	Log(log_buf);

	return CMD_OK;
}

void HandleIncomingCommand(app_state_t fallback_state)
{
	rx_flag = 0;
	if (*rx_buffer == USS_ID && board_status.state != STATE_IGNORE){
	  if(delayed_flag == 1){ // USS was in STATE_DELAYED_PICTURE
		  delayed_flag = 0;
		  Log("Canceling scheduled photo...\r\n");
	  }
	  Log("Received valid USS request\r\n");
	  cmd_ret = LoadInstructionBuffer();									// Loads the instruction buffer - 1B instruction + 5B opcode
	  rx_flag = 0;															// Resets rx_flag for next command
	  if(cmd_ret == CMD_OK) board_status.state = STATE_EXECUTE_COMMAND;		// If not ok, ignore and return to IDLE
	  else{
		  board_status.state = STATE_TRANSMIT_RESPONSE;
	  }
	}
	else if (*rx_buffer == LS02_ID){
	  Log("Received LS-02 command\r\n");
	  DisableRS485();
	  if(delayed_flag == 1){ 				// USS was in STATE_DELAYED_PICTURE
		  Log("Ignoring...\r\n");
	  }
	  board_status.state = fallback_state;
	}
	else {
		Log("Unknown board ID, ignoring...\r\n");
		board_status.state = STATE_IDLE;
	}
}

void DisableRS485(void)
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); 	// RE high (disabled)
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);	// DE low (disabled)
	HAL_UART_AbortReceive(&huart1);							// Aborts UART1 reception
}

void EnableListenRS485(void)
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);	// RE low (enabled)
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);	// DE low (disabled)
	HAL_UARTEx_ReceiveToIdle_IT(&huart1, (uint8_t*)rx_buffer, AIRMAC_SIZE+1);  // re-arm UART1
}

int PollUSSReset(void)
{
	// Raw GPIO
	GPIO_PinState raw = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8);
	if (raw == GPIO_PIN_RESET) {
		return 0;   // Pin is low
	}
	else
		return 1;	// pin is high
}

void LogRawFrameDebug(uint8_t slot, uint32_t offset, uint32_t frame_size,
                       uint32_t chunk_size, uint32_t remaining)
{
	char log_buf[96];

	Log("--- CMD_SendRawFrame debug ---\r\n");

	sprintf(log_buf, "slot: %u\r\n", slot);
	Log(log_buf);

	sprintf(log_buf, "offset: %lu (0x%06lX)\r\n", offset, offset);
	Log(log_buf);

	sprintf(log_buf, "frame_size: %lu bytes\r\n", frame_size);
	Log(log_buf);

	sprintf(log_buf, "chunk_size: %lu bytes\r\n", chunk_size);
	Log(log_buf);

	sprintf(log_buf, "remaining (before this chunk): %lu bytes\r\n", remaining);
	Log(log_buf);

	uint8_t is_final_chunk = (chunk_size < (AIRMAC_SIZE - HEADER_SIZE));
	sprintf(log_buf, "final_chunk: %s\r\n", is_final_chunk ? "yes" : "no");
	Log(log_buf);

	if (is_final_chunk) {
		uint32_t pad_bytes = (AIRMAC_SIZE - HEADER_SIZE) - chunk_size;
		sprintf(log_buf, "zero_pad_bytes: %lu\r\n", pad_bytes);
		Log(log_buf);
	}

	Log("Payload hex dump (tx_buffer[2..]):\r\n");

	for (uint32_t row = 0; row < (AIRMAC_SIZE - HEADER_SIZE); row += 16) {
		int pos = sprintf(log_buf, "  %04lX: ", row);

		uint32_t row_len = ((AIRMAC_SIZE - HEADER_SIZE) - row < 16) ? ((AIRMAC_SIZE - HEADER_SIZE) - row) : 16;

		for (uint32_t col = 0; col < row_len; col++) {
			pos += sprintf(log_buf + pos, "%02X ", tx_buffer[HEADER_SIZE + row + col]);
		}

		sprintf(log_buf + pos, "\r\n");
		Log(log_buf);
	}

	Log("---------------------------------------------------\r\n");
}

void LogRawHeaderDebug(uint8_t slot, volatile raw_photo_t *raw_buffer, uint32_t header_size)
{
	char log_buf[96];

	Log("--- CMD_SendRawHeader debug ---\r\n");

	sprintf(log_buf, "slot: %u\r\n", slot);
	Log(log_buf);

	sprintf(log_buf, "designator: %u\r\n", raw_buffer->designator);
	Log(log_buf);

	int pos = sprintf(log_buf, "opcode: ");
	for (int i = 0; i < OPCODE_SIZE; i++) {
		pos += sprintf(log_buf + pos, "%04X ", raw_buffer->opcode[i]);
	}
	sprintf(log_buf + pos, "\r\n");
	Log(log_buf);

	uint32_t timestamp = ((uint32_t)raw_buffer->timestamp_MSB << 16) | raw_buffer->timestamp_LSB;
	sprintf(log_buf, "timestamp: %lu (0x%08lX)\r\n", timestamp, timestamp);
	Log(log_buf);

	uint32_t black_pixels = ((uint32_t)raw_buffer->black_pixels_MSB << 16) | raw_buffer->black_pixels_LSB;
	sprintf(log_buf, "black_pixels: %lu\r\n", black_pixels);
	Log(log_buf);

	sprintf(log_buf, "header_size: %lu bytes\r\n", header_size);
	Log(log_buf);

	Log("Header hex dump (tx_buffer[2..]):\r\n");

	for (uint32_t row = 0; row < header_size; row += 16) {
		int hpos = sprintf(log_buf, "  %04lX: ", row);

		uint32_t row_len = (header_size - row < 16) ? (header_size - row) : 16;

		for (uint32_t col = 0; col < row_len; col++) {
			hpos += sprintf(log_buf + hpos, "%02X ", tx_buffer[HEADER_SIZE + row + col]);
		}

		sprintf(log_buf + hpos, "\r\n");
		Log(log_buf);
	}

	Log("---------------------------------------------------\r\n");
}

void LogCompHeaderDebug(uint8_t index, uint32_t fram_address, uint32_t header_size)
{
	char log_buf[96];

	Log("--- CMD_SendCompHeader decoded ---\r\n");

	// Decode each field directly from tx_buffer[HEADER_SIZE..], matching compressed_photo_t layout
	compressed_photo_t *hdr = (compressed_photo_t *)&tx_buffer[HEADER_SIZE];

	sprintf(log_buf, "index: %u\r\n", hdr->index);
	Log(log_buf);

	sprintf(log_buf, "designator: %u\r\n", hdr->designator);
	Log(log_buf);

	int pos = sprintf(log_buf, "opcode: ");
	for (int i = 0; i < OPCODE_SIZE; i++) {
		pos += sprintf(log_buf + pos, "%04X ", hdr->opcode[i]);
	}
	sprintf(log_buf + pos, "\r\n");
	Log(log_buf);

	sprintf(log_buf, "quality: %u\r\n", hdr->quality);
	Log(log_buf);

	uint32_t comp_size = ((uint32_t)hdr->size_MSB << 16) | hdr->size_LSB;
	sprintf(log_buf, "compression_size: %lu bytes\r\n", comp_size);
	Log(log_buf);

	uint32_t timestamp = ((uint32_t)hdr->timestamp_MSB << 16) | hdr->timestamp_LSB;
	sprintf(log_buf, "timestamp: %lu (0x%08lX)\r\n", timestamp, timestamp);
	Log(log_buf);

	uint32_t black_pixels = ((uint32_t)hdr->black_pixels_MSB << 16) | hdr->black_pixels_LSB;
	sprintf(log_buf, "black_pixels: %lu\r\n", black_pixels);
	Log(log_buf);

	sprintf(log_buf, "fram_address: 0x%06lX\r\n", fram_address);
	Log(log_buf);

	sprintf(log_buf, "header_size: %lu bytes\r\n", header_size);
	Log(log_buf);

	Log("---------------------------------------------------\r\n");
}

void LogCompFrameDebug(uint8_t index, uint32_t offset, uint32_t header_size, uint32_t total_size,
                        uint32_t jpeg_size, uint32_t jpeg_start, uint32_t chunk_size, uint32_t remaining)
{
	char log_buf[96];

	Log("--- CMD_SendCompFrame debug ---\r\n");

	sprintf(log_buf, "index: %u\r\n", index);
	Log(log_buf);

	sprintf(log_buf, "offset: %lu (0x%06lX)\r\n", offset, offset);
	Log(log_buf);

	sprintf(log_buf, "header_size: %lu bytes\r\n", header_size);
	Log(log_buf);

	sprintf(log_buf, "total_size: %lu bytes\r\n", total_size);
	Log(log_buf);

	sprintf(log_buf, "jpeg_size: %lu bytes\r\n", jpeg_size);
	Log(log_buf);

	sprintf(log_buf, "jpeg_start (FRAM addr): 0x%06lX\r\n", jpeg_start);
	Log(log_buf);

	sprintf(log_buf, "chunk_size: %lu bytes\r\n", chunk_size);
	Log(log_buf);

	sprintf(log_buf, "remaining (before this chunk): %lu bytes\r\n", remaining);
	Log(log_buf);

	uint8_t is_final_chunk = (chunk_size < (AIRMAC_SIZE - HEADER_SIZE));
	sprintf(log_buf, "final_chunk: %s\r\n", is_final_chunk ? "yes" : "no");
	Log(log_buf);

	if (is_final_chunk) {
		uint32_t pad_bytes = (AIRMAC_SIZE - HEADER_SIZE) - chunk_size;
		sprintf(log_buf, "zero_pad_bytes: %lu\r\n", pad_bytes);
		Log(log_buf);
	}

	Log("Payload hex dump (tx_buffer[2..]):\r\n");

	for (uint32_t row = 0; row < (AIRMAC_SIZE - HEADER_SIZE); row += 16) {
		int pos = sprintf(log_buf, "  %04lX: ", row);

		uint32_t row_len = ((AIRMAC_SIZE - HEADER_SIZE) - row < 16) ? ((AIRMAC_SIZE - HEADER_SIZE) - row) : 16;

		for (uint32_t col = 0; col < row_len; col++) {
			pos += sprintf(log_buf + pos, "%02X ", tx_buffer[HEADER_SIZE + row + col]);
		}

		sprintf(log_buf + pos, "\r\n");
		Log(log_buf);
	}

	Log("-------------------------------\r\n");
}


void CMD_PopulateEcho(uint8_t *opcode)
{
    tx_buffer[2] = board_status.last_instruction;
    tx_buffer[3] = opcode[0];
    tx_buffer[4] = opcode[1];
    tx_buffer[5] = opcode[2];
    tx_buffer[6] = opcode[3];
    tx_buffer[7] = opcode[4];
}
