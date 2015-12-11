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
//#include "commstack_siot_mesh_support.h"
#include <simpleiot/siot_m_protocol.h>
#include <simpleiot_hal/siot_mem_mngmt.h>

#define NEW_UPDATE_PROCESSING

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
#include <simpleiot/siot_m_protocol.h>
#include <vector>
#include <list>
using namespace std;

typedef vector< SIOT_MESH_ROUTE > SIOT_M_ROUTE_TABLE_TYPE;
typedef vector< SIOT_MESH_LINK > SIOT_M_LINK_TABLE_TYPE;
typedef vector< int > SIOT_M_BUS_TYPE_LIST_TYPE;

typedef struct _SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA // used to keep copies of respective tables at devices
{
	uint16_t device_id;
	bool is_retransmitter;
	uint16_t last_from_santa_request_id;
	SIOT_M_ROUTE_TABLE_TYPE siot_m_route_table_confirmed;
	SIOT_M_LINK_TABLE_TYPE siot_m_link_table_confirmed;
	SIOT_M_ROUTE_TABLE_TYPE siot_m_route_table_planned;
	SIOT_M_LINK_TABLE_TYPE siot_m_link_table_planned;
	SIOT_M_BUS_TYPE_LIST_TYPE bus_type_list;
} SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA;

typedef vector< SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA > SIOT_MESH_ROUTING_DATA;
typedef vector< SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA >::iterator SIOT_MESH_ROUTING_DATA_ITERATOR;

SIOT_MESH_ROUTING_DATA mesh_routing_data; // confirmed copies of respective tables at devices
SIOT_MESH_ROUTING_DATA mesh_routing_data_being_constructed; // planned copies of respective tables at devices

uint16_t siot_mesh_calculate_route_table_checksum( SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA* data )
{
	return 0;
}

///////////////////   Common staff: checksums   //////////////////////

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

void update_checksum_with_route_entry( const SIOT_MESH_ROUTE* rt, uint16_t* state )
{
	update_fletcher_checksum_16( (uint8_t)(rt->TARGET_ID), state );
	update_fletcher_checksum_16( (uint8_t)(rt->TARGET_ID >> 8), state );
	update_fletcher_checksum_16( (uint8_t)(rt->LINK_ID), state );
	update_fletcher_checksum_16( (uint8_t)(rt->LINK_ID >> 8), state );
}

void update_checksum_with_link_entry( const SIOT_MESH_LINK* lt, uint16_t* state )
{
	update_fletcher_checksum_16( (uint8_t)(lt->LINK_ID), state );
	update_fletcher_checksum_16( (uint8_t)(lt->LINK_ID >> 8), state );
	update_fletcher_checksum_16( (uint8_t)(lt->BUS_ID), state );
	update_fletcher_checksum_16( (uint8_t)(lt->BUS_ID >> 8), state );
	update_fletcher_checksum_16( (uint8_t)(lt->NEXT_HOP), state );
	update_fletcher_checksum_16( (uint8_t)(lt->NEXT_HOP >> 8), state );

	// TODO: add other members
	/*update_fletcher_checksum_16( (uint8_t)(lt->INTRA_BUS_ID), state );
	update_fletcher_checksum_16( (uint8_t)(lt->INTRA_BUS_ID >> 8), state );
	update_fletcher_checksum_16( (uint8_t)(lt->INTRA_BUS_ID >> 16), state );
	update_fletcher_checksum_16( (uint8_t)(lt->INTRA_BUS_ID >> 24), state );*/
}

uint16_t calculate_table_checksum( const SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA* dev_data )
{
	uint16_t ret = 0;
	uint16_t i, sz;

	sz = dev_data->siot_m_route_table_confirmed.size();
	update_fletcher_checksum_16( (uint8_t)sz, &ret );
	update_fletcher_checksum_16( (uint8_t)(sz >> 8), &ret );
	for ( i=0; i<sz; i++ )
		update_checksum_with_route_entry( &(dev_data->siot_m_route_table_confirmed[i]), &ret );

	sz = dev_data->siot_m_link_table_confirmed.size();
	update_fletcher_checksum_16( (uint8_t)sz, &ret );
	update_fletcher_checksum_16( (uint8_t)(sz >> 8), &ret );
	for ( i=0; i<sz; i++ )
		update_checksum_with_link_entry( &(dev_data->siot_m_link_table_confirmed[i]), &ret );
	return ret;
}

///////////////////   DEBUG and VERIFICATIONS   //////////////////////

void dbg_siot_mesh_at_root_validate_device_tables( const SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA* data )
{
#ifdef SA_DEBUG
	uint16_t i, j;

	// I. check confirmed version

	// target IDs in the route table are in increasing order
	for ( i=1; i<data->siot_m_route_table_confirmed.size(); i++ )
	{
		bool ordered = data->siot_m_route_table_confirmed[ i - 1 ].TARGET_ID < data->siot_m_route_table_confirmed[ i ].TARGET_ID;
if ( !ordered )
{
	ordered = ordered;
}
		ZEPTO_DEBUG_ASSERT( ordered );
	}

	// link IDs in the link table are in increasing order
	for ( i=1; i<data->siot_m_link_table_confirmed.size(); i++ )
	{
		bool ordered = data->siot_m_link_table_confirmed[ i - 1 ].LINK_ID < data->siot_m_link_table_confirmed[ i ].LINK_ID;
if ( !ordered )
{
	ordered = ordered;
}
		ZEPTO_DEBUG_ASSERT( ordered );
	}

	// link ID in the route table correspond to an entry in the link table
	for ( i=0; i<data->siot_m_route_table_confirmed.size(); i++ )
	{
		bool found = false;
		for ( j=0; j<data->siot_m_link_table_confirmed.size(); j++ )
			if ( data->siot_m_route_table_confirmed[i].LINK_ID == data->siot_m_link_table_confirmed[j].LINK_ID )
			{
				found = true;
				break;
			}
		if ( ! found )
		{
			ZEPTO_DEBUG_ASSERT( 0 == "LINK_ID in the route table corresponds to nothing in the link table" );
		}
	}

	// all links in the link table are in use
	for ( i=0; i<data->siot_m_link_table_confirmed.size(); i++ )
	{
		bool found = false;
		for ( j=0; j<data->siot_m_route_table_confirmed.size(); j++ )
			if ( data->siot_m_route_table_confirmed[j].LINK_ID == data->siot_m_link_table_confirmed[i].LINK_ID )
			{
				found = true;
				break;
			}
		if ( ! found )
		{
			ZEPTO_DEBUG_ASSERT( 0 == "LINK_ID in the link table corresponds to nothing in the route table" );
		}
	}

	// II. check planned version

	// target IDs in the route table are in increasing order
	for ( i=1; i<data->siot_m_route_table_planned.size(); i++ )
	{
		bool ordered = data->siot_m_route_table_planned[ i - 1 ].TARGET_ID < data->siot_m_route_table_planned[ i ].TARGET_ID;
if ( !ordered )
{
	ordered = ordered;
}
		ZEPTO_DEBUG_ASSERT( ordered );
	}

	// link IDs in the link table are in increasing order
	for ( i=1; i<data->siot_m_link_table_planned.size(); i++ )
	{
		bool ordered = data->siot_m_link_table_planned[ i - 1 ].LINK_ID < data->siot_m_link_table_planned[ i ].LINK_ID;
if ( !ordered )
{
	ordered = ordered;
}
		ZEPTO_DEBUG_ASSERT( ordered );
	}

	// link ID in the route table correspond to an entry in the link table
	for ( i=0; i<data->siot_m_route_table_planned.size(); i++ )
	{
		bool found = false;
		for ( j=0; j<data->siot_m_link_table_planned.size(); j++ )
			if ( data->siot_m_route_table_planned[i].LINK_ID == data->siot_m_link_table_planned[j].LINK_ID )
			{
				found = true;
				break;
			}
		if ( ! found )
		{
			ZEPTO_DEBUG_ASSERT( 0 == "LINK_ID in the route table corresponds to nothing in the link table" );
		}
	}

	// all links in the link table are in use
	for ( i=0; i<data->siot_m_link_table_planned.size(); i++ )
	{
		bool found = false;
		for ( j=0; j<data->siot_m_route_table_planned.size(); j++ )
			if ( data->siot_m_route_table_planned[j].LINK_ID == data->siot_m_link_table_planned[i].LINK_ID )
			{
				found = true;
				break;
			}
		if ( ! found )
		{
			ZEPTO_DEBUG_ASSERT( 0 == "LINK_ID in the link table corresponds to nothing in the route table" );
		}
	}
#endif
}

void dbg_siot_mesh_at_root_validate_all_device_tables()
{
#ifdef SA_DEBUG
	SIOT_MESH_ROUTING_DATA_ITERATOR it;
	for ( it = mesh_routing_data.begin(); it != mesh_routing_data.end(); ++it )
		dbg_siot_mesh_at_root_validate_device_tables( &*it );
	for ( it = mesh_routing_data_being_constructed.begin(); it != mesh_routing_data_being_constructed.end(); ++it )
		dbg_siot_mesh_at_root_validate_device_tables( &*it );
#endif
}

///////////////////   Basic calls: initializing  //////////////////////

void siot_mesh_init_tables()  // TODO: this call reflects current development stage and may or may not survive in the future
{
	// manual device adding
	// as soon as pairing is implemented below code will be removed or heavily revised

	SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA data;

	// 0. Root device
	data.device_id = 0;
	data.is_retransmitter = false;
	data.last_from_santa_request_id = 0;
	data.bus_type_list.push_back( 1 );
	mesh_routing_data.push_back( data );
	mesh_routing_data_being_constructed.push_back( data );

	// 1. retransmitter
	data.device_id = 1;
	data.is_retransmitter = true;
	data.last_from_santa_request_id = 0;
	data.bus_type_list.clear();
	data.bus_type_list.push_back( 0 );
	data.bus_type_list.push_back( 1 );
	mesh_routing_data.push_back( data );
	mesh_routing_data_being_constructed.push_back( data );

	// 2-5. terminating
	data.is_retransmitter = false;
	data.last_from_santa_request_id = 0;
	data.device_id = 2;
	data.bus_type_list.clear();
	data.bus_type_list.push_back( 0 );
	mesh_routing_data.push_back( data );
	mesh_routing_data_being_constructed.push_back( data );
	data.device_id = 3;
	mesh_routing_data.push_back( data );
	mesh_routing_data_being_constructed.push_back( data );
	data.device_id = 4;
	mesh_routing_data.push_back( data );
	mesh_routing_data_being_constructed.push_back( data );
	data.device_id = 5;
	mesh_routing_data.push_back( data );
	mesh_routing_data_being_constructed.push_back( data );
}

///////////////////   Basic calls: device list  //////////////////////
/*
uint8_t siot_mesh_at_root_add_device( uint16_t device_id )
{
	uint16_t i;
	for ( i=0; i<mesh_routing_data.size(); i++)
		if ( mesh_routing_data[i].device_id == device_id )
			return SIOT_MESH_AT_ROOT_RET_ALREADY_EXISTS;
	SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA dev_data;
	dev_data.device_id = device_id;
	mesh_routing_data.push_back( dev_data );
	mesh_routing_data_being_constructed.push_back( data );
	return SIOT_MESH_AT_ROOT_RET_OK;
}

uint8_t siot_mesh_at_root_remove_device( uint16_t device_id )
{
	uint16_t i;
	for ( i=0; i<mesh_routing_data.size(); i++)
		if ( mesh_routing_data[i].device_id == device_id )
		{
			mesh_routing_data.erase( mesh_routing_data.begin() + i );
			return SIOT_MESH_AT_ROOT_RET_OK;
		}
	for ( i=0; i<mesh_routing_data_being_constructed.size(); i++)
		if ( mesh_routing_data_being_constructed[i].device_id == device_id )
		{
			mesh_routing_data_being_constructed.erase( mesh_routing_data_being_constructed.begin() + i );
			return SIOT_MESH_AT_ROOT_RET_OK;
		}
	return SIOT_MESH_AT_ROOT_RET_NOT_FOUND;
}
*/
uint8_t siot_mesh_at_root_get_device_data( uint16_t device_id, SIOT_MESH_ROUTING_DATA_ITERATOR& it )
{
	for ( it = mesh_routing_data.begin(); it != mesh_routing_data.end(); ++it )
		if ( it->device_id == device_id )
			return SIOT_MESH_AT_ROOT_RET_OK;

	return SIOT_MESH_AT_ROOT_RET_NOT_FOUND;
}

uint8_t siot_mesh_at_root_get_device_data_being_constructed( uint16_t device_id, SIOT_MESH_ROUTING_DATA_ITERATOR& it )
{
	for ( it = mesh_routing_data_being_constructed.begin(); it != mesh_routing_data_being_constructed.end(); ++it )
		if ( it->device_id == device_id )
			return SIOT_MESH_AT_ROOT_RET_OK;

	return SIOT_MESH_AT_ROOT_RET_NOT_FOUND;
}

uint16_t write_retransmitter_list_for_from_santa_packet( MEMORY_HANDLE mem_h )
{
	SIOT_MESH_ROUTING_DATA_ITERATOR it;
	uint16_t count = 0;
	for ( it = mesh_routing_data.begin(); it != mesh_routing_data.end(); ++it )
		if ( it->is_retransmitter )
		{
			uint8_t more_data_in_record = 0;
			uint16_t header = more_data_in_record | ( ( it->device_id + 1 ) << 1 );
			zepto_parser_encode_and_append_uint16( mem_h, header );
			count++;
		}
	zepto_parser_encode_and_append_uint16( mem_h, 0 ); // terminator of the list, "EXTRA_DATA_FOLLOWS=0 and NODE-ID=0"
	return count;
}


///////////////////   Basic calls: requests to route table of the ROOT   //////////////////////

uint8_t siot_mesh_get_link( uint16_t link_id, SIOT_MESH_LINK* link )
{
	uint16_t i, j;
	for ( i=0; i<mesh_routing_data.size(); i++)
		if ( mesh_routing_data[i].device_id == 0 )
			for ( j=0; j<mesh_routing_data[i].siot_m_link_table_confirmed.size(); j++ )
				if ( mesh_routing_data[i].siot_m_link_table_confirmed[j].LINK_ID == link_id )
				{
					ZEPTO_MEMCPY( link, &(mesh_routing_data[i].siot_m_link_table_confirmed[j]), sizeof(SIOT_MESH_LINK) );
					return SIOT_MESH_RET_OK;
				}
	return SIOT_MESH_RET_ERROR_NOT_FOUND;
}

