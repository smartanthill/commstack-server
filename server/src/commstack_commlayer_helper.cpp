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

#include "commstack_commlayer_helper.h"
#include <stdio.h>

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

typedef struct _INCOMING_PACKET
{
	uint8_t src;
	uint16_t sz;
	uint16_t addr;
	uint8_t* data;
} INCOMING_PACKET;

typedef list<INCOMING_PACKET> PACKETS;
PACKETS packets;

uint8_t cscl_is_queued_packet()
{
	return packets.size() ? COMMLAYER_SYNC_STATUS_OK : COMMLAYER_SYNC_STATUS_NO_MORE_PACKETS;
}

void cscl_fill_packet( INCOMING_PACKET* packet, uint8_t src, uint16_t sz, uint16_t addr, uint8_t* data )
{
	packet->src = src;
	packet->sz = sz;
	packet->addr = addr;
	if ( sz )
	{
		packet->data = new uint8_t [sz];
		ZEPTO_DEBUG_ASSERT( packet->data != NULL );
		ZEPTO_MEMCPY( packet->data, data, sz );
	}
	else
		packet->data = NULL;
}

void cscl_clean_packet( INCOMING_PACKET* packet )
{
	if ( packet->data ) delete [] (packet->data);
}

void cscl_add_new_packet_to_queue( uint8_t src, uint16_t sz, uint16_t addr, uint8_t* data )
{
	INCOMING_PACKET packet;
	cscl_fill_packet( &packet, src, sz, addr, data );
	packets.push_back( packet );
}

uint8_t cscl_get_oldest_packet_size( uint16_t* sz )
{
	if ( packets.size() == 0 )
	{
		*sz = 0;
		return COMMLAYER_SYNC_STATUS_NO_MORE_PACKETS;
	}
	*sz = packets.begin()->sz;
	return COMMLAYER_SYNC_STATUS_OK;
}

uint8_t cscl_get_oldest_packet_and_remove_from_queue( uint8_t* src, uint16_t* sz, uint16_t* addr, uint8_t* data, uint16_t max_sz )
{
	if ( packets.size() == 0 )
	{
		*sz = 0;
		return COMMLAYER_SYNC_STATUS_NO_MORE_PACKETS;
	}
	*src = packets.begin()->src;
	*sz = packets.begin()->sz;
	*addr = packets.begin()->addr;
	*sz = packets.begin()->sz;
	ZEPTO_DEBUG_ASSERT( *sz <= max_sz );
	if ( *sz )
	{
		ZEPTO_DEBUG_ASSERT( packets.begin()->data != NULL );
		ZEPTO_MEMCPY( data, packets.begin()->data, *sz );
	}
	cscl_clean_packet( &(*(packets.begin())) );
	packets.erase( packets.begin() );
	return COMMLAYER_SYNC_STATUS_OK;
}