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
#ifndef _ENDPOINT_H
#define _ENDPOINT_H

#include <stdint.h>
#include "sn_coap_header.h"
#include "sn_nsdl.h"

#define DEBUG(x,fmt,...) printf(fmt, ##__VA_ARGS__)

/* Resource types */
#define RESOURCE_TYPE_IPV6 "ns:v6addr"
#define RESOURCE_TYPE_TEXT "t"
#define RESOURCE_TYPE_NONE "none"

#define PRIO_CS0	(0x00)
#define PRIO_AF21 	(0x48)
#define PRIO_AF12 	(0x30)
#define PRIO_DEFAULT PRIO_CS0

typedef enum {
	IF_BACKHAUL,
	IF_6LOWPAN,
} interface_id;

extern const char *nsp_addr;
extern const uint16_t nsp_port;
extern struct nsdl_s *nsdl_handle;

/* Resource registration functions */
void create_nw_resources(void); /* Resources from endpoint_nw.c */
void create_ns_resources(void); /* Resources from endpoint_ns.c */
void create_hw_resources(void); /* Resources from endpoint_hw.c */


/* Helper functions used in endpoint */
int uri_match(sn_coap_hdr_s *coap, const char *s);
void send_coap_text_response(sn_coap_hdr_s *coap, sn_nsdl_addr_s *address, const char *payload, int16_t traffic_class);
void send_coap_unhandled_response(sn_coap_hdr_s *coap, sn_nsdl_addr_s *address);
void send_coap_error_response(sn_coap_hdr_s *coap, sn_nsdl_addr_s *address);
char *getmac(interface_id id); /**< Get mac address of linux interface. */
char *str_replace(const char *src, const char *search, const char *replace); /**< Search&Replace string inside of another. */
int8_t get_endnode_tasklet_id(void);

#endif /* _ENDPOINT_H */
