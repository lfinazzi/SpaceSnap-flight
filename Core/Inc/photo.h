#ifndef __PHOTO_H__
#define __PHOTO_H__

#include "command.h"

#include <stdint.h>

#define L 						(480U)		// Raw image length
#define H 						(640U)		// Raw image height

typedef struct {					  		// all 16b variables to avoid struct padding
	uint16_t designator;			  		// global raw photo number taken
	uint16_t opcode[OPCODE_SIZE]; 			// opcodes sent to take picture
	uint32_t timestamp;			      		// timestamp is uint32_t
	uint16_t data[L*H];               		// Image data in YCbCr 4:2:2 format
} raw_photo_t;

typedef struct {
	uint16_t index;					  		// index of compressed photo
	uint16_t *address;			  	 		// memory address start for picture
	uint32_t size;				 	  		// size of compressed photo
	uint32_t timestamp;				  		// internal timestamp
	uint16_t opcode[OPCODE_SIZE];			// instruction + opcode, saved in 16b to avoid padding
} compressed_metadata_t;

typedef struct {
    uint16_t *data;  						// compressed photo data
} compressed_photo_t;



#endif
