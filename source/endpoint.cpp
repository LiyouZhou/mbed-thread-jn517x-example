/*
 * Copyright (c) 2014-2015 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eventOS_event.h"
#include "eventOS_event_timer.h"
#include "sn_nsdl.h"
#include "sn_coap_header.h"
#include "sn_coap_protocol.h"
#include "sn_nsdl_lib.h"
#include "nsdl_helper.h"
#include "socket_api.h"
#include "ns_address.h"
#include "endpoint.h"
#include "net_interface.h"
#include "router.h"
#include "ns_trace.h"
#include "ip6string.h"
#include "config.h"
#include "static_config.h"

/* Resource paths and registration parameters */
static const char res_mgf[] = "/dev/mfg";
static const char res_mdl[] = "/dev/mdl";
static const char ep_type[] = "Node";

#define RECONNECT_TIMEOUT 30000

/* Function templates */
static void parse_coap(uint8_t *payload, int16_t length, ns_address_t addr);
static void udb_socket_callback(void *cb);
uint8_t tx_function(struct nsdl_s *handle, sn_nsdl_capab_e protocol, uint8_t *data_ptr, uint16_t data_len, sn_nsdl_addr_s *dst);
static int init_libs(void);
static int open_socket(void);
static void create_resources(void);
static void send_registration(void);
static int get_nsp_address(void);

/* NanoStack globals */
int8_t endpoint_tasklet_id = -1;

extern uint8_t access_point_status;		/* Variable where this application keep connection status of an access point: 0 = No Connection, 1 = Connection established */

/* Socket globals */
int8_t sock_nosecure;

/* CoAP related globals*/
static uint16_t nsdl_clock = 0;
struct nsdl_s *nsdl_handle;

/*Timer definitions*/
enum endpoint_events
{
	START_ENDPOINT=1,
	UPDATE_EVENT,
};

enum endpoint_timers
{
	REGISTRATION_TIMER,
	RE_REGISTRATION_TIMER,
	NSDL_TIMER,
	COUNTER_TIMER,
	CALCULATE_ECC,
	RECONNECT_TIMER,
};

/* Resource related globals*/
uint8_t *reg_location = 0;
int8_t reg_location_len;
uint8_t obs_token[8];
uint8_t obs_token_len = 0;

static int get_nsp_address()
{
	const char *addr;
	uint8_t address[16];

	addr = cfg_string(global_config, "NSP", "::1");
	stoip6(addr, strlen(addr), address);

	set_NSP_address(nsdl_handle, (uint8_t *)address, cfg_int(global_config, "NSP_PORT", 5683), SN_NSDL_ADDRESS_TYPE_IPV6);
	return 0;
}

static void parse_coap(uint8_t *payload, int16_t length, ns_address_t addr)
{
	sn_nsdl_addr_s nsdl_addr;
	nsdl_addr.addr_ptr = addr.address;
	nsdl_addr.type = SN_NSDL_ADDRESS_TYPE_IPV6;
	nsdl_addr.port = addr.identifier;
	nsdl_addr.addr_len = 16;
	if (0 != sn_nsdl_process_coap(nsdl_handle, payload, length, &nsdl_addr))
	{
		DEBUG(ERROR, "sn_nsdl_process_coap() failed\n");
	}
}

static void udb_socket_callback(void *cb)
{
	uint8_t *payload;
	socket_callback_t *s = (socket_callback_t *) cb;
	ns_address_t addr;
	int16_t length;

	if(s->event_type == SOCKET_DATA)
	{

		if ( s->d_len > 0)
		{
			payload = (uint8_t *) own_malloc(s->d_len);
			if (payload)
			{
				length = socket_read(s->socket_id, &addr, payload, s->d_len);
				parse_coap(payload, length, addr);
				own_free(payload);
			}
		}
		else
		{
			DEBUG(ERROR,"RX socket %hhu, No Data\n", s->socket_id);
		}
	}
	else if(s->event_type != SOCKET_TX_DONE)
	{
		/* No Route to Packet Destination Address */
		if(s->event_type == SOCKET_NO_ROUTE)
		{
			DEBUG(ERROR,"ND/RPL not ready\n");
		}
		/* Link Layer TX Fail for socket packet */
		else if(s->event_type == SOCKET_TX_FAIL)
		{
			DEBUG(ERROR,"Link Layer Tx fail\n");
		}
	}
}

/**
 * Transmit COAP content.
 * Called from libCoap handlers.
 * NSP data is pushed to EDTLS connection
 * \return EDTLS_SUCCESS on success.
 */
