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
#include "endpoint.h"
#include "sn_nsdl.h"
#include "sn_coap_header.h"
#include "sn_coap_protocol.h"
#include "sn_nsdl_lib.h"
#include "nsdl_helper.h"
#include "net_interface.h"
#include "net_rpl.h"
#include "ip6string.h"
#include "node_tasklet.h"


static const char res_6lowpan_ipaddr[] = "/nw/6lowpan/ipaddr";     // IPv6 address of the node
static const char res_6lowpan_pipaddr[] = "/nw/6lowpan/pipaddr";

#define RESPONSE_BUF_SIZE 56
static char response[RESPONSE_BUF_SIZE];

/* Prototypes */
static uint8_t ip_address_request(struct nsdl_s *handle, sn_coap_hdr_s *coap, sn_nsdl_addr_s *address, sn_nsdl_capab_e proto);
static uint8_t parent_ip_address_request(struct nsdl_s *handle, sn_coap_hdr_s *coap, sn_nsdl_addr_s *address, sn_nsdl_capab_e proto);

/*
 * NETWORK RESOURCES
 */
void create_nw_resources()
{
	create_dynamic_resource(res_6lowpan_ipaddr, RESOURCE_TYPE_IPV6, SN_GRS_GET_ALLOWED, 0, ip_address_request);
	create_dynamic_resource(res_6lowpan_pipaddr, RESOURCE_TYPE_IPV6, SN_GRS_GET_ALLOWED, 0, parent_ip_address_request);
}

/***** IP ADDRESS QUERIES *****/
static void get_6lowpan_ip_addr(char *buf)
{
    static uint8_t binary_ipv6[16];
    buf[0] = '\0';

    if (0 == arm_net_address_get(net_rf_id, ADDR_IPV6_GP, binary_ipv6))
    {
    	ip6tos(binary_ipv6, buf);
    }
}

static void get_6lowpan_parent_ip_addr(char *buf)
{
	uint8_t instance_id;
	rpl_dodag_info_t dodag_info;
	
    buf[0] = '\0';
	
    if(!rpl_instance_list_read(&instance_id, 1))
    	return;
		
    if (rpl_read_dodag_info(&dodag_info, instance_id))
    {
    	if(dodag_info.parent_flags & RPL_PRIMARY_PARENT_SET)
    	{
    		ip6tos(dodag_info.primary_parent, buf);
    	}
    }
}

/*********** MESSAGE BUILDER FUCTIONS **************/
static uint8_t ip_address_request(struct nsdl_s *handle, sn_coap_hdr_s *coap, sn_nsdl_addr_s *address, sn_nsdl_capab_e proto)
{
	(void)proto;
	(void)handle;

    get_6lowpan_ip_addr(response);

    send_coap_text_response(coap, address, response, 0);

    return 0;
}

static uint8_t parent_ip_address_request(struct nsdl_s *handle, sn_coap_hdr_s *coap, sn_nsdl_addr_s *address, sn_nsdl_capab_e proto)
{
	(void)proto;
	(void)handle;

    get_6lowpan_parent_ip_addr(response);

    send_coap_text_response(coap, address, response, 0);

    return 0;
}


