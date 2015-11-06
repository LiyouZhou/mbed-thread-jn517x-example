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
/*
 * main.c
 *
 */
#include <stdio.h>
#include <string.h>
#include "mbed.h"
#include "eventOS_event.h"
#include "eventOS_scheduler.h"
#include "nsdynmemLIB.h"
#include "randLIB.h"
#include "platform/arm_hal_aes.h"
#include "net_interface.h"
#include "platform/arm_hal_timer.h"
#include "ip6string.h"
#include "node_tasklet.h"
#include "router.h"
#if defined(TARGET_K64F)
#include "atmel-rf-driver/driverRFPhy.h"
#endif
#if defined(TARGET_JN517X)
#include "nxp-rf-driver/MMAC.h"
#endif
#define TRACE_GROUP "App"
#define HAVE_DEBUG
#include "ns_trace.h"

#if defined(TARGET_K64F)
#define APP_DEFINED_HEAP_SIZE 32500
#endif

#if defined(TARGET_JN517X)
#define APP_DEFINED_HEAP_SIZE 11008
#endif

static uint8_t app_stack_heap[APP_DEFINED_HEAP_SIZE +1];

static int8_t rf_phy_device_register_id = -1;

/*Function prototypes*/
static void app_heap_error_handler(heap_fail_t event);

void network_ready(void)
{
		send_signal_start_endpoint();
}

Serial pc(USBTX, USBRX);
void trace_printer(const char* str)
{
	pc.printf("%s\r\n", str);
}

void app_start(int, char **)
{
	char if_desciption[] = "6LoWPAN_NODE";

	pc.baud(115200);  //Setting the Baud-Rate for trace output
    ns_dyn_mem_init(app_stack_heap, APP_DEFINED_HEAP_SIZE, app_heap_error_handler,0);
    randLIB_seed_random();
    platform_timer_enable();
    eventOS_scheduler_init();
    trace_init();
	set_trace_print_function( trace_printer );
    set_trace_config(TRACE_ACTIVE_LEVEL_DEBUG|TRACE_CARRIAGE_RETURN);
	tr_debug("M \r\n");
    net_init_core();
    rf_phy_device_register_id = rf_device_register();
    net_rf_id = arm_nwk_interface_init(NET_INTERFACE_RF_6LOWPAN, rf_phy_device_register_id, if_desciption);
    eventOS_event_handler_create(&tasklet_main, ARM_LIB_TASKLET_INIT_EVENT);
}

/* Catch heap errors */
void app_heap_error_handler(heap_fail_t event)
{
	switch (event)
	{
		case NS_DYN_MEM_NULL_FREE:
		case NS_DYN_MEM_DOUBLE_FREE:
		case NS_DYN_MEM_ALLOCATE_SIZE_NOT_VALID:
		case NS_DYN_MEM_POINTER_NOT_VALID:
		case NS_DYN_MEM_HEAP_SECTOR_CORRUPTED:
		case NS_DYN_MEM_HEAP_SECTOR_UNITIALIZED:
			break;
		default:
			break;
	}
	while(1);
}