uint8_t tx_function(struct nsdl_s *handle, sn_nsdl_capab_e protocol, uint8_t *data_ptr, uint16_t data_len, sn_nsdl_addr_s *dst)
{
	ns_address_t addr;
	(void)protocol;
	(void)handle;

	addr.type = ADDRESS_IPV6;
	addr.identifier = dst->port;
	memcpy(addr.address,dst->addr_ptr,16);

	DEBUG(INFO, "TX to port %hu\n", addr.identifier);

	switch(socket_sendto(sock_nosecure, &addr, data_ptr, data_len))
	{
		case 0:
			return data_len;
		case -1:
			DEBUG(ERROR, "sendto: Invalid socket ID\n");
			break;
		default:
			break;
			DEBUG(ERROR, "sendto() failed, socket=%hhd, data=%p len=%hu\n", sock_nosecure, data_ptr, data_len);
	}
return 0;
}


/*
 *
 */
static int init_libs()
{
	/* Initialize the libNsdl */
	nsdl_handle = sn_nsdl_init(&tx_function, &rx_function, &own_malloc, &own_free);

	if(NULL == nsdl_handle)
	{
		DEBUG(ERROR,"Failed to initialize NSDL library\n");
		return -EXIT_FAILURE;
	}
	get_nsp_address();

	return 0;
}

/*
 * Initialize network socket
 */
static int open_socket()
{
	uint16_t nonsecure_port  = 5683;
	int8_t s;

	/* Open listening socket */
	s = socket_open(SOCKET_UDP, nonsecure_port, udb_socket_callback);
	if (s < 0)
	{
		DEBUG(ERROR, "Cannot open socket (port=%hu)\n", nonsecure_port);
		return -2;
	}
	sock_nosecure = s;

	return 0;
}

/* strdup() is not defined in ArmCC and malloc() should not be used in embedded */
static char *my_strdup(const char *s)
{
	char *p;
	p = (char*)own_malloc(strlen(s)+1);
	if (!p)
		return NULL;
	strcpy(p, s);
	return p;
}

/*
 * Search&Replace string inside of another.
 * \return new string that must be freed after use
 */
char *str_replace(const char *src, const char *search, const char *replace)
{
	char *s,*p,*tmp;
	s = (char*)own_malloc(strlen(src)+strlen(replace)+1);
	if(!s)
		return NULL;

	*s = 0;
	tmp = my_strdup(src);

	p = strstr(tmp, search);
	if (p)
	{
		/* Cut string, where text is found */
		*p = 0;
		p+=strlen(search); /* Skip searched text */
		strcat(s,tmp); /* copy prefix */
		strcat(s,replace); /* Copy replace test */
		strcat(s,p); /* append rest of string */
	}
	else
	{
		strcat(s, tmp); /* Not found, copy unmodified */
	}
	own_free(tmp);
	return s;
}

/*
 * Initialize sockets
 */
static void create_resources()
{
	char *model;
	char *manufacturer;

	manufacturer = (char*)cfg_string(global_config, "MANUFACTURER", "");
	model = (char*)cfg_string(global_config, "MODEL", "Nanorouter %V");
	/* Static device resources */
	create_static_resource(res_mgf, RESOURCE_TYPE_TEXT, manufacturer);
	create_static_resource(res_mdl, RESOURCE_TYPE_TEXT, model);

	create_nw_resources(); /* Resources from endpoint_nw.c */
	create_ns_resources(); /* Resources from endpoint_ns.c */
	create_hw_resources(); /* Resources from endpoint_hw.c */
}

/*
 * Send NSP registration message
 */
static void send_registration(void)
{
	static int lifetime = 600;
	static char *name = NULL;
	if (!lifetime)
		lifetime = cfg_int(global_config, "NSP_LIFETIME", 120);
	if (!name)
	{
		char *mac = getmac(IF_BACKHAUL);
		name = (char*)cfg_string(global_config, "NAME", "router-%M");
		if (mac)
		{
			char* appended_name = str_replace(name, "%M", mac);
			if(appended_name)
				name = appended_name;
			own_free(mac);
		}
	}
	DEBUG(INFO, "NSP Registration: %s\n", name);
	register_endpoint(name, ep_type, lifetime);

}

/*
 * Endpoint tasklet. Started from Nanostack.
 * Should initialize NSP connection and required timers.
 */
