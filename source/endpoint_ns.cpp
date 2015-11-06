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
#include <string.h>
#include "endpoint.h"
#include "sn_nsdl.h"
#include "sn_coap_header.h"
#include "sn_coap_protocol.h"
#include "sn_nsdl_lib.h"
#include "nsdl_helper.h"
#include "config.h"
#include "ip6string.h"

#define BUFF_SIZE 128

/* Nanoservice resources */
static const char res_nspaddr[] = "/ns/nspaddr";

/* Prototypes */
static uint8_t nsp_address_request(struct nsdl_s *handle, sn_coap_hdr_s *coap, sn_nsdl_addr_s *address, sn_nsdl_capab_e proto);

/**
 * Register NS resources.
 */
void create_ns_resources(void)
{
	create_dynamic_resource(res_nspaddr, RESOURCE_TYPE_IPV6, SN_GRS_GET_ALLOWED, 0, nsp_address_request);
}


/*************** CALLBACKS ********************/

static void send_text_address(sn_coap_hdr_s *coap, sn_nsdl_addr_s *address)
{
	static char addr[BUFF_SIZE];
	snprintf(addr, BUFF_SIZE, "[%s]:%u", cfg_string(global_config, "NSP", "::1"), cfg_int(global_config, "NSP_PORT", 5683));
	send_coap_text_response(coap, address, addr, PRIO_AF21);
}

static void send_binary_address(sn_coap_hdr_s *coap, sn_nsdl_addr_s *address)
{
	static uint8_t addr[18];
	static sn_coap_hdr_s *resp;
	const char *nsp;
	uint16_t port;
	
	nsp = cfg_string(global_config, "NSP", "::1");
	port = (uint16_t)cfg_int(global_config, "NSP_PORT", 5683);
	stoip6(nsp, strlen(nsp), addr);
	addr[16] = (uint8_t) (port>>8)&0xff;
	addr[17] = (uint8_t) port & 0xff;

	resp = sn_nsdl_build_response(nsdl_handle, coap, (uint8_t)COAP_MSG_CODE_RESPONSE_CONTENT);

	resp->payload_ptr = (uint8_t*)addr;
	resp->payload_len = sizeof(addr);
	sn_nsdl_send_coap_message(nsdl_handle, address, resp);
	sn_nsdl_release_allocated_coap_msg_mem(nsdl_handle, resp);
}

static uint8_t nsp_address_request(struct nsdl_s *handle, sn_coap_hdr_s *coap, sn_nsdl_addr_s *address, sn_nsdl_capab_e proto)
{
	(void)proto;
	(void)handle;

	send_text_address(coap, address);

	return 0;
}
