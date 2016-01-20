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

v0.0g

This file contains PRELIMINARY notes on packet exchange with CommStackServer

NOTE: it is assumed and implemented in a way that CommStackServer has no own means for communication with devices and rather processes packets received externally for/from respective devices.
Neither it has own permanent storage means (but rather requests to store or read some data).

I Data stored externally

All data necessary for CommStackServer to function is stored externally. 
Currently this data is only specific to devices in the system and thus can be stored on a per-device basis.
There are two types of data: initialization data, and data generated while CommStackServer works.
The first is supplied during CommStackServer initialization packet exchange (sdee below).
The second is stored in a single field of a variable size and is available for reading/writing on demand.
Details of implementation of a storage to hold this data is beyond this document and is up to implementer as long as it provides required services.

II. Packet exchange

In present implementation CommStackServer resides in a separate process and communication with it is done in a form of TCP packet exchange.
Each packet has a prefix and payload that depends on a nature of a packet: | prefix | payload |
Prefix has the following structure: | size (2 bytes, low, high) | address (2 bytes, low, high) | type (1 byte) | 
where 'size' is a size of payload in bytes, and meaning of 'address' depends on a value of 'type' (will be discussed below in more details).
Numerical values of types are TBD.

There are two phases of such communication: initialization and further packet exchange

1. Initialization

When CommStackServer is run, it must be initialized. During initialization a number of packets with initialization data are sent terminating with a packet indicating the end of initialization process.
Initialization data packet has its 
type = COMMLAYER_FROM_CU_STATUS_INITIALIZER, 
and packet terminating initialization has its
type = COMMLAYER_FROM_CU_STATUS_INITIALIZER_LAST,
whyle the 'address' starts from 0 and must be incremented with each next packet 
(thus the value of 'address' in a packet with type = COMMLAYER_FROM_CU_STATUS_INITIALIZER_LAST will give a number of previously sent packets with type = COMMLAYER_FROM_CU_STATUS_INITIALIZER).
Payload of each COMMLAYER_FROM_CU_STATUS_INITIALIZER packet has the following structure:
| device_id (2 bytes, low, high) | encryption_key (16 bytes) | is_retransmitter (1 byte) | bus_count (1 byte) | bus_type_count (1 byte) | bus_types (variable size) |
weherein bus_count is a value such that each bus_id of the device is in the range [0 .. bus_count-1].
This information is typically generated at time of device programming.
COMMLAYER_FROM_CU_STATUS_INITIALIZER_LAST packet has no payload.

Untill initialization is done packets of no other types should be sent to CommStackServer.

When initialization is done CommStackServer send a packet of type COMMLAYER_TO_CU_STATUS_INITIALIZATION_DONE 
with 'address' set to 
- the value of 'address' of COMMLAYER_FROM_CU_STATUS_INITIALIZER_LAST packet in case of successfull initialization, or
- the value of 'address' of a failing packet,
and payload consisting of error code: | error_code (1 byte) |
wherein error_code is one of
COMMLAYER_TO_CU_STATUS_OK: operation completed successfully;
COMMLAYER_TO_CU_STATUS_FAILED_UNEXPECTED_PACKET: operation failed; in this case 'address' will be that of a packet causing error.
COMMLAYER_TO_CU_STATUS_FAILED_INCOMPLETE_OR_CORRUPTED_DATA: operation failed; a packet is with incomplete or corrupted data.
In case of any error during initialization phase CommStackServer process terminates.
Note that in case of any error a packet COMMLAYER_TO_CU_STATUS_INITIALIZATION_DONE can be sent yet before the packet COMMLAYER_FROM_CU_STATUS_INITIALIZER_LAST is received by CommStaackServer.


2. Regular packet exchange

2.1. Packets to CommStackServer

Packets coming from devices are payloads of a packet of type COMMLAYER_FROM_CU_STATUS_FROM_SLAVE. The value of 'address' for this type of a packet is a bus ID from which the packet has been received at a calling side.
Packets coming to be processed for sending to a particular device are of type COMMLAYER_FROM_CU_STATUS_FOR_SLAVE, and the value of 'address' is a device_id of the intended device.

2.2. Packets from CommStackServer

Packet prepared for sending to a device is a payloads of a packet of type COMMLAYER_TO_CU_STATUS_FOR_SLAVE, and 'address' is a bus_id to use.
Packet received from a device after processing is sent back as payload of a packet of type COMMLAYER_TO_CU_STATUS_FROM_SLAVE, and 'address' id a device_id of the source device.

3. Sychronous requests.

