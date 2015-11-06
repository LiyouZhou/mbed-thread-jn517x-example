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
#include <string.h>
#include "mbed.h"
#include "eventOS_event_timer.h"
#include "net_interface.h"
#include "ns_address.h"
#include "socket_api.h"
#include "ns_trace.h"
#include "node_tasklet.h"
#include "common_functions.h"
#include "config.h"
#include "thread_management_if.h"
#if defined(TARGET_K64F)
#include "atmel-rf-driver/driverRFPhy.h"
#endif
#if defined(TARGET_JN517X)
#include "nxp-rf-driver/MMAC.h"
#endif

#define TRACE_GROUP  "cApp"

#include "nanostack/thread_management_if.h"
#include "nanostack/thread_dhcpv6_server.h"

#define THREAD_NTW_PAN_ID  		0x0691
#define RF_CHANNEL         		24
#define THREAD_PROTOCOL_ID 		3
#define THREAD_PROTOCOL_VERSION 1

int8_t net_rf_id = -1;
static link_layer_address_s app_link_address_info;
static network_layer_address_s app_nd_address_info;

uint8_t access_point_status = 0;
/** Configurable channel list for beacon scan*/
static uint32_t channel_list = 1 << RF_CHANNEL;
static int8_t node_main_tasklet_id = -1;
static void app_parse_network_event(arm_event_s *event);