void endpoint_tasklet(arm_event_s *event)
{

	static uint8_t re_register = 0;

	if (ARM_LIB_TASKLET_INIT_EVENT == event->event_type)
	{
		DEBUG(INFO,"endpoint_tasklet(): init\n");
		endpoint_tasklet_id = event->receiver; // Save my event id for later use.
		if (0 != init_libs())
		{
			DEBUG(ERROR,"failed to initialize endpoint libraries\n");
			return;
		}

		create_resources();
		if(open_socket() != 0)
		{
			DEBUG(ERROR, "Failed to open socket!\n");
			return;
		}
		return;
	}
	else if (event->event_type == APPLICATION_EVENT)
	{
		switch(event->event_id)
		{
			case START_ENDPOINT:
				eventOS_event_timer_cancel(NSDL_TIMER, endpoint_tasklet_id);
				eventOS_event_timer_request(REGISTRATION_TIMER,ARM_LIB_SYSTEM_TIMER_EVENT,endpoint_tasklet_id, 1);

				eventOS_event_timer_request(NSDL_TIMER,ARM_LIB_SYSTEM_TIMER_EVENT,endpoint_tasklet_id, 1000);
				break;

		}
	}
	else if (ARM_LIB_SYSTEM_TIMER_EVENT == event->event_type)
	{
		switch(event->event_id)
		{
			case REGISTRATION_TIMER:
				if(access_point_status)
				{
					if(sn_nsdl_is_ep_registered(nsdl_handle) && !re_register)
					{
						eventOS_event_timer_request(RE_REGISTRATION_TIMER,ARM_LIB_SYSTEM_TIMER_EVENT,endpoint_tasklet_id, 200000);
					}
					else
					{
						re_register = 0;
						send_registration();
						eventOS_event_timer_request(REGISTRATION_TIMER,ARM_LIB_SYSTEM_TIMER_EVENT,endpoint_tasklet_id, 5000);
					}
				}
				else
				{
					eventOS_event_timer_request(RE_REGISTRATION_TIMER,ARM_LIB_SYSTEM_TIMER_EVENT,endpoint_tasklet_id, 4000);
					debug("No NW -> Reg fail\n");
				}
				break;
			case RE_REGISTRATION_TIMER:
				re_register = 1;
				eventOS_event_timer_request(REGISTRATION_TIMER,ARM_LIB_SYSTEM_TIMER_EVENT,endpoint_tasklet_id, 1000);
				break;
			case NSDL_TIMER:
				sn_nsdl_exec(nsdl_handle, nsdl_clock++);
				eventOS_event_timer_request(NSDL_TIMER,ARM_LIB_SYSTEM_TIMER_EVENT,endpoint_tasklet_id, 1000);
				break;
			case RECONNECT_TIMER:
				eventOS_event_timer_cancel(RECONNECT_TIMER, endpoint_tasklet_id);
				send_signal_start_endpoint();
				break;
			default:
				break;
		}
	}
}

int8_t get_endnode_tasklet_id(void)
{
	return endpoint_tasklet_id;
}

void send_signal_start_endpoint(void)
{
	if(endpoint_tasklet_id == -1)
	{
		endpoint_tasklet_id = eventOS_event_handler_create(&endpoint_tasklet, ARM_LIB_TASKLET_INIT_EVENT);
	}
	arm_event_s event;
	event.sender = endpoint_tasklet_id;
	event.event_type = APPLICATION_EVENT;
	event.receiver = endpoint_tasklet_id;
	event.event_id = 1;
	event.data_ptr = NULL;
	event.priority = ARM_LIB_LOW_PRIORITY_EVENT;
	eventOS_event_send(&event);
}

/* RX function for libNsdl. Passes CoAP responses sent from application to this function. Also response to registration message */
uint8_t rx_function(struct nsdl_s *handle, sn_coap_hdr_s *coap_header, sn_nsdl_addr_s *address_ptr)
{
	(void)address_ptr;
	(void)coap_header;
	(void)handle;
	return 0;
}

static int8_t set_socket_traffic_class(int16_t tc)
{
	return socket_setsockopt(sock_nosecure, SOCKET_IPPROTO_IPV6,SOCKET_IPV6_TCLASS, &tc,sizeof(tc));
}

/**
 * Send simple text response to coap query
 */
void send_coap_text_response(sn_coap_hdr_s *coap, sn_nsdl_addr_s *address, const char *payload, int16_t traffic_class)
{
	sn_coap_hdr_s *resp;
	resp = sn_nsdl_build_response(nsdl_handle, coap, COAP_MSG_CODE_RESPONSE_CONTENT);
	resp->payload_ptr = (uint8_t*)payload;
	resp->payload_len = strlen(payload);
	if (traffic_class)
		set_socket_traffic_class(traffic_class);
	sn_nsdl_send_coap_message(nsdl_handle, address, resp);
	if (traffic_class)
		set_socket_traffic_class(0);
	sn_nsdl_release_allocated_coap_msg_mem(nsdl_handle, resp);
}

/**
 * Get MAC address of interface.
 * Wrapper to check either linux interface mac or radio mac if iface name is TUN interface
 * \return string containing mac address. Must be freed after use.
 */
char *getmac(interface_id id)
{
	int8_t iface = id;
	link_layer_address_s mac;

	if(0 == arm_nwk_mac_address_read(iface, &mac))
	{
		char *ret = (char*)own_malloc(MAC_STRING_LENGTH);
		int i;
		for (i = 0; i < 8; ++i)
			snprintf(ret+i*2,MAC_STRING_LENGTH-i*2,"%02x", mac.mac_long[i]);
		DEBUG(INFO,"Requested 6lowpan mac: %s\n", ret);
		return ret;
	}
	else
	{
		DEBUG(ERROR,"arm_nwk_mac_address_read() failed!\n");
	}
	return NULL;
}