void siot_mesh_at_root_add_or_update_route( SIOT_M_ROUTE_TABLE_TYPE* route_table, const SIOT_MESH_ROUTE* route )
{
	unsigned int i;
	for ( i=0; i<route_table->size(); i++ )
	{
		if ( route->TARGET_ID < (*route_table)[i].TARGET_ID )
		{
			route_table->insert( route_table->begin() + i, *route );
			return;
		}
		else if ( route->TARGET_ID == (*route_table)[i].TARGET_ID )
		{
			(*route_table)[i].LINK_ID = route->LINK_ID;
			return;
		}
	}

	route_table->push_back( *route );
	return;
}

void siot_mesh_at_root_remove_route( SIOT_M_ROUTE_TABLE_TYPE* route_table, uint16_t target_id )
{
	unsigned int i;
	for ( i=0; i<route_table->size(); i++ )
	{
		if ( (*route_table)[i].TARGET_ID == target_id )
		{
			route_table->erase( route_table->begin() + i );
			return;
		}
	}
}

void siot_mesh_at_root_add_or_update_link( SIOT_M_LINK_TABLE_TYPE* link_table, const SIOT_MESH_LINK* link )
{
	unsigned int i;
	for ( i=0; i<link_table->size(); i++ )
	{
		if ( link->LINK_ID < (*link_table)[i].LINK_ID )
		{
			link_table->insert( link_table->begin() + i, *link );
			return;
		}
		else if ( link->LINK_ID == (*link_table)[i].LINK_ID )
		{
			ZEPTO_MEMCPY( &((*link_table)[i]), link, sizeof (SIOT_MESH_LINK) );
			return;
		}
	}

	link_table->push_back( *link );
	return;
}

void siot_mesh_at_root_insert_link( SIOT_M_LINK_TABLE_TYPE* link_table, SIOT_MESH_LINK* link )
{
	unsigned int i;
	for ( i=0; i<link_table->size(); i++ )
	{
		if ( i < (*link_table)[i].LINK_ID )
		{
			link->LINK_ID = i;
			link_table->insert( link_table->begin() + i, *link );
			return;
		}
	}

	link->LINK_ID = link_table->size() == 0 ? 0 : ( link_table->end() - 1 )->LINK_ID + 1;
	link_table->push_back( *link );
	return;
}

void siot_mesh_at_root_remove_link( SIOT_M_LINK_TABLE_TYPE* link_table, uint16_t link_id )
{
	unsigned int i;
	for ( i=0; i<link_table->size(); i++ )
	{
		if ( (*link_table)[i].LINK_ID == link_id )
		{
			link_table->erase( link_table->begin() + i );
			return;
		}
	}
}

///////////////////   Basic calls: route table  //////////////////////


typedef struct _SIOT_MESH_LAST_HOP_DATA
{
	uint16_t last_hop_id;
	uint16_t last_hop_bus_id;
	uint8_t conn_quality;
} SIOT_MESH_LAST_HOP_DATA;

typedef vector< SIOT_MESH_LAST_HOP_DATA > LAST_HOPS;

typedef struct _DEVICE_LAST_HOPS
{
	uint16_t device_id;
	uint16_t request_id;
	sa_time_val end_of_receiving;
	LAST_HOPS last_hops_in;
	LAST_HOPS last_hops_out;
} DEVICE_LAST_HOPS;

typedef vector< DEVICE_LAST_HOPS > LAST_HOPS_OF_ALL_DEVICES;

LAST_HOPS_OF_ALL_DEVICES last_hops_of_all_devices;
static uint16_t from_santa_request_id = 0;

uint16_t get_next_from_santa_request_id() {return ++from_santa_request_id; }
bool is_from_santa_request_id_less_eq( uint16_t prev_id, uint16_t new_id ) {return ((uint16_t)( new_id - prev_id  )) < (uint16_t)0x8000; }

void siot_mesh_at_root_add_last_hop_in_data( const sa_time_val* currt, uint16_t request_id, uint16_t src_id, uint16_t last_hop_id, uint16_t last_hop_bus_id, uint8_t conn_q )
{
	ZEPTO_DEBUG_PRINTF_5( "siot_mesh_at_root_add_last_hop_in_data( src_id = %d, last_hop_id = %d, last_hop_bus_id = %d, conn_q = %d )\n",  src_id, last_hop_id, last_hop_bus_id, conn_q );
	SIOT_MESH_LAST_HOP_DATA hop;
	SIOT_MESH_ROUTING_DATA_ITERATOR it;

	hop.last_hop_id = last_hop_id;
	hop.last_hop_bus_id = last_hop_bus_id;
	hop.conn_quality = conn_q;

	uint16_t i, j;

	bool found = false;
	int ini_sz = last_hops_of_all_devices.size();
	for ( i=0; i<ini_sz; i++ )
	{
		uint8_t ret_code = siot_mesh_at_root_get_device_data( src_id, it );
		ZEPTO_DEBUG_ASSERT( ret_code == SIOT_MESH_AT_ROOT_RET_OK ); // TODO: consider a case when src_id points to nothing (note that it come over a network!)
		if ( last_hops_of_all_devices[i].device_id == src_id/* && request_id == it->last_from_santa_request_id*/ )
		{
			// we assume that device returns the same info no matter which way it arrives
			bool found2 = false;
			for ( j=0; j<last_hops_of_all_devices[i].last_hops_in.size(); j++ )
				if ( last_hops_of_all_devices[i].last_hops_in[j].last_hop_id == hop.last_hop_id && last_hops_of_all_devices[i].last_hops_in[j].last_hop_bus_id == hop.last_hop_bus_id )
				{
					ZEPTO_DEBUG_ASSERT( last_hops_of_all_devices[i].last_hops_in[j].conn_quality == hop.conn_quality );
					found2 = true;
					break;
				}
			if ( !found2 )
				last_hops_of_all_devices[i].last_hops_in.push_back( hop );
			found = true;
			break;
		}
	}

	if ( !found )
	{
		uint8_t ret_code = siot_mesh_at_root_get_device_data( src_id, it );
		ZEPTO_DEBUG_ASSERT( ret_code == SIOT_MESH_AT_ROOT_RET_OK ); // TODO: consider a case when src_id points to nothing (note that it come over a network!)
		if ( is_from_santa_request_id_less_eq( request_id, it->last_from_santa_request_id ) )
			return; // unlucky guy... nj late to come...
		it->last_from_santa_request_id = request_id;
		DEVICE_LAST_HOPS dev_hops;
		dev_hops.device_id = src_id;
		dev_hops.request_id = request_id;
		sa_time_val diff_tval;
		TIME_MILLISECONDS16_TO_TIMEVAL( MESH_RECEIVING_HOPS_PERIOD_MS, diff_tval );
		sa_hal_time_val_copy_from( &(dev_hops.end_of_receiving), currt );
		SA_TIME_INCREMENT_BY_TICKS( dev_hops.end_of_receiving, diff_tval );
		dev_hops.last_hops_in.push_back( hop );
		last_hops_of_all_devices.push_back( dev_hops );
	}
}

void siot_mesh_at_root_add_last_hop_out_data( const sa_time_val* currt, uint16_t request_id, uint16_t src_id, uint16_t bus_id_at_src, uint16_t first_receiver_id, uint8_t conn_q )
{
	ZEPTO_DEBUG_PRINTF_5( "siot_mesh_at_root_add_last_hop_out_data( src_id = %d, bus_id_at_src = %d, first_receiver_id = %d, conn_q = %d )\n",  src_id, bus_id_at_src, first_receiver_id, conn_q );
	SIOT_MESH_LAST_HOP_DATA hop;
	SIOT_MESH_ROUTING_DATA_ITERATOR it;

	hop.last_hop_id = first_receiver_id;
	hop.last_hop_bus_id = bus_id_at_src;
	hop.conn_quality = conn_q;

	uint16_t i, j;

	bool found = false;
	int ini_sz = last_hops_of_all_devices.size();
	for ( i=0; i<ini_sz; i++ )
	{
		uint8_t ret_code = siot_mesh_at_root_get_device_data( src_id, it );
		ZEPTO_DEBUG_ASSERT( ret_code == SIOT_MESH_AT_ROOT_RET_OK ); // TODO: consider a case when src_id points to nothing (note that it come over a network!)
		if ( last_hops_of_all_devices[i].device_id == src_id/* && request_id == it->last_from_santa_request_id*/ )
		{
			// we assume that device returns the same info no matter which way it arrives
			bool found2 = false;
			for ( j=0; j<last_hops_of_all_devices[i].last_hops_out.size(); j++ )
				if ( last_hops_of_all_devices[i].last_hops_out[j].last_hop_id == hop.last_hop_id && last_hops_of_all_devices[i].last_hops_out[j].last_hop_bus_id == hop.last_hop_bus_id )
				{
					ZEPTO_DEBUG_ASSERT( last_hops_of_all_devices[i].last_hops_out[j].conn_quality == hop.conn_quality );
					found2 = true;
					break;
				}
			if ( !found2 )
				last_hops_of_all_devices[i].last_hops_out.push_back( hop );
			found = true;
			break;
		}
	}
	if ( !found )
	{
		uint8_t ret_code = siot_mesh_at_root_get_device_data( src_id, it );
		ZEPTO_DEBUG_ASSERT( ret_code == SIOT_MESH_AT_ROOT_RET_OK ); // TODO: consider a case when src_id points to nothing (note that it come over a network!)
		if ( request_id <= it->last_from_santa_request_id )
			return; // unlucky guy... nj late to come...
		DEVICE_LAST_HOPS dev_hops;
		dev_hops.device_id = src_id;
		dev_hops.request_id = request_id;
		sa_time_val diff_tval;
		TIME_MILLISECONDS16_TO_TIMEVAL( MESH_RESEND_PERIOD_MS, diff_tval );
		sa_hal_time_val_copy_from( &(dev_hops.end_of_receiving), currt );
		SA_TIME_INCREMENT_BY_TICKS( dev_hops.end_of_receiving, diff_tval );
		dev_hops.last_hops_in.push_back( hop );
		last_hops_of_all_devices.push_back( dev_hops );
	}
}


#define SIOT_MESH_IS_QUALITY_OF_INCOMING_CONNECTION_ADMISSIBLE( x ) ( (x) < 0x7F )
#define SIOT_MESH_IS_QUALITY_OF_OUTGOING_CONNECTION_ADMISSIBLE( x ) ( (x) < 0x7F )
#define SIOT_MESH_IS_QUALITY_OF_FIRST_INOUT_CONNECTION_BETTER( in1, out1, in2, out2 ) ( (in1<in2)||((in1==in2)&&(out1<out2)) ) /*TODO: this is a quick solution; think about better ones*/

uint8_t siot_mesh_at_root_find_best_route_to_device( const sa_time_val* currt, sa_time_val* time_to_next_event, uint16_t* target_id, uint16_t* bus_id_at_target, uint16_t* id_prev, uint16_t* bus_id_at_prev, uint16_t* id_next )
{
dbg_siot_mesh_at_root_validate_all_device_tables();
static uint16_t ctr = 0;
ctr++;
ZEPTO_DEBUG_PRINTF_3( "*** ctr--siot_mesh_at_root_find_best_route_to_device = %d, last_hops_of_all_devices.size() = %d ***\n", ctr, last_hops_of_all_devices.size() );
/*if ( ctr == 2897 )
{
	ctr = ctr;
}*/
	if ( last_hops_of_all_devices.size() == 0 )
		return SIOT_MESH_AT_ROOT_RET_FAILED;

	uint16_t i, j, k;
	uint16_t last_in, last_out;
	uint16_t match_cnt = 0;
	uint8_t ret = SIOT_MESH_AT_ROOT_RET_RESEND_TASK_NOT_NOW;
	bool restart;
	do
	{
		restart = false; // will be true in case of erasing not because of success
		for ( i=0; i<last_hops_of_all_devices.size(); i++ )
		{
	//		if ( last_hops_of_all_devices[i].device_id == target_id )
			if ( sa_hal_time_val_is_less_eq( &(last_hops_of_all_devices[i].end_of_receiving), currt ) ) // receiving period is over
			{
				if ( last_hops_of_all_devices[i].last_hops_in.size() == 0 || last_hops_of_all_devices[i].last_hops_out.size() == 0 )
				{
					restart = true;
					last_hops_of_all_devices.erase( last_hops_of_all_devices.begin() + i );
					break;
				}

				*target_id = last_hops_of_all_devices[i].device_id;

				for ( j=0; j<last_hops_of_all_devices[i].last_hops_in.size(); j++ )
					if ( SIOT_MESH_IS_QUALITY_OF_INCOMING_CONNECTION_ADMISSIBLE( last_hops_of_all_devices[i].last_hops_in[j].conn_quality ) )
						for ( k=0; k<last_hops_of_all_devices[i].last_hops_out.size(); k++ )
							if ( ( last_hops_of_all_devices[i].last_hops_in[j].last_hop_id == last_hops_of_all_devices[i].last_hops_out[k].last_hop_id ) && SIOT_MESH_IS_QUALITY_OF_OUTGOING_CONNECTION_ADMISSIBLE( last_hops_of_all_devices[i].last_hops_out[k].conn_quality ) )
								if ( match_cnt == 0 || SIOT_MESH_IS_QUALITY_OF_FIRST_INOUT_CONNECTION_BETTER( last_hops_of_all_devices[i].last_hops_in[j].conn_quality, last_hops_of_all_devices[i].last_hops_out[k].conn_quality, last_hops_of_all_devices[i].last_hops_in[last_in].conn_quality, last_hops_of_all_devices[i].last_hops_out[last_out].conn_quality ) )
								{
									last_in = i;
									last_out = j;
									*id_prev = last_hops_of_all_devices[i].last_hops_in[j].last_hop_id;
									*id_next = last_hops_of_all_devices[i].last_hops_out[k].last_hop_id;
									*bus_id_at_target = last_hops_of_all_devices[i].last_hops_out[k].last_hop_bus_id;
									*bus_id_at_prev = last_hops_of_all_devices[i].last_hops_in[j].last_hop_bus_id;
									match_cnt++;
								}
				if ( match_cnt != 0 )
				{
					ret = SIOT_MESH_AT_ROOT_RET_OK;
					last_hops_of_all_devices.erase( last_hops_of_all_devices.begin() + i );
					restart = false;
					break;
				}
				else
				{
					last_hops_of_all_devices.erase( last_hops_of_all_devices.begin() + i );
					restart = true;
					break;
				}
			}
		}
	}
	while ( restart );

	// TODO: consider more relaxed matches...
	for ( i=0; i<last_hops_of_all_devices.size(); i++ )
		sa_hal_time_val_get_remaining_time( currt, &(last_hops_of_all_devices[i].end_of_receiving), time_to_next_event );

	return ret;
}