/* Default security key */
static const uint8_t default_net_security_key[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
/*Thread ML prefix*/
uint8_t br_thread_ml_prefix[16] = {0xfd, 0x00, 0x0d, 0xb8, 0};
/*
 * \brief A function which will be eventually called by NanoStack OS when ever the OS has an event to deliver.
 * @param event, describes the sender, receiver and event type.
 *
 * NOTE: Interrupts requested by HW are possible during this function!
 */
void tasklet_main(arm_event_s *event)
{
	uint8_t  master_key[16];
	/**Border Router ND setup*/
    static link_configuration_s link_setup;
    device_configuration_s device_config;
    uint8_t networkid[16];
    uint32_t channel_mask;
    channel_mask = (uint32_t) 1 << RF_CHANNEL;
    char psdK[] = "Kukkuu!";
    device_config.PSKd_ptr = psdK;
	device_config.leaderCap = true;

	int8_t retval;
	if (event->sender == 0)
	{
		arm_library_event_type_e event_type;
		event_type = (arm_library_event_type_e) event->event_type;
		switch (event_type)
		{
			//This event is delivered every and each time when there is new information of network connectivity.
			case ARM_LIB_NWK_INTERFACE_EVENT:
				/* Network Event state event handler */
				app_parse_network_event(event);
				break;

			case ARM_LIB_TASKLET_INIT_EVENT:
				/*Event with type EV_INIT is an initialiser event of NanoStack OS.
				 * The event is delivered when the NanoStack OS is running fine. This event should be delivered ONLY ONCE.
				 */

				node_main_tasklet_id = event->receiver;

				/* Bootsrapping Thread network */
				arm_nwk_interface_configure_6lowpan_bootstrap_set(net_rf_id, NET_6LOWPAN_ROUTER, NET_6LOWPAN_THREAD);

				arm_nwk_6lowpan_link_scan_paramameter_set(net_rf_id, channel_list, 5);

				rf_read_mac_address(link_setup.extended_random_mac);
				link_setup.extended_random_mac[0] |= 0x02;

				//Initialize Thread stack to leader mode
				link_setup.steering_data_len = 0;

                memcpy(networkid, "Arm Powered Core", 16);
                memcpy(link_setup.name, networkid, 16);

                link_setup.panId = THREAD_NTW_PAN_ID;
            	link_setup.rfChannel = RF_CHANNEL;

				link_setup.Protocol_id = THREAD_PROTOCOL_ID;
            	link_setup.version = THREAD_PROTOCOL_VERSION;
            	memcpy(link_setup.mesh_local_ula_prefix,br_thread_ml_prefix, 8);

                arm_nwk_6lowpan_gp_address_mode(net_rf_id, NET_6LOWPAN_GP16_ADDRESS, 0xffff, 1);

				memcpy(master_key, default_net_security_key, 16);
				memcpy(link_setup.master_key,default_net_security_key, 16 );
				link_setup.key_rotation = 3600;
				link_setup.key_sequence = 0;
				memcpy(link_setup.mesh_local_ula_prefix,br_thread_ml_prefix, 8);
				thread_managenet_node_init(net_rf_id, channel_mask, &device_config, &link_setup);

				retval = arm_nwk_interface_up(net_rf_id);
				if (retval != 0)
				{
					//debug_int(retval);
					tr_debug("Start Fail code: %d", retval);
				}
				else
				{
					tr_debug("6LoWPAN IP Bootstrap started");
					network_ready();
				}
				break;
				case ARM_LIB_SYSTEM_TIMER_EVENT:
					eventOS_event_timer_cancel(event->event_id, node_main_tasklet_id);

					if(event->event_id == 1)
					{
						if(arm_nwk_interface_up(net_rf_id) == 0)
						{
							tr_debug("Start Network bootstrap Again");
						}

					}
				break;
				default:
				break;
		}
	}
}


/**
  * \brief Network state event handler.
  * \param event show network start response or current network state.
  *
  * - ARM_NWK_BOOTSTRAP_READY: Save NVK peristant data to NVM and Net role
  * - ARM_NWK_NWK_SCAN_FAIL: Link Layer Active Scan Fail, Stack is Already at Idle state
  * - ARM_NWK_IP_ADDRESS_ALLOCATION_FAIL: No ND Router at current Channel Stack is Already at Idle state
  * - ARM_NWK_NWK_CONNECTION_DOWN: Connection to Access point is lost wait for Scan Result
  * - ARM_NWK_NWK_PARENT_POLL_FAIL: Host should run net start without any PAN-id filter and all channels
  * - ARM_NWK_AUHTENTICATION_FAIL: Pana Authentication fail, Stack is Already at Idle state
  */
void app_parse_network_event(arm_event_s *event )
{
	arm_nwk_interface_status_type_e status = (arm_nwk_interface_status_type_e)event->event_data;
	switch (status)
	{
		  case ARM_NWK_BOOTSTRAP_READY:
			  /* Network is ready and node is connect to Access Point */
			  if(access_point_status==0)
			  {
				  uint8_t temp_ipv6[16];
				  tr_debug("Network Connection Ready");
				  access_point_status=1;

				  if( arm_nwk_nd_address_read(net_rf_id,&app_nd_address_info) != 0)
				  {
					  tr_debug("ND Address read fail");
				  }
				  else
				  {
					  debug("ND Access Point:");
						printf_ipv6_address(app_nd_address_info.border_router);

						debug("ND Prefix 64:");
						printf_array(app_nd_address_info.prefix, 8);

						if(arm_net_address_get(net_rf_id,ADDR_IPV6_GP,temp_ipv6) == 0)
						{
							debug("GP IPv6:");
							printf_ipv6_address(temp_ipv6);
						}
					}

				  if( arm_nwk_mac_address_read(net_rf_id,&app_link_address_info) != 0)
					{
						tr_debug("MAC Address read fail\n");
					}
					else
					{
						uint8_t temp[2];
						debug("MAC 16-bit:");
						common_write_16_bit(app_link_address_info.PANId,temp);
						debug("PAN ID:");
						printf_array(temp, 2);
						debug("MAC 64-bit:");
						printf_array(app_link_address_info.mac_long, 8);
						debug("IID (Based on MAC 64-bit address):");
						printf_array(app_link_address_info.iid_eui64, 8);
					}

			  }

			  break;
		  case ARM_NWK_NWK_SCAN_FAIL:
			  /* Link Layer Active Scan Fail, Stack is Already at Idle state */
			  tr_debug("Link Layer Scan Fail: No Beacons");
			  access_point_status=0;
			  break;
		  case ARM_NWK_IP_ADDRESS_ALLOCATION_FAIL:
			  /* No ND Router at current Channel Stack is Already at Idle state */
			  tr_debug("ND Scan/ GP REG fail");
			  access_point_status=0;
			  break;
		  case ARM_NWK_NWK_CONNECTION_DOWN:
			  /* Connection to Access point is lost wait for Scan Result */
			  tr_debug("ND/RPL scan new network");
			  access_point_status=0;
			  break;
		  case ARM_NWK_NWK_PARENT_POLL_FAIL:
			  access_point_status=0;
			  break;
		  case ARM_NWK_AUHTENTICATION_FAIL:
			  tr_debug("Network authentication fail");
			  access_point_status=0;
			  break;
		  default:
			  debug_hex(status);
			  debug("Unknow event");
			  break;
	 }

	if(access_point_status == 0)
	{
		//Set Timer for new network scan
		eventOS_event_timer_request(1, ARM_LIB_SYSTEM_TIMER_EVENT,node_main_tasklet_id,5000); // 5 sec timer started
	}
}

