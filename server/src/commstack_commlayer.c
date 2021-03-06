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

#include "commstack_commlayer.h"
#include "commstack_commlayer_helper.h"
#include "debugging.h"
#include <stdio.h>

#define MAX_PACKET_SIZE 80

uint8_t sync_status = COMMLAYER_SYNC_STATUS_GO_THROUGH;


#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#if defined _MSC_VER || defined __MINGW32__

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#define CLOSE_SOCKET( x ) closesocket( x )

#else // _MSC_VER

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h> // for close() for socket
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define CLOSE_SOCKET( x ) close( x )

#endif // _MSC_VER



bool communication_preinitialize()
{
#if defined _MSC_VER || defined __MINGW32__
	// do Windows magic
	WSADATA wsaData;
	int iResult;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		ZEPTO_DEBUG_PRINTF_2("WSAStartup failed with error: %d\n", iResult);
		return false;
	}
	return true;
#else
	return true;
#endif
}





#if defined _MSC_VER || defined __MINGW32__
SOCKET sock_with_cl;
SOCKET sock_with_cl_accepted;
#else
int sock_with_cl;
int sock_with_cl_accepted;
#endif
const char* inet_addr_as_string_with_cl = "127.0.0.1";
struct sockaddr_in sa_self_with_cl, sa_other_with_cl;

uint16_t self_port_num_with_cl = 7665;
uint16_t other_port_num_with_cl = 7655;

uint16_t buffer_in_with_cl_pos;

bool communication_with_comm_layer_initialize()
{
	//Zero out socket address
	memset(&sa_self_with_cl, 0, sizeof sa_self_with_cl);
	memset(&sa_other_with_cl, 0, sizeof sa_other_with_cl);

	//create an internet, datagram, socket using UDP
	sock_with_cl = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (-1 == sock_with_cl) /* if socket failed to initialize, exit */
	{
		ZEPTO_DEBUG_PRINTF_1("Error Creating Socket\n");
		return false;
	}

	//The address is ipv4
	sa_other_with_cl.sin_family = AF_INET;
	sa_self_with_cl.sin_family = AF_INET;

	//ip_v4 adresses is a uint32_t, convert a string representation of the octets to the appropriate value
	sa_self_with_cl.sin_addr.s_addr = inet_addr( inet_addr_as_string_with_cl );
	sa_other_with_cl.sin_addr.s_addr = inet_addr( inet_addr_as_string_with_cl );

	//sockets are unsigned shorts, htons(x) ensures x is in network byte order, set the port to 7654
	sa_self_with_cl.sin_port = htons( self_port_num_with_cl );
	sa_other_with_cl.sin_port = htons( other_port_num_with_cl );

	if (-1 == bind(sock_with_cl, (struct sockaddr *)&sa_self_with_cl, sizeof(sa_self_with_cl)))
	{
#if defined _MSC_VER || defined __MINGW32__
		int error = WSAGetLastError();
#else
		int error = errno;
#endif
		ZEPTO_DEBUG_PRINTF_2( "bind sock_with_cl failed; error %d\n", error );
		CLOSE_SOCKET(sock_with_cl);
		return false;
	}

	if(-1 == listen(sock_with_cl, 10))
    {
      perror("error listen failed");
      CLOSE_SOCKET(sock_with_cl);
      return false;
    }

	struct sockaddr_in sock_in;
	socklen_t sock_len = sizeof(sock_in);
	if (getsockname(sock_with_cl, (struct sockaddr *)&sock_in, &sock_len) == -1)
	    perror("getsockname");
	else
		ZEPTO_DEBUG_PRINTF_2( "socket: started on port %d\n", ntohs(sock_in.sin_port) );

	sock_with_cl_accepted = accept(sock_with_cl, NULL, NULL);

      if ( 0 > sock_with_cl_accepted )
      {
        perror("error accept failed");
        CLOSE_SOCKET(sock_with_cl);
        exit(EXIT_FAILURE);
      }

	  sock_with_cl = sock_with_cl_accepted; /*just to keep names*/

#if defined _MSC_VER || defined __MINGW32__
    unsigned long ul = 1;
    ioctlsocket(sock_with_cl, FIONBIO, &ul);
#else
    fcntl(sock_with_cl,F_SETFL,O_NONBLOCK);
#endif

	return true;
}