uint8_t siot_mesh_at_root_find_best_route( const sa_time_val* currt, sa_time_val* time_to_next_event, uint16_t* target_id, uint16_t* bus_id_at_target, uint16_t* id_prev, uint16_t* bus_id_at_prev, uint16_t* id_next )
{
//	*target_id = last_hops_of_all_devices[0].device_id; // TODO: we may want to try all possibilities, not just the first one
	return siot_mesh_at_root_find_best_route_to_device( currt, time_to_next_event, target_id, bus_id_at_target, id_prev, bus_id_at_prev, id_next );
}
/*
uint8_t siot_mesh_at_root_remove_last_hop_data( uint16_t target_id )
{
	uint16_t i;
	for ( i=0; i<last_hops_of_all_devices.size(); i++ )
		if ( last_hops_of_all_devices[i].device_id == target_id )
		{
			last_hops_of_all_devices.erase( last_hops_of_all_devices.begin() + i );
			return SIOT_MESH_AT_ROOT_RET_OK;
		}

	return SIOT_MESH_AT_ROOT_RET_FAILED;
}
*/
//////////////////////////////////////////////////////////////////////////////////////////////////
// pending mesh-level resends

// TODO (URGENT): we need here either dynamical memory handle acquiring mechanism, or alike

#define MESH_PENDING_RESEND_FROM_SANTA 0
#define MESH_PENDING_RESEND_TYPE_SELF_REPEATED 1

typedef struct _MESH_PENDING_RESENDS
{
	uint8_t type;
	uint8_t* packet_data;
	uint16_t packet_sz;
	uint8_t resend_cnt;
	sa_time_val next_resend_time;
	uint16_t checksum;
	uint16_t target_id;
	uint16_t bus_id;
	uint16_t next_hop_id;
} MESH_PENDING_RESENDS;

typedef list< MESH_PENDING_RESENDS > PENDING_RESENDS; // NOTE: switching to vector requires revision of code around calls to erase()
typedef list< MESH_PENDING_RESENDS >::iterator PENDING_RESENDS_ITERATOR;
PENDING_RESENDS pending_resends;

void siot_mesh_at_root_add_resend_task( MEMORY_HANDLE packet, const sa_time_val* currt, uint16_t checksum, uint16_t target_id, uint16_t bus_id, uint16_t next_hop_id, sa_time_val* time_to_next_event )
{
	// 1. add resend task
	MESH_PENDING_RESENDS resend;
	resend.type = MESH_PENDING_RESEND_TYPE_SELF_REPEATED;
	resend.target_id = target_id;
	resend.bus_id = bus_id;
	resend.next_hop_id = next_hop_id;
	resend.packet_sz = memory_object_get_response_size( packet );
	resend.packet_data = new uint8_t [resend.packet_sz];
	ZEPTO_DEBUG_ASSERT( resend.packet_sz != 0 );
	parser_obj po, po1;
	zepto_response_to_request( packet );
	zepto_parser_init( &po, packet );
	zepto_parse_read_block( &po, resend.packet_data, resend.packet_sz );
	zepto_parser_init( &po, packet );
	zepto_parser_init( &po1, packet );
	zepto_parse_skip_block( &po1, resend.packet_sz );
	zepto_convert_part_of_request_to_response( packet, &po, &po1 );
	resend.checksum = checksum;
	resend.resend_cnt = SIOT_MESH_SUBJECT_FOR_MESH_RESEND;
	sa_time_val diff_tval;
	TIME_MILLISECONDS16_TO_TIMEVAL( MESH_RESEND_PERIOD_MS, diff_tval );
	sa_hal_time_val_copy_from( &(resend.next_resend_time), currt );
	SA_TIME_INCREMENT_BY_TICKS( resend.next_resend_time, diff_tval );
	pending_resends.push_back( resend );

	// 2. calculate time to the nearest event
	PENDING_RESENDS_ITERATOR it;
//	SA_TIME_SET_INFINITE_TIME( *time_to_next_event );
	sa_time_val remaining;
	for ( it = pending_resends.begin(); it != pending_resends.end(); ++it )
	{
		uint8_t in_future = sa_hal_time_val_get_remaining_time( currt, &(it->next_resend_time), &remaining );
		sa_hal_time_val_copy_from_if_src_less( time_to_next_event, &remaining );
		if ( !in_future )
			break;
	}
}

void siot_mesh_at_root_add_send_from_santa_task( MEMORY_HANDLE packet, const sa_time_val* currt, uint16_t bus_id )
{
	// 1. add resend task
	MESH_PENDING_RESENDS resend;
	resend.type = MESH_PENDING_RESEND_FROM_SANTA;
	resend.bus_id = bus_id;
	resend.target_id = SIOT_MESH_TARGET_UNDEFINED;
	resend.packet_sz = memory_object_get_request_size( packet );
	resend.packet_data = new uint8_t [resend.packet_sz];
	ZEPTO_DEBUG_ASSERT( resend.packet_sz != 0 );
	parser_obj po;
	zepto_parser_init( &po, packet );
	zepto_parse_read_block( &po, resend.packet_data, resend.packet_sz );
	resend.checksum = 0;
	resend.resend_cnt = 1;
	sa_hal_time_val_copy_from( &(resend.next_resend_time), currt );
	pending_resends.push_back( resend );
}

uint8_t siot_mesh_at_root_get_resend_task( MEMORY_HANDLE packet, const sa_time_val* currt, uint16_t* target_id, uint16_t* bus_id, uint16_t* next_hop_id, sa_time_val* time_to_next_event )
{
	PENDING_RESENDS_ITERATOR it, it_oldest = pending_resends.end();
	sa_time_val oldest_time_point;
	sa_hal_time_val_copy_from( &oldest_time_point, currt );

	if ( pending_resends.size() == 0 )
		return SIOT_MESH_AT_ROOT_RET_RESEND_TASK_NONE_EXISTS;

//	bool one_found = false;

	for ( it = pending_resends.begin(); it != pending_resends.end(); ++it )
	{
		ZEPTO_DEBUG_ASSERT( it->resend_cnt > 0 );
		if ( it_oldest == pending_resends.end() )
		{
			if ( sa_hal_time_val_is_less_eq( &(it->next_resend_time), &oldest_time_point ) ) // used slot is yet older
			{
				sa_hal_time_val_copy_from( &oldest_time_point, &(it->next_resend_time) );
				it_oldest = it;
			}
		}
		else
		{
			if ( sa_hal_time_val_is_less( &(it->next_resend_time), &oldest_time_point ) ) // used slot is yet older
			{
				sa_hal_time_val_copy_from( &oldest_time_point, &(it->next_resend_time) );
				it_oldest = it;
			}
		}
	}

	if ( it_oldest == pending_resends.end() ) // none good for present time; just calculate time to wake up us next time
	{
		for ( it = pending_resends.begin(); it != pending_resends.end(); ++it )
			sa_hal_time_val_get_remaining_time( currt, &(it->next_resend_time), time_to_next_event );
		return SIOT_MESH_AT_ROOT_RET_RESEND_TASK_NOT_NOW;
	}

	// there is a packet to resend
	*target_id = it_oldest->target_id;
	*bus_id = it_oldest->bus_id;
	*next_hop_id = it_oldest->next_hop_id;
	switch ( it_oldest->type )
	{
		case MESH_PENDING_RESEND_TYPE_SELF_REPEATED:
		{
			ZEPTO_DEBUG_ASSERT( *target_id != SIOT_MESH_TARGET_UNDEFINED );
			ZEPTO_DEBUG_ASSERT( *bus_id != SIOT_MESH_BUS_UNDEFINED );
			bool final_in_seq = it_oldest->resend_cnt == 1;
			ZEPTO_DEBUG_ASSERT( it_oldest->resend_cnt > 0 );
			zepto_parser_free_memory( packet );
			zepto_write_block( packet, it_oldest->packet_data, it_oldest->packet_sz );
			if ( final_in_seq )
			{
				if ( it_oldest->packet_data != NULL )
					delete [] it_oldest->packet_data;
				pending_resends.erase( it_oldest );
			}
			else
			{
				(it_oldest->resend_cnt) --;
				sa_time_val diff_tval;
				TIME_MILLISECONDS16_TO_TIMEVAL( MESH_RESEND_PERIOD_MS, diff_tval );
				sa_hal_time_val_copy_from( &(it_oldest->next_resend_time), currt );
				SA_TIME_INCREMENT_BY_TICKS( it_oldest->next_resend_time, diff_tval );
			}

			// now we calculate remaining time for only actually remaining tasks
			for ( it = pending_resends.begin(); it != pending_resends.end(); ++it )
				sa_hal_time_val_get_remaining_time( currt, &(it->next_resend_time), time_to_next_event );

			return final_in_seq ? SIOT_MESH_AT_ROOT_RET_RESEND_TASK_FINAL : SIOT_MESH_AT_ROOT_RET_RESEND_TASK_INTERM;
			break;
		}
		case MESH_PENDING_RESEND_FROM_SANTA:
		{
			ZEPTO_DEBUG_ASSERT( *target_id == SIOT_MESH_TARGET_UNDEFINED );
			ZEPTO_DEBUG_ASSERT( *bus_id != SIOT_MESH_BUS_UNDEFINED );
			ZEPTO_DEBUG_ASSERT( it_oldest->resend_cnt == 1 );
			zepto_parser_free_memory( packet );
			zepto_write_block( packet, it_oldest->packet_data, it_oldest->packet_sz );
			if ( it_oldest->packet_data != NULL )
				delete [] it_oldest->packet_data;
			pending_resends.erase( it_oldest );

			// now we calculate remaining time for only actually remaining tasks
			for ( it = pending_resends.begin(); it != pending_resends.end(); ++it )
				sa_hal_time_val_get_remaining_time( currt, &(it->next_resend_time), time_to_next_event );

			return SIOT_MESH_AT_ROOT_RET_RESEND_TASK_FROM_SANTA;
			break;
		}
		default:
		{
			// we should not be here, anyway
			ZEPTO_DEBUG_ASSERT( 0 == "Unexpected resend task type" );
			return SIOT_MESH_AT_ROOT_RET_RESEND_TASK_NOT_NOW;
		}
	}
}

void siot_mesh_at_root_remove_resend_task_by_hash( uint16_t checksum, const sa_time_val* currt, sa_time_val* time_to_next_event )
{
	PENDING_RESENDS_ITERATOR it = pending_resends.begin(), it_erase;
	sa_time_val oldest_time_point;
	sa_hal_time_val_copy_from( &oldest_time_point, currt );

	if ( pending_resends.size() == 0 )
		return;

	while ( it != pending_resends.end() )
	{
		if ( it->type == MESH_PENDING_RESEND_TYPE_SELF_REPEATED && it->checksum == checksum )
		{
			it_erase = it;
			++it;
			if ( it_erase->packet_data )
				delete [] it_erase->packet_data;
			pending_resends.erase( it_erase);
		}
		else
			++it;
	}

	// now we calculate remaining time for only actually remaining tasks
	for ( it = pending_resends.begin(); it != pending_resends.end(); ++it )
		sa_hal_time_val_get_remaining_time( currt, &(it->next_resend_time), time_to_next_event );
}

