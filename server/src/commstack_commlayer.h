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

// received packet status
#define COMMLAYER_FROM_CU_STATUS_FAILED 0
#define COMMLAYER_FROM_CU_STATUS_FOR_SLAVE 38
#define COMMLAYER_FROM_CU_STATUS_FROM_SLAVE 40
#define COMMLAYER_FROM_CU_STATUS_INITIALIZER 50
#define COMMLAYER_FROM_CU_STATUS_INITIALIZER_LAST 51
#define COMMLAYER_FROM_CU_STATUS_ADD_DEVICE 55
#define COMMLAYER_FROM_CU_STATUS_REMOVE_DEVICE 60
#define COMMLAYER_FROM_CU_STATUS_SYNC_RESPONSE 57
#define COMMLAYER_FROM_CU_STATUS_GET_DEV_PERF_COUNTERS_REQUEST 70

// sent packet status
#define COMMLAYER_TO_CU_STATUS_RESERVED_FAILED 0
#define COMMLAYER_TO_CU_STATUS_FOR_SLAVE 37
#define COMMLAYER_TO_CU_STATUS_FROM_SLAVE 35
#define COMMLAYER_TO_CU_STATUS_SLAVE_ERROR 47
#define COMMLAYER_TO_CU_STATUS_SYNC_REQUEST 56
#define COMMLAYER_TO_CU_STATUS_INITIALIZATION_DONE 60
#define COMMLAYER_TO_CU_STATUS_DEVICE_ADDED 61
#define COMMLAYER_TO_CU_STATUS_DEVICE_REMOVED 62
#define COMMLAYER_FROM_CU_STATUS_GET_DEV_PERF_COUNTERS_REPLY 70

// REQUEST/REPLY CODES
#define REQUEST_TO_CU_WRITE_DATA 0
#define REQUEST_TO_CU_READ_DATA 1
#define RESPONSE_FROM_CU_WRITE_DATA 0
#define RESPONSE_FROM_CU_READ_DATA 1

// error codes
#define COMMLAYER_TO_CU_STATUS_OK 0
#define COMMLAYER_TO_CU_STATUS_FAILED_EXISTS 1
#define COMMLAYER_TO_CU_STATUS_FAILED_INCOMPLETE_OR_CORRUPTED_DATA 2
#define COMMLAYER_TO_CU_STATUS_FAILED_UNKNOWN_REASON 3
#define COMMLAYER_TO_CU_STATUS_FAILED_UNEXPECTED_PACKET 4

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

uint8_t send_within_master( MEMORY_HANDLE mem_h, uint16_t bus_id, uint8_t destination );
uint8_t internal_try_get_message_within_master( MEMORY_HANDLE mem_h, uint16_t* bus_id );
uint8_t internal_wait_for_communication_event( waiting_for* wf );

uint8_t send_device_initialization_completion_to_central_unit( uint16_t initialization_packet_count, MEMORY_HANDLE mem_h );
uint8_t send_device_add_completion_to_central_unit( MEMORY_HANDLE mem_h, uint16_t packet_id );
uint8_t send_device_remove_completion_to_central_unit( MEMORY_HANDLE mem_h, uint16_t packet_id );
uint8_t send_stats_to_central_unit( MEMORY_HANDLE mem_h, uint16_t device_id );

uint8_t send_error_to_central_unit( MEMORY_HANDLE mem_h, uint16_t src_id );
void send_sync_request_to_central_unit_to_save_data( MEMORY_HANDLE mem_h, uint16_t deice_id, uint8_t field_id );
void send_sync_request_to_central_unit_to_get_data( MEMORY_HANDLE mem_h, uint16_t deice_id, uint8_t field_id );

#ifdef __cplusplus
}
#endif



#endif // __SA_COMMLAYER_H__