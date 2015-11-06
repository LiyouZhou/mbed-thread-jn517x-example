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

#ifndef NSDL_HELPER_H_
#define NSDL_HELPER_H_

#include <stddef.h>
#include "ns_address.h"
#include "sn_nsdl.h"
#include "sn_nsdl_lib.h"
#include "sn_coap_header.h"
#include "sn_coap_protocol.h"

typedef uint8_t coap_callback(struct nsdl_s *, sn_coap_hdr_s *, sn_nsdl_addr_s *, sn_nsdl_capab_e);

int open_listening_socket(uint16_t port, int family);
int receive_msg(int socket, void *buf, size_t len);
uint8_t rx_function(struct nsdl_s *handle, sn_coap_hdr_s *coap_header, sn_nsdl_addr_s *address_ptr);

int create_static_resource(const char *path, const char *type, const char *value);
int create_dynamic_resource(const char *path, const char *type, int flags, char is_observable, coap_callback *callback);
int register_endpoint(const char *name, const char *type_name, unsigned int lifetime);
void *own_malloc(uint16_t len);
void own_free(void *ptr);

#endif /* NSDL_HELPER_H_ */
