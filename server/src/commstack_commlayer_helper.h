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

#if !defined __SA_COMMLAYER_HELPER_H__
#define __SA_COMMLAYER_HELPER_H__

#include <simpleiot/siot_common.h>
#include <zepto_mem_mngmt_hal_spec.h>
#include <simpleiot_hal/siot_mem_mngmt.h>
#include <simpleiot_hal/hal_waiting.h>

// status codes
#define COMMLAYER_SYNC_STATUS_GO_THROUGH 0
#define COMMLAYER_SYNC_STATUS_WAIT_FOR_REPLY 1

// RET codes
#define COMMLAYER_SYNC_STATUS_OK 0
#define COMMLAYER_SYNC_STATUS_NO_MORE_PACKETS 1


#ifdef __cplusplus
extern "C" {
#endif

void cscl_start_waiting( uint16_t packet_id );
void cscl_add_new_packet_to_queue( uint8_t src, uint16_t sz, uint16_t addr, uint8_t* data );
uint8_t cscl_is_queued_packet();
uint8_t cscl_get_oldest_packet_size( uint16_t* sz );
uint8_t cscl_get_oldest_packet_and_remove_from_queue( uint8_t* src, uint16_t* sz, uint16_t* addr, uint8_t* data, uint16_t max_sz );

#ifdef __cplusplus
}
#endif



#endif // __SA_COMMLAYER_HELPER_H__