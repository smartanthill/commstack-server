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
#include "commstack_commlayer.h"
#include <hal_time_provider.h>
#include <simpleiot/siot_oud_protocol.h>
#include <simpleiot/siot_s_protocol.h>
#include <simpleiot/siot_gd_protocol.h>
#include <simpleiot/siot_m_protocol.h>
//#include "saccp_protocol_client_side.h"
//#include "test_generator.h"
#include <stdio.h>
#include "sa_config.h"
#include "hal_commstack_persistent_storage.h"

#include <stdlib.h>     /* atoi */

DECLARE_AES_ENCRYPTION_KEY


int main_loop()
{
	uint8_t pid[ SASP_NONCE_SIZE ];
	sa_uint48_t nonce;

#ifdef ENABLE_COUNTER_SYSTEM
	INIT_COUNTER_SYSTEM
#endif // ENABLE_COUNTER_SYSTEM


	ZEPTO_DEBUG_PRINTF_1("starting CLIENT's COMMM STACK...\n");
	ZEPTO_DEBUG_PRINTF_1("================================\n\n");

	// TODO: actual key loading, etc
//	uint8_t AES_ENCRYPTION_KEY[16];
//	ZEPTO_MEMCPY( AES_ENCRYPTION_KEY, "16-byte fake key", 16 );
//	memset( AES_ENCRYPTION_KEY, 0xab, 16 );

//	timeout_action tact;
//	tact.action = 0;
	sa_time_val currt;
	waiting_for wait_for;
	ZEPTO_MEMSET( &wait_for, 0, sizeof( waiting_for ) );
	wait_for.wait_packet = 1;
	TIME_MILLISECONDS16_TO_TIMEVAL( 1000, wait_for.wait_time ) //+++TODO: actual processing throughout the code

	uint8_t timer_val = 0x1;
	uint16_t wake_time;
	// TODO: revise time/timer management

	uint8_t ret_code;

//	uint8_t wait_to_continue_processing = 0;
//	uint16_t wake_time_continue_processing;

	// do necessary initialization
/*	SAGDP_DATA sagdp_data;
	SASP_DATA sasp_data;
	sagdp_init( &sagdp_data );
	sasp_init_at_lifestart( &sasp_data );*/
	sasp_restore_from_backup();
	sagdp_init();
	siot_mesh_init_tables();

	// Try to initialize connection
	if ( !communication_initialize() )
		return -1;

//	REQUEST_REPLY_HANDLE working_handle = MEMORY_HANDLE_MAIN_LOOP_2;
//	REQUEST_REPLY_HANDLE packet_getting_handle = MEMORY_HANDLE_MAIN_LOOP_1;

	// prepare fake return address
	zepto_write_uint8( MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, 0 );
	zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR );
	zepto_write_uint8( MEMORY_HANDLE_MAIN_LOOP_2_SAOUDP_ADDR, 0 );
	zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_2_SAOUDP_ADDR );


	// MAIN LOOP
	for (;;)
	{
wait_for_comm_event:
		// [[QUICK CHECK FOR UNITS POTENTIALLY WAITING FOR TIMEOUT start]]
		// we ask each potential unit; if it reports activity, let it continue; otherwise, ask a next one
		// IMPORTANT: once an order of units is selected and tested, do not change it without extreme necessity

		sa_get_time( &currt );

		// 1. test GDP
		ret_code = handler_sagdp_timer( &currt, &wait_for, NULL, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR/*, &sagdp_data*/ );
		if ( ret_code == SAGDP_RET_NEED_NONCE )
		{
			ret_code = handler_sasp_get_packet_id( nonce/*, &sasp_data*/ );
			ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
			sa_get_time( &currt );
			ret_code = handler_sagdp_timer( &currt, &wait_for, nonce, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR/*, &sagdp_data*/ );
			ZEPTO_DEBUG_ASSERT( ret_code == SAGDP_RET_TO_LOWER_REPEATED );
			zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
			goto saspsend;
		}

		// 2. (next candidate)
		ret_code = handler_siot_mesh_timer( &currt, &wait_for, MEMORY_HANDLE_MAIN_LOOP_1 );
		switch ( ret_code )
		{
			case SIOT_MESH_RET_PASS_TO_CCP:
			{
				// quite dirty and temporary solution
				zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
				goto sagdpsend;
				break;
			}
		}

		// [[QUICK CHECK FOR UNITS POTENTIALLY WAITING FOR TIMEOUT end]]

		ret_code = wait_for_communication_event( &wait_for );
//		ZEPTO_DEBUG_PRINTF_4( "=============================================Msg wait event; ret = %d, rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );

		switch ( ret_code )
		{
			case COMMLAYER_RET_FAILED:
			{
				// fatal communication error
				return 0;
				break;
			}
			case COMMLAYER_RET_FROM_CENTRAL_UNIT:
			{
				// regular processing will be done below in the next block
				ret_code = try_get_message_within_master( MEMORY_HANDLE_MAIN_LOOP_1 );
				if ( ret_code == COMMLAYER_RET_FAILED )
					return 0;
				if ( ret_code == COMMLAYER_RET_OK_AS_CU )
				{
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
					goto client_received;
					break;
				}
				else if ( ret_code == COMMLAYER_RET_OK_AS_SLAVE )
				{
					// regular processing will be done below in the next block
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
					goto siotmp_rec;
					break;
				}
				else
				{
					ZEPTO_DEBUG_ASSERT( 0 );
				}
			}
			case COMMLAYER_RET_TIMEOUT:
			{
#if 0
				// ZEPTO_DEBUG_PRINTF_1( "no reply received; the last message (if any) will be resent by timer\n" );
				sa_get_time( &currt );
				ret_code = handler_sagdp_timer( &currt, &wait_for, NULL, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR/*, &sagdp_data*/ );
				if ( ret_code == SAGDP_RET_OK )
				{
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
					goto wait_for_comm_event;
				}
				else if ( ret_code == SAGDP_RET_NEED_NONCE )
				{
					ret_code = handler_sasp_get_packet_id( nonce/*, &sasp_data*/ );
					ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
					sa_get_time( &currt );
					ret_code = handler_sagdp_timer( &currt, &wait_for, nonce, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR/*, &sagdp_data*/ );
					ZEPTO_DEBUG_ASSERT( ret_code == SAGDP_RET_TO_LOWER_REPEATED );
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
					goto saspsend;
					break;
				}
				else
				{
					ZEPTO_DEBUG_PRINTF_2( "ret_code = %d\n", ret_code );
					ZEPTO_DEBUG_ASSERT( 0 );
				}
#endif // 0
					
				goto wait_for_comm_event;
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}


		ZEPTO_DEBUG_PRINTF_1("Message from server received\n");
		ZEPTO_DEBUG_PRINTF_4( "ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );


		// 2.0. Pass to siot/mesh
	siotmp_rec:
#if SIOT_MESH_IMPLEMENTATION_WORKS
		ret_code = handler_siot_mesh_receive_packet( MEMORY_HANDLE_MAIN_LOOP_1, 0 ); // TODO: add actual connection quality
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );

		switch ( ret_code )
		{
			case SIOT_MESH_RET_PASS_TO_PROCESS:
			{
				// regular processing will be done below in the next block
				break;
			}
			case SIOT_MESH_RET_PASS_TO_SEND:
			{
				goto hal_send;
				break;
			}
			case SIOT_MESH_RET_GARBAGE_RECEIVED:
			{
				goto wait_for_comm_event;
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}
#endif

		// 2.1. Pass to SAoUDP
//saoudp_in:
		ret_code = handler_saoudp_receive( MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR );
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );

		switch ( ret_code )
		{
			case SAOUDP_RET_OK:
			{
				// regular processing will be done below in the next block
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

		// 2.2. Pass to SASP
		ret_code = handler_sasp_receive( AES_ENCRYPTION_KEY, pid, MEMORY_HANDLE_MAIN_LOOP_1/*, &sasp_data*/ );
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		ZEPTO_DEBUG_PRINTF_4( "SASP1:  ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );

		switch ( ret_code )
		{
			case SASP_RET_IGNORE_PACKET_BROKEN:
			case SASP_RET_IGNORE_PACKET_LAST_REPEATED:
			case SASP_RET_IGNORE_PACKET_NONCE_LS_NOT_APPLIED:
			{
				ZEPTO_DEBUG_PRINTF_1( "SASP: ignoring packet\n" );
				goto wait_for_comm_event;
				break;
			}
			case SASP_RET_TO_LOWER_ERROR:
			{
				goto saoudp_send;
				break;
			}
			case SASP_RET_TO_HIGHER_NEW:
			{
				// regular processing will be done below in the next block
				break;
			}
			case SASP_RET_TO_HIGHER_LAST_SEND_FAILED:
			{
				ZEPTO_DEBUG_PRINTF_1( "NONCE_LAST_SENT has been reset; the last message (if any) will be resent\n" );
				sa_get_time( &currt );
				ret_code = handler_sagdp_receive_request_resend_lsp( &currt, &wait_for, NULL, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR/*, &sagdp_data*/ );
				if ( ret_code == SAGDP_RET_TO_LOWER_NONE )
				{
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
					continue;
				}
				else if ( ret_code == SAGDP_RET_NEED_NONCE )
				{
					ret_code = handler_sasp_get_packet_id( nonce/*, &sasp_data*/ );
					ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
					sa_get_time( &currt );
					ret_code = handler_sagdp_receive_request_resend_lsp( &currt, &wait_for, nonce, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR/*, &sagdp_data*/ );
					ZEPTO_DEBUG_ASSERT( ret_code != SAGDP_RET_NEED_NONCE && ret_code != SAGDP_RET_TO_LOWER_NONE );
				}
				zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
				goto saspsend;
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

		// 3. pass to SAGDP a new packet
		sa_get_time( &currt );
		ret_code = handler_sagdp_receive_up( &currt, &wait_for, NULL, pid, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR/*, &sagdp_data*/ );
		if ( ret_code == SAGDP_RET_NEED_NONCE )
		{
			ret_code = handler_sasp_get_packet_id( nonce/*, &sasp_data*/ );
			ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
			sa_get_time( &currt );
			ret_code = handler_sagdp_receive_up( &currt, &wait_for, nonce, pid, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR/*, &sagdp_data*/ );
			ZEPTO_DEBUG_ASSERT( ret_code != SAGDP_RET_NEED_NONCE );
		}
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		ZEPTO_DEBUG_PRINTF_4( "SAGDP1: ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );

		switch ( ret_code )
		{
			case SAGDP_RET_OK:
			{
				ZEPTO_DEBUG_PRINTF_1( "master received unexpected or repeated packet. ignored\n" );
				goto wait_for_comm_event;
				break;
			}
			case SAGDP_RET_TO_HIGHER:
			{
				// regular processing will be done below, but we need to jump over
				break;
			}
			case SAGDP_RET_SYS_CORRUPTED:
			{
				send_error_to_central_unit( MEMORY_HANDLE_MAIN_LOOP_1 );
				sagdp_init();
				ZEPTO_DEBUG_PRINTF_1( "Internal error. System is to be reinitialized\n" );
				goto wait_for_comm_event;
				break;
			}
#if 0
			case SAGDP_RET_TO_HIGHER_ERROR:
			{
				sagdp_init( &sagdp_data );
				// TODO: reinit the rest of stack (where applicable)
				ret_code = send_to_central_unit_error( MEMORY_HANDLE_MAIN_LOOP_1 );
				//+++TODO: where to go?
				goto wait_for_comm_event;
				break;
			}
#endif // 0
			case SAGDP_RET_TO_LOWER_REPEATED:
			{
				goto saspsend;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

#ifdef MASTER_ENABLE_ALT_TEST_MODE
		ret_code = send_to_central_unit( MEMORY_HANDLE_MAIN_LOOP_1 );
		goto wait_for_comm_event;
#else

		// 4. pass to SACCP a new packet
#if 0 // we cannot do any essential processing here in comm stack...
		ret_code = handler_saccp_receive( MEMORY_HANDLE_MAIN_LOOP_1/*, sasp_nonce_type chain_id*/ ); //master_process( &wait_to_continue_processing, MEMORY_HANDLE_MAIN_LOOP_1 );
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		ZEPTO_DEBUG_PRINTF_4( "SACCP1: ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );
		switch ( ret_code )
		{
			case SACCP_RET_PASS_TO_CENTRAL_UNIT:
			{
				ret_code = send_to_central_unit( MEMORY_HANDLE_MAIN_LOOP_1 );
				// TODO: check ret_code
				goto wait_for_comm_event;
				break;
			}
			case SACCP_RET_FAILED:
			{
				ZEPTO_DEBUG_PRINTF_1( "Failure in SACCP. handling is not implemented. Aborting\n" );
				return 0;
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

#else	// ...instead we just send whatever we have received  to the Central Unit.
		// Note: we may need to add some data (such as chain ID) or to somehow restructure the packet data;
		//       in this case this is a right place to do that

		ret_code = send_to_central_unit( MEMORY_HANDLE_MAIN_LOOP_1 );
		// TODO: check ret_code
		goto wait_for_comm_event;

#endif // 0

#endif




	client_received:
#if 0 // this functionality is trivial and will be done on a Central Unit side
		// 4. SACCP (prepare packet)
		ret_code = handler_saccp_prepare_to_send( MEMORY_HANDLE_MAIN_LOOP_1 );
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		ZEPTO_DEBUG_PRINTF_4( "SACCP2: ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );
		// TODO: analyze and process ret_code
#endif

		// 5. SAGDP
sagdpsend:
		ZEPTO_DEBUG_PRINTF_3( "@client_received: rq_size: %d, rsp_size: %d\n", ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );
		sa_get_time( &currt );
		ret_code = handler_sagdp_receive_hlp( &currt, &wait_for, NULL, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR/*, &sagdp_data*/ );
		if ( ret_code == SAGDP_RET_NEED_NONCE )
		{
			ret_code = handler_sasp_get_packet_id( nonce/*, &sasp_data*/ );
			ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
			sa_get_time( &currt );
			ret_code = handler_sagdp_receive_hlp( &currt, &wait_for, nonce, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR/*, &sagdp_data*/ );
			ZEPTO_DEBUG_ASSERT( ret_code != SAGDP_RET_NEED_NONCE );
		}
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		ZEPTO_DEBUG_PRINTF_4( "SAGDP2: ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );

		switch ( ret_code )
		{
			case SAGDP_RET_SYS_CORRUPTED:
			{
				send_error_to_central_unit( MEMORY_HANDLE_MAIN_LOOP_1 );
				sagdp_init();
				ZEPTO_DEBUG_PRINTF_1( "Internal error. System is to be reinitialized\n" );
				goto wait_for_comm_event;
				break;
			}
			case SAGDP_RET_TO_LOWER_NEW:
			{
				// regular processing will be done below in the next block
				wake_time = getTime() + timer_val;
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

		// SASP
saspsend:
		ret_code = handler_sasp_send( AES_ENCRYPTION_KEY, nonce, MEMORY_HANDLE_MAIN_LOOP_1/*, &sasp_data*/ );
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		ZEPTO_DEBUG_PRINTF_4( "SASP2: ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );

		switch ( ret_code )
		{
			case SASP_RET_TO_LOWER_REGULAR:
			{
				// regular processing will be done below in the next block
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

		// SAoUDP
saoudp_send:
		ret_code = handler_saoudp_send( MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_2_SAOUDP_ADDR );
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );

		switch ( ret_code )
		{
			case SAOUDP_RET_OK:
			{
				// regular processing will be done below in the next block
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

#if SIOT_MESH_IMPLEMENTATION_WORKS
		uint16_t link_id;
		ret_code = handler_siot_mesh_send_packet( MEMORY_HANDLE_MAIN_LOOP_1, 1, &link_id ); // currently we know only about a single client with id=1
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );

		switch ( ret_code )
		{
			case SIOT_MESH_RET_OK:
			{
				// regular processing will be done below in the next block
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}
#endif

		// send packet
hal_send:
//		ZEPTO_DEBUG_ASSERT( link_id == 0 ); // TODO: link_id must be a part of send_packet() call; we are now just in the middle of development...
		ret_code = send_message( MEMORY_HANDLE_MAIN_LOOP_1 );
		zepto_parser_free_memory( MEMORY_HANDLE_MAIN_LOOP_1 );
		if (ret_code != COMMLAYER_RET_OK )
		{
			return -1;
		}

	}

	communication_terminate();

	return 0;
}

void set_port_from_command_line(int argc, char *argv[])
{
	uint8_t i;
	for ( i = 0; i<argc; i++ )
	{
		if ( ZEPTO_MEMCMP( argv[i], "--port=", 7 ) == 0 )
		{
			int port = atoi( argv[i]+7);
			ZEPTO_DEBUG_ASSERT( port >= 0 && port < 0x10000 );
			ZEPTO_DEBUG_PRINTF_2( "port to be actually used: %d\n", port );
			self_port_num_with_cl = port;
			return;
		}
	}
}

char* get_persistent_storage_path_from_command_line(int argc, char *argv[])
{
	uint8_t i;
	for ( i = 0; i<argc; i++ )
		if ( ZEPTO_MEMCMP( argv[i], "--psp=", 6 ) == 0 )
		{
			ZEPTO_DEBUG_PRINTF_2( "persistent storage is at: \"%s\"\n", argv[i]+6 );
			return argv[i]+6;
		}
	ZEPTO_DEBUG_PRINTF_1( "default persistent storage location will be used\n" );
	return NULL;
}

int main(int argc, char *argv[])
{
	setbuf(stdout, NULL);

	set_port_from_command_line( argc, argv );

	zepto_mem_man_init_memory_management();

	// TODO: logic of accessing/intializing persistent storage must be totally revised toward more secure version
//	if (!init_eeprom_access())
//		return 0;
	uint8_t rid[DATA_REINCARNATION_ID_SIZE];
	ZEPTO_MEMCPY( rid, AES_ENCRYPTION_KEY, DATA_REINCARNATION_ID_SIZE );
	char* persistent_storage_path = get_persistent_storage_path_from_command_line( argc, argv );
	uint8_t ret_code = hal_init_eeprom_access( persistent_storage_path );
	switch ( ret_code )
	{
		case HAL_PS_INIT_FAILED:
		{
			ZEPTO_DEBUG_PRINTF_1( "init_eeprom_access() failed\n" );
			return 0;
		}
		case HAL_PS_INIT_OK:
		{
			ZEPTO_DEBUG_PRINTF_1( "hal_init_eeprom_access() passed\n" );
/*			if ( !eeprom_check_at_start() ) // corrupted data; so far, at least one of slots cannot be recovered
			{
				sasp_init_eeprom_data_at_lifestart();
			}*/
			ret_code = eeprom_check_reincarnation( rid );
			switch ( ret_code )
			{
				case EEPROM_RET_REINCARNATION_ID_OLD:
				{
					sasp_init_eeprom_data_at_lifestart();
					eeprom_update_reincarnation_if_necessary( rid );
					break;
				}
				case EEPROM_RET_REINCARNATION_ID_OK_ONE_OK:
				{
					if ( !eeprom_check_at_start() ) // corrupted data; so far, at least one of slots cannot be recovered
					{
						sasp_init_eeprom_data_at_lifestart();
					}
					eeprom_update_reincarnation_if_necessary( rid );
					break;
				}
				case EEPROM_RET_REINCARNATION_ID_OK_BOTH_OK:
				{
					if ( !eeprom_check_at_start() ) // corrupted data; so far, at least one of slots cannot be recovered
					{
						sasp_init_eeprom_data_at_lifestart();
					}
					break;
				}
				default:
				{
					ZEPTO_DEBUG_ASSERT( 0 == "Unexpected ret code" );
					break;
				}
			}
			break;
		}
		case HAL_PS_INIT_OK_NEEDS_INITIALIZATION:
		{
			sasp_init_eeprom_data_at_lifestart();
			eeprom_update_reincarnation_if_necessary( rid );
			ZEPTO_DEBUG_PRINTF_1( "format_eeprom_at_lifestart() passed\n" );
			break;
		}
	}

	return main_loop();
//	return 0;
}
