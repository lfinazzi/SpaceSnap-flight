#include "command.h"
#include "photo.h"
#include "status.h"
#include "sram.h"
#include "comms.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "stm32f2xx_hal.h"

extern IWDG_HandleTypeDef hiwdg;

// Command table — add new entries here, matching the extern declaration in command.h
const command_t command_table[] = {
    { "CMD_TakePicture",        CMD_TAKE_PICTURE_ID,          CMD_TakePicture,         HAS_OPCODE},
    { "CMD_TakePictureDelayed", CMD_TAKE_PICTURE_DELAYED_ID,  CMD_TakePictureDelayed,  HAS_OPCODE},
    { "CMD_GetStatus",          CMD_GET_STATUS_ID,            CMD_GetStatus,           NO_OPCODE},
    { "CMD_DumpPictureFrame",   CMD_DUMP_PICTUREFRAME_ID,     CMD_DumpPictureFrame,    HAS_OPCODE}
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
		tx_buffer[1] = COMMAND_SUCCESS;		// Command executing successfully
	}
	else if (st == CMD_ERROR) {
	  tx_buffer[1] = COMMAND_ERROR;			// Command return generic error!
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
    	memcpy(board_status.last_opcode, instr_opcode, sizeof(instr_opcode));		// Logs last command opcode
    }
    return st;
}

