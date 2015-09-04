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
#ifdef fprintf
#undef fprintf
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
//	uint16_t next_device_id; // has a valid value, if !is_last
	bool check_path_first; // set to true for a device that has no route to yet (note: this status is set when update is added and MAY change only when root routing table is updated)
	bool is_last; // between operations this value must be up-to-date
	bool in_progress; // true in the time range between the request to update has been sent, and confirmation is received; before this period requests to the same device can be merged; after the end of this period the request is removed from the set of outstanding requests
	SIOT_M_ROUTE_TABLE_TYPE siot_m_route_table_update;
	SIOT_M_LINK_TABLE_TYPE siot_m_link_table_update;
#ifdef SA_DEBUG
	uint16_t resulting_checksum;
//	SIOT_M_ROUTE_TABLE_TYPE siot_m_route_table_resulting;
//	SIOT_M_LINK_TABLE_TYPE siot_m_link_table_resulting;
#endif // SA_DEBUG
} SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE;

typedef vector< SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE > SIOT_MESH_ROUTING_DATA_UPDATES;
typedef list< SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE > SIOT_MESH_ALL_ROUTING_DATA_UPDATES;
typedef list< SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE >::iterator SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR;

SIOT_MESH_ALL_ROUTING_DATA_UPDATES mesh_routing_data_updates;

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


///////////////////   Basic calls: device list  //////////////////////

void siot_mesh_at_root_siot_mesh_at_root_init_route_update_data( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update, uint16_t device_id, uint16_t prev_device_id )
{
	update->siot_m_link_table_update.clear();
	update->siot_m_route_table_update.clear();
	update->device_id = device_id;
	update->in_progress = false;
	update->is_last = false;
	update->prev_device_id = prev_device_id;
//	update->next_device_id = 0;
	update->check_path_first = false;
}

uint8_t siot_mesh_at_root_get_list_of_updated_devices( SIOT_MESH_ROUTING_DATA_UPDATES& update_list, uint16_t id_target, uint16_t id_from, uint16_t id_to /*more data may be required*/ )
{
	SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE update;
	SIOT_MESH_ROUTING_DATA_ITERATOR rd_it;
	SIOT_MESH_ROUTE route;
	SIOT_MESH_LINK link;
	route.TARGET_ID = id_target;
	uint16_t dev_id, prev_dev_id;
	uint16_t i, j;
	update_list.clear();
	ZEPTO_DEBUG_ASSERT( mesh_routing_data.size() && mesh_routing_data[0].device_id == 0 );

	// 1. Collect necessary changes

	// adding items for all devices from ROOT to the predecessor of id_from
	dev_id = 0;
	prev_dev_id = 0;;
	while ( dev_id != id_from )
	{
#ifdef SA_DEBUG
		bool next_found = false;
#endif // SA_DEBUG
		siot_mesh_at_root_siot_mesh_at_root_init_route_update_data( &update, dev_id, prev_dev_id );
		siot_mesh_at_root_get_device_data( dev_id, rd_it );
		for ( i=0; i<rd_it->siot_m_route_table.size(); i++ )
			if ( rd_it->siot_m_route_table[i].TARGET_ID == id_from )
			{
				route.LINK_ID = rd_it->siot_m_route_table[i].LINK_ID;
				update.siot_m_route_table_update.push_back( route );
				for ( j=0; j<rd_it->siot_m_link_table.size(); j++ )
					if ( rd_it->siot_m_link_table[j].LINK_ID == rd_it->siot_m_route_table[i].LINK_ID )
					{
						prev_dev_id = dev_id;
						dev_id = rd_it->siot_m_link_table[i].NEXT_HOP;
#ifdef SA_DEBUG
						next_found = true;
#endif // SA_DEBUG
						break;
					}
				break;
			}
		update_list.push_back( update );
#ifdef SA_DEBUG
		ZEPTO_DEBUG_ASSERT( next_found );
		ZEPTO_DEBUG_ASSERT( prev_dev_id != dev_id );
#endif // SA_DEBUG
	}

	// the last device (as well as a target device) should receive LINK record; and we need to somehow assign a LINK_ID on a target device

	// adding item for the last retransmitter
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
	update.siot_m_link_table_update.push_back( link );
	update.siot_m_route_table_update.push_back( route);
	update.is_last = true; // within current collection
	update_list.push_back( update );

	// additing item for the target device
	// TODO: (!!!) INFORMATION IS MISSING: physics of retransmission
	link.LINK_ID = 0;
	link.NEXT_HOP = id_to;
	route.LINK_ID = link.LINK_ID;
	route.TARGET_ID = 0;
	siot_mesh_at_root_siot_mesh_at_root_init_route_update_data( &update, id_target, id_from );
	update.is_last = true; // within current collection
	update.check_path_first = true;
	update_list.push_back( update );

	return SIOT_MESH_AT_ROOT_RET_OK;
}

