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
#include "commstack_siot_mesh_support.h"

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

#include <xkeycheck.h>
#include <simpleiot/siot_m_protocol.h>
#include <vector>
#include <list>
using namespace std;

typedef vector< SIOT_MESH_ROUTE > SIOT_M_ROUTE_TABLE_TYPE;
typedef vector< SIOT_MESH_LINK > SIOT_M_LINK_TABLE_TYPE;

typedef struct _SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA
{
	uint16_t device_id;
	SIOT_M_ROUTE_TABLE_TYPE siot_m_route_table;
	SIOT_M_LINK_TABLE_TYPE siot_m_link_table;
} SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA;

typedef vector< SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA > SIOT_MESH_ROUTING_DATA;
typedef vector< SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA >::iterator SIOT_MESH_ROUTING_DATA_ITERATOR;

SIOT_MESH_ROUTING_DATA mesh_routing_data;

typedef struct _SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA_UPDATE
{
	uint16_t device_id;
	uint16_t prev_device_id; // previous device on the way; as soon as a current device is successfully updated previous device has a chance to become 'last' (note: a devive can be 'prev' for more than a single device)
	uint16_t next_device_id; // has a valid value, if !is_last
	bool is_last; // between operations this value must be up-to-date
	bool in_progress; // true in the time range between the request to update has been sent, and confirmation is received; before this period requests to the same device can be merged; after the end of this period the request is removed from the set of outstanding requests
	SIOT_M_ROUTE_TABLE_TYPE siot_m_route_table;
	SIOT_M_LINK_TABLE_TYPE siot_m_link_table;
} SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE;

typedef list< SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE > SIOT_MESH_ROUTING_DATA_UPDATES;
typedef list< SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE >::iterator SIOT_MESH_ROUTING_DATA_UPDATES_ITERATOR;

SIOT_MESH_ROUTING_DATA_UPDATES mesh_routing_data_updates;

#define SIOT_MESH_AT_ROOT_RET_OK 0
#define SIOT_MESH_AT_ROOT_RET_FAILED 1
#define SIOT_MESH_AT_ROOT_RET_ALREADY_EXISTS 2
#define SIOT_MESH_AT_ROOT_RET_NOT_FOUND 3
#define SIOT_MESH_AT_ROOT_RET_NO_UPDATES 4
#define SIOT_MESH_AT_ROOT_RET_NO_READY_UPDATES 5

///////////////////   Basic calls: device list  //////////////////////

uint8_t siot_mesh_at_root_add_device( uint16_t device_id )
{
	uint16_t i;
	for ( i=0; i<mesh_routing_data.size(); i++)
		if ( mesh_routing_data[i].device_id == device_id )
			return SIOT_MESH_AT_ROOT_RET_ALREADY_EXISTS;
	SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA dev_data;
	dev_data.device_id = device_id;
	mesh_routing_data.push_back( dev_data );
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
	return SIOT_MESH_AT_ROOT_RET_NOT_FOUND;
}

uint8_t siot_mesh_at_root_get_device_data( uint16_t device_id, SIOT_MESH_ROUTING_DATA_ITERATOR& it )
{
	for ( it = mesh_routing_data.begin(); it != mesh_routing_data.end(); ++it )
		if ( it->device_id == device_id )
			return SIOT_MESH_AT_ROOT_RET_OK;

	return SIOT_MESH_AT_ROOT_RET_NOT_FOUND;
}

void siot_mesh_at_root_siot_mesh_at_root_init_route_update_data( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update, uint16_t device_id, uint16_t prev_device_id )
{
	update->siot_m_link_table.clear();
	update->siot_m_route_table.clear();
	update->device_id = device_id;
	update->in_progress = false;
	update->is_last = false;
	update->prev_device_id = prev_device_id;
	update->next_device_id = 0;
}