void siot_mesh_at_root_remove_resend_task_by_device_id( uint16_t target_id, const sa_time_val* currt, sa_time_val* time_to_next_event )
{
	PENDING_RESENDS_ITERATOR it = pending_resends.begin(), it_erase;
	sa_time_val oldest_time_point;
	sa_hal_time_val_copy_from( &oldest_time_point, currt );

	if ( pending_resends.size() == 0 )
		return;

	while ( it != pending_resends.end() )
	{
		if ( it->type == MESH_PENDING_RESEND_TYPE_SELF_REPEATED && it->target_id == target_id )
		{
			it_erase = it;
			++it;
			if ( it_erase->packet_data )
				delete [] it_erase->packet_data;
			pending_resends.erase( it_erase);
		}
		else
			++it;
	}

	// now we calculate remaining time for only actually remaining tasks
	for ( it = pending_resends.begin(); it != pending_resends.end(); ++it )
		sa_hal_time_val_get_remaining_time( currt, &(it->next_resend_time), time_to_next_event );
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// complex packet generation

void siot_mesh_form_packets_from_santa_and_add_to_task_list( const sa_time_val* currt, waiting_for* wf, MEMORY_HANDLE mem_h, uint16_t target_id )
{
	// TODO++++: revision required
	// ASSUMPTIONS OF THE CURRENT IMPLEMENTATION
	// 1. As seen from this function parameters in the current implementation we assume only one device at a time to be found
	// 2. [CHECK] We also assume that the Root has a single bus

	// Santa Packet structure: | SAMP-FROM-SANTA-DATA-PACKET-AND-TTL | OPTIONAL-EXTRA-HEADERS | LAST-HOP | LAST-HOP-BUS-ID | REQUEST-ID | OPTIONAL-DELAY-UNIT | MULTIPLE-RETRANSMITTING-ADDRESSES | BROADCAST-BUS-TYPE-LIST | Target-Address | OPTIONAL-TARGET-REPLY-DELAY | OPTIONAL-PAYLOAD-SIZE | HEADER-CHECKSUM | PAYLOAD | FULL-CHECKSUM |
	// TODO: here and then use bit-field processing instead

	ZEPTO_DEBUG_ASSERT( target_id && target_id != SIOT_MESH_TARGET_UNDEFINED ); // root is never a target (sanity check)

	// 1. prepare common parts

	MEMORY_HANDLE prefix_h = acquire_memory_handle();
	ZEPTO_DEBUG_ASSERT( prefix_h != MEMORY_HANDLE_INVALID );
	MEMORY_HANDLE retransmitters_h = acquire_memory_handle();
	ZEPTO_DEBUG_ASSERT( retransmitters_h != MEMORY_HANDLE_INVALID );

	// SAMP-FROM-SANTA-DATA-PACKET-AND-TTL, OPTIONAL-EXTRA-HEADERS
	uint16_t header = 1 | ( SIOT_MESH_FROM_SANTA_DATA_PACKET << 1 ) | ( 4 << 5 ); // '1', packet type, 0 (no extra headers), TTL = 4
	zepto_parser_encode_and_append_uint16( prefix_h, header );

	// LAST-HOP
	zepto_parser_encode_and_append_uint16( prefix_h, 0 ); // ROOT

	// LAST-HOP-BUS-ID
	// (will be added later);

	// REQUEST-ID
	uint16_t rq_id = get_next_from_santa_request_id();
	// (will be added later);

	// OPTIONAL-DELAY-UNIT is present only if EXPLICIT-TIME-SCHEDULING flag is present; currently we did not added it

	MEMORY_HANDLE bus_type_h = acquire_memory_handle();
	ZEPTO_DEBUG_ASSERT( bus_type_h != MEMORY_HANDLE_INVALID );

	// BROADCAST-BUS-TYPE-LIST
	// TODO: if more than a single target device is to be found, revise lines below (do it with respect to all targets or what?)
	SIOT_MESH_ROUTING_DATA_ITERATOR it_target;
	uint8_t ret_code = siot_mesh_at_root_get_device_data( target_id, it_target );
if ( ret_code != SIOT_MESH_AT_ROOT_RET_OK )
{
	ret_code = ret_code;
}
	ZEPTO_DEBUG_ASSERT( ret_code == SIOT_MESH_AT_ROOT_RET_OK );
	unsigned int i;
	for ( i=0; i<it_target->bus_type_list.size(); i++ )
		zepto_write_uint8( bus_type_h, it_target->bus_type_list[i] + 1 );
	zepto_write_uint8( bus_type_h, 0 );

	// 2. get a list of retransmitters with links to
	uint16_t link_id;
//	SIOT_MESH_LINK link;
	SIOT_MESH_ROUTING_DATA_ITERATOR it;
//	int known_retr_cnt = 0;
	for ( it = mesh_routing_data.begin(); it != mesh_routing_data.end(); ++it )
		if ( it->is_retransmitter )
		{
			// TODO: consider filtering based on availability of a bus of a proper type
			ret_code = siot_mesh_at_root_target_to_link_id( it->device_id, &link_id );
			if ( ret_code == SIOT_MESH_RET_OK ) // we know link to this retransmitter; prepare a packet for it
			{
				uint8_t more_data_in_record = 0;
				uint16_t retr_header = more_data_in_record | ( ( it->device_id + 1 ) << 1 );
				zepto_parser_encode_and_append_uint16( retransmitters_h, retr_header );
//				known_retr_cnt++;
			}
		}
	zepto_parser_encode_and_append_uint16( retransmitters_h, 0 ); // terminator of the list, "EXTRA_DATA_FOLLOWS=0 and NODE-ID=0"


	// 3. generate packets (in current implementation we assume that the root has a single bus)
	// TODO: if a root, in general, is supposed to have more than one bus, consider this case (packets for each bus)

	MEMORY_HANDLE output_h = acquire_memory_handle();
	ZEPTO_DEBUG_ASSERT( output_h != MEMORY_HANDLE_INVALID );

	// for ( each bus id )
	{
				zepto_parser_free_memory( output_h );

				// generated prefix (the same for all packets)
				zepto_copy_response_to_response_of_another_handle( prefix_h, output_h );

				// LAST-HOP-BUS-ID
				zepto_parser_encode_and_append_uint16( output_h, 0 );

				// REQUEST-ID
				zepto_parser_encode_and_append_uint16( output_h, rq_id );

				//  OPTIONAL-DELAY-UNIT

				// MULTIPLE-RETRANSMITTING-ADDRESSES
				zepto_append_response_to_response_of_another_handle( retransmitters_h, output_h );

				// BROADCAST-BUS-TYPE-LIST 
				zepto_append_response_to_response_of_another_handle( bus_type_h, output_h );

				// Multiple-Target-Addresses
				header = 0 | ( target_id << 1 ); // NODE-ID, no more data
				zepto_parser_encode_and_append_uint16( output_h, header );
				zepto_parser_encode_and_append_uint16( output_h, 0 );

				// OPTIONAL-TARGET-REPLY-DELAY

				// OPTIONAL-PAYLOAD-SIZE

				// HEADER-CHECKSUM
				uint16_t rsp_sz = memory_object_get_response_size( output_h );
				uint16_t checksum = zepto_parser_calculate_checksum_of_part_of_response( output_h, 0, rsp_sz, 0 );
				zepto_write_uint8( output_h, (uint8_t)checksum );
				zepto_write_uint8( output_h, (uint8_t)(checksum>>8) );

				// PAYLOAD
				parser_obj po, po1;
				zepto_parser_init( &po, mem_h );
				zepto_parser_init( &po1, mem_h );
				zepto_parse_skip_block( &po1, zepto_parsing_remaining_bytes( &po ) );
				zepto_append_part_of_request_to_response_of_another_handle( mem_h, &po, &po1, output_h );

				// FULL-CHECKSUM
				checksum = zepto_parser_calculate_checksum_of_part_of_response( output_h, rsp_sz + 2, memory_object_get_response_size( output_h ) - (rsp_sz + 2), checksum );
				zepto_write_uint8( output_h, (uint8_t)checksum );
				zepto_write_uint8( output_h, (uint8_t)(checksum>>8) );

				zepto_response_to_request( output_h );
				siot_mesh_at_root_add_send_from_santa_task( output_h, currt, 0 );
				TIME_MILLISECONDS16_TO_TIMEVAL( 0, wf->wait_time );
	}

	release_memory_handle( output_h );
	release_memory_handle( bus_type_h );
	release_memory_handle( retransmitters_h );
	release_memory_handle( prefix_h );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef NEW_UPDATE_PROCESSING

typedef struct _SIOT_MESH_ROUTE_UPDATE
{
	bool to_add;
	SIOT_MESH_ROUTE route; // NOTE: if operation is to remove, only ROUTE_ID is in use
} SIOT_MESH_ROUTE_UPDATE;

typedef struct _SIOT_MESH_LINK_UPDATE
{
	bool to_add;
	SIOT_MESH_LINK link; // NOTE: if operation is to remove, only LINK_ID is in use
} SIOT_MESH_LINK_UPDATE;

typedef vector< SIOT_MESH_ROUTE_UPDATE > SIOT_M_ROUTE_TABLE_UPDATE_TYPE;
typedef vector< SIOT_MESH_LINK_UPDATE > SIOT_M_LINK_TABLE_UPDATE_TYPE;

typedef struct _SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA_UPDATE
{
//	bool in_progress; // true in the time range between the request to update has been sent, and confirmation is received; before this period requests to the same device can be merged; after the end of this period the request is removed from the set of outstanding requests
	uint16_t device_id;
	bool clear_tables_first;
	SIOT_M_ROUTE_TABLE_UPDATE_TYPE siot_m_route_table_update;
	SIOT_M_LINK_TABLE_UPDATE_TYPE siot_m_link_table_update;
	vector<int> affected_routes;
} SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE;

typedef list< SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE > SIOT_MESH_ALL_ROUTING_DATA_UPDATES;
typedef list< SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE >::iterator SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR;


//SIOT_MESH_ALL_ROUTING_DATA_UPDATES mesh_routing_data_updates;
SIOT_MESH_ALL_ROUTING_DATA_UPDATES mesh_routing_data_updates_in_progress;

typedef struct _SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA_UPDATE_INITIATOR
{
	uint16_t device_id;
	bool enforce_full_update;
	vector<int> affected_routes;
} SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE_INITIATOR;

typedef list< SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE_INITIATOR > SIOT_MESH_ALL_ROUTING_DATA_UPDATE_INITIATORS;
typedef list< SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE_INITIATOR >::iterator SIOT_MESH_ALL_ROUTING_DATA_UPDATE_INITIATORS_ITERATOR;
SIOT_MESH_ALL_ROUTING_DATA_UPDATE_INITIATORS planned_updates;

#if 0
void siot_mesh_at_root_init_route_update_data( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update, uint16_t device_id, bool clear_tables_first, uint16_t affected_device_id )
{
	update->clear_tables_first = clear_tables_first;
	update->siot_m_link_table_update.clear();
	update->siot_m_route_table_update.clear();
	update->device_id = device_id;
	update->in_progress = false;
	update->affected_routes.clear();
	update->affected_routes.push_back(affected_device_id);
}
#endif

void siot_mesh_at_root_init_route_update_data( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE_INITIATOR* update, uint16_t device_id, uint16_t affected_device_id )
{
	update->device_id = device_id;
	update->enforce_full_update = false;
	update->affected_routes.clear();
	update->affected_routes.push_back(affected_device_id);
}

void siot_mesh_at_root_add_planned_route_update( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE_INITIATOR* update )
{
	ZEPTO_DEBUG_ASSERT( update->enforce_full_update == false );
	SIOT_MESH_ALL_ROUTING_DATA_UPDATE_INITIATORS_ITERATOR it;
	for ( it=planned_updates.begin(); it!= planned_updates.end(); ++it )
		if ( it->device_id == update->device_id )
		{
			it->affected_routes.insert( it->affected_routes.end(), update->affected_routes.begin(), update->affected_routes.end() );
			return;
		}
	planned_updates.push_back( *update ); 
}

void siot_mesh_at_root_add_route_update_requiring_full_erase( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update )
{
	SIOT_MESH_ALL_ROUTING_DATA_UPDATE_INITIATORS_ITERATOR it;
	for ( it=planned_updates.begin(); it!= planned_updates.end(); ++it )
		if ( it->device_id == update->device_id )
		{
			it->affected_routes.insert( it->affected_routes.end(), update->affected_routes.begin(), update->affected_routes.end() );
			it->enforce_full_update = true;
			return;
		}
	SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE_INITIATOR planned_update;
	planned_update.device_id = update->device_id;
	planned_update.enforce_full_update = true;
	planned_update.affected_routes = update->affected_routes;
	planned_updates.push_front( planned_update ); 
}

#if 0
void siot_mesh_at_root_init_route_update_data_with_many_affected_devices( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update, uint16_t device_id, bool clear_tables_first, const vector<uint16_t>& affected_device_ids )
{
	update->clear_tables_first = clear_tables_first;
	update->siot_m_link_table_update.clear();
	update->siot_m_route_table_update.clear();
	update->device_id = device_id;
	update->in_progress = false;
	update->affected_routes.clear();
	for ( unsigned int i=0; i<affected_device_ids.size(); i++ )
		update->affected_routes.push_back(affected_device_ids[i]);
}
#endif

void siot_mesh_at_root_init_route_update_data_with_many_affected_devices( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE_INITIATOR* update, uint16_t device_id, const vector<uint16_t>& affected_device_ids )
{
	update->device_id = device_id;
	update->enforce_full_update = false;
	update->affected_routes.clear();
	for ( unsigned int i=0; i<affected_device_ids.size(); i++ )
		update->affected_routes.push_back(affected_device_ids[i]);
}

#if 0
void siot_mesh_at_root_init_route_update_to_erase_all( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update, uint16_t device_id )
{
	update->clear_tables_first = true;
	update->siot_m_link_table_update.clear();
	update->siot_m_route_table_update.clear();
	update->device_id = device_id;
	update->in_progress = false;
	update->affected_routes.clear();
}
#endif

void siot_mesh_at_root_apply_update_to_device_routing_data( const SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update, SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA* dev_data ) 
{
	ZEPTO_DEBUG_ASSERT( update->device_id == dev_data->device_id );

	uint16_t i, j;

	if ( update->clear_tables_first )
	{
		dev_data->siot_m_route_table_confirmed.clear();
		dev_data->siot_m_link_table_confirmed.clear();
	}

	for ( i=0; i<update->siot_m_route_table_update.size(); i++ )
	{
		if ( update->siot_m_route_table_update[i].to_add )
		{
			bool applied = false;
			int ini_sz = dev_data->siot_m_route_table_confirmed.size();
			if ( ini_sz == 0 || dev_data->siot_m_route_table_confirmed[0].TARGET_ID > update->siot_m_route_table_update[i].route.TARGET_ID )
			{
				dev_data->siot_m_route_table_confirmed.insert( dev_data->siot_m_route_table_confirmed.begin(), update->siot_m_route_table_update[i].route );
				applied = true;
			}
			if ( !applied )
			{
				for ( j=0; j<ini_sz; j++ )
					if ( dev_data->siot_m_route_table_confirmed[j].TARGET_ID == update->siot_m_route_table_update[i].route.TARGET_ID )
					{
						dev_data->siot_m_route_table_confirmed[j].LINK_ID = update->siot_m_route_table_update[i].route.LINK_ID;
						applied = true;
						break;
					}
					else if ( dev_data->siot_m_route_table_confirmed[j].TARGET_ID > update->siot_m_route_table_update[i].route.TARGET_ID ) // as soon as.... (we exploit canonicity here)
					{
						dev_data->siot_m_route_table_confirmed.insert( dev_data->siot_m_route_table_confirmed.begin() + j, update->siot_m_route_table_update[i].route );
						applied = true;
						break;
					}
			}
			if ( !applied ) // something new
			{
				ZEPTO_DEBUG_ASSERT( dev_data->siot_m_route_table_confirmed.size() == 0 || dev_data->siot_m_route_table_confirmed[ dev_data->siot_m_route_table_confirmed.size() - 1 ].TARGET_ID < update->siot_m_route_table_update[i].route.TARGET_ID );
				dev_data->siot_m_route_table_confirmed.push_back( update->siot_m_route_table_update[i].route );
			}
		}
		else
		{
			int ini_sz = dev_data->siot_m_route_table_confirmed.size();
			bool applied = false;
			for ( j=0; j<ini_sz; j++ )
				if ( dev_data->siot_m_route_table_confirmed[j].TARGET_ID == update->siot_m_route_table_update[i].route.TARGET_ID )
				{
					dev_data->siot_m_route_table_confirmed.erase( dev_data->siot_m_route_table_confirmed.begin() + j );
					applied = true;
					break;
				}
			ZEPTO_DEBUG_ASSERT( applied );
		}
	}

	for ( i=0; i<update->siot_m_link_table_update.size(); i++ )
	{
		if ( update->siot_m_link_table_update[i].to_add )
		{
			bool applied = false;
			int ini_sz = dev_data->siot_m_link_table_confirmed.size();
			if ( ini_sz == 0 || dev_data->siot_m_link_table_confirmed[0].LINK_ID > update->siot_m_link_table_update[i].link.LINK_ID )
			{
				dev_data->siot_m_link_table_confirmed.insert( dev_data->siot_m_link_table_confirmed.begin(), update->siot_m_link_table_update[i].link );
				applied = true;
			}
			if ( !applied ) // something new
			{
				for ( j=0; j<ini_sz; j++ )
					if ( dev_data->siot_m_link_table_confirmed[j].LINK_ID == update->siot_m_link_table_update[i].link.LINK_ID )
					{
						dev_data->siot_m_link_table_confirmed[j].LINK_ID = update->siot_m_link_table_update[i].link.LINK_ID;
						// TODO: other items
						applied = true;
						break;
					}
					else if ( dev_data->siot_m_link_table_confirmed[j].LINK_ID > update->siot_m_link_table_update[i].link.LINK_ID ) // as soon as.... (we exploit canonicity here)
					{
						dev_data->siot_m_link_table_confirmed.insert( dev_data->siot_m_link_table_confirmed.begin() + j, update->siot_m_link_table_update[i].link );
						applied = true;
						break;
					}
			}
			if ( !applied ) // something new
			{
				ZEPTO_DEBUG_ASSERT( dev_data->siot_m_link_table_confirmed.size() == 0 || dev_data->siot_m_link_table_confirmed[ dev_data->siot_m_link_table_confirmed.size() - 1 ].LINK_ID < update->siot_m_link_table_update[i].link.LINK_ID );
				dev_data->siot_m_link_table_confirmed.push_back( update->siot_m_link_table_update[i].link );
			}
		}
		else
		{
			bool applied = false;
			int ini_sz = dev_data->siot_m_link_table_confirmed.size();
			for ( j=0; j<ini_sz; j++ )
				if ( dev_data->siot_m_link_table_confirmed[j].LINK_ID == update->siot_m_link_table_update[i].link.LINK_ID )
				{
					dev_data->siot_m_link_table_confirmed.erase( dev_data->siot_m_link_table_confirmed.begin() + j );
					applied = true;
					break;
				}
			ZEPTO_DEBUG_ASSERT( applied );
		}
	}
}

uint8_t siot_mesh_at_root_apply_update_to_local_copy( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update ) 
{
dbg_siot_mesh_at_root_validate_all_device_tables();

	// NOTE: in this implementation we assume that an update is either to add or to remove respective items
	unsigned int total_cnt = update->siot_m_link_table_update.size() + update->siot_m_route_table_update.size();
	ZEPTO_DEBUG_ASSERT( total_cnt );

	SIOT_MESH_ROUTING_DATA_ITERATOR dev_data;
	uint8_t ret_code = siot_mesh_at_root_get_device_data( update->device_id, dev_data );
	ZEPTO_DEBUG_ASSERT( ret_code == SIOT_MESH_AT_ROOT_RET_OK );
	siot_mesh_at_root_apply_update_to_device_routing_data( update, &(*dev_data) );

dbg_siot_mesh_at_root_validate_all_device_tables();
	return SIOT_MESH_AT_ROOT_RET_OK;
}

void siot_mesh_at_root_get_confirmed_table_state_after_applying_update( const SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update, SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA* dev_resulting_data)
{
	SIOT_MESH_ROUTING_DATA_ITERATOR dev_data;
	uint8_t ret_code = siot_mesh_at_root_get_device_data( update->device_id, dev_data );
	ZEPTO_DEBUG_ASSERT( ret_code == SIOT_MESH_AT_ROOT_RET_OK );
	dev_resulting_data->device_id = dev_data->device_id;
	dev_resulting_data->siot_m_link_table_confirmed = dev_data->siot_m_link_table_confirmed;
	dev_resulting_data->siot_m_route_table_confirmed = dev_data->siot_m_route_table_confirmed;
	dev_resulting_data->bus_type_list = dev_data->bus_type_list;
	dev_resulting_data->is_retransmitter = dev_data->is_retransmitter;

	siot_mesh_at_root_apply_update_to_device_routing_data( update, dev_resulting_data );
}

#if 0
void siot_mesh_at_root_get_diff_as_update( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update, const SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA* curr_dev_data, const SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA* new_dev_data ) 
{
	unsigned int i, j;
	SIOT_MESH_ROUTE_UPDATE route_update;
	SIOT_MESH_LINK_UPDATE link_update;

	ZEPTO_DEBUG_ASSERT( curr_dev_data->device_id == new_dev_data->device_id );

	// route table changes
	j=0;
	for ( i=0; i<curr_dev_data->siot_m_route_table.size(); i++ )
	{
		if ( j >= new_dev_data->siot_m_route_table.size() )
			break;
		if ( curr_dev_data->siot_m_route_table[i].TARGET_ID < new_dev_data->siot_m_route_table[j].TARGET_ID ) // no longer exists
		{
			route_update.to_add = false;
			route_update.route.TARGET_ID = curr_dev_data->siot_m_route_table[i].TARGET_ID;
			update->siot_m_route_table_update.push_back( route_update );
		}
		else if ( curr_dev_data->siot_m_route_table[i].TARGET_ID == new_dev_data->siot_m_route_table[j].TARGET_ID ) // no changes
		{
			j++;
		}
		else while ( curr_dev_data->siot_m_route_table[i].TARGET_ID > new_dev_data->siot_m_route_table[j].TARGET_ID ) // a new one
		{
			route_update.to_add = true;
			route_update.route = new_dev_data->siot_m_route_table[i];
			update->siot_m_route_table_update.push_back( route_update );
			j++;
			if ( j >= new_dev_data->siot_m_route_table.size() )
				break;
		}
	}
	for ( ; i<curr_dev_data->siot_m_route_table.size(); i++ )
	{
		route_update.to_add = false;
		route_update.route.TARGET_ID = curr_dev_data->siot_m_route_table[i].TARGET_ID;
		update->siot_m_route_table_update.push_back( route_update );
	}
	for ( ; j<new_dev_data->siot_m_route_table.size(); j++ )
	{
		route_update.to_add = true;
		route_update.route = new_dev_data->siot_m_route_table[i];
		update->siot_m_route_table_update.push_back( route_update );
	}

	// link table changes
	j=0;
	for ( i=0; i<curr_dev_data->siot_m_link_table.size(); i++ )
	{
		if ( j >= new_dev_data->siot_m_link_table.size() )
			break;
		if ( curr_dev_data->siot_m_link_table[i].LINK_ID < new_dev_data->siot_m_link_table[j].LINK_ID ) // no longer exists
		{
			link_update.to_add = false;
			link_update.link.LINK_ID = curr_dev_data->siot_m_link_table[i].LINK_ID;
			update->siot_m_link_table_update.push_back( link_update );
		}
		else if ( curr_dev_data->siot_m_link_table[i].LINK_ID == new_dev_data->siot_m_link_table[j].LINK_ID ) // no changes
		{
			j++;
		}
		else while ( curr_dev_data->siot_m_link_table[i].LINK_ID > new_dev_data->siot_m_link_table[j].LINK_ID ) // a new one
		{
			link_update.to_add = true;
			link_update.link = new_dev_data->siot_m_link_table[i];
			update->siot_m_link_table_update.push_back( link_update );
			j++;
			if ( j >= new_dev_data->siot_m_link_table.size() )
				break;
		}
	}
	for ( ; i<curr_dev_data->siot_m_link_table.size(); i++ )
	{
		link_update.to_add = false;
		link_update.link.LINK_ID = curr_dev_data->siot_m_link_table[i].LINK_ID;
		update->siot_m_link_table_update.push_back( link_update );
	}
	for ( ; j<new_dev_data->siot_m_link_table.size(); j++ )
	{
		link_update.to_add = true;
		link_update.link = new_dev_data->siot_m_link_table[i];
		update->siot_m_link_table_update.push_back( link_update );
	}
#ifdef SA_DEBUG
	// let's check whether we're right
	SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA dev_data;
	dev_data.device_id = curr_dev_data->device_id;
	dev_data.siot_m_link_table = curr_dev_data->siot_m_link_table;
	dev_data.siot_m_route_table = curr_dev_data->siot_m_route_table;
	dev_data.bus_type_list = curr_dev_data->bus_type_list;
	dev_data.is_retransmitter = curr_dev_data->is_retransmitter;

	siot_mesh_at_root_apply_update_to_device_routing_data( update, &dev_data );

	ZEPTO_DEBUG_ASSERT( dev_data.siot_m_route_table.size() == new_dev_data->siot_m_route_table.size() );
	for ( i=0; i<dev_data.siot_m_route_table.size(); i++ )
	{
		ZEPTO_DEBUG_ASSERT( dev_data.siot_m_route_table[i].TARGET_ID == new_dev_data->siot_m_route_table[i].TARGET_ID );
		ZEPTO_DEBUG_ASSERT( dev_data.siot_m_route_table[i].LINK_ID == new_dev_data->siot_m_route_table[i].LINK_ID );
	}

	ZEPTO_DEBUG_ASSERT( dev_data.siot_m_link_table.size() == new_dev_data->siot_m_link_table.size() );
	for ( i=0; i<dev_data.siot_m_link_table.size(); i++ )
	{
		ZEPTO_DEBUG_ASSERT( dev_data.siot_m_link_table[i].LINK_ID == new_dev_data->siot_m_link_table[i].LINK_ID );
		ZEPTO_DEBUG_ASSERT( dev_data.siot_m_link_table[i].BUS_ID == new_dev_data->siot_m_link_table[i].BUS_ID );
		ZEPTO_DEBUG_ASSERT( dev_data.siot_m_link_table[i].NEXT_HOP == new_dev_data->siot_m_link_table[i].NEXT_HOP );
		ZEPTO_DEBUG_ASSERT( dev_data.siot_m_link_table[i].INTRA_BUS_ID == new_dev_data->siot_m_link_table[i].INTRA_BUS_ID );
	}
#endif // SA_DEBUG
}
#endif // 0

void siot_mesh_at_root_get_diff_as_update( const SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA* dev_data, SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update ) 
{
	unsigned int i, j;
	SIOT_MESH_ROUTE_UPDATE route_update;
	SIOT_MESH_LINK_UPDATE link_update;

	update->device_id = dev_data->device_id;
	update->clear_tables_first = false;

	// route table changes
	j=0;
	for ( i=0; i<dev_data->siot_m_route_table_confirmed.size(); i++ )
	{
		if ( j >= dev_data->siot_m_route_table_planned.size() )
			break;
		if ( dev_data->siot_m_route_table_confirmed[i].TARGET_ID < dev_data->siot_m_route_table_planned[j].TARGET_ID ) // no longer exists
		{
			route_update.to_add = false;
			route_update.route.TARGET_ID = dev_data->siot_m_route_table_confirmed[i].TARGET_ID;
			update->siot_m_route_table_update.push_back( route_update );
		}
		else if ( dev_data->siot_m_route_table_confirmed[i].TARGET_ID == dev_data->siot_m_route_table_planned[j].TARGET_ID ) // no changes
		{
			j++;
		}
		else 
		{
			while ( dev_data->siot_m_route_table_confirmed[i].TARGET_ID > dev_data->siot_m_route_table_planned[j].TARGET_ID ) // a new one
			{
				route_update.to_add = true;
				route_update.route = dev_data->siot_m_route_table_planned[i];
				update->siot_m_route_table_update.push_back( route_update );
				j++;
				if ( j >= dev_data->siot_m_route_table_planned.size() )
					break;
			}
			i--; // repeat with the same value of 'i' and other values of 'j' again
		}
	}
	for ( ; i<dev_data->siot_m_route_table_confirmed.size(); i++ )
	{
		route_update.to_add = false;
		route_update.route.TARGET_ID = dev_data->siot_m_route_table_confirmed[i].TARGET_ID;
		update->siot_m_route_table_update.push_back( route_update );
	}
	for ( ; j<dev_data->siot_m_route_table_planned.size(); j++ )
	{
		route_update.to_add = true;
		route_update.route = dev_data->siot_m_route_table_planned[j];
		update->siot_m_route_table_update.push_back( route_update );
	}

	// link table changes
	j=0;
	for ( i=0; i<dev_data->siot_m_link_table_confirmed.size(); i++ )
	{
		if ( j >= dev_data->siot_m_link_table_planned.size() )
			break;
		if ( dev_data->siot_m_link_table_confirmed[i].LINK_ID < dev_data->siot_m_link_table_planned[j].LINK_ID ) // no longer exists
		{
			link_update.to_add = false;
			link_update.link.LINK_ID = dev_data->siot_m_link_table_confirmed[i].LINK_ID;
			update->siot_m_link_table_update.push_back( link_update );
		}
		else if ( dev_data->siot_m_link_table_confirmed[i].LINK_ID == dev_data->siot_m_link_table_planned[j].LINK_ID ) // no changes
		{
			j++;
		}
		else 
		{
			while ( dev_data->siot_m_link_table_confirmed[i].LINK_ID > dev_data->siot_m_link_table_planned[j].LINK_ID ) // a new one
			{
				link_update.to_add = true;
				link_update.link = dev_data->siot_m_link_table_planned[i];
				update->siot_m_link_table_update.push_back( link_update );
				j++;
				if ( j >= dev_data->siot_m_link_table_planned.size() )
					break;
			}
			i--; // repeat with the same value of 'i' and other values of 'j' again
		}
	}
	for ( ; i<dev_data->siot_m_link_table_confirmed.size(); i++ )
	{
		link_update.to_add = false;
		link_update.link.LINK_ID = dev_data->siot_m_link_table_confirmed[i].LINK_ID;
		update->siot_m_link_table_update.push_back( link_update );
	}
	for ( ; j<dev_data->siot_m_link_table_planned.size(); j++ )
	{
		link_update.to_add = true;
		link_update.link = dev_data->siot_m_link_table_planned[j];
		update->siot_m_link_table_update.push_back( link_update );
	}

#ifdef SA_DEBUG
	// let's check whether we're right
	SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA test_dev_data;
	test_dev_data.device_id = dev_data->device_id;
	test_dev_data.siot_m_link_table_planned = dev_data->siot_m_link_table_planned;
	test_dev_data.siot_m_route_table_planned = dev_data->siot_m_route_table_planned;
	test_dev_data.siot_m_link_table_confirmed = dev_data->siot_m_link_table_confirmed;
	test_dev_data.siot_m_route_table_confirmed = dev_data->siot_m_route_table_confirmed;
	test_dev_data.bus_type_list = dev_data->bus_type_list;
	test_dev_data.is_retransmitter = dev_data->is_retransmitter;

	siot_mesh_at_root_apply_update_to_device_routing_data( update, &test_dev_data );

	ZEPTO_DEBUG_ASSERT( test_dev_data.siot_m_route_table_confirmed.size() == test_dev_data.siot_m_route_table_planned.size() );
	for ( i=0; i<test_dev_data.siot_m_route_table_confirmed.size(); i++ )
	{
		ZEPTO_DEBUG_ASSERT( test_dev_data.siot_m_route_table_confirmed[i].TARGET_ID == dev_data->siot_m_route_table_planned[i].TARGET_ID );
		ZEPTO_DEBUG_ASSERT( test_dev_data.siot_m_route_table_confirmed[i].LINK_ID == dev_data->siot_m_route_table_planned[i].LINK_ID );
	}

	ZEPTO_DEBUG_ASSERT( test_dev_data.siot_m_link_table_confirmed.size() == dev_data->siot_m_link_table_planned.size() );
	for ( i=0; i<test_dev_data.siot_m_link_table_confirmed.size(); i++ )
	{
		ZEPTO_DEBUG_ASSERT( test_dev_data.siot_m_link_table_confirmed[i].LINK_ID == dev_data->siot_m_link_table_planned[i].LINK_ID );
		ZEPTO_DEBUG_ASSERT( test_dev_data.siot_m_link_table_confirmed[i].BUS_ID == dev_data->siot_m_link_table_planned[i].BUS_ID );
		ZEPTO_DEBUG_ASSERT( test_dev_data.siot_m_link_table_confirmed[i].NEXT_HOP == dev_data->siot_m_link_table_planned[i].NEXT_HOP );
		ZEPTO_DEBUG_ASSERT( test_dev_data.siot_m_link_table_confirmed[i].INTRA_BUS_ID == dev_data->siot_m_link_table_planned[i].INTRA_BUS_ID );
	}
#endif // SA_DEBUG
}

#if 0
uint8_t siot_mesh_at_root_build_full_update_based_on_local_copy( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update ) 
{
	// NOTE: in this implementation we assume that an update is either to add or to remove respective items
	unsigned int total_cnt = update->siot_m_link_table_update.size() + update->siot_m_route_table_update.size();
	ZEPTO_DEBUG_ASSERT( total_cnt );

	SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA dev_data_copy;
	siot_mesh_at_root_get_confirmed_table_state_after_applying_update( update, &dev_data_copy );

dbg_siot_mesh_at_root_validate_device_tables( &dev_data_copy );

	update->clear_tables_first = true;
	update->siot_m_route_table_update.clear();
	update->siot_m_link_table_update.clear();

	unsigned int i;
	SIOT_MESH_ROUTE_UPDATE route_update;
	route_update.to_add = true;
	SIOT_MESH_LINK_UPDATE link_update;
	link_update.to_add = true;

	for ( i=0; i<dev_data_copy.siot_m_route_table.size(); i++ )
	{
		route_update.route = dev_data_copy.siot_m_route_table[i];
		update->siot_m_route_table_update.push_back( route_update );
	}

	for ( i=0; i<dev_data_copy.siot_m_link_table.size(); i++ )
	{
		link_update.link = dev_data_copy.siot_m_link_table[i];
		update->siot_m_link_table_update.push_back( link_update );
	}

	return SIOT_MESH_AT_ROOT_RET_OK;
}
#endif // 0

uint8_t siot_mesh_at_root_build_full_update( const SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA* dev_data, SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update ) 
{
dbg_siot_mesh_at_root_validate_device_tables( &(*dev_data) );

	update->clear_tables_first = true;
	update->device_id = dev_data->device_id;
	update->siot_m_route_table_update.clear();
	update->siot_m_link_table_update.clear();

	unsigned int i;
	SIOT_MESH_ROUTE_UPDATE route_update;
	route_update.to_add = true;
	SIOT_MESH_LINK_UPDATE link_update;
	link_update.to_add = true;

	for ( i=0; i<dev_data->siot_m_route_table_planned.size(); i++ )
	{
		route_update.route = dev_data->siot_m_route_table_planned[i];
		update->siot_m_route_table_update.push_back( route_update );
	}

	for ( i=0; i<dev_data->siot_m_link_table_planned.size(); i++ )
	{
		link_update.link = dev_data->siot_m_link_table_planned[i];
		update->siot_m_link_table_update.push_back( link_update );
	}

	return SIOT_MESH_AT_ROOT_RET_OK;
}

#if 0
uint8_t siot_mesh_at_root_get_list_of_updated_devices_when_route_is_added( SIOT_MESH_ALL_ROUTING_DATA_UPDATES& update_list, uint16_t id_target, uint16_t bus_to_send_from_target, uint16_t id_prev, uint16_t bust_to_send_from_prev, uint16_t id_next )
{
	// NOTE: prepares updates for a target and all retransmitters on the way

	SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE update;
	SIOT_MESH_ROUTING_DATA_ITERATOR rd_it;
	SIOT_MESH_ROUTE_UPDATE route_update;
	SIOT_MESH_LINK_UPDATE link_update;
	route_update.to_add = true;
	route_update.route.TARGET_ID = id_target;
	link_update.to_add = true;
	uint16_t dev_id, prev_dev_id;
	uint16_t i, j;
	update_list.clear();
	ZEPTO_DEBUG_ASSERT( mesh_routing_data.size() && mesh_routing_data[0].device_id == 0 );
	ZEPTO_DEBUG_ASSERT( id_target != 0 );

	// adding items for all devices from ROOT to the predecessor of id_prev
	dev_id = 0;
	prev_dev_id = 0;
	while ( dev_id != id_prev )
	{
		bool is_root = dev_id == 0;
#ifdef SA_DEBUG
		bool next_found = false;
#endif // SA_DEBUG
		siot_mesh_at_root_init_route_update_data( &update, dev_id, false, id_target );
		siot_mesh_at_root_get_device_data( dev_id, rd_it );
		int ini_sz = rd_it->siot_m_route_table_planned.size();
		for ( i=0; i<ini_sz; i++ )
			if ( rd_it->siot_m_route_table_planned[i].TARGET_ID == id_prev )
			{
				route_update.route.LINK_ID = rd_it->siot_m_route_table_planned[i].LINK_ID;
				update.siot_m_route_table_update.push_back( route_update );
				for ( j=0; j<rd_it->siot_m_link_table_planned.size(); j++ )
					if ( rd_it->siot_m_link_table_planned[j].LINK_ID == rd_it->siot_m_route_table_planned[i].LINK_ID )
					{
						prev_dev_id = dev_id;
						dev_id = rd_it->siot_m_link_table_planned[i].NEXT_HOP;
#ifdef SA_DEBUG
						next_found = true;
#endif // SA_DEBUG
						break;
					}
				break;
			}
		bool is_update = update.siot_m_link_table_update.size() || update.siot_m_route_table_update.size() || update.clear_tables_first;
//		ZEPTO_DEBUG_ASSERT( is_update );
		if ( is_update )
		{
			if ( is_root )
				siot_mesh_at_root_apply_update_to_local_copy( &update );
			else
				update_list.push_back( update );
		}
#ifdef SA_DEBUG
if ( prev_dev_id == dev_id )
{
	prev_dev_id = prev_dev_id;
}
//		ZEPTO_DEBUG_ASSERT( next_found );
		ZEPTO_DEBUG_ASSERT( prev_dev_id != dev_id ); 
#endif // SA_DEBUG
	}

	// the last device (as well as a target device) should receive LINK record; and we need to somehow assign a LINK_ID on a target device

	// adding item for the last retransmitter
	siot_mesh_at_root_get_device_data( id_prev, rd_it );
	bool link_found = false;
	for ( j=0; j<rd_it->siot_m_link_table_planned.size(); j++ )
	{
		if ( rd_it->siot_m_link_table_planned[j].NEXT_HOP == id_target )
		{
			link_update.link.LINK_ID = rd_it->siot_m_link_table_planned[j].LINK_ID;
			link_found = true;
			break;
		}
	}
	if ( !link_found )
	{
		link_update.link.LINK_ID = rd_it->siot_m_link_table_planned.size();
		for ( j=0; j<rd_it->siot_m_link_table_planned.size(); j++ )
			if ( j < rd_it->siot_m_link_table_planned[j].LINK_ID )
			{
				link_update.link.LINK_ID = j;
				break;
			}
	}
	link_update.link.NEXT_HOP = id_target;
	link_update.link.BUS_ID = bust_to_send_from_prev;
	// TODO: (!!!) MISSING INFORMATION: link other values
	route_update.route.LINK_ID = link_update.link.LINK_ID;
	siot_mesh_at_root_init_route_update_data( &update, id_prev, false, id_target );
	update.siot_m_link_table_update.push_back( link_update );
	update.siot_m_route_table_update.push_back( route_update );
	ZEPTO_DEBUG_ASSERT( update.siot_m_link_table_update.size() || update.siot_m_route_table_update.size() );
	if ( id_prev == 0 )
		siot_mesh_at_root_apply_update_to_local_copy( &update );
	else
		update_list.push_back( update );

	// adding item for the target device
	// TODO: (!!!) INFORMATION IS MISSING: physics of retransmission
	link_update.link.LINK_ID = 0;
	link_update.link.NEXT_HOP = id_next;
	link_update.link.BUS_ID = bus_to_send_from_target;
	route_update.route.LINK_ID = link_update.link.LINK_ID;
	route_update.route.TARGET_ID = 0;
	siot_mesh_at_root_init_route_update_data( &update, id_target, false, id_target );
	update.siot_m_link_table_update.push_back( link_update );
	update.siot_m_route_table_update.push_back( route_update );
	ZEPTO_DEBUG_ASSERT( update.siot_m_link_table_update.size() || update.siot_m_route_table_update.size() );
	update_list.push_back( update );

	return SIOT_MESH_AT_ROOT_RET_OK;
}
#endif // 0
uint8_t siot_mesh_at_root_update_device_data_when_route_is_added( uint16_t id_target, uint16_t bus_to_send_from_target, uint16_t id_prev, uint16_t bust_to_send_from_prev, uint16_t id_next )
{
	// NOTE: prepares updates for a target and all retransmitters on the way

	SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE_INITIATOR update;
	SIOT_MESH_ROUTING_DATA_ITERATOR rd_it;
	SIOT_MESH_ROUTE route;
	SIOT_MESH_LINK link;
	uint16_t dev_id, prev_dev_id;
	uint16_t i, j;
	ZEPTO_DEBUG_ASSERT( mesh_routing_data.size() && mesh_routing_data[0].device_id == 0 );
	ZEPTO_DEBUG_ASSERT( id_target != 0 );

	// adding items for all devices from ROOT to the predecessor of id_prev
	dev_id = 0;
	prev_dev_id = 0;
	while ( dev_id != id_prev )
	{
		bool is_root = dev_id == 0;
#ifdef SA_DEBUG
		bool next_found = false;
#endif // SA_DEBUG
		siot_mesh_at_root_init_route_update_data( &update, dev_id, id_target );
		siot_mesh_at_root_get_device_data( dev_id, rd_it );
		int ini_sz = rd_it->siot_m_route_table_planned.size();
		for ( i=0; i<ini_sz; i++ )
			if ( rd_it->siot_m_route_table_planned[i].TARGET_ID == id_prev )
			{
//				route_update.route.LINK_ID = rd_it->siot_m_route_table_planned[i].LINK_ID;
//				update.siot_m_route_table_update.push_back( route_update );
				route.TARGET_ID = id_target;
				route.LINK_ID = rd_it->siot_m_route_table_planned[i].LINK_ID;
				siot_mesh_at_root_add_or_update_route( &(rd_it->siot_m_route_table_planned), &route );
				if ( is_root )
					siot_mesh_at_root_add_or_update_route( &(rd_it->siot_m_route_table_confirmed), &route );
				else
					siot_mesh_at_root_add_planned_route_update( &update );
				for ( j=0; j<rd_it->siot_m_link_table_planned.size(); j++ )
//					if ( rd_it->siot_m_link_table_planned[j].LINK_ID == rd_it->siot_m_route_table_planned[i].LINK_ID )
					if ( rd_it->siot_m_link_table_planned[j].LINK_ID == route.LINK_ID )
					{
						prev_dev_id = dev_id;
						dev_id = rd_it->siot_m_link_table_planned[i].NEXT_HOP;
#ifdef SA_DEBUG
						next_found = true;
#endif // SA_DEBUG
						break;
					}
				break;
			}
/*		bool is_update = update.siot_m_link_table_update.size() || update.siot_m_route_table_update.size() || update.clear_tables_first;
//		ZEPTO_DEBUG_ASSERT( is_update );
		if ( is_update )
		{
			if ( is_root )
				siot_mesh_at_root_apply_update_to_local_copy( &update );
			else
				update_list.push_back( update );
		}*/
#ifdef SA_DEBUG
		ZEPTO_DEBUG_ASSERT( prev_dev_id != dev_id ); 
#endif // SA_DEBUG
	}

	// the last device (as well as a target device) should receive LINK record; and we need to somehow assign a LINK_ID on a target device

	// adding item for the last retransmitter
	siot_mesh_at_root_get_device_data( id_prev, rd_it );
	link.LINK_ID = SIOT_MESH_LINK_ID_UNDEFINED;
	link.NEXT_HOP = id_target;
	link.BUS_ID = bust_to_send_from_prev;
	for ( j=0; j<rd_it->siot_m_link_table_planned.size(); j++ )
	{
		if ( rd_it->siot_m_link_table_planned[j].NEXT_HOP == id_target )
		{
			rd_it->siot_m_link_table_planned[j].BUS_ID = bust_to_send_from_prev;
			link.LINK_ID = rd_it->siot_m_link_table_planned[j].LINK_ID;
			if ( rd_it->device_id == 0 )
			{
				ZEPTO_DEBUG_ASSERT( rd_it->siot_m_link_table_confirmed[j].NEXT_HOP == id_target );
				rd_it->siot_m_link_table_confirmed[j].BUS_ID = bust_to_send_from_prev;
			}
			break;
		}
	}
	if ( link.LINK_ID == SIOT_MESH_LINK_ID_UNDEFINED )
	{
		siot_mesh_at_root_insert_link( &(rd_it->siot_m_link_table_planned), &link );
		if ( rd_it->device_id == 0 )
			siot_mesh_at_root_insert_link( &(rd_it->siot_m_link_table_confirmed), &link );
	}
	ZEPTO_DEBUG_ASSERT( link.LINK_ID != SIOT_MESH_LINK_ID_UNDEFINED );
	// TODO: (!!!) MISSING INFORMATION: link other values
	route.TARGET_ID = id_target;
	route.LINK_ID = link.LINK_ID;
	siot_mesh_at_root_add_or_update_route( &(rd_it->siot_m_route_table_planned), &route );
	if ( rd_it->device_id == 0 )
		siot_mesh_at_root_add_or_update_route( &(rd_it->siot_m_route_table_confirmed), &route );
	else
	{
		siot_mesh_at_root_init_route_update_data( &update, rd_it->device_id, id_target );
		siot_mesh_at_root_add_planned_route_update( &update );
	}

	// adding item for the target device
	// TODO: (!!!) INFORMATION IS MISSING: physics of retransmission
	siot_mesh_at_root_get_device_data( id_target, rd_it );
	ZEPTO_DEBUG_ASSERT( id_target !=  0 );
	link.LINK_ID = 0;
	link.NEXT_HOP = id_next;
	link.BUS_ID = bus_to_send_from_target;
	route.TARGET_ID = 0;
	route.LINK_ID = 0;
	siot_mesh_at_root_add_or_update_route( &(rd_it->siot_m_route_table_planned), &route );
	siot_mesh_at_root_add_or_update_link( &(rd_it->siot_m_link_table_planned), &link );

	siot_mesh_at_root_init_route_update_data( &update, id_target, id_target );
	siot_mesh_at_root_add_planned_route_update( &update );

	return SIOT_MESH_AT_ROOT_RET_OK;
}

#if 0
uint8_t siot_mesh_at_root_add_or_merge_updates_when_route_is_added( SIOT_MESH_ALL_ROUTING_DATA_UPDATES& update_list )
{
	// we start from the end of the list and add updates one by one;
	// if an update to the same device is already in the set of updates and is not in progress, we merge updates
	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it;
	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it_backward;
	for ( it = update_list.begin(); it != update_list.end(); ++it )
	{
		ZEPTO_DEBUG_ASSERT( it->siot_m_link_table_update.size() || it->siot_m_route_table_update.size() || it->clear_tables_first );
		ZEPTO_DEBUG_ASSERT( it->in_progress == false );
		// try to do at least minimal merging (with the nearest "erase all" update
		ZEPTO_DEBUG_ASSERT( it->clear_tables_first == false ); // not expected to be here
		it_backward=mesh_routing_data_updates.end();
		while ( it_backward != mesh_routing_data_updates.begin() )
		{
			--it_backward;
			if ( it_backward->device_id == it->device_id && it_backward->clear_tables_first && it_backward->siot_m_route_table_update.size() == 0 && it_backward->siot_m_link_table_update.size() == 0  )
			{
				it->clear_tables_first = true;
				mesh_routing_data_updates.erase( it_backward );
				break;
			}
		}
		mesh_routing_data_updates.push_back( *it );
	}

	return SIOT_MESH_AT_ROOT_RET_OK;
}
#endif // 0

uint8_t siot_mesh_at_root_add_updates_for_device_when_route_is_added( uint16_t id_target, uint16_t bus_to_send_from_target, uint16_t id_prev, uint16_t bust_to_send_from_prev, uint16_t id_next )
{
//	SIOT_MESH_ALL_ROUTING_DATA_UPDATES update_list;
//	siot_mesh_at_root_get_list_of_updated_devices_when_route_is_added( update_list, id_target, bus_to_send_from_target, id_prev, bust_to_send_from_prev, id_next /*more data may be required*/ );
//	siot_mesh_at_root_add_or_merge_updates_when_route_is_added( update_list );
	siot_mesh_at_root_update_device_data_when_route_is_added( id_target, bus_to_send_from_target, id_prev, bust_to_send_from_prev, id_next /*more data may be required*/ );
	// TODO: check ret codes

	return SIOT_MESH_AT_ROOT_RET_OK;
}

#if 0
uint8_t siot_mesh_at_root_get_next_update( SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR& update )
{
	// TODO: here we assume that is_last is set correctly (and should be updated at time of item adding/removal). Think whether this assumption is practically good (seems to be, indeed...).
	if ( mesh_routing_data_updates.begin() == mesh_routing_data_updates.end() )
		return SIOT_MESH_AT_ROOT_RET_NO_UPDATES;

	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it;

	for ( update = mesh_routing_data_updates.begin(); update != mesh_routing_data_updates.end(); ++update )
	{
		ZEPTO_DEBUG_ASSERT( update->device_id != 0 ); // must be applied at time of birth
		if ( update->clear_tables_first && update->siot_m_route_table_update.size() == 0 && update->siot_m_link_table_update.size() == 0 )
		{
#ifdef SA_DEBUG
			SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it1;
			it1 = update;
			++it1;
			for ( ; it1 != mesh_routing_data_updates.end(); ++it1 )
				ZEPTO_DEBUG_ASSERT( it1->device_id != update->device_id );
#endif // SA_DEBUG
			continue;
		}
		if ( !update->in_progress ) // potential candidate
		{
			bool skip = false;
			for ( it = mesh_routing_data_updates.begin(); it != mesh_routing_data_updates.end(); ++it )
				if ( it->device_id == update->device_id && it->in_progress )
				{
					skip = true;
					break;
				}
			if ( skip ) continue;
			update->in_progress = true;
			return SIOT_MESH_AT_ROOT_RET_OK;
		}
	}

	return SIOT_MESH_AT_ROOT_RET_NO_READY_UPDATES;
}
#endif // 0

uint8_t siot_mesh_at_root_get_next_update( SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR& update )
{//mesh_routing_data_updates_in_progress;planned_updates
	// TODO: here we assume that is_last is set correctly (and should be updated at time of item adding/removal). Think whether this assumption is practically good (seems to be, indeed...).
	if ( planned_updates.begin() == planned_updates.end() )
		return SIOT_MESH_AT_ROOT_RET_NO_UPDATES;

	SIOT_MESH_ALL_ROUTING_DATA_UPDATE_INITIATORS_ITERATOR it_planned;
	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it_initiated;

	for ( it_planned=planned_updates.begin(); it_planned!=planned_updates.end(); ++it_planned )
	{
		uint16_t device_id = it_planned->device_id;
		bool in_progress = false;
		for ( it_initiated=mesh_routing_data_updates_in_progress.begin(); it_initiated!=mesh_routing_data_updates_in_progress.end(); ++it_initiated )
			if ( it_initiated->device_id == it_planned->device_id )
			{
				in_progress = true;
				break;
			}
		if ( in_progress )
			continue;

		SIOT_MESH_ROUTING_DATA_ITERATOR dev_data;
		uint8_t ret_code = siot_mesh_at_root_get_device_data( device_id, dev_data );
		ZEPTO_DEBUG_ASSERT( ret_code == SIOT_MESH_AT_ROOT_RET_OK );

		SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE update_in_progress;
		if ( it_planned->enforce_full_update )
			siot_mesh_at_root_build_full_update( &(*dev_data), &update_in_progress );
		else
			siot_mesh_at_root_get_diff_as_update( &(*dev_data), &update_in_progress );

		unsigned int total_cnt = update_in_progress.siot_m_link_table_update.size() + update_in_progress.siot_m_route_table_update.size();
		if ( total_cnt == 0 )
			continue;

		mesh_routing_data_updates_in_progress.push_back( update_in_progress );
		update = (mesh_routing_data_updates_in_progress.end());
		--update;
		planned_updates.erase( it_planned );

		return SIOT_MESH_AT_ROOT_RET_OK;
	}

	return SIOT_MESH_AT_ROOT_RET_NO_READY_UPDATES;
}

void siot_mesh_at_root_update_to_packet( MEMORY_HANDLE mem_h, SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR update )
{
	uint16_t i;
	uint16_t header;
	uint16_t more;
//	ZEPTO_DEBUG_ASSERT( update->in_progress );

	// TEMPORARY CODE: add ccp staff
	zepto_write_uint8( mem_h, 0x5 ); // first, control
	zepto_write_uint8( mem_h, 0x5 ); // SACCP_PHY_AND_ROUTING_DATA

	// FLAGS: Encoded uint16; bit[0]: DISCARD-RT-FIRST, bit[1]: UPDATE-MAX-TTL flag, bit[2]: UPDATE-FORWARD-TO-SANTA-DELAY flag, bit[3]: UPDATE-MAX-NODE-RANDOM-DELAY flag, bits[4..]: reserved (MUST be zeros)
	uint16_t flags = update->clear_tables_first ? 1 : 0;
	// TODO: fill other bits: flags |= ...
	zepto_parser_encode_and_append_uint16( mem_h, flags );

	// here we should add initial checksum
	if ( !update->clear_tables_first )
	{
		uint16_t ini_checkksum = calculate_table_checksum( &(mesh_routing_data[update->device_id]) );
		zepto_write_uint8( mem_h, (uint8_t)ini_checkksum );
		zepto_write_uint8( mem_h, (uint8_t)(ini_checkksum >> 8) );
	}

	unsigned int total_cnt = update->siot_m_link_table_update.size() + update->siot_m_route_table_update.size();
	ZEPTO_DEBUG_ASSERT( total_cnt );
	unsigned int cnt = 0;

	for ( i=0; i<update->siot_m_route_table_update.size(); i++ )
	{
		cnt++;
		more = cnt == total_cnt ? 0 : 1;
		if ( update->siot_m_route_table_update[i].to_add )
		{
			// | ADD-OR-MODIFY-ROUTE-ENTRY-AND-LINK-ID | TARGET-ID |
			header = more | ( ADD_OR_MODIFY_ROUTE_ENTRY << 1 ) | ( update->siot_m_route_table_update[i].route.LINK_ID << 3 );
			zepto_parser_encode_and_append_uint16( mem_h, header );
			zepto_parser_encode_and_append_uint16( mem_h, update->siot_m_route_table_update[i].route.TARGET_ID );
		}
		else
		{
			// | DELETE-ROUTE-ENTRY-AND-TARGET-ID |
			header = more | ( DELETE_ROUTE_ENTRY << 1 ) | ( update->siot_m_route_table_update[i].route.TARGET_ID << 3 );
			zepto_parser_encode_and_append_uint16( mem_h, header );
		}
	}

	for ( i=0; i<update->siot_m_link_table_update.size(); i++ )
	{
		cnt++;
		more = cnt == total_cnt ? 0 : 1;
		if ( update->siot_m_link_table_update[i].to_add )
		{
			// | ADD-OR-MODIFY-LINK-ENTRY-AND-LINK-ID | BUS-ID | NEXT-HOP-ACKS-AND-INTRA-BUS-ID-PLUS-1 | OPTIONAL-LINK-DELAY-UNIT | OPTIONAL-LINK-DELAY | OPTIONAL-LINK-DELAY-ERROR |
			header = more | ( ADD_OR_MODIFY_LINK_ENTRY << 1 ) | ( update->siot_m_link_table_update[i].link.LINK_ID << 4 );
			zepto_parser_encode_and_append_uint16( mem_h, header );
			zepto_parser_encode_and_append_uint16( mem_h, update->siot_m_link_table_update[i].link.BUS_ID );
			// TODO: ZEPTO_DEBUG_ASSERT( update->siot_m_link_table_update[i].link.NEXT_HOP < "max. dev. id" )
			zepto_parser_encode_and_append_uint16( mem_h, update->siot_m_link_table_update[i].link.NEXT_HOP );
			zepto_parser_encode_and_append_uint32( mem_h, 0 ); // intra-bus id
		}
		else
		{
			// | DELETE-LINK-ENTRY-AND-LINK-ID |
			header = more | ( DELETE_LINK_ENTRY << 1 ) | ( update->siot_m_link_table_update[i].link.LINK_ID << 3 );
			zepto_parser_encode_and_append_uint16( mem_h, header );
		}
	}

	// add resulting checksum
	SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA dev_data_copy;
	siot_mesh_at_root_get_confirmed_table_state_after_applying_update( &(*update), &dev_data_copy );
	uint16_t resulting_checkksum = calculate_table_checksum( &dev_data_copy );

	zepto_write_uint8( mem_h, (uint8_t)resulting_checkksum );
	zepto_write_uint8( mem_h, (uint8_t)(resulting_checkksum >> 8 ) );
}

uint8_t siot_mesh_at_root_load_update_to_packet( MEMORY_HANDLE mem_h, uint16_t* recipient )
{
	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR update;
	uint8_t ret_code;
	ret_code = siot_mesh_at_root_get_next_update( update );
	if ( ret_code != SIOT_MESH_AT_ROOT_RET_OK )
		return ret_code;
	siot_mesh_at_root_update_to_packet( mem_h, update );
	*recipient = update->device_id;
	ZEPTO_DEBUG_ASSERT( *recipient > 0 );
	return SIOT_MESH_AT_ROOT_RET_OK;
}

uint8_t siot_mesh_at_root_update_done( uint16_t device_id )
{
	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it;
	for ( it = mesh_routing_data_updates_in_progress.begin(); it != mesh_routing_data_updates_in_progress.end(); ++it )
//		if ( it->device_id == device_id && it->in_progress ) // note: while current update was 'in progress', other updates of the same device could be added to the collection; and they could not be merged with one already being in progress; thus they will stay with status !in_progress
		if ( it->device_id == device_id )
		{
#if 0
#ifdef SA_DEBUG
			int cnt = 0;
			SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it1;
			for ( it1 = mesh_routing_data_updates.begin(); it1 != mesh_routing_data_updates.end(); ++it1 )
				if ( it1->device_id == device_id && it1->in_progress )
					cnt++;
			ZEPTO_DEBUG_ASSERT( cnt == 1 ); // only a single update to the same device can be in progress
#endif // SA_DEBUG
#endif // 0

			// apply update to the respective local copy
			siot_mesh_at_root_apply_update_to_local_copy( &(*it) );

			mesh_routing_data_updates_in_progress.erase( it );
			return SIOT_MESH_AT_ROOT_RET_OK;
		}
	return SIOT_MESH_AT_ROOT_RET_FAILED;
}

uint8_t siot_mesh_at_root_update_failed( uint16_t device_id )
{
	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it;
	for ( it = mesh_routing_data_updates_in_progress.begin(); it != mesh_routing_data_updates_in_progress.end(); ++it )
//		if ( it->device_id == device_id && it->in_progress ) // note: while current update was 'in progress', other updates of the same device could be added to the collection; and they could not be merged with one already being in progress; thus they will stay with status !in_progress
		if ( it->device_id == device_id ) // note: while current update was 'in progress', other updates of the same device could be added to the collection; and they could not be merged with one already being in progress; thus they will stay with status !in_progress
		{
#if 0
#ifdef SA_DEBUG
			int cnt = 0;
			SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it1;
			for ( it1 = mesh_routing_data_updates.begin(); it1 != mesh_routing_data_updates.end(); ++it1 )
				if ( it1->device_id == device_id && it1->in_progress )
					cnt++;
			ZEPTO_DEBUG_ASSERT( cnt == 1 ); // only a single update to the same device can be in progress
#endif // SA_DEBUG
#endif // 0

			// get a "full" update
			siot_mesh_at_root_add_route_update_requiring_full_erase( &(*it) );
			mesh_routing_data_updates_in_progress.erase( it );
//			it->in_progress = false;

			return SIOT_MESH_AT_ROOT_RET_OK;
		}
	return SIOT_MESH_AT_ROOT_RET_FAILED;
}

void siot_mesh_at_root_remove_link_to_target_route_error_reported( uint16_t reporting_id, uint16_t failed_hop_id, uint16_t failed_target_id, uint8_t from_root )
{
dbg_siot_mesh_at_root_validate_all_device_tables();

	uint16_t i, j, k, m;
	uint16_t failed_index = (uint16_t)(-1); 
	uint16_t root_index = (uint16_t)(-1); 
	uint16_t idx = root_index;
	vector<int> affected_list1, affected_list2;
	vector<int>& prev_list = affected_list1;
	vector<int>& next_list = affected_list2;
	SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE_INITIATOR update;

	// 1. find index of root data
	for ( i=0; i<mesh_routing_data.size(); i++)
		if ( mesh_routing_data[i].device_id == 0 )
		{
			root_index = i;
			break;
		}
	ZEPTO_DEBUG_ASSERT( root_index != (uint16_t)(-1) );

	if ( from_root )
	{
		if ( failed_hop_id == SIOT_MESH_NEXT_HOP_UNDEFINED ) // reporting hop failed to reach target (note: target might itself happen to be a retransmitter, so we must consider this case as well)
			failed_hop_id = failed_target_id;
	}
	else
	{
		failed_hop_id = reporting_id;
	}

	// 2. get indexes of all lost devices (that with 'failed_hop_id' and all devices in its table
	vector<uint16_t> non_reached_device_ids;
	non_reached_device_ids.push_back( failed_hop_id );
	for ( i=0; i<mesh_routing_data.size(); i++)
		if ( mesh_routing_data[i].device_id == failed_hop_id ) // find a respective entry
		{
			for ( j=0; j<mesh_routing_data[i].siot_m_route_table_planned.size(); j++ )
			{
				if ( mesh_routing_data[i].siot_m_route_table_planned[j].TARGET_ID != 0 )
				non_reached_device_ids.push_back( mesh_routing_data[i].siot_m_route_table_planned[j].TARGET_ID );
			}
			failed_index = i;
			break;
		}

	// 3. starting from ROOT, calculate and add updates. Note: we can only hope that it's a single chain of devices
	prev_list.push_back( root_index );
	do
	{
		next_list.clear();
		for ( idx=0; idx<prev_list.size(); idx++ ) // go through all devices in the list (hopefully, there is a single one there)
		{
				
			SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA& updated_dev_data = mesh_routing_data[prev_list[idx]];
			siot_mesh_at_root_init_route_update_data_with_many_affected_devices( &update, updated_dev_data.device_id, non_reached_device_ids );

			int link_table_size = updated_dev_data.siot_m_link_table_planned.size();
			int* used_links = new int [ link_table_size ];
			ZEPTO_MEMSET( used_links, 0, sizeof( int ) * link_table_size );

			// which links are in use now?
			for ( k=0; k<updated_dev_data.siot_m_route_table_planned.size(); k++ ) // go along the route table of an updated device
				for ( m=0; m<link_table_size; m++ ) // go along the link table of an updated device
					if ( updated_dev_data.siot_m_route_table_planned[k].LINK_ID == updated_dev_data.siot_m_link_table_planned[m].LINK_ID )
						(used_links[m])++;

#ifdef SA_DEBUG
			for ( m=0; m<link_table_size; m++ ) // go along the link table of an updated device
				ZEPTO_DEBUG_ASSERT( used_links[m] != 0 ); // all links are in use (sanity check); TODO: think about replacing ASSERT by a counter and respective fixing the issue
#endif // SA_DEBUG

			for ( k=0; k<updated_dev_data.siot_m_route_table_planned.size(); k++ ) // go along the route table of an updated device
			{
				bool killed = false;
				uint16_t link_id = updated_dev_data.siot_m_route_table_planned[k].LINK_ID;
				uint16_t lnk_idx;

				for ( j=0; j<non_reached_device_ids.size(); j++ ) // go along list of affected devices...
					if ( updated_dev_data.siot_m_route_table_planned[k].TARGET_ID == non_reached_device_ids[j] ) // ...if one of them corresponds to an entry
					{
						killed = true;
						siot_mesh_at_root_remove_route( &(updated_dev_data.siot_m_route_table_planned), non_reached_device_ids[j] );
						if ( updated_dev_data.device_id == 0 )
							siot_mesh_at_root_remove_route( &(updated_dev_data.siot_m_route_table_confirmed), non_reached_device_ids[j] );
/*						SIOT_MESH_ROUTE_UPDATE route_update;
						route_update.to_add = false;
						route_update.route.TARGET_ID = non_reached_device_ids[j];
						update.siot_m_route_table_update_planned.push_back( route_update );*/

						for ( lnk_idx=0; lnk_idx<link_table_size; lnk_idx++ )
							if ( updated_dev_data.siot_m_link_table_planned[lnk_idx].LINK_ID == link_id )
							{
								uint16_t next_hop_id = updated_dev_data.siot_m_link_table_planned[lnk_idx].NEXT_HOP;
								bool found2 = false;
								unsigned int rt;
								for ( rt=0; rt<next_list.size(); rt++ )
									if ( next_hop_id == next_list[rt] ) // already in the list
									{
										found2 = true;
										break;
									}
								if ( !found2 && next_hop_id != failed_hop_id ) // anyway we cannot reach it
									next_list.push_back( next_hop_id );
								( used_links[ lnk_idx ] ) --;
								break;
							}

						break;
					}
			}

#ifdef SA_DEBUG
			for ( m=0; m<link_table_size; m++ ) // go along the link table of an updated device
				ZEPTO_DEBUG_ASSERT( used_links[m] >= 0 ); // (sanity check)
#endif // SA_DEBUG

			// determine which links are now out of use
			for ( k=0; k<link_table_size; k++ )
			{
				if ( used_links[k] == 0 )
				{
/*					SIOT_MESH_LINK_UPDATE link_update;
					link_update.to_add = false;
					link_update.link.LINK_ID = updated_dev_data.siot_m_link_table_planned[k].LINK_ID;
					update.siot_m_link_table_update_planned.push_back( link_update );*/
					uint16_t link_to_remove = updated_dev_data.siot_m_link_table_planned[k].LINK_ID;
					siot_mesh_at_root_remove_link( &(updated_dev_data.siot_m_link_table_planned), link_to_remove );
					if ( updated_dev_data.device_id == 0 )
						siot_mesh_at_root_remove_link( &(updated_dev_data.siot_m_link_table_confirmed), link_to_remove );
				}
			}
			if ( used_links != NULL ) delete [] used_links;

			if ( updated_dev_data.device_id != 0 )
				siot_mesh_at_root_add_planned_route_update( &update );
/*			// NOTE: above procedures do not guarantee that the 'update' is not empty
			// TODO: ensure that empty 'update' is legitimate in all cases
			if ( update.siot_m_link_table_update.size() || update.siot_m_route_table_update.size() )
			{
				if ( update.device_id == 0 ) // root; apply right now
					siot_mesh_at_root_apply_update_to_local_copy( &update );
				else // not a root; add to the list
				{
					SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it_backward;
					it_backward=mesh_routing_data_updates.end();
					while ( it_backward != mesh_routing_data_updates.begin() )
					{
						--it_backward;
						if ( it_backward->device_id == update.device_id && it_backward->clear_tables_first && it_backward->siot_m_route_table_update.size() == 0 && it_backward->siot_m_link_table_update.size() == 0  )
						{
							update.clear_tables_first = true;
							mesh_routing_data_updates.erase( it_backward );
							break;
						}
					}
					mesh_routing_data_updates.push_back( update );
				}
			}*/
		}

		vector<int>& tmp = prev_list;
		prev_list = next_list;
		next_list = tmp;

		// note: by construction prev_list contains IDs, not indexes; do conversion
		for ( idx=0; idx<prev_list.size(); idx++ )
		{
			bool found = false;
			for ( k=0; k<mesh_routing_data.size(); k++ )
				if ( mesh_routing_data[k].device_id == prev_list[idx] )
				{
					prev_list[idx] = k;
					found = true;
					break;
				}
			ZEPTO_DEBUG_ASSERT( found ); // TODO: might it be a legitimate case?
		}
	}
	while ( prev_list.size() );

	if ( from_root )
	{
		// 4. add updates invalidating routing data of affected devices
		for ( j=0; j<non_reached_device_ids.size(); j++ )
		{
			if ( non_reached_device_ids[j] != 0 )
			{
				siot_mesh_at_root_init_route_update_data( &update, non_reached_device_ids[j], non_reached_device_ids[j] );
				siot_mesh_at_root_add_planned_route_update( &update );
			}
			SIOT_MESH_ROUTING_DATA_ITERATOR rd_it;
			siot_mesh_at_root_get_device_data( non_reached_device_ids[j], rd_it );
			rd_it->siot_m_route_table_planned.clear();
			rd_it->siot_m_link_table_planned.clear();
			if ( non_reached_device_ids[j] == 0 )
			{
				rd_it->siot_m_route_table_confirmed.clear();
				rd_it->siot_m_link_table_confirmed.clear();
			}
/*			SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it_backward;
			it_backward=mesh_routing_data_updates.end();
			while ( it_backward != mesh_routing_data_updates.begin() )
			{
				--it_backward;
				if ( it_backward->device_id == update.device_id && it_backward->clear_tables_first && it_backward->siot_m_route_table_update.size() == 0 && it_backward->siot_m_link_table_update.size() == 0  )
				{
					update.clear_tables_first = true;
					mesh_routing_data_updates.erase( it_backward );
					break;
				}
			}
			mesh_routing_data_updates.push_back( update );*/
		}
	}

dbg_siot_mesh_at_root_validate_all_device_tables();
}

bool siot_mesh_at_root_is_route_under_update( uint16_t device_id )
{
	unsigned int i;

	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it;
	for ( it = mesh_routing_data_updates_in_progress.begin(); it != mesh_routing_data_updates_in_progress.end(); ++it ) 
		for ( i=0; i<it->affected_routes.size(); i++ )
			if ( it->affected_routes[i] == device_id )
				return true;

	SIOT_MESH_ALL_ROUTING_DATA_UPDATE_INITIATORS_ITERATOR it1;
	for ( it1 = planned_updates.begin(); it1 != planned_updates.end(); ++it1 ) 
		for ( i=0; i<it1->affected_routes.size(); i++ )
			if ( it1->affected_routes[i] == device_id )
				return true;

	return false;
}

uint8_t siot_mesh_at_root_target_to_link_id( uint16_t target_id, uint16_t* link_id )
{
	uint16_t i, j;

	for ( i=0; i<mesh_routing_data.size(); i++)
		if ( mesh_routing_data[i].device_id == 0 )
			for ( j=0; j<mesh_routing_data[i].siot_m_route_table_confirmed.size(); j++ )
				if ( mesh_routing_data[i].siot_m_route_table_confirmed[j].TARGET_ID == target_id )
				{
					*link_id = mesh_routing_data[i].siot_m_route_table_confirmed[j].LINK_ID;
					bool under_construction = siot_mesh_at_root_is_route_under_update( target_id );
					return under_construction ? SIOT_MESH_RET_ERROR_ROUTE_UNDER_CONSTRUCTION : SIOT_MESH_RET_OK;
				}
	return SIOT_MESH_RET_ERROR_NOT_FOUND;
}

void siot_mesh_at_root_remove_link_to_target_no_ack_from_immediate_hop( uint16_t target_id, uint16_t next_hop_id )
{
/*	uint16_t link_id;
	SIOT_MESH_LINK link;
	siot_mesh_at_root_target_to_link_id( target_id, &link_id );
	siot_mesh_get_link( link_id, &link );*/

	siot_mesh_at_root_remove_link_to_target_route_error_reported( 0, next_hop_id, target_id, true );
}


#endif // NEW_UPDATE_PROCESSING