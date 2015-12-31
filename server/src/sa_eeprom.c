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

#include <simpleiot/siot_common.h>
#include <simpleiot_hal/siot_mem_mngmt.h>
#include "commstack_commlayer.h"

#define EEPROM_CHECKSUM_SIZE 4


void update_fletcher_checksum_16( uint8_t bt, uint16_t* state )
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
		update_fletcher_checksum_16( buff[i], checksum );
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
	MEMORY_HANDLE mem_h = acquire_memory_handle();
	ZEPTO_DEBUG_ASSERT( mem_h != MEMORY_HANDLE_INVALID );

	uint8_t checksum[ EEPROM_CHECKSUM_SIZE ]; init_checksum( checksum );
//	ZEPTO_DEBUG_ASSERT( id < EEPROM_SLOT_MAX );
	ZEPTO_DEBUG_ASSERT( sizeof( checksum) == EEPROM_CHECKSUM_SIZE );
	calculate_checksum( data, sz, checksum );
	zepto_write_block( mem_h, data, sz );
	zepto_write_block( mem_h, (uint8_t*)checksum, EEPROM_CHECKSUM_SIZE );
	zepto_response_to_request( mem_h );

	send_sync_request_to_central_unit_to_save_data( mem_h, device_id, item );

	release_memory_handle( mem_h );
}

void eeprom_read( uint16_t device_id, uint8_t item, uint16_t sz, uint8_t* data )
{
	MEMORY_HANDLE mem_h = acquire_memory_handle();
	ZEPTO_DEBUG_ASSERT( mem_h != MEMORY_HANDLE_INVALID );
	send_sync_request_to_central_unit_to_get_data( mem_h, device_id, item );

	uint8_t checksum_calculated[ EEPROM_CHECKSUM_SIZE ];
	uint8_t checksum_read[ EEPROM_CHECKSUM_SIZE ];
	parser_obj po;
	zepto_parser_init( &po, mem_h );
	uint16_t full_sz = zepto_parsing_remaining_bytes( &po );
	ZEPTO_DEBUG_ASSERT( full_sz >= EEPROM_CHECKSUM_SIZE );
	zepto_parse_read_block( &po, data, full_sz - EEPROM_CHECKSUM_SIZE );
	zepto_parse_read_block( &po, checksum_read, EEPROM_CHECKSUM_SIZE );

	init_checksum( checksum_calculated );
	calculate_checksum( data, full_sz - EEPROM_CHECKSUM_SIZE, checksum_calculated );
	bool same = is_same_checksum( checksum_read, checksum_calculated );
	ZEPTO_DEBUG_ASSERT( same ); // TODO: what should we do in production mode in case of failure?
	ZEPTO_DEBUG_ASSERT( sz == full_sz - EEPROM_CHECKSUM_SIZE ); // TODO: what should we do in production mode in case of failure?
}