When a data is to be stored externally or read from external permanent storage, a synchronous request is sent. 
A word 'synchronous' is more related to CommStackServer; its meaning is that until response is received no other packets are processed (but stored in a queue).
Synchronous requests reside in packets of type COMMLAYER_TO_CU_STATUS_SYNC_REQUEST; and 'address' is a packet number that should be supplied with response.
Response to a synchronous request reside in packets of type COMMLAYER_FROM_CU_STATUS_SYNC_RESPONSE, and the 'address' repeats the value of 'address' of a packet with respective request.
Payload of each packet has a structure | command_type (1 byte) | type_dependent_data (variable size) |

Currently there are two types of 'synchronous' requests: to write and to read data.

Payload of a request to write data has the following structure: 
| command = REQUEST_TO_CU_WRITE_DATA | row_id (2 bytes, low, high; usually, device_id) | reserved = 0 (1 byte) | data_sz (2 bytes, low, high) | data (variable size) |
Payload of a request to read data has the following structure: 
| command = REQUEST_TO_CU_READ_DATA | row_id (2 bytes, low, high; usually, device_id) | reserved = 0 (1 byte) |

Payloads of responses to respective requests are structured as follows:
Payload of a response to a request to write data:
| command = RESPONSE_FROM_CU_WRITE_DATA | row_id (2 bytes, low, high; usually, device_id) | reserved = 0 (1 byte) | data_sz (2 bytes, low, high) |
Payload of a response to a request to read data:
| command = RESPONSE_FROM_CU_READ_DATA | row_id (2 bytes, low, high; usually, device_id) | reserved = 0 (1 byte) | data_sz (2 bytes, low, high) | data (variable size) |

4. Dynamical device adding/removing

After CommStackServer is initialized, new devices can be added or existing devices can be removed.

To add a device a packet of type COMMLAYER_FROM_CU_STATUS_ADD_DEVICE is sent. The value of 'address' is a unique number. Its payload is field 0 for a particular device (see above).
In response to this request a packet of type COMMLAYER_TO_CU_STATUS_DEVICE_ADDED is sent; its 'address' field is a copy of that field from a respective request,
and its payload contains ID of the device added: | device_id (2 bytes, low, high ) | error_code (1 byte) |
wherein error_code is one of the following values:
COMMLAYER_TO_CU_STATUS_OK: operation successfull
COMMLAYER_TO_CU_STATUS_FAILED_EXISTS: a device with device_id specified in the request is already added (repeated attempt to add?)
COMMLAYER_TO_CU_STATUS_FAILED_INCOMPLETE_OR_CORRUPTED_DATA: payload of the request is inconsistent
COMMLAYER_TO_CU_STATUS_FAILED_UNKNOWN_REASON: any other reason to fail (should never happen)

NOTE: from time COMMLAYER_FROM_CU_STATUS_ADD_DEVICE is received and COMMLAYER_TO_CU_STATUS_DEVICE_ADDED is sent a number of 'synchronous' requests to read or write data can be sent to Client

To remove a device a packet of type COMMLAYER_FROM_CU_STATUS_REMOVE_DEVICE is sent. The value of 'address' is a unique number. Its payload is | device_id (2 byets, low, high) | of a device to be removed.
In response to this request a packet of type COMMLAYER_TO_CU_STATUS_DEVICE_REMOVED is sent; its 'address' field is a copy of that field from a respective request,
and its payload contains ID of the device removed: | device_id (2 bytes, low, high ) |
NOTE: from time COMMLAYER_FROM_CU_STATUS_REMOVE_DEVICE is received and COMMLAYER_TO_CU_STATUS_DEVICE_REMOVED is sent a number of 'synchronous' requests to read or write data can be sent to Client
TODO: think about error reporting

5. Error reporting

Currently a packet of type COMMLAYER_TO_CU_STATUS_SLAVE_ERROR is used to let the Client know that packet exchange chain with a device is broken for any reason. 
In this case the Client is supposed to start a new chain.
Packet with this status has its 'address' field filled with a device_id of a failed device; and its payload is case-specific.

6. Requesting device stats

Client can send a packet of type COMMLAYER_FROM_CU_STATUS_GET_DEV_PERF_COUNTERS_REQUEST to request values of performance counters. 
In current implementation this packet has no payload, and its 'address' is equal to ID of the device to get counters from.
This request should be treated as asynchronous.
 
In response to COMMLAYER_FROM_CU_STATUS_GET_DEV_PERF_COUNTERS_REQUEST CommStackServer sends a packet of type COMMLAYER_FROM_CU_STATUS_GET_DEV_PERF_COUNTERS_REPLY.
Its 'address' is set to the ID of the device, and its payload depends on firmware of the device (CommStackServer just forwards the data).
Note: getting response to COMMLAYER_FROM_CU_STATUS_GET_DEV_PERF_COUNTERS_REQUEST may take substantial time (depends on quality of connection on the way to the device of interest)

-------
TBC...






