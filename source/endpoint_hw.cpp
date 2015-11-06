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
#include "mbed.h"
#include "endpoint.h"
#include "sn_nsdl.h"
#include "sn_coap_header.h"
#include "sn_coap_protocol.h"
#include "sn_nsdl_lib.h"
#include "nsdl_helper.h"
#include "config.h"
#include "ip6string.h"

#define S1_BUTTON 1

#if defined(TARGET_K64F)
#define APP_BUTTON SW2
#elif defined(TARGET_NUCLEO_F401RE)
#define APP_BUTTON USER_BUTTON
#elif defined(TARGET_JN517X)
#define APP_BUTTON SW2
#else
#define APP_BUTTON D4
#endif

DigitalOut led[] = {
	DigitalOut(LED1),
	DigitalOut(LED2),
	DigitalOut(LED3),
};

int led_states[][3] = {
	{0, 0, 0},
	{1, 0, 0},
	{1, 1, 0},
	{1, 1, 1},
	{0, 1, 1},
	{0, 0, 1},
};

InterruptIn button1(SW3);
InterruptIn button2(SW2);

/* Nanoservice resources */
static const char res_button[] = "/hw/button";
static const char res_led[] = "/hw/led";

/* Notification related stuff */
static uint8_t token_number[16];
static uint8_t token_len;
static uint8_t observation_number = 0;

/* Prototypes */
static uint8_t button_callback(struct nsdl_s *handle, sn_coap_hdr_s *coap, sn_nsdl_addr_s *address, sn_nsdl_capab_e proto);
static uint8_t led_callback(struct nsdl_s *handle, sn_coap_hdr_s *coap, sn_nsdl_addr_s *address, sn_nsdl_capab_e proto);
static void buttonHandler(void);

/**
 * Register NS resources.
 */
void create_hw_resources(void)
{
	create_dynamic_resource(res_button, RESOURCE_TYPE_NONE, SN_GRS_GET_ALLOWED, 1, button_callback);
	create_dynamic_resource(res_led, RESOURCE_TYPE_NONE, (SN_GRS_GET_ALLOWED|SN_GRS_PUT_ALLOWED) , 0, led_callback);
	button1.fall(&buttonHandler);
	button2.fall(&buttonHandler);
}


/*************** CALLBACKS ********************/
static uint8_t button_callback(struct nsdl_s *handle, sn_coap_hdr_s *coap, sn_nsdl_addr_s *address, sn_nsdl_capab_e proto)
{
	(void)proto;
	(void)handle;
	sn_coap_hdr_s *response;

	response = sn_nsdl_build_response(nsdl_handle, coap, COAP_MSG_CODE_RESPONSE_CONTENT);
	if (!response)
		return 1;

	if(coap->options_list_ptr && coap->options_list_ptr->observe)
	{
		if(coap->token_len)
		{
			token_len = (coap->token_len>16?16:coap->token_len);
			memcpy(token_number, coap->token_ptr, token_len);
			response->options_list_ptr = (sn_coap_options_list_s*)own_malloc(sizeof(sn_coap_options_list_s));
			if (!response->options_list_ptr)
				goto END;
			response->options_list_ptr->observe_len = 1;
			response->options_list_ptr->observe_ptr = &observation_number;
			if(!observation_number)
				observation_number = 1;
		}
	}

	response->payload_len = 1;
	response->payload_ptr = &observation_number;

	sn_nsdl_send_coap_message(nsdl_handle, address, response);

	if(response->options_list_ptr)
		response->options_list_ptr->observe_ptr = 0;

END:
	sn_nsdl_release_allocated_coap_msg_mem(nsdl_handle, response);

	return 0;
}

static uint8_t led_callback(struct nsdl_s *handle, sn_coap_hdr_s *coap, sn_nsdl_addr_s *address, sn_nsdl_capab_e proto)
{
	(void)proto;
	(void)handle;
	static uint8_t led_state = 0;

	sn_coap_hdr_s *response = 0;

	if(coap->msg_code == COAP_MSG_CODE_REQUEST_PUT)
	{
		sn_coap_msg_code_e response_code = COAP_MSG_CODE_RESPONSE_BAD_REQUEST;

		if((*coap->payload_ptr == '1') && (coap->payload_len == 1))
		{
			led_state = 1;
			response_code = COAP_MSG_CODE_RESPONSE_CHANGED;
		}
		else if ((*coap->payload_ptr == '0') && (coap->payload_len == 1))
		{
			led_state = 0;
			response_code = COAP_MSG_CODE_RESPONSE_CHANGED;
		}

		response = sn_nsdl_build_response(nsdl_handle, coap, response_code);
	}

	else if(coap->msg_code == COAP_MSG_CODE_REQUEST_GET)
	{
		// Change led status in every query
		// Rotate through table
		led_state = (led_state+1)%(sizeof(led_states)/sizeof(led_states[0]));
		response = sn_nsdl_build_response(nsdl_handle, coap, COAP_MSG_CODE_RESPONSE_CONTENT);
		response->payload_len = 1;
		response->payload_ptr = &led_state;
	}

	for (int i=0; i<3; i++) {
		led[i] = led_states[led_state][i];
	}
	sn_nsdl_send_coap_message(nsdl_handle, address, response);

	sn_nsdl_release_allocated_coap_msg_mem(nsdl_handle, response);

	return 0;
}

/********************
 * APM Demonstrator *
 ********************
 * Send broadcast CoAP messages to all nearby nodes.
 * This CoAP GET request is handled by the led_callback() above. So it will togle the
 * color of led on all devices.
 * ff02::1 all neighbors
 * ff03::1 whole mesh
 */

static void send_paketti() {
	sn_coap_hdr_s request;
	sn_nsdl_addr_s dst_addr;
	static const uint8_t bcast_neigh[16] = {0xff, 0x02, 0,0,0,0,0,0,0,0,0,0,0,0,0,1};
	memset(&request, 0, sizeof(request));
    dst_addr.addr_ptr = (uint8_t *) bcast_neigh; // Cast away const and trust that nsdl doesn't modify...
    dst_addr.addr_len  =  16;
    dst_addr.type  =  SN_NSDL_ADDRESS_TYPE_IPV6;
    dst_addr.port  =  5683;
    request.msg_type = COAP_MSG_TYPE_NON_CONFIRMABLE;
    request.msg_code = COAP_MSG_CODE_REQUEST_GET;
    request.uri_path_ptr = (uint8_t *)res_led;
    request.uri_path_len = strlen(res_led);
    sn_nsdl_send_coap_message(nsdl_handle, &dst_addr, &request);
}

static void buttonHandler(void)
{
	send_paketti();
	if(observation_number != 0) // flag for that, otherwise it does not send notification when counter goes around
	{
		sn_nsdl_send_observation_notification(nsdl_handle, token_number, token_len, &observation_number, sizeof(observation_number),
				&observation_number, sizeof(observation_number), COAP_MSG_TYPE_NON_CONFIRMABLE, 0);
		observation_number++;
	}
}
