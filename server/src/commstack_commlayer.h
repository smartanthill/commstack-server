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

#if !defined __SA_COMMLAYER_H__
#define __SA_COMMLAYER_H__

#include <simpleiot/siot_common.h>
#include <zepto_mem_mngmt_hal_spec.h>
#include <simpleiot_hal/siot_mem_mngmt.h>
#include <simpleiot_hal/hal_waiting.h>

// RET codes
#define COMMLAYER_RET_FAILED 0
#define COMMLAYER_RET_OK 1
#define COMMLAYER_RET_PENDING 2

#define HAL_GET_PACKET_BYTES_IN_PROGRESS 0
#define HAL_GET_PACKET_BYTES_FAILED 1
#define HAL_GET_PACKET_BYTES_DONE 2

#define COMMLAYER_RET_FROM_CENTRAL_UNIT 10
#define COMMLAYER_RET_FROM_DEV 11
#define COMMLAYER_RET_TIMEOUT 12

#define COMMLAYER_RET_FROM_COMMM_STACK 10

// received packet status
#define COMMLAYER_RET_OK_CU_FOR_SLAVE 38
#define COMMLAYER_RET_OK_SLAVE_FOR_CU 40
#define COMMLAYER_RET_OK_INITIALIZER 50
#define COMMLAYER_RET_OK_INITIALIZER_LAST 51
#define COMMLAYER_RET_OK_ADD_DEVICE 55
#define COMMLAYER_RET_OK_SYNC_CONFIRMATION 57

// sent packet status
#define COMMLAYER_STATUS_FOR_CU_FROM_SLAVE 35
#define COMMLAYER_STATUS_FOR_SLAVE 37
#define COMMLAYER_STATUS_FOR_CU_SLAVE_ERROR 47
#define COMMLAYER_STATUS_FOR_CU_SYNC_REQUEST 55

// REQUEST CODES
#define REQUEST_WRITE_DATA 0
#define REQUEST_READ_DATA 1

#ifdef __cplusplus
extern "C" {
#endif

extern uint16_t self_port_num_with_cl;

bool communication_initialize();
void communication_terminate();
uint8_t send_message( MEMORY_HANDLE mem_h, uint16_t bus_id );
//uint8_t hal_get_packet_bytes( MEMORY_HANDLE mem_h );

uint8_t wait_for_communication_event( waiting_for* wf );
uint8_t try_get_message_within_master( MEMORY_HANDLE mem_h, uint16_t* bus_id );
uint8_t send_to_central_unit( MEMORY_HANDLE mem_h, uint16_t src_id );

uint8_t send_error_to_central_unit( MEMORY_HANDLE mem_h, uint16_t src_id );
void send_sync_request_to_central_unit_to_save_data( MEMORY_HANDLE mem_h, uint16_t deice_id, uint8_t field_id );
void send_sync_request_to_central_unit_to_get_data( MEMORY_HANDLE mem_h, uint16_t deice_id, uint8_t field_id );

#ifdef __cplusplus
}
#endif



#endif // __SA_COMMLAYER_H__