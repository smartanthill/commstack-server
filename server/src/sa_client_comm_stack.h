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

#if !defined __SA_CLIENT_COMMSTACK_H__
#define __SA_CLIENT_COMMSTACK_H__

#include <simpleiot/siot_common.h>
#include "commstack_commlayer.h"
#include <hal_time_provider.h>
#include <simpleiot/siot_oud_protocol.h>
#include <simpleiot/siot_s_protocol.h>
#include <simpleiot/siot_gd_protocol.h>
#include <simpleiot/siot_m_protocol.h>
#include <stdio.h>
#include "debugging.h"

#include <stdlib.h>     /* atoi */

typedef struct _DEVICE_CONTEXT
{
	uint16_t device_id;
	uint8_t AES_ENCRYPTION_KEY[16];
	SASP_DATA sasp_data;
	SAGDP_DATA sagdp_context_app;
	SAGDP_DATA sagdp_context_ctr;
	MEMORY_HANDLE MEMORY_HANDLE_SAGDP_LSM_APP;
	MEMORY_HANDLE MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR;
	MEMORY_HANDLE MEMORY_HANDLE_SAGDP_LSM_CTR;
	MEMORY_HANDLE MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR;
} DEVICE_CONTEXT;

typedef struct _PACKET_ASSOCIATED_DATA
{
	REQUEST_REPLY_HANDLE packet_h;
	REQUEST_REPLY_HANDLE addr_h;
	uint8_t mesh_val;
	uint8_t resend_cnt;
} PACKET_ASSOCIATED_DATA;

#define MAIN_DEVICES_RET_OK 0
#define MAIN_DEVICES_RET_ALREADY_EXISTS 1

#ifdef __cplusplus
extern "C" {
#endif

unsigned int main_get_device_count();
DEVICE_CONTEXT* main_get_device_data_by_device_id( unsigned int device_id );
DEVICE_CONTEXT* main_get_device_data_by_index( unsigned int idx );
uint8_t main_add_new_device( uint16_t device_id, uint8_t* key );
uint8_t main_init_device( uint16_t device_id, uint8_t* key );
uint8_t main_remove_device( uint16_t device_id );

#ifdef __cplusplus
}
#endif


#endif // __SA_CLIENT_COMMSTACK_H__