uint8_t siot_mesh_at_root_get_list_of_updated_devices( SIOT_MESH_ROUTING_DATA_UPDATES& update_list, uint16_t id_target, uint16_t id_from, uint16_t id_to /*more data may be required*/ )
{
	SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE update;
	SIOT_MESH_ROUTING_DATA_ITERATOR rd_it;
	SIOT_MESH_ROUTE route;
	SIOT_MESH_LINK link;
	route.TARGET_ID = id_target;
	uint16_t dev_id = 0, prev_dev_id;
	uint16_t i, j;
	update_list.clear();
	ZEPTO_DEBUG_ASSERT( mesh_routing_data.size() && mesh_routing_data[0].device_id == 0 );
	// get all chain down starting from ROOT...
	do
	{
		prev_dev_id = dev_id;
		siot_mesh_at_root_siot_mesh_at_root_init_route_update_data( &update, dev_id, prev_dev_id );
		siot_mesh_at_root_get_device_data( dev_id, rd_it );
		for ( i=0; i<rd_it->siot_m_route_table.size(); i++ )
			if ( rd_it->siot_m_route_table[i].TARGET_ID == id_from )
			{
				route.LINK_ID = rd_it->siot_m_route_table[i].LINK_ID;
				update.siot_m_route_table.push_back( route );
				for ( j=0; j<rd_it->siot_m_link_table.size(); j++ )
					if ( rd_it->siot_m_link_table[j].LINK_ID == rd_it->siot_m_route_table[i].LINK_ID )
					{
						dev_id = rd_it->siot_m_link_table[i].NEXT_HOP;
						break;
					}
				break;
			}
		update_list.push_back( update );
	}
	while ( dev_id != id_from );

	// the last device (as well as a target device) should receive LINK record; and we need to somehow assign a LINK_ID on a target device

	siot_mesh_at_root_get_device_data( id_from, rd_it );
	link.LINK_ID = 0;
	for ( j=0; j<rd_it->siot_m_link_table.size(); j++ )
		if ( link.LINK_ID != rd_it->siot_m_link_table[j].LINK_ID ) // we exploit here canonicity of the link table
			break;
		else
			(link.LINK_ID)++;
	link.NEXT_HOP = id_target;
	// TODO: (!!!) MISSING INFORMATION: link other values
	route.LINK_ID = link.LINK_ID;
	siot_mesh_at_root_siot_mesh_at_root_init_route_update_data( &update, id_from, dev_id );
	update.siot_m_link_table.push_back( link );
	update.siot_m_route_table.push_back( route);
	update_list.push_back( update );



	return 0;
}


///////////////////   Basic calls: device list  //////////////////////

uint8_t siot_mesh_at_root_get_next_update( SIOT_MESH_ROUTING_DATA_UPDATES_ITERATOR& it )
{
	if ( mesh_routing_data_updates.begin() == mesh_routing_data_updates.end() )
		return SIOT_MESH_AT_ROOT_RET_NO_UPDATES;
	for ( it = mesh_routing_data_updates.begin(); it != mesh_routing_data_updates.end(); ++it )
		if ( it->is_last && (!it->in_progress) )
			return SIOT_MESH_AT_ROOT_RET_OK;
	return SIOT_MESH_AT_ROOT_RET_NO_READY_UPDATES;
}

uint8_t siot_mesh_at_root_update_done( uint16_t device_id )
{
	// basically we need to do three main things: to apply the update to the local copy of routing table, to update (and check) remaining items, and to remove the item
	SIOT_MESH_ROUTING_DATA_UPDATES_ITERATOR it, it1, it2;
	for ( it = mesh_routing_data_updates.begin(); it != mesh_routing_data_updates.end(); ++it )
		if ( it->device_id == device_id && it->in_progress ) // note: while current update was 'in progress', other updates of the same device could be added to the collection; and they could not be merged with one already being in progress; thus they will stay with status !in_progress
		{
#ifdef SA_DEBUG
			uint16_t cnt = 0;
			for ( it1 = mesh_routing_data_updates.begin(); it1 != mesh_routing_data_updates.end(); ++it1 )
				if ( it->device_id == it1->device_id )
					cnt++;
			ZEPTO_DEBUG_ASSERT( cnt == 1 ); // only a single update to the same device can be in progress
#endif // SA_DEBUG
			// TODO: apply update to the respective local copy

			// put collection of updates in a consistent state: get a list of updates for devices 'previous' to current (TODO: can it be more than a single one?)
			if ( it->device_id != 0 )
				for ( it1 = mesh_routing_data_updates.begin(); it1 != mesh_routing_data_updates.end(); ++it1 )
					if ( it1->device_id == it->prev_device_id )
					{
						ZEPTO_DEBUG_ASSERT( !it1->in_progress );
						ZEPTO_DEBUG_ASSERT( !it1->is_last );
					}
			mesh_routing_data_updates.erase( it );
			return SIOT_MESH_AT_ROOT_RET_OK;
		}
	return SIOT_MESH_AT_ROOT_RET_FAILED;
}

///////////////////   Basic calls: requests to route table of the ROOT   //////////////////////
extern "C"{
uint8_t siot_mesh_at_root_target_to_link_id( uint16_t target_id, uint16_t* link_id )
{
	uint16_t i, j;
	if ( mesh_routing_data.size() == 0 )
		return SIOT_MESH_RET_ERROR_NOT_FOUND; 

	for ( i=0; i<mesh_routing_data.size(); i++)
		if ( mesh_routing_data[i].device_id == 0 )
			for ( j=0; j<mesh_routing_data[i].siot_m_route_table.size(); j++ )
				if ( mesh_routing_data[i].siot_m_route_table[j].TARGET_ID == target_id )
				{
					*link_id = mesh_routing_data[i].siot_m_route_table[j].LINK_ID;
					return SIOT_MESH_RET_OK;
				}
	return SIOT_MESH_RET_ERROR_NOT_FOUND;
}
}
///////////////////   Basic calls: route table  //////////////////////

