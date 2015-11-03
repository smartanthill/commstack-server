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

typedef struct _SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA // used to keep copies of respective tables at devices
{
	uint16_t device_id;
	SIOT_M_ROUTE_TABLE_TYPE siot_m_route_table;
	SIOT_M_LINK_TABLE_TYPE siot_m_link_table;
} SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA;

typedef vector< SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA > SIOT_MESH_ROUTING_DATA;
typedef vector< SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA >::iterator SIOT_MESH_ROUTING_DATA_ITERATOR;

SIOT_MESH_ROUTING_DATA mesh_routing_data; // copies of respective tables at devices

/*#define SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA_UPDATE_TYPE_ADD_OR_MODIFY_LINK 0
#define SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA_UPDATE_TYPE_DELETE_LINK 1
#define SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA_UPDATE_TYPE_ADD_OR_MODIFY_TARGET 2
#define SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA_UPDATE_TYPE_DELETE_TARGET 3*/

typedef struct _SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA_UPDATE
{
	bool to_add; // true, if data is to be added
	// NOTE: in the current implementation updates are either to add or to remove records;
	//       merging records of different types is not yet supported (TODO: support merging)
	//       update internal details is subject to chenge;
	//       Externally exposed is only a (part of a) ccp packet with update data filled which is, in any case, makes sense
	uint16_t device_id;
	uint16_t prev_device_id; // previous device on the way; as soon as a current device is successfully updated previous device has a chance to become 'last' (note: a devive can be 'prev' for more than a single device)
	bool is_last; // between operations this value must be up-to-date
	// NOTE: UPDATED MEANING: 'previous' means next-to-update; 'last' means candidate for update (when update is done, new 'last' is defined by prev_device_id
	// TODO: consider renaming
	bool check_path_first; // set to true for a device that has no route to yet (note: this status is set when update is added and MAY change only when root routing table is updated)
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

///////////////////   DEBUG and VERIFICATIONS   //////////////////////

void dbg_siot_mesh_at_root_validate_device_tables( SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA* data )
{
#ifdef SA_DEBUG
	uint16_t i, j;

	// target IDs in the route table are in increasing order
	for ( i=1; i<data->siot_m_route_table.size(); i++ )
		ZEPTO_DEBUG_ASSERT( data->siot_m_route_table[ i - 1 ].TARGET_ID < data->siot_m_route_table[ i ].TARGET_ID );

	// link IDs in the link table are in increasing order
	for ( i=1; i<data->siot_m_link_table.size(); i++ )
		ZEPTO_DEBUG_ASSERT( data->siot_m_link_table[ i - 1 ].LINK_ID < data->siot_m_link_table[ i ].LINK_ID );

	// link ID in the route table correspond to an entry in the link table
	for ( i=0; i<data->siot_m_route_table.size(); i++ )
	{
		bool found = false;
		for ( j=0; j<data->siot_m_link_table.size(); j++ )
			if ( data->siot_m_route_table[i].LINK_ID == data->siot_m_link_table[j].LINK_ID )
			{
				found = true;
				break;
			}
			else if ( data->siot_m_route_table[i].LINK_ID > data->siot_m_link_table[j].LINK_ID ) // because of canonicity means "not found"
				break;
		if ( ! found )
			ZEPTO_DEBUG_ASSERT( 0 == "LINK_ID in the route table corresponds to nothing in the link table" );
	}
#endif
}

void dbg_siot_mesh_at_root_validate_all_device_tables()
{
	SIOT_MESH_ROUTING_DATA_ITERATOR it;
	for ( it = mesh_routing_data.begin(); it != mesh_routing_data.end(); ++it )
		dbg_siot_mesh_at_root_validate_device_tables( &*it );
}

///////////////////   Basic calls: initializing  //////////////////////

void siot_mesh_init_tables()  // TODO: this call reflects current development stage and may or may not survive in the future
{
	// as soon as pairing is implemented below code will be removed
	SIOT_MESH_DEVICE_ROUTE_AND_LINK_DATA data;
	data.device_id = 0;
	mesh_routing_data.push_back( data );
	data.device_id = 1;
	mesh_routing_data.push_back( data );
}

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

void siot_mesh_at_root_siot_mesh_at_root_init_route_update_data( SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE* update, bool for_adding, uint16_t device_id, uint16_t prev_device_id )
{
	update->to_add = for_adding;
	update->siot_m_link_table_update.clear();
	update->siot_m_route_table_update.clear();
	update->device_id = device_id;
	update->in_progress = false;
	update->is_last = false;
	update->prev_device_id = prev_device_id;
//	update->next_device_id = 0;
	update->check_path_first = false;
}

uint8_t siot_mesh_at_root_get_list_of_updated_devices_when_route_is_added( SIOT_MESH_ROUTING_DATA_UPDATES& update_list, uint16_t id_target, uint16_t bus_to_send_from_target, uint16_t id_prev, uint16_t bust_to_send_from_prev, uint16_t id_next /*more data may be required*/ )
{
	// NOTE: prepares updates for a target and all retransmitters on the way
	// Collect necessary changes

	SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE update;
	SIOT_MESH_ROUTING_DATA_ITERATOR rd_it;
	SIOT_MESH_ROUTE route;
	SIOT_MESH_LINK link;
	route.TARGET_ID = id_target;
	uint16_t dev_id, prev_dev_id;
	uint16_t i, j;
	update_list.clear();
	ZEPTO_DEBUG_ASSERT( mesh_routing_data.size() && mesh_routing_data[0].device_id == 0 );

	// adding items for all devices from ROOT to the predecessor of id_prev
	dev_id = 0;
	prev_dev_id = 0;;
	while ( dev_id != id_prev )
	{
#ifdef SA_DEBUG
		bool next_found = false;
#endif // SA_DEBUG
		siot_mesh_at_root_siot_mesh_at_root_init_route_update_data( &update, true, dev_id, prev_dev_id );
		siot_mesh_at_root_get_device_data( dev_id, rd_it );
		int ini_sz = rd_it->siot_m_route_table.size();
		for ( i=0; i<ini_sz; i++ )
			if ( rd_it->siot_m_route_table[i].TARGET_ID == id_prev )
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
		ZEPTO_DEBUG_ASSERT( update.siot_m_link_table_update.size() || update.siot_m_route_table_update.size() );
		update_list.push_back( update );
#ifdef SA_DEBUG
		ZEPTO_DEBUG_ASSERT( next_found );
		ZEPTO_DEBUG_ASSERT( prev_dev_id != dev_id );
#endif // SA_DEBUG
	}

	// the last device (as well as a target device) should receive LINK record; and we need to somehow assign a LINK_ID on a target device

	// adding item for the last retransmitter
	siot_mesh_at_root_get_device_data( id_prev, rd_it );
	link.LINK_ID = 0;
	for ( j=0; j<rd_it->siot_m_link_table.size(); j++ )
		if ( link.LINK_ID != rd_it->siot_m_link_table[j].LINK_ID ) // we exploit here canonicity of the link table
			break;
		else
			(link.LINK_ID)++;
	link.NEXT_HOP = id_target;
	link.BUS_ID = bust_to_send_from_prev;
	// TODO: (!!!) MISSING INFORMATION: link other values
	route.LINK_ID = link.LINK_ID;
	siot_mesh_at_root_siot_mesh_at_root_init_route_update_data( &update, true, id_prev, dev_id );
	update.siot_m_link_table_update.push_back( link );
	update.siot_m_route_table_update.push_back( route);
	update.is_last = true; // within current collection
	ZEPTO_DEBUG_ASSERT( update.siot_m_link_table_update.size() || update.siot_m_route_table_update.size() );
	update_list.push_back( update );

	// additing item for the target device
	// TODO: (!!!) INFORMATION IS MISSING: physics of retransmission
	link.LINK_ID = 0;
	link.NEXT_HOP = id_next;
	link.BUS_ID = bus_to_send_from_target;
	route.LINK_ID = link.LINK_ID;
	route.TARGET_ID = 0;
	siot_mesh_at_root_siot_mesh_at_root_init_route_update_data( &update, true, id_target, id_prev );
	update.siot_m_link_table_update.push_back( link );
	update.siot_m_route_table_update.push_back( route);
	update.is_last = true; // within current collection
	update.check_path_first = true;
	ZEPTO_DEBUG_ASSERT( update.siot_m_link_table_update.size() || update.siot_m_route_table_update.size() );
	update_list.push_back( update );

	return SIOT_MESH_AT_ROOT_RET_OK;
}

uint8_t siot_mesh_at_root_merge_route_and_link_table_updates_both_to_add_data( SIOT_M_ROUTE_TABLE_TYPE& present_route_changes, SIOT_M_ROUTE_TABLE_TYPE& new_route_changes, SIOT_M_LINK_TABLE_TYPE& present_link_changes, SIOT_M_LINK_TABLE_TYPE& new_link_changes )
{
	// NOTE: in the current implementation we can merge updates of the same type only; it might be changed later
	// TODO: consider merging updates of the same type

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
uint8_t siot_mesh_at_root_merge_route_and_link_table_updates_both_to_drop_data( SIOT_M_ROUTE_TABLE_TYPE& present_route_changes, SIOT_M_ROUTE_TABLE_TYPE& new_route_changes, SIOT_M_LINK_TABLE_TYPE& present_link_changes, SIOT_M_LINK_TABLE_TYPE& new_link_changes )
{
	// NOTE: in the current implementation we can merge updates of the same type only; it might be changed later
	// TODO: consider merging updates of the same type

	// It looks like at this point changes to route table and to link table have no interdependences; we process them separately assuming that newer cancels older
	// TODO: think about sanity check right before applying changes
	uint16_t i, j;
	for ( i=0; i<new_route_changes.size(); i++ )
	{
		bool applied = false;
		for ( j=0; j<present_route_changes.size(); j++ )
			if ( present_route_changes[j].TARGET_ID == new_route_changes[i].TARGET_ID )
			{
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
*/
uint8_t siot_mesh_at_root_add_or_merge_updates_when_route_is_added( SIOT_MESH_ROUTING_DATA_UPDATES& update_list )
{
	// we start from the end of the list and add updates one by one;
	// if an update to the same device is already in the set of updates and is not in progress, we merge updates
	int16_t i;
	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it;
	for ( i=update_list.size()-1; i>=0; i-- )
	{
		ZEPTO_DEBUG_ASSERT( update_list[i].siot_m_link_table_update.size() || update_list[i].siot_m_route_table_update.size() );
		bool added = false;
		for ( it = mesh_routing_data_updates.begin(); it != mesh_routing_data_updates.end(); ++it )
		{
			ZEPTO_DEBUG_ASSERT( it->siot_m_link_table_update.size() || it->siot_m_route_table_update.size() );
			ZEPTO_DEBUG_ASSERT( update_list[i].to_add );
			if ( update_list[i].device_id == it->device_id && update_list[i].to_add == it->to_add ) // merge only if the immediate next in the list is of the same type (TODO: think about merging updates of different types)
			{
				if ( ! it->in_progress ) // we can merge
				{
					siot_mesh_at_root_merge_route_and_link_table_updates_both_to_add_data( it->siot_m_route_table_update, update_list[i].siot_m_route_table_update,  it->siot_m_link_table_update, update_list[i].siot_m_link_table_update );
					it->is_last = it->is_last && update_list[i].is_last;
				}
				else
				{
					// is_last flag remains unchanged
					ZEPTO_DEBUG_ASSERT( update_list[i].in_progress == false );
					mesh_routing_data_updates.push_back( update_list[i] );
				}
				added = true;
				break;
			}
		}
		if ( !added )
		{
			// is_last flag remains unchanged
			ZEPTO_DEBUG_ASSERT( update_list[i].in_progress == false );
			mesh_routing_data_updates.push_back( update_list[i] );
		}
	}

	return SIOT_MESH_AT_ROOT_RET_OK;
}
/*
uint8_t siot_mesh_at_root_add_or_merge_updates_when_route_is_droped( SIOT_MESH_ROUTING_DATA_UPDATES& update_list )
{
	// we start from the end of the list and add updates one by one;
	// if an update to the same device is already in the set of updates and is not in progress, we merge updates
	int16_t i;
	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it;
	for ( i=update_list.size()-1; i>=0; i-- )
	{
		bool added = false;
		for ( it = mesh_routing_data_updates.begin(); it != mesh_routing_data_updates.end(); ++it )
		{
			ZEPTO_DEBUG_ASSERT( it->siot_m_link_table_update.size() || it->siot_m_route_table_update.size() );
			ZEPTO_DEBUG_ASSERT( !update_list[i].to_add );
			if ( update_list[i].device_id == it->device_id && update_list[i].to_add == it->to_add ) // merge only if the immediate next in the list is of the same type (TODO: think about merging updates of different types)
			{
				if ( ! it->in_progress ) // we can merge
				{
					siot_mesh_at_root_merge_route_and_link_table_updates_both_to_drop_data( it->siot_m_route_table_update, update_list[i].siot_m_route_table_update,  it->siot_m_link_table_update, update_list[i].siot_m_link_table_update );
					it->is_last = it->is_last && update_list[i].is_last;
				}
				else
				{
					// is_last flag remains unchanged
					ZEPTO_DEBUG_ASSERT( update_list[i].in_progress == false );
					mesh_routing_data_updates.push_back( update_list[i] );
				}
				added = true;
				break;
			}
		}
		if ( !added )
		{
			// is_last flag remains unchanged
			ZEPTO_DEBUG_ASSERT( update_list[i].in_progress == false );
			mesh_routing_data_updates.push_back( update_list[i] );
		}
	}

	return SIOT_MESH_AT_ROOT_RET_OK;
}
*/
uint8_t siot_mesh_at_root_add_updates_for_device_when_route_is_added( uint16_t id_target, uint16_t bus_to_send_from_target, uint16_t id_prev, uint16_t bust_to_send_from_prev, uint16_t id_next /*more data may be required*/ )
{
	SIOT_MESH_ROUTING_DATA_UPDATES update_list;
	siot_mesh_at_root_get_list_of_updated_devices_when_route_is_added( update_list, id_target, bus_to_send_from_target, id_prev, bust_to_send_from_prev, id_next /*more data may be required*/ );
	siot_mesh_at_root_add_or_merge_updates_when_route_is_added( update_list );
	// TODO: check ret codes

	return SIOT_MESH_AT_ROOT_RET_OK;
}

uint8_t siot_mesh_at_root_apply_update_to_local_copy( SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR update )
{
	// NOTE: in this implementation we assume that an update is either to add or to remove respective items
	ZEPTO_DEBUG_ASSERT( update->siot_m_link_table_update.size() || update->siot_m_route_table_update.size() );

	SIOT_MESH_ROUTING_DATA_ITERATOR dev_data;
	siot_mesh_at_root_get_device_data( update->device_id, dev_data );
	ZEPTO_DEBUG_ASSERT( update->device_id == dev_data->device_id );

	uint16_t i, j;

	for ( i=0; i<update->siot_m_route_table_update.size(); i++ )
	{
		bool applied = false;
		int ini_sz = dev_data->siot_m_route_table.size();
		if ( ini_sz == 0 || dev_data->siot_m_route_table[0].TARGET_ID > update->siot_m_route_table_update[i].TARGET_ID )
		{
			if ( update->to_add )
				dev_data->siot_m_route_table.insert( dev_data->siot_m_route_table.begin(), update->siot_m_route_table_update[i] );
			applied = true;
		}
		if ( !applied )
		{
			for ( j=0; j<ini_sz; j++ )
				if ( dev_data->siot_m_route_table[j].TARGET_ID == update->siot_m_route_table_update[i].TARGET_ID )
				{
					if ( update->to_add )
						dev_data->siot_m_route_table[j].LINK_ID = update->siot_m_route_table_update[i].LINK_ID;
					else
						dev_data->siot_m_route_table.erase( dev_data->siot_m_route_table.begin() + j );
					applied = true;
					break;
				}
				else if ( dev_data->siot_m_route_table[j].TARGET_ID < update->siot_m_route_table_update[i].TARGET_ID ) // as soon as.... (we exploit canonicity here)
				{
					if ( update->to_add )
						dev_data->siot_m_route_table.insert( dev_data->siot_m_route_table.begin() + j + 1, update->siot_m_route_table_update[i] );
					applied = true;
					break;
				}
		}
		if ( !applied ) // something new
		{
			if ( update->to_add )
			{
				ZEPTO_DEBUG_ASSERT( dev_data->siot_m_route_table.size() == 0 || dev_data->siot_m_route_table[ dev_data->siot_m_route_table.size() - 1 ].TARGET_ID < update->siot_m_route_table_update[i].TARGET_ID );
				dev_data->siot_m_route_table.push_back( update->siot_m_route_table_update[i] );
			}
			// TODO: think about 'else' case
		}
	}

	for ( i=0; i<update->siot_m_link_table_update.size(); i++ )
	{
		bool applied = false;
		int ini_sz = dev_data->siot_m_link_table.size();
		if ( ini_sz == 0 || dev_data->siot_m_link_table[0].LINK_ID > update->siot_m_link_table_update[i].LINK_ID )
		{
			if ( update->to_add )
				dev_data->siot_m_link_table.insert( dev_data->siot_m_link_table.begin(), update->siot_m_link_table_update[i] );
			applied = true;
		}
		if ( !applied ) // something new
		{
			for ( j=0; j<ini_sz; j++ )
				if ( dev_data->siot_m_link_table[j].LINK_ID == update->siot_m_link_table_update[i].LINK_ID )
				{
					if ( update->to_add )
						dev_data->siot_m_link_table[j].LINK_ID = update->siot_m_link_table_update[i].LINK_ID;
					else
						dev_data->siot_m_link_table.erase( dev_data->siot_m_link_table.begin() + j );
					applied = true;
					break;
				}
				else if ( dev_data->siot_m_link_table[j].LINK_ID < update->siot_m_link_table_update[i].LINK_ID ) // as soon as.... (we exploit canonicity here)
				{
					if ( update->to_add )
						dev_data->siot_m_link_table.insert( dev_data->siot_m_link_table.begin() + j + 1, update->siot_m_link_table_update[i] );
					applied = true;
					break;
				}
		}
		if ( !applied ) // something new
		{
			if ( update->to_add )
			{
				ZEPTO_DEBUG_ASSERT( dev_data->siot_m_link_table.size() == 0 || dev_data->siot_m_link_table[ dev_data->siot_m_link_table.size() - 1 ].LINK_ID < update->siot_m_link_table_update[i].LINK_ID );
				dev_data->siot_m_link_table.push_back( update->siot_m_link_table_update[i] );
			}
			// TODO: think about 'else' case
		}
	}

	return SIOT_MESH_AT_ROOT_RET_OK;
}

uint8_t siot_mesh_at_root_get_next_update( SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR& update )
{
	// TODO: here we assume that is_last is set correctly (and should be updated at time of item adding/removal). Think whether this assumption is practically good (seems to be, indeed...).
	if ( mesh_routing_data_updates.begin() == mesh_routing_data_updates.end() )
		return SIOT_MESH_AT_ROOT_RET_NO_UPDATES;

	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it;

	bool start_big_loop_over;
	bool continue_inner_loop;
	do // note: we have to repeat the body of this cycle, if we remove anything from mesh_routing_data_updates (see code below)
	{
		start_big_loop_over = false;
		for ( update = mesh_routing_data_updates.begin(); update != mesh_routing_data_updates.end(); ++update )
			if ( update->is_last && (!update->in_progress) ) // potential candidate
			{
				if ( update->device_id != 0 ) // not a ROOT
				{
					continue_inner_loop = false;
					for ( it = mesh_routing_data_updates.begin(); it != mesh_routing_data_updates.end(); ++it )
						if ( it->device_id == update->device_id && it->in_progress ) // already in progress; subsequent update must wait until done
						{
							continue_inner_loop = true; // find a new candidate
							break;
						}

					if ( continue_inner_loop )
						continue;

					update->in_progress = true;
					return SIOT_MESH_AT_ROOT_RET_OK;
				}
				else // a non-Root candidate is found; it is not guaranteed that no updates to the same device is in progress; need to check it first
				{
					siot_mesh_at_root_apply_update_to_local_copy( update );
					mesh_routing_data_updates.erase( update );
					start_big_loop_over = true; // after erasing we're not guaranteed that iterator survives (especially, if we switch to vector>; it's safer to start over
					break;
				}
			}
	}
	while ( start_big_loop_over );
	return SIOT_MESH_AT_ROOT_RET_NO_READY_UPDATES;
}

void siot_mesh_at_root_update_to_packet( MEMORY_HANDLE mem_h, SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR update )
{
	ZEPTO_DEBUG_ASSERT( update->siot_m_link_table_update.size() || update->siot_m_route_table_update.size() );

	if ( update->to_add )
	{
		uint16_t i;
		uint16_t header;
		uint16_t more;
		ZEPTO_DEBUG_ASSERT( update->in_progress );
		for ( i=0; i<update->siot_m_route_table_update.size(); i++ )
		{
			// | ADD-OR-MODIFY-ROUTE-ENTRY-AND-LINK-ID | TARGET-ID |
			more = ( i == update->siot_m_route_table_update.size() - 1 && update->siot_m_link_table_update.size() == 0 ) ? 0 : 1;
			header = more | ( ADD_OR_MODIFY_ROUTE_ENTRY << 1 ) | ( update->siot_m_route_table_update[i].LINK_ID << 3 );
			zepto_parser_encode_and_append_uint16( mem_h, header );
			zepto_parser_encode_and_append_uint16( mem_h, update->siot_m_route_table_update[i].TARGET_ID );
		}
		for ( i=0; i<update->siot_m_link_table_update.size(); i++ )
		{
			more = ( i == update->siot_m_link_table_update.size() - 1 ) ? 0 : 1;
			// | ADD-OR-MODIFY-LINK-ENTRY-AND-LINK-ID | BUS-ID | NEXT-HOP-ACKS-AND-INTRA-BUS-ID-PLUS-1 | OPTIONAL-LINK-DELAY-UNIT | OPTIONAL-LINK-DELAY | OPTIONAL-LINK-DELAY-ERROR |
			header = more | ( ADD_OR_MODIFY_LINK_ENTRY << 1 ) | ( update->siot_m_route_table_update[i].LINK_ID << 4 );
			zepto_parser_encode_and_append_uint16( mem_h, header );
			zepto_parser_encode_and_append_uint16( mem_h, update->siot_m_link_table_update[i].BUS_ID );
			zepto_parser_encode_and_append_uint32( mem_h, 0 ); // intra-bus id
		}
	}
	else
	{
		uint16_t i;
		uint16_t header;
		uint16_t more;
		ZEPTO_DEBUG_ASSERT( update->in_progress );
		for ( i=0; i<update->siot_m_route_table_update.size(); i++ )
		{
			// | DELETE-ROUTE-ENTRY-AND-TARGET-ID |
			more = ( i == update->siot_m_route_table_update.size() - 1 && update->siot_m_link_table_update.size() == 0 ) ? 0 : 1;
			header = more | ( DELETE_ROUTE_ENTRY << 1 ) | ( update->siot_m_route_table_update[i].TARGET_ID << 3 );
			zepto_parser_encode_and_append_uint16( mem_h, header );
		}
		for ( i=0; i<update->siot_m_link_table_update.size(); i++ )
		{
			// | DELETE-LINK-ENTRY-AND-LINK-ID |
			more = ( i == update->siot_m_link_table_update.size() - 1 ) ? 0 : 1;
			// | ADD-OR-MODIFY-LINK-ENTRY-AND-LINK-ID | BUS-ID | NEXT-HOP-ACKS-AND-INTRA-BUS-ID-PLUS-1 | OPTIONAL-LINK-DELAY-UNIT | OPTIONAL-LINK-DELAY | OPTIONAL-LINK-DELAY-ERROR |
			header = more | ( DELETE_LINK_ENTRY << 1 ) | ( update->siot_m_route_table_update[i].LINK_ID << 4 );
			zepto_parser_encode_and_append_uint16( mem_h, header );
		}
	}
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
	// TODO: revize implementation
	
	// basically we need to do three main things: to apply the update to the local copy of routing table, to update (and check) remaining items, and to remove the item
	SIOT_MESH_ALL_ROUTING_DATA_UPDATES_ITERATOR it, it1, it2;
	for ( it = mesh_routing_data_updates.begin(); it != mesh_routing_data_updates.end(); ++it )
		if ( it->device_id == device_id && it->in_progress ) // note: while current update was 'in progress', other updates of the same device could be added to the collection; and they could not be merged with one already being in progress; thus they will stay with status !in_progress
		{
#ifdef SA_DEBUG
			uint16_t cnt = 0;
			for ( it1 = mesh_routing_data_updates.begin(); it1 != mesh_routing_data_updates.end(); ++it1 )
				if ( it->device_id == it1->device_id && it1->in_progress )
					cnt++;
			ZEPTO_DEBUG_ASSERT( cnt == 1 ); // only a single update to the same device can be in progress
#endif // SA_DEBUG

			// apply update to the respective local copy
			siot_mesh_at_root_apply_update_to_local_copy( it );

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

uint8_t siot_mesh_at_root_target_to_link_id( uint16_t target_id, uint16_t* link_id )
{
	uint16_t i, j;

	for ( i=0; i<mesh_routing_data.size(); i++)
		if ( mesh_routing_data[i].device_id == 0 )
			for ( j=0; j<mesh_routing_data[i].siot_m_route_table.size(); j++ )
				if ( mesh_routing_data[i].siot_m_route_table[j].TARGET_ID == target_id )
				{
					*link_id = mesh_routing_data[i].siot_m_route_table[j].LINK_ID;
if (*link_id > 100 )
{
	*link_id = *link_id;
}
					return SIOT_MESH_RET_OK;
				}
	return SIOT_MESH_RET_ERROR_NOT_FOUND;
}

void siot_mesh_at_root_remove_link_to_target_no_ack_from_immediate_hop( uint16_t target_id )
{
	uint16_t link_id;
	SIOT_MESH_LINK link;
	siot_mesh_at_root_target_to_link_id( target_id, &link_id );
	siot_mesh_get_link( link_id, &link );

	siot_mesh_at_root_remove_link_to_target_route_error_reported( 0, link.NEXT_HOP );
}

void siot_mesh_at_root_remove_link_to_target_route_error_reported( uint16_t reporting_id, uint16_t failed_hop_id )
{
	uint16_t i, j, k;
	uint16_t failed_index = (uint16_t)(-1); 
	uint16_t root_index = (uint16_t)(-1); 
	uint16_t idx = root_index;
	vector<int> affected_list1, affected_list2;
	vector<int>& prev_list = affected_list1;
	vector<int>& next_list = affected_list2;

	SIOT_MESH_DEVICE_ROUTING_DATA_UPDATE update;

	// 1. find index of root data
	for ( i=0; i<mesh_routing_data.size(); i++)
		if ( mesh_routing_data[i].device_id == 0 )
		{
			root_index = i;
			break;
		}
	ZEPTO_DEBUG_ASSERT( root_index != (uint16_t)(-1) );


	// 2. get indexes of all lost devices (that with 'failed_hop_id' and all devices in its table
	vector<uint16_t> non_reached_device_ids;
	non_reached_device_ids.push_back( failed_hop_id );
	for ( i=0; i<mesh_routing_data.size(); i++)
		if ( mesh_routing_data[i].device_id == failed_hop_id )
		{
			for ( j=0; j<mesh_routing_data[i].siot_m_route_table.size(); j++ )
				non_reached_device_ids.push_back( mesh_routing_data[i].siot_m_route_table[j].TARGET_ID );
			failed_index = i;
			break;
		}

	// 3. starting from ROOT, calculate and add updates. Note: we can only hope that it's a single chain of devices
	prev_list.push_back( root_index );
	do
	{
		next_list.clear();
		for ( idx=0; idx<prev_list.size(); idx++ )
		{
			// go through all entries in the list
			siot_mesh_at_root_siot_mesh_at_root_init_route_update_data( &update, false, mesh_routing_data[prev_list[idx]].device_id, -1 );
			int link_table_size = mesh_routing_data[prev_list[idx]].siot_m_link_table.size();
			int* used_links = new int [ link_table_size ];
			ZEPTO_MEMSET( used_links, 0, sizeof( int ) * link_table_size );
			for ( k=0; k<mesh_routing_data[prev_list[idx]].siot_m_route_table.size(); k++ )
			{
				bool killed = false;
				uint16_t link_id = mesh_routing_data[prev_list[idx]].siot_m_route_table[k].LINK_ID;
				for ( j=0; j<non_reached_device_ids.size(); j++ )
					if ( mesh_routing_data[prev_list[idx]].siot_m_route_table[k].TARGET_ID == non_reached_device_ids[j] ) // subject to remove
					{
						killed = true;
						SIOT_MESH_ROUTE route;
						route.TARGET_ID = non_reached_device_ids[j];
						update.siot_m_route_table_update.push_back( route );
						int lnk;
						for ( lnk=0; lnk<mesh_routing_data[prev_list[idx]].siot_m_link_table.size(); lnk++ )
							if ( mesh_routing_data[prev_list[idx]].siot_m_link_table[lnk].LINK_ID == lnk )
							{
								uint16_t next_hop_id = mesh_routing_data[prev_list[idx]].siot_m_link_table[lnk].NEXT_HOP;
								bool found2 = false;
								int rt;
								for ( rt=0; rt<next_list.size(); rt++ )
									if ( next_hop_id == next_list[rt] )
									{
										found2 = true;
										break;
									}
								if ( !found2 && next_hop_id != failed_hop_id ) // anyway we cannot reach it
									next_list.push_back( next_hop_id );
							}
					}
				if ( !killed  )
					( used_links[ link_id ] ) ++;
			}

			// determine which links are now out of use
			for ( k=0; k<link_table_size; k++ )
			{
				if ( used_links[k] == 0 )
				{
					SIOT_MESH_LINK link;
					link.LINK_ID = mesh_routing_data[prev_list[idx]].siot_m_link_table[k].LINK_ID;
					update.siot_m_link_table_update.push_back( link );
				}
			}
			if ( used_links != NULL ) delete [] used_links;

			// NOTE: above procedures do not guarantee that the 'update' is not empty
			// TODO: ensure that empty 'update' is legitimate in all cases
			if ( update.siot_m_link_table_update.size() || update.siot_m_route_table_update.size() )
				mesh_routing_data_updates.push_back( update );
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
}

uint8_t siot_mesh_get_link( uint16_t link_id, SIOT_MESH_LINK* link )
{
	uint16_t i, j;
	for ( i=0; i<mesh_routing_data.size(); i++)
		if ( mesh_routing_data[i].device_id == 0 )
			for ( j=0; j<mesh_routing_data[i].siot_m_link_table.size(); j++ )
				if ( mesh_routing_data[i].siot_m_link_table[j].LINK_ID == link_id )
				{
					ZEPTO_MEMCPY( link, &(mesh_routing_data[i].siot_m_link_table[j]), sizeof(SIOT_MESH_LINK) );
					return SIOT_MESH_RET_OK;
				}
	return SIOT_MESH_RET_ERROR_NOT_FOUND;
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
	LAST_HOPS last_hops_in;
	LAST_HOPS last_hops_out;
} DEVICE_LAST_HOPS;

typedef vector< DEVICE_LAST_HOPS > LAST_HOPS_OF_ALL_DEVICES;

LAST_HOPS_OF_ALL_DEVICES last_hops_of_all_devices;

void siot_mesh_at_root_add_last_hop_in_data( uint16_t src_id, uint16_t last_hop_id, uint16_t last_hop_bus_id, uint8_t conn_q )
{
	SIOT_MESH_LAST_HOP_DATA hop;

	hop.last_hop_id = last_hop_id;
	hop.last_hop_bus_id = last_hop_bus_id;
	hop.conn_quality = conn_q;

	uint16_t i, j;

	bool found = false;
	int ini_sz = last_hops_of_all_devices.size();
	for ( i=0; i<ini_sz; i++ )
		if ( last_hops_of_all_devices[i].device_id == src_id )
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
	if ( !found )
	{
		DEVICE_LAST_HOPS dev_hops;
		dev_hops.device_id = src_id;
		dev_hops.last_hops_in.push_back( hop );
		last_hops_of_all_devices.push_back( dev_hops );
	}
}

void siot_mesh_at_root_add_last_hop_out_data( uint16_t src_id, uint16_t bus_id_at_src, uint16_t first_receiver_id, uint8_t conn_q )
{
	SIOT_MESH_LAST_HOP_DATA hop;

	hop.last_hop_id = first_receiver_id;
	hop.last_hop_bus_id = bus_id_at_src;
	hop.conn_quality = conn_q;

	uint16_t i, j;

	bool found = false;
	int ini_sz = last_hops_of_all_devices.size();
	for ( i=0; i<ini_sz; i++ )
		if ( last_hops_of_all_devices[i].device_id == src_id )
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
	if ( !found )
	{
		DEVICE_LAST_HOPS dev_hops;
		dev_hops.device_id = src_id;
		dev_hops.last_hops_in.push_back( hop );
		last_hops_of_all_devices.push_back( dev_hops );
	}
}


#define SIOT_MESH_IS_QUALITY_OF_INCOMING_CONNECTION_ADMISSIBLE( x ) ( (x) < 0x7F )
#define SIOT_MESH_IS_QUALITY_OF_OUTGOING_CONNECTION_ADMISSIBLE( x ) ( (x) < 0x7F )
#define SIOT_MESH_IS_QUALITY_OF_FIRST_INOUT_CONNECTION_BETTER( in1, out1, in2, out2 ) ( (in1<in2)||((in1==in2)&&(out1<out2)) ) /*TODO: this is a quick solution; think about better ones*/

uint8_t siot_mesh_at_root_find_best_route_to_device( uint16_t target_id, uint16_t* bus_id_at_target, uint16_t* id_prev, uint16_t* bus_id_at_prev, uint16_t* id_next )
{
	uint16_t i, j, k;
	uint16_t last_in, last_out;
	uint16_t match_cnt = 0;
	for ( i=0; i<last_hops_of_all_devices.size(); i++ )
		if ( last_hops_of_all_devices[i].device_id == target_id )
		{
			if ( last_hops_of_all_devices[i].last_hops_in.size() == 0 || last_hops_of_all_devices[i].last_hops_out.size() == 0 )
				return SIOT_MESH_AT_ROOT_RET_FAILED;

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
				return SIOT_MESH_AT_ROOT_RET_OK;
			else
				return SIOT_MESH_AT_ROOT_RET_FAILED;
		}

	// TODO: consider more relaxed matches...

	return SIOT_MESH_AT_ROOT_RET_FAILED;
}

uint8_t siot_mesh_at_root_find_best_route( uint16_t* target_id, uint16_t* bus_id_at_target, uint16_t* id_prev, uint16_t* bus_id_at_prev, uint16_t* id_next )
{
	if ( last_hops_of_all_devices.size() == 0 )
		return SIOT_MESH_AT_ROOT_RET_FAILED;
	*target_id = last_hops_of_all_devices[0].device_id; // TODO: we may want to try all possibilities, not just the first one
	return siot_mesh_at_root_find_best_route_to_device( *target_id, bus_id_at_target, id_prev, bus_id_at_prev, id_next );
}

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

//////////////////////////////////////////////////////////////////////////////////////////////////
// pending mesh-level resends

// TODO (URGENT): we need here either dynamical memory handle ackuiring mechanism, or alike

typedef struct _MESH_PENDING_RESENDS
{
//	MEMORY_HANDLE packet_h;
	uint8_t* packet_data;
	uint16_t packet_sz;
	uint8_t resend_cnt;
	sa_time_val next_resend_time;
	uint16_t checksum;
	uint16_t target_id;
} MESH_PENDING_RESENDS;

typedef list< MESH_PENDING_RESENDS > PENDING_RESENDS; // NOTE: switching to vector requires revision of code around calls to erase()
typedef list< MESH_PENDING_RESENDS >::iterator PENDING_RESENDS_ITERATOR;
PENDING_RESENDS pending_resends;

void siot_mesh_at_root_add_resend_task( MEMORY_HANDLE packet, const sa_time_val* currt, uint16_t checksum, uint16_t target_id, sa_time_val* time_to_next_event )
{
	// 1. add resend task
	MESH_PENDING_RESENDS resend;
	resend.target_id = target_id;
	resend.packet_sz = memory_object_get_request_size( packet );
	resend.packet_data = new uint8_t [resend.packet_sz];
	ZEPTO_DEBUG_ASSERT( resend.packet_sz != 0 );
	parser_obj po;
	zepto_parser_init( &po, packet );
	zepto_parse_read_block( &po, resend.packet_data, resend.packet_sz );
	resend.checksum = checksum;
	resend.resend_cnt = SIOT_MESH_SUBJECT_FOR_MESH_RESEND;
	sa_time_val diff_tval;
	TIME_MILLISECONDS16_TO_TIMEVAL( MESH_RESEND_PERIOD_MS, diff_tval );
	sa_hal_time_val_copy_from( &(resend.next_resend_time), currt );
	SA_TIME_INCREMENT_BY_TICKS( resend.next_resend_time, diff_tval );
	pending_resends.push_back( resend );

	// 2. calculate time to the nearest event
	PENDING_RESENDS_ITERATOR it;
	SA_TIME_SET_INFINITE_TIME( *time_to_next_event );
	sa_time_val remaining;
	for ( it = pending_resends.begin(); it != pending_resends.end(); ++it )
	{
		uint8_t in_future = sa_hal_time_val_get_remaining_time( currt, &(it->next_resend_time), &remaining );
		sa_hal_time_val_copy_from_if_src_less( time_to_next_event, &remaining );
		if ( !in_future )
			break;
	}
}

uint8_t siot_mesh_at_root_get_resend_task( MEMORY_HANDLE packet, const sa_time_val* currt, uint16_t* target_id, sa_time_val* time_to_next_event )
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
	bool final_in_seq = it_oldest->resend_cnt == 1;
	ZEPTO_DEBUG_ASSERT( it_oldest->resend_cnt > 0 );
	*target_id = it_oldest->target_id;
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
		if ( it->checksum == checksum )
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
		if ( it->target_id == target_id )
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