CMD_ReturnStatus CMD_TakePicture(uint8_t *opcode)
{
	// TODO: Opcode parsing

	// TODO: For now, turns CAMB (using it for debug)
	ActivateCAMB();

	char log_buf[64];


    /* Functionality test — verify sensor responded correctly on boot */
    if (Camera_CommsTest(CAM_I2C_ADDR_B) != HAL_OK) {
        Log("CAMB: boot test FAILED\r\n");
        DeactivateCAMB();
        return CMD_CAM_BOOT_ERROR;
    }

    if (ASX340AT_Init(CAM_I2C_ADDR_B) != HAL_OK) {
        Log("CAMB: init FAILED\r\n");
        DeactivateCAMB();
        return CMD_CAM_BOOT_ERROR;
    }
    Log("CAMB: boot test OK\r\n");				// only tries to read some camera registers to check successful boot


    uint16_t r0030, r0032;
    CAM_ReadReg(CAM_I2C_ADDR_B, 0x0030, &r0030);
    CAM_ReadReg(CAM_I2C_ADDR_B, 0x0032, &r0032);
    snprintf(log_buf, sizeof(log_buf),
             "0030=0x%04X 0032=0x%04X\r\n", r0030, r0032);
    Log(log_buf);

    /* Read cam_port_parallel_control to confirm parallel port enabled */
    uint16_t reg_val = 0;
    CAM_ReadReg(CAM_I2C_ADDR_B, 0xC972, &reg_val);

    snprintf(log_buf, sizeof(log_buf), "CAM: C972 = 0x%04X\r\n", reg_val);
    Log(log_buf);
    // Expected: 0x0003 (port enabled, interlaced)

    /* Read DCMI CR register to confirm DCMI is enabled */
    snprintf(log_buf, sizeof(log_buf),
             "DCMI CR = 0x%08lX\r\n", (uint32_t)DCMI->CR);
    Log(log_buf);
    // Bit 14 (ENABLE) should be 1 after HAL_DCMI_Start_DMA

    /* Read sensor streaming status */
    uint16_t status_val = 0;

    /* Read DCMI_SR — DCMI status register, shows live pin states */
    snprintf(log_buf, sizeof(log_buf),
             "DCMI SR = 0x%08lX\r\n", (uint32_t)DCMI->SR);
    Log(log_buf);
    /* Bit 0 HSYNC: current state of HSYNC pin
       Bit 1 VSYNC: current state of VSYNC pin
       Bit 2 FNE:   FIFO not empty */

    /* Read sensor monitor variables */
    CAM_ReadReg(CAM_I2C_ADDR_B, 0x8000, &status_val);  // mon_major_version
    snprintf(log_buf, sizeof(log_buf),
             "CAM MON: 0x%04X\r\n", status_val);
    Log(log_buf);

    /* Read ae_track_zone — tells us if AE is running */
    CAM_ReadReg(CAM_I2C_ADDR_B, 0xA81B, &status_val);
    snprintf(log_buf, sizeof(log_buf),
             "CAM AE zone: 0x%04X\r\n", status_val);
    Log(log_buf);

    /* Read cam_frame_scan_control to confirm scan mode */
    CAM_ReadReg(CAM_I2C_ADDR_B, 0xC858, &status_val);
    snprintf(log_buf, sizeof(log_buf),
             "CAM C858: 0x%04X\r\n", status_val);
    Log(log_buf);

    /* coarse_integration_time — changes every frame when AE is running */
    uint16_t int_time_1, int_time_2;
    CAM_ReadReg(CAM_I2C_ADDR_B, 0xC840, &int_time_1);
    HAL_Delay(100);
    CAM_ReadReg(CAM_I2C_ADDR_B, 0xC840, &int_time_2);

    snprintf(log_buf, sizeof(log_buf),
             "CAM inttime: 0x%04X → 0x%04X\r\n",
             int_time_1, int_time_2);

    Log(log_buf);

    uint16_t dbg_val = 0;

    /* NTSC page registers — what did auto-config set? */
    CAM_ReadReg(CAM_I2C_ADDR_B, 0x9420, &dbg_val);
    snprintf(log_buf, sizeof(log_buf), "9420: 0x%04X\r\n", dbg_val);
    Log(log_buf);

    CAM_ReadReg(CAM_I2C_ADDR_B, 0x9422, &dbg_val);
    snprintf(log_buf, sizeof(log_buf), "9422: 0x%04X\r\n", dbg_val);
    Log(log_buf);

    CAM_ReadReg(CAM_I2C_ADDR_B, 0x9424, &dbg_val);
    snprintf(log_buf, sizeof(log_buf), "9424: 0x%04X\r\n", dbg_val);
    Log(log_buf);

    CAM_ReadReg(CAM_I2C_ADDR_B, 0x9426, &dbg_val);
    snprintf(log_buf, sizeof(log_buf), "9426: 0x%04X\r\n", dbg_val);
    Log(log_buf);

    /* TX_SS parallel hardware registers */
    CAM_ReadReg(CAM_I2C_ADDR_B, 0x3C00, &dbg_val);
    snprintf(log_buf, sizeof(log_buf), "3C00: 0x%04X\r\n", dbg_val);
    Log(log_buf);

    CAM_ReadReg(CAM_I2C_ADDR_B, 0x3C02, &dbg_val);
    snprintf(log_buf, sizeof(log_buf), "3C02: 0x%04X\r\n", dbg_val);
    Log(log_buf);

    CAM_ReadReg(CAM_I2C_ADDR_B, 0x3C04, &dbg_val);
    snprintf(log_buf, sizeof(log_buf), "3C04: 0x%04X\r\n", dbg_val);
    Log(log_buf);

    /* Output enable register */
    CAM_ReadReg(CAM_I2C_ADDR_B, 0x0032, &dbg_val);
    snprintf(log_buf, sizeof(log_buf), "0032: 0x%04X\r\n", dbg_val);
    Log(log_buf);

    /* IFP SOC2 page — parallel port control */
    CAM_ReadReg(CAM_I2C_ADDR_B, 0x3640, &dbg_val);
    snprintf(log_buf, sizeof(log_buf), "3640: 0x%04X\r\n", dbg_val);
    Log(log_buf);

    CAM_ReadReg(CAM_I2C_ADDR_B, 0x3642, &dbg_val);
    snprintf(log_buf, sizeof(log_buf), "3642: 0x%04X\r\n", dbg_val);
    Log(log_buf);

    /* DCMI SR multiple samples */
    for (int i = 0; i < 3; i++) {
        HAL_Delay(20);
        snprintf(log_buf, sizeof(log_buf),
                 "DCMI SR[%d]: 0x%02lX\r\n", i,
                 (uint32_t)(DCMI->SR & 0x07));
        Log(log_buf);
    }


    // TODO: Insert correct parameters for this
    if (Photo_CaptureRaw(0, board_status.photos_taken, opcode) != HAL_OK) {
        Log("CAMB: photo capture FAILED\r\n");
        DeactivateCAMB();
        return CMD_CAM_DCMI_ERROR;
    }

	DeactivateCAMB();
	board_status.photos_taken++; 		// Increment the number of photos taken

	// TODO: Check if camera booted correctly. If not, return CMD_CAM_BOOT_ERROR or something

	return CMD_OK;
}

// CRC handled by Endurosat OBC
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

CMD_ReturnStatus CMD_GetStatus(uint8_t *opcode)
{
    board_status.uptime_ms = HAL_GetTick();
    _Static_assert(sizeof(board_status_t) <= AIRMAC_SIZE - 1, "board_status_t too large for tx_buffer");	// static assert for status_t size

    memcpy(&tx_buffer[1], &board_status, sizeof(board_status_t));		// outputs the board status to the tx_buffer
    return CMD_OK;
}

// MAX chunk: 13FF - TODO: Why doesn't this reach until the end. FIX, there's a bug here
CMD_ReturnStatus CMD_DumpPictureFrame(uint8_t *opcode)
{

	return CMD_OK;
}
