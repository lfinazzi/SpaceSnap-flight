#include "main.h"

volatile uint8_t rx_buffer[AIRMAC_SIZE+1] = {0}; 		// EnduroSat RS-485 incoming buffer
uint8_t tx_buffer[AIRMAC_SIZE] = {0};					// EnduroSat RS-485 outgoing buffer
uint8_t instr_number = 0;								// Current instruction number
uint8_t instr_opcode[OPCODE_SIZE] = {0};				// Current instruction opcode

volatile uint8_t rx_flag = 0;							// Flag for new incoming message
volatile uint16_t rx_size = 0;							// size of incoming message	through RS-485

volatile uint8_t uss_comm_reset = 0;					// LS-02 input reset

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart4;

void Log(char *message)
{
	timestamp = HAL_GetTick();
	total_seconds = timestamp / 1000;
	seconds = total_seconds % 60;
	minutes = (total_seconds % 3600) / 60;
	hours = total_seconds / 3600;
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
	tx_buffer[0] = RESPONSE_INIT_BYTE; 										// An init byte to identify USS responses
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);						// RE high (disabled)
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);						// DE high (enabled)
	HAL_UART_Transmit(&huart1, (uint8_t*) tx_buffer, AIRMAC_SIZE, 200);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);					// RE low (enabled)
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);					// DE low (disabled)

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
	instr_number = rx_buffer[1];

	if (instr_number != CMD_GET_STATUS_ID)							// Saves last command ID
	        board_status.last_instruction = instr_number;
	const command_t* cmd = GetCommand(instr_number);
	if (cmd == NULL){
		tx_buffer[1] = COMMAND_NOT_FOUND_FAILURE;
		board_status.last_cmd_status = CMD_NOT_FOUND;
		return CMD_NOT_FOUND;
	}

	// This block is used to block and command you want to send with the incorrect number of opcode bytes
	if (rx_size == 2 && cmd->takes_opcode == NO_OPCODE){						// instruction with no opcode
		memset(instr_opcode, 0, sizeof(instr_opcode));							// fills opcode with zeroes, unused here
	}
	else if (rx_size == OPCODE_SIZE+2 && cmd->takes_opcode == HAS_OPCODE){		// instruction with opcode
		for(int i = 2; i < OPCODE_SIZE+2; i++){
			instr_opcode[i-2] = rx_buffer[i];
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
	if (*rx_buffer == USS_ID && state != STATE_IGNORE){
	  if(delayed_flag == 1){ // USS was in STATE_DELAYED_PICTURE
		  delayed_flag = 0;
		  Log("Canceling scheduled photo...\r\n");
	  }
	  Log("Received valid USS request\r\n");
	  cmd_ret = LoadInstructionBuffer();						// Loads the instruction buffer - 1B instruction + 5B opcode
	  rx_flag = 0;												// Resets rx_flag for next command
	  if(cmd_ret == CMD_OK) state = STATE_EXECUTE_COMMAND;		// If not ok, ignore and return to IDLE
	  else{
		  state = STATE_TRANSMIT_RESPONSE;
	  }
	}
	else{
	  Log("Received LS-02 command\r\n");
	  DisableRS485();
	  if(delayed_flag == 1){ 				// USS was in STATE_DELAYED_PICTURE
		  Log("Ignoring...\r\n");
	  }
	  state = fallback_state;
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

