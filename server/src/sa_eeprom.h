/*******************************************************************************
Copyright (C) 2015 OLogN Technologies AG

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*******************************************************************************/

#if !defined __SA_EEPROM_H__
#define __SA_EEPROM_H__

#include <simpleiot/siot_common.h>

// data IDs (for communication with eeprom
#define EEPROM_SLOT_DATA_SASP_NONCE_LW_ID 0 // Nonce Lower Watermark
#define EEPROM_SLOT_DATA_SASP_NONCE_LS_ID 1 // Nonce to use For Sending

// data sizes
#define DATA_REINCARNATION_ID_SIZE 16 // reincarnation id is treated as a not listed slot with special processing of DATA_REINCARNATION_ID_SIZE * 2 size
#define DATA_SASP_NONCE_LW_SIZE 6 // Nonce Lower Watermark
#define DATA_SASP_NONCE_LS_SIZE 6 // Nonce to use For Sending

#define EEPROM_SLOT_MAX 2
// ...to be continued

// ret codes
#define EEPROM_RET_REINCARNATION_ID_OLD 0
#define EEPROM_RET_REINCARNATION_ID_OK_ONE_OK 1
#define EEPROM_RET_REINCARNATION_ID_OK_BOTH_OK 2

//#define DATA_CONTINUE_LIFE_ID 0Xff // FAKE data used at simulator startup: if not present, a new life (whatever it means) is started


// calls
bool init_eeprom_access();

uint8_t eeprom_check_reincarnation( uint8_t* rid ); // returns one of EEPROM_RET_REINCARNATION_ID_XXX
bool eeprom_check_at_start(); // returns true, if all slots are OK; TODO: it should be upper level logic to determine what to do with each corrupted slot separately
void eeprom_update_reincarnation_if_necessary( uint8_t* rid );

void eeprom_write( uint8_t id, uint8_t* data, uint16_t param );
void eeprom_read( uint8_t id, uint8_t* data, uint16_t param);

#endif // __SA_EEPROM_H__