void communication_with_comm_layer_terminate()
{
	CLOSE_SOCKET(sock_with_cl);
}

bool communication_initialize()
{
	return communication_preinitialize() && communication_with_comm_layer_initialize();
}

void communication_terminate()
{
//	_communication_terminate();
	communication_with_comm_layer_terminate();
}

uint8_t try_get_packet_within_master_loop( uint8_t* buff, uint16_t sz )
{
	socklen_t fromlen = sizeof(sa_other_with_cl);
	int recsize = recvfrom(sock_with_cl, (char *)(buff + buffer_in_with_cl_pos), sz - buffer_in_with_cl_pos, 0, (struct sockaddr *)&sa_other_with_cl, &fromlen);
	if (recsize < 0)
	{
#if defined _MSC_VER || defined __MINGW32__
		int error = WSAGetLastError();
		if ( error == WSAEWOULDBLOCK )
#else
		int error = errno;
		if ( error == EAGAIN || error == EWOULDBLOCK )
#endif
		{
			return COMMLAYER_RET_PENDING;
		}
		else
		{
			ZEPTO_DEBUG_PRINTF_2( "unexpected error %d received while getting message\n", error );
			return COMMLAYER_RET_FAILED;
		}
	}
	else
	{
		buffer_in_with_cl_pos += recsize;
		if ( buffer_in_with_cl_pos < sz )
		{
			return COMMLAYER_RET_PENDING;
		}
		return COMMLAYER_RET_OK;
	}

}

uint8_t try_get_packet_header_within_master_loop( uint8_t* buff, uint16_t sz )
{
	socklen_t fromlen = sizeof(sa_other_with_cl);
	int recsize = recvfrom(sock_with_cl, (char *)(buff + buffer_in_with_cl_pos), sz - buffer_in_with_cl_pos, 0, (struct sockaddr *)&sa_other_with_cl, &fromlen);
	if (recsize < 0)
	{
#if defined _MSC_VER || defined __MINGW32__
		int error = WSAGetLastError();
		if ( error == WSAEWOULDBLOCK )
#else
		int error = errno;
		if ( error == EAGAIN || error == EWOULDBLOCK )
#endif
		{
			return COMMLAYER_RET_PENDING;
		}
		else
		{
			ZEPTO_DEBUG_PRINTF_2( "unexpected error %d received while getting message\n", error );
			return COMMLAYER_RET_FAILED;
		}
	}
	else
	{
		buffer_in_with_cl_pos += recsize;
		if ( buffer_in_with_cl_pos < sz )
		{
			return COMMLAYER_RET_PENDING;
		}
		return COMMLAYER_RET_OK;
	}

}

uint8_t internal_try_get_message_within_master( MEMORY_HANDLE mem_h, uint16_t* bus_id )
{
	// do cleanup
	memory_object_response_to_request( mem_h );
	memory_object_response_to_request( mem_h );
	uint8_t* buff = memory_object_append( mem_h, MAX_PACKET_SIZE );

	buffer_in_with_cl_pos = 0;
	uint8_t ret;

	do //TODO: add delays or some waiting
	{
		ret = try_get_packet_header_within_master_loop( buff, 5 );
	}
	while ( ret == COMMLAYER_RET_PENDING );
	if ( ret != COMMLAYER_RET_OK )
		return ret;
	uint16_t sz = buff[1]; sz <<= 8; sz += buff[0];
	*bus_id = buff[3]; *bus_id <<= 8; *bus_id += buff[2];
	uint8_t packet_src = buff[4];

	buffer_in_with_cl_pos = 0;
	do //TODO: add delays or some waiting
	{
		ret = try_get_packet_within_master_loop( buff, sz );
	}
	while ( ret == COMMLAYER_RET_PENDING );
	if ( ret == COMMLAYER_RET_FAILED )
		return COMMLAYER_FROM_CU_STATUS_FAILED;
	ZEPTO_DEBUG_ASSERT( ret == COMMLAYER_RET_OK );

	memory_object_response_to_request( mem_h );
	memory_object_cut_and_make_response( mem_h, 0, sz );

#ifdef SA_DEBUG
	if ( packet_src == COMMLAYER_FROM_CU_STATUS_FROM_SLAVE )
	{
	uint16_t i;
	uint16_t sz = memory_object_get_response_size( mem_h );
	uint8_t* rsp = memory_object_get_response_ptr( mem_h );
	ZEPTO_DEBUG_PRINTF_1( "PACKET RECEIVED FROM DEVICE: " );
	for ( i=0; i<sz; i++ )
		ZEPTO_DEBUG_PRINTF_2( "%02x ", rsp[i] );
	ZEPTO_DEBUG_PRINTF_1( "\n" );
	}
#endif

	return packet_src;
}

uint8_t try_get_message_within_master( MEMORY_HANDLE mem_h, uint16_t* bus_id )
{
	if ( cscl_is_queued_packet() == COMMLAYER_SYNC_STATUS_OK )
	{
		uint16_t sz;
		uint8_t ret_code, ret_out;
		ret_code = cscl_get_oldest_packet_size( &sz );
		ZEPTO_DEBUG_ASSERT( ret_code == COMMLAYER_SYNC_STATUS_OK );
		zepto_parser_free_memory( mem_h );
		uint8_t* buff = memory_object_prepend( mem_h, sz );
		ZEPTO_DEBUG_ASSERT( buff != NULL );
		ret_code = cscl_get_oldest_packet_and_remove_from_queue( &ret_out, &sz, bus_id, buff, sz );
		ZEPTO_DEBUG_ASSERT( ret_code == COMMLAYER_SYNC_STATUS_OK );
		return ret_out;
	}
	else
	{
		return HAL_INTERNAL_GET_PACKET_BYTES( mem_h, bus_id );
	}
}

uint8_t send_within_master( MEMORY_HANDLE mem_h, uint16_t bus_id, uint8_t destination )
{
	ZEPTO_DEBUG_PRINTF_3( "send_within_master() called: status = %d, sddr = %d...\n", destination, bus_id );

	uint16_t sz = memory_object_get_request_size( mem_h );
	memory_object_request_to_response( mem_h );
	ZEPTO_DEBUG_ASSERT( sz == memory_object_get_response_size( mem_h ) );
if ( sz == 0 )
{
	sz = sz;
}
//	ZEPTO_DEBUG_ASSERT( destination == COMMLAYER_FROM_CU_STATUS_INITIALIZER_LAST || sz != 0 ); // note: any valid message would have to have at least some bytes for headers, etc, so it cannot be empty
	uint8_t* buff = memory_object_prepend( mem_h, 5 );
	ZEPTO_DEBUG_ASSERT( buff != NULL );
	buff[0] = (uint8_t)sz;
	buff[1] = sz >> 8;
	buff[2] = (uint8_t)bus_id;
	buff[3] = bus_id >> 8;
	buff[4] = destination;
	int bytes_sent = sendto(sock_with_cl, (char*)buff, sz+5, 0, (struct sockaddr*)&sa_other_with_cl, sizeof sa_other_with_cl);
	// do full cleanup
	memory_object_response_to_request( mem_h );
	memory_object_response_to_request( mem_h );


	if (bytes_sent < 0)
	{
#if defined _MSC_VER || defined __MINGW32__
		int error = WSAGetLastError();
		ZEPTO_DEBUG_PRINTF_2( "Error %d sending packet\n", error );
#else
		ZEPTO_DEBUG_PRINTF_2("Error sending packet: %s\n", strerror(errno));
#endif
		return COMMLAYER_RET_FAILED;
	}
#if defined _MSC_VER || defined __MINGW32__
	ZEPTO_DEBUG_PRINTF_6( "[%d] message sent within master; mem_h = %d, size = %d, bus_id = %x, destination = %d\n", GetTickCount(), mem_h, sz, bus_id, destination );
#else
	ZEPTO_DEBUG_PRINTF_5( "[--] message sent within master; mem_h = %d, size = %d, bus_id = %x, destination = %d\n", mem_h, sz, bus_id, destination );
#endif
	return COMMLAYER_RET_OK;
}


//uint8_t wait_for_communication_event( unsigned int timeout )
uint8_t internal_wait_for_communication_event( waiting_for* wf )
{
	unsigned int timeout = wf->wait_time.high_t;
	timeout <<= 16;
	timeout += wf->wait_time.low_t;

	// ZEPTO_DEBUG_PRINTF_1( "wait_for_communication_event()\n" );
    fd_set rfds;
    struct timeval tv;
    int retval;
	int fd_cnt;

    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&rfds);

    FD_SET(sock_with_cl, &rfds);
	fd_cnt = (int)(sock_with_cl + 1);

    /* Wait */
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = ((long)timeout % 1000) * 1000;

    retval = select(fd_cnt, &rfds, NULL, NULL, &tv);
    /* Don't rely on the value of tv now! */

    if (retval == -1)
	{
#if defined _MSC_VER || defined __MINGW32__
		int error = WSAGetLastError();
//		if ( error == WSAEWOULDBLOCK )
		ZEPTO_DEBUG_PRINTF_2( "error %d\n", error );
#else
        perror("select()");
//		int error = errno;
//		if ( error == EAGAIN || error == EWOULDBLOCK )
#endif
		ZEPTO_DEBUG_ASSERT(0);
		return COMMLAYER_RET_FAILED;
	}
    else if (retval)
	{
		return COMMLAYER_RET_FROM_CENTRAL_UNIT;
	}
    else
	{
        return COMMLAYER_RET_TIMEOUT;
	}
}

uint8_t wait_for_communication_event( waiting_for* wf )
{
	if ( cscl_is_queued_packet() == COMMLAYER_SYNC_STATUS_OK )
		return COMMLAYER_RET_FROM_CENTRAL_UNIT;
	return HAL_INTERNAL_WAIT_FOR_COMM_EVENT( wf );
}

uint8_t send_message( MEMORY_HANDLE mem_h, uint16_t bus_id )
{
#ifdef SA_DEBUG
	uint16_t i;
	uint16_t sz = memory_object_get_request_size( mem_h );
	uint8_t* rq = memory_object_get_request_ptr( mem_h );
	ZEPTO_DEBUG_PRINTF_1( "PACKET BEING SENT TO DEVICE: " );
	for ( i=0; i<sz; i++ )
		ZEPTO_DEBUG_PRINTF_2( "%02x ", rq[i] );
	ZEPTO_DEBUG_PRINTF_1( "\n" );
#endif
	ZEPTO_DEBUG_ASSERT( bus_id != 0xFFFF );
	return HAL_SEND_WITHIN_MASTER( mem_h, bus_id, COMMLAYER_TO_CU_STATUS_FOR_SLAVE );
}

uint8_t send_to_central_unit( MEMORY_HANDLE mem_h, uint16_t src_id )
{
	return HAL_SEND_WITHIN_MASTER( mem_h, src_id, COMMLAYER_TO_CU_STATUS_FROM_SLAVE );
}

uint8_t send_error_to_central_unit( MEMORY_HANDLE mem_h, uint16_t src_id )
{
	return HAL_SEND_WITHIN_MASTER( mem_h, src_id, COMMLAYER_TO_CU_STATUS_SLAVE_ERROR );
}

uint8_t send_device_initialization_completion_to_central_unit( uint16_t initialization_packet_count, MEMORY_HANDLE mem_h )
{
	return HAL_SEND_WITHIN_MASTER( mem_h, initialization_packet_count, COMMLAYER_TO_CU_STATUS_INITIALIZATION_DONE );
}

uint8_t send_device_add_completion_to_central_unit( MEMORY_HANDLE mem_h, uint16_t packet_id )
{
	return HAL_SEND_WITHIN_MASTER( mem_h, packet_id, COMMLAYER_TO_CU_STATUS_DEVICE_ADDED );
}

uint8_t send_device_remove_completion_to_central_unit( MEMORY_HANDLE mem_h, uint16_t packet_id )
{
	return HAL_SEND_WITHIN_MASTER( mem_h, packet_id, COMMLAYER_TO_CU_STATUS_DEVICE_REMOVED );
}

uint8_t send_stats_to_central_unit( MEMORY_HANDLE mem_h, uint16_t device_id )
{
	return HAL_SEND_WITHIN_MASTER( mem_h, device_id, COMMLAYER_FROM_CU_STATUS_GET_DEV_PERF_COUNTERS_REPLY );
}

void internal_send_sync_request_to_central_unit( MEMORY_HANDLE mem_h )
{
	static uint16_t packet_id = 0;
	packet_id++;
	uint8_t ret_code_send = HAL_SEND_WITHIN_MASTER( mem_h, packet_id, COMMLAYER_TO_CU_STATUS_SYNC_REQUEST );
	ZEPTO_DEBUG_ASSERT( ret_code_send == COMMLAYER_RET_OK );
	sync_status = COMMLAYER_SYNC_STATUS_WAIT_FOR_REPLY;
	waiting_for wf;
	uint8_t ret_code;
	MEMORY_HANDLE tmp_mem_h = acquire_memory_handle();
	ZEPTO_DEBUG_ASSERT( tmp_mem_h != MEMORY_HANDLE_INVALID );
	uint16_t addr;
	uint8_t ret_code_get;
	do
	{
		wf.wait_packet = 1;
		wf.wait_time.high_t = 0xFFFF;
		wf.wait_time.low_t = 0xFFFF;
		ret_code = HAL_INTERNAL_WAIT_FOR_COMM_EVENT( &wf );
		ZEPTO_DEBUG_ASSERT( ret_code == COMMLAYER_RET_FROM_CENTRAL_UNIT ); // with infinite timeout the third option is connection failure
		
		ret_code_get = HAL_INTERNAL_GET_PACKET_BYTES( tmp_mem_h, &addr );
		if ( ret_code_get == COMMLAYER_FROM_CU_STATUS_SYNC_RESPONSE && addr == packet_id )
		{
			zepto_copy_response_to_response_of_another_handle( tmp_mem_h, mem_h );
			break;
		}
		// TODO: check packet
		uint8_t* data = memory_object_get_response_ptr( tmp_mem_h );
		uint16_t sz = memory_object_get_response_size( tmp_mem_h );
		cscl_add_new_packet_to_queue( ret_code_get, sz, addr, data );
	}
	while ( 1 );
	release_memory_handle( tmp_mem_h );
}

void send_sync_request_to_central_unit_to_save_data( MEMORY_HANDLE mem_h, uint16_t deice_id )
{
	// packet_structure: | command (1 byte) | row_id (2 bytes, low, high; usually, device_id) | field_id (1 byte) | data_sz (2 bytes, low, high) | data (variable size) |
	uint16_t sz = memory_object_get_response_size( mem_h );
	uint8_t* prefix = memory_object_prepend( mem_h, 6 );
	prefix[0] = REQUEST_TO_CU_WRITE_DATA;
	prefix[1] = (uint8_t)deice_id;
	prefix[2] = (uint8_t)(deice_id>>8);
	prefix[3] = 0;
	prefix[4] = (uint8_t)sz;
	prefix[5] = (uint8_t)(sz>>8);
	zepto_response_to_request( mem_h );
	internal_send_sync_request_to_central_unit( mem_h );
	// TODO: check response
}

void send_sync_request_to_central_unit_to_get_data( MEMORY_HANDLE mem_h, uint16_t deice_id )
{
	// packet_structure: | command (1 byte) | row_id (2 bytes, low, high; usually, device_id) | field_id (1 byte) |
	zepto_parser_free_memory( mem_h );
	uint8_t* prefix = memory_object_prepend( mem_h, 4 );
	prefix[0] = REQUEST_TO_CU_READ_DATA;
	prefix[1] = (uint8_t)deice_id;
	prefix[2] = (uint8_t)(deice_id>>8);
	prefix[3] = 0;
	zepto_response_to_request( mem_h );
	internal_send_sync_request_to_central_unit( mem_h );
	ZEPTO_DEBUG_ASSERT( memory_object_get_response_size( mem_h ) >= 6 );

	zepto_response_to_request( mem_h );
	// packet_structure: | command (1 byte) | row_id (2 bytes, low, high; usually, device_id) | field_id (1 byte) | data_sz (2 bytes, low, high) | data (variable size) |
	uint8_t prefix_out[6];
	parser_obj po, po1;
	zepto_parser_init( &po, mem_h );
	zepto_parse_read_block( &po, prefix_out, 6 );
	uint16_t declared_data_sz = prefix_out[5];
	declared_data_sz <<= 8;
	declared_data_sz += prefix_out[4];
	// TODO: check response
	zepto_parser_init_by_parser( &po1, &po );
	uint16_t actual_data_sz = zepto_parsing_remaining_bytes( &po );
	ZEPTO_DEBUG_ASSERT( declared_data_sz == actual_data_sz ); // TODO: think about error handling/reporting instead
	zepto_parse_skip_block( &po1, actual_data_sz );
	zepto_convert_part_of_request_to_response( mem_h, &po, &po1 );
}