uint8_t siot_mesh_at_root_merge_route_and_link_table_updates( SIOT_M_ROUTE_TABLE_TYPE& present_route_changes, SIOT_M_ROUTE_TABLE_TYPE& new_route_changes, SIOT_M_LINK_TABLE_TYPE& present_link_changes, SIOT_M_LINK_TABLE_TYPE& new_link_changes )
{
	// It looks like at this point changes to route table and to link table have no interdependences; we process them separately assuming that newer cancels older
	// TODO: think about sanity check right before applying changes
	uint16_t i, j;
	for ( i=0; i<new_route_changes.size(); i++ )
	{
		bool applied = false;
		for ( j=0; j<present_route_changes.size(); j++ )
			if ( present_route_changes[j].TARGET_ID == new_route_changes[i].TARGET_ID )
			{
				present_route_changes[j].LINK_ID = new_route_changes[i].LINK_ID;
				applied = true;
				break;
			}
		if ( !applied ) // something new
		{
			present_route_changes.push_back( new_route_changes[i] );
		}
	}

	for ( i=0; i<new_link_changes.size(); i++ )
	{
		bool applied = false;
		for ( j=0; j<present_link_changes.size(); j++ )
			if ( present_link_changes[j].LINK_ID == new_link_changes[i].LINK_ID )
			{
				present_link_changes[j] = new_link_changes[i];
				applied = true;
				break;
			}
		if ( !applied ) // something new
		{
			present_link_changes.push_back( new_link_changes[i] );
		}
	}

	return SIOT_MESH_AT_ROOT_RET_OK;
}
/*
uint8_t siot_mesh_at_root_merge_link_table_updates( )
{
	// TODO: implement
	return SIOT_MESH_AT_ROOT_RET_OK;
}
*/
uint8_t siot_mesh_at_root_add_updates( SIOT_MESH_ROUTING_DATA_UPDATES& update_list )
{
	// we start from the end of the list and add updates one by one;
	// if an update to the same device is already in the set of updates and is not in progress, we merge updates
	uint16_t i;
	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it;
	for ( i=update_list.size(); i; i-- )
	{
		bool added = false;
		for ( it = mesh_routing_data_updates.begin(); it != mesh_routing_data_updates.end(); ++it )
		{
	//		SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE& cu = update_list[i-1];
			if ( update_list[i].device_id == it->device_id )
			{
				if ( ! it->in_progress ) // we can merge
				{
//					siot_mesh_at_root_merge_route_table_updates( it->siot_m_route_table_update, update_list[i].siot_m_route_table_update );
//					siot_mesh_at_root_merge_link_table_updates( it->siot_m_link_table, update_list[i].siot_m_link_table );
					siot_mesh_at_root_merge_route_and_link_table_updates( it->siot_m_route_table_update, update_list[i].siot_m_route_table_update,  it->siot_m_link_table_update, update_list[i].siot_m_link_table_update );
					it->is_last = it->is_last && update_list[i].is_last;
				}
				else
				{
					// is_last flag remains unchanged
					mesh_routing_data_updates.push_back( update_list[i] );
				}
			}
			added = true;
			break;
		}
		if ( !added )
		{
			// is_last flag remains unchanged
			mesh_routing_data_updates.push_back( update_list[i] );
		}
	}

	return SIOT_MESH_AT_ROOT_RET_OK;
}

uint8_t siot_mesh_at_root_get_next_update( SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR& it )
{
	// TODO: here we assume that is_last is set correctly (and should be updated at time of item adding/removal). Think whether this assumption is practically good (seems to be, indeed...).
	if ( mesh_routing_data_updates.begin() == mesh_routing_data_updates.end() )
		return SIOT_MESH_AT_ROOT_RET_NO_UPDATES;
	for ( it = mesh_routing_data_updates.begin(); it != mesh_routing_data_updates.end(); ++it )
		if ( it->is_last && (!it->in_progress) )
			return SIOT_MESH_AT_ROOT_RET_OK;
	return SIOT_MESH_AT_ROOT_RET_NO_READY_UPDATES;
}

uint8_t siot_mesh_at_root_update_done( uint16_t device_id )
{
	// TODO: revize implementation
	
	// basically we need to do three main things: to apply the update to the local copy of routing table, to update (and check) remaining items, and to remove the item
	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it, it1, it2;
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

			// TODO: APPLY UPDATE TO THE RESPECTIVE LOCAL COPY

			// put collection of updates in a consistent state: get a list of updates for devices 'previous' to current (TODO: can it be more than a single one?)
			if ( it->device_id != 0 )
				for ( it1 = mesh_routing_data_updates.begin(); it1 != mesh_routing_data_updates.end(); ++it1 )
					if ( it1 != it && it1->device_id == it->prev_device_id )
					{
						// we have found a predecessor, which has a chance to become 'last', and, therefore, a canditate to be processed next.
						// we need to make sure that it is not a predecessor for any other item
						ZEPTO_DEBUG_ASSERT( !it1->in_progress );
						ZEPTO_DEBUG_ASSERT( !it1->is_last );
						bool is_predecessor = false; // as an assumption
						for ( it2 = mesh_routing_data_updates.begin(); it2 != mesh_routing_data_updates.end(); ++it2 )
							if ( it1 != it2 && it2 != it && it1->device_id == it2->prev_device_id )
							{
								is_predecessor = true;
								break;
							}
						if ( !is_predecessor )
							it1->is_last = true;
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
