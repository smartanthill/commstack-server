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
#include "sa_client_comm_stack.h"
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

typedef vector< DEVICE_CONTEXT > CONTEXTS_OF_ALL_DEVICES;
CONTEXTS_OF_ALL_DEVICES all_devices;


unsigned int main_get_device_count()
{
	return all_devices.size();
}

DEVICE_CONTEXT* main_get_device_data_by_device_id( unsigned int device_id )
{
	for ( unsigned int i=0; i<all_devices.size(); i++ )
		if ( all_devices[i].device_id == device_id )
			return &(all_devices[i]);
	return NULL;
}

DEVICE_CONTEXT* main_get_device_data_by_index( unsigned int idx )
{
	if ( idx >= all_devices.size() )
		return NULL;
	return &(all_devices[idx]);
}

uint8_t main_add_new_device( uint16_t device_id, uint8_t* key )
{
	for ( unsigned int i=0; i<all_devices.size(); i++ )
		if ( all_devices[i].device_id == device_id )
			return MAIN_DEVICES_RET_ALREADY_EXISTS;
	DEVICE_CONTEXT device;
	device.device_id = device_id;
	ZEPTO_MEMCPY( device.AES_ENCRYPTION_KEY, key, 16 );
	device.MEMORY_HANDLE_SAGDP_LSM_APP = acquire_memory_handle();
	device.MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR = acquire_memory_handle();
	device.MEMORY_HANDLE_SAGDP_LSM_CTR = acquire_memory_handle();
	device.MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR = acquire_memory_handle();
	// TODO: check that all handles were successfully allocated
	sasp_init_eeprom_data_at_lifestart( &(device.sasp_data), device.device_id );
	sagdp_init( &(device.sagdp_context_app) );
	sagdp_init( &(device.sagdp_context_ctr) );
	all_devices.push_back( device );
	return MAIN_DEVICES_RET_OK;
}

uint8_t main_preinit_device( uint16_t device_id, uint8_t* key )
{
	for ( unsigned int i=0; i<all_devices.size(); i++ )
		if ( all_devices[i].device_id == device_id )
			return MAIN_DEVICES_RET_ALREADY_EXISTS;
	DEVICE_CONTEXT device;
	device.device_id = device_id;
	ZEPTO_MEMCPY( device.AES_ENCRYPTION_KEY, key, 16 );

/*	device.MEMORY_HANDLE_SAGDP_LSM_APP = acquire_memory_handle();
	device.MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR = acquire_memory_handle();
	device.MEMORY_HANDLE_SAGDP_LSM_CTR = acquire_memory_handle();
	device.MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR = acquire_memory_handle();
	// TODO: check that all handles were successfully allocated
	sasp_restore_from_backup( &(device.sasp_data), device.device_id );
	sagdp_init( &(device.sagdp_context_app) );
	sagdp_init( &(device.sagdp_context_ctr) );*/
	all_devices.push_back( device );
	return MAIN_DEVICES_RET_OK;
}

uint8_t main_postinit_device( uint16_t device_id )
{
	for ( unsigned int i=0; i<all_devices.size(); i++ )
		if ( all_devices[i].device_id == device_id )
		{
			DEVICE_CONTEXT& device = all_devices[i];
			device.MEMORY_HANDLE_SAGDP_LSM_APP = acquire_memory_handle();
			device.MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR = acquire_memory_handle();
			device.MEMORY_HANDLE_SAGDP_LSM_CTR = acquire_memory_handle();
			device.MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR = acquire_memory_handle();
			sasp_restore_from_backup( &(device.sasp_data), device.device_id );
			sagdp_init( &(device.sagdp_context_app) );
			sagdp_init( &(device.sagdp_context_ctr) );
			return MAIN_DEVICES_RET_OK;
		}
	return MAIN_DEVICES_RET_DOES_NOT_EXIST;
}

uint8_t main_postinit_all_devices()
{
	for ( unsigned int i=0; i<all_devices.size(); i++ )
	{
		DEVICE_CONTEXT& device = all_devices[i];
		device.MEMORY_HANDLE_SAGDP_LSM_APP = acquire_memory_handle();
		device.MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR = acquire_memory_handle();
		device.MEMORY_HANDLE_SAGDP_LSM_CTR = acquire_memory_handle();
		device.MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR = acquire_memory_handle();
		sasp_restore_from_backup( &(device.sasp_data), device.device_id );
		sagdp_init( &(device.sagdp_context_app) );
		sagdp_init( &(device.sagdp_context_ctr) );
	}
	return MAIN_DEVICES_RET_OK;
}

uint8_t main_remove_device( uint16_t device_id )
{
	for ( unsigned int i=0; i<all_devices.size(); i++ )
		if ( all_devices[i].device_id == device_id )
		{
			DEVICE_CONTEXT& device = all_devices[i];
			release_memory_handle(device.MEMORY_HANDLE_SAGDP_LSM_APP);
			release_memory_handle(device.MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR);
			release_memory_handle(device.MEMORY_HANDLE_SAGDP_LSM_CTR);
			release_memory_handle(device.MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR);
			handler_sasp_save_state( &(device.sasp_data), device.device_id );
			all_devices.erase( all_devices.begin() + i );
			return MAIN_DEVICES_RET_OK;
		}
	return MAIN_DEVICES_RET_ALREADY_EXISTS;
}

