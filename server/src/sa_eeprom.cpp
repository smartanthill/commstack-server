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

extern "C" {
#include <simpleiot/siot_common.h>
#include <simpleiot_hal/siot_mem_mngmt.h>
#include "commstack_commlayer.h"
#include <simpleiot_hal/siot_eeprom.h>
}

// items below were defined for various reasons in this projec;
// STL does not like such redefinitions (see <xkeycheck.h> for details); we do favor for STL
// If one knows how this could be addressed properly, just do it!

#ifdef bool
#undef bool
#endif
#ifdef true
#undef true
#endif
#ifdef false
#undef false
#endif
#ifdef printf
#undef printf
#endif
#ifdef fprintf
#undef fprintf
#endif

//#include <xkeycheck.h>
#include <vector>
#include <list>
using namespace std;

#define EEPROM_CHECKSUM_SIZE 4

typedef struct _PER_DEVICE_STORAGE_DATA
{
	uint16_t device_id;
	uint8_t* dev_data;
	uint16_t dev_data_sz;
} PER_DEVICE_STORAGE_DATA;

typedef vector< PER_DEVICE_STORAGE_DATA > STORAGE_DATA_OF_ALL_DEVICES;
STORAGE_DATA_OF_ALL_DEVICES storage_data_of_all_devices;

uint8_t perm_storage_add_device( uint16_t device_id )
{
	for ( unsigned int i=0; i<storage_data_of_all_devices.size(); i++ )
		if ( storage_data_of_all_devices[i].device_id == device_id )
			return PERM_STORAGE_RET_ALREADY_EXISTS;
	PER_DEVICE_STORAGE_DATA device;
	device.device_id = device_id;
	device.dev_data_sz = DATA_SASP_NONCE_LW_SIZE + DATA_SASP_NONCE_LS_SIZE + 2 * EEPROM_CHECKSUM_SIZE;
	device.dev_data = new uint8_t[ device.dev_data_sz ];
	if ( device.dev_data == NULL )
		return PERM_STORAGE_RET_OUT_OF_MEM;
	storage_data_of_all_devices.push_back( device );
	return PERM_STORAGE_RET_OK;
}

uint8_t perm_storage_remove_device( uint16_t device_id )
{
	for ( unsigned int i=0; i<storage_data_of_all_devices.size(); i++ )
		if ( storage_data_of_all_devices[i].device_id == device_id )
		{
			if ( storage_data_of_all_devices[i].dev_data != NULL )
				delete [] storage_data_of_all_devices[i].dev_data;
			storage_data_of_all_devices.erase( storage_data_of_all_devices.begin() + i );
			return PERM_STORAGE_RET_OK;
		}
	return PERM_STORAGE_RET_DOES_NOT_EXIST;
}

PER_DEVICE_STORAGE_DATA* perm_storage_get_device_data_by_device_id( unsigned int device_id )
{
	for ( unsigned int i=0; i<storage_data_of_all_devices.size(); i++ )
		if ( storage_data_of_all_devices[i].device_id == device_id )
			return &(storage_data_of_all_devices[i]);
	return NULL;
}


void update_fletcher_checksum16( uint8_t bt, uint16_t* state )
{
	// quick and dirty solution
	// TODO: implement
	uint8_t tmp = (uint8_t)(*state);
	uint8_t tmp1 = (*state) >> 8;
	tmp += bt;
	if ( tmp < bt )
		tmp += 1;
	if ( tmp == 0xFF )
		tmp = 0;
	tmp1 += tmp;
	if ( tmp1 < tmp )
		tmp1 += 1;
	if ( tmp1 == 0xFF )
		tmp1 = 0;
	*state = tmp1;
	*state <<= 8;
	*state += tmp;
}

void update_fletcher_checksum_32( uint16_t wrd, uint16_t* state )
{
	// quick and dirty solution
	// TODO: efficient implementation
	state[0] += wrd;
	if ( state[0] < wrd )
		state[0] += 1;
	if ( state[0] == 0xFFFF )
		state[0] = 0;
	state[1] += state[0];
	if ( state[1] < state[0] )
		state[1] += 1;
	if ( state[1] == 0xFFFF )
		state[1] = 0;
}

void init_checksum( uint8_t* checksum )
{
	ZEPTO_MEMSET( checksum, 0, EEPROM_CHECKSUM_SIZE );
}

bool is_same_checksum( uint8_t* checksum1, uint8_t* checksum2 )
{
	return ZEPTO_MEMCMP( checksum1, checksum2, EEPROM_CHECKSUM_SIZE ) == 0;
}

void calculate_checksum( const uint8_t* buff, uint16_t sz, uint8_t* checksum )
{
	uint16_t i;
	init_checksum( checksum );
#ifdef EEPROM_CHECKSUM_SIZE
#if (EEPROM_CHECKSUM_SIZE == 2)
	for ( i=0; i<sz; i++ )
		update_fletcher_checksum16( buff[i], checksum );
#elif (EEPROM_CHECKSUM_SIZE == 4)
	for ( i=0; i<(sz>>1); i++ )
		update_fletcher_checksum_32( ((uint16_t*)buff)[i], (uint16_t*)checksum );
	if ( sz & 1 )
	{
		uint16_t wrd = buff[sz-1];
		update_fletcher_checksum_32( wrd, (uint16_t*)checksum );
	}
#else
#error unexpected value of EEPROM_CHECKSUM_SIZE
#endif
#else
#error EEPROM_CHECKSUM_SIZE is undefined
#endif
}


void eeprom_write( uint16_t device_id, uint8_t item, uint16_t sz, uint8_t* data )
{
	PER_DEVICE_STORAGE_DATA* dev = perm_storage_get_device_data_by_device_id( device_id );
	ZEPTO_DEBUG_ASSERT( dev );
	// quick and dirty implementation TODO: more sofisticated solution
	ZEPTO_DEBUG_ASSERT( DATA_SASP_NONCE_LW_SIZE == DATA_SASP_NONCE_LS_SIZE );
	ZEPTO_DEBUG_ASSERT( ( item == EEPROM_SLOT_DATA_SASP_NONCE_LW_ID || item == EEPROM_SLOT_DATA_SASP_NONCE_LS_ID ) && sz == DATA_SASP_NONCE_LW_SIZE );

	uint8_t checksum[ EEPROM_CHECKSUM_SIZE ]; init_checksum( checksum );
	ZEPTO_DEBUG_ASSERT( sizeof( checksum) == EEPROM_CHECKSUM_SIZE );
	calculate_checksum( data, sz, checksum );

	uint8_t* buff = dev->dev_data + item * ( DATA_SASP_NONCE_LS_SIZE + EEPROM_CHECKSUM_SIZE );
	ZEPTO_MEMCPY( buff, data, DATA_SASP_NONCE_LS_SIZE );
	ZEPTO_MEMCPY( buff + DATA_SASP_NONCE_LS_SIZE, checksum, EEPROM_CHECKSUM_SIZE );

	MEMORY_HANDLE mem_h = acquire_memory_handle();
	ZEPTO_DEBUG_ASSERT( mem_h != MEMORY_HANDLE_INVALID );

	zepto_write_block( mem_h, buff, dev->dev_data_sz );
	zepto_response_to_request( mem_h );

	send_sync_request_to_central_unit_to_save_data( mem_h, device_id );

	release_memory_handle( mem_h );
}

void eeprom_read( uint16_t device_id, uint8_t item, uint16_t sz, uint8_t* data )
{
	PER_DEVICE_STORAGE_DATA* dev = perm_storage_get_device_data_by_device_id( device_id );
	ZEPTO_DEBUG_ASSERT( dev );

	// quick and dirty implementation TODO: more sofisticated solution
	ZEPTO_DEBUG_ASSERT( DATA_SASP_NONCE_LW_SIZE == DATA_SASP_NONCE_LS_SIZE );
	ZEPTO_DEBUG_ASSERT( ( item == EEPROM_SLOT_DATA_SASP_NONCE_LW_ID || item == EEPROM_SLOT_DATA_SASP_NONCE_LS_ID ) && sz == DATA_SASP_NONCE_LW_SIZE );

	MEMORY_HANDLE mem_h = acquire_memory_handle();
	ZEPTO_DEBUG_ASSERT( mem_h != MEMORY_HANDLE_INVALID );
	send_sync_request_to_central_unit_to_get_data( mem_h, device_id );

	parser_obj po;
	zepto_parser_init( &po, mem_h );
	uint16_t full_sz = zepto_parsing_remaining_bytes( &po );
	ZEPTO_DEBUG_ASSERT( full_sz == DATA_SASP_NONCE_LW_SIZE + DATA_SASP_NONCE_LS_SIZE + 2 * EEPROM_CHECKSUM_SIZE );
	ZEPTO_DEBUG_ASSERT( dev->dev_data_sz == DATA_SASP_NONCE_LW_SIZE + DATA_SASP_NONCE_LS_SIZE + 2 * EEPROM_CHECKSUM_SIZE );
	zepto_parse_read_block( &po, data, DATA_SASP_NONCE_LW_SIZE + DATA_SASP_NONCE_LS_SIZE + 2 * EEPROM_CHECKSUM_SIZE );

	uint8_t* buff = dev->dev_data + item * ( DATA_SASP_NONCE_LS_SIZE + EEPROM_CHECKSUM_SIZE );
	uint8_t checksum_calculated[ EEPROM_CHECKSUM_SIZE ]; init_checksum( checksum_calculated );
	uint8_t* checksum_read = buff + DATA_SASP_NONCE_LS_SIZE;
	ZEPTO_MEMCPY( data, buff, DATA_SASP_NONCE_LS_SIZE );
	calculate_checksum( data, DATA_SASP_NONCE_LW_SIZE, checksum_calculated );
	bool same = is_same_checksum( checksum_read, checksum_calculated );
	ZEPTO_DEBUG_ASSERT( same ); // TODO: what should we do in production mode in case of failure?

	release_memory_handle( mem_h );
}
