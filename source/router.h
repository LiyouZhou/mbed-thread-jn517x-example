/*
 * Copyright (c) 2014 ARM Limited. All rights reserved.
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
#ifndef ROUTER_H
#define ROUTER_H

#include "ns_types.h"
#include "eventOS_event.h"

/* Global variables that are used from NSP endpoint */
extern int8_t net_6lowpan_id;
extern int8_t net_backhaul_id;

/* APP specific event ID:s */
enum event_id {
	EVENT_STOP_ROUTER,
	EVENT_START_ROUTER,
	EVENT_RESTART_ROUTER,
};

#define MAC_STRING_LENGTH 17

/**
 * Endpoint taskelet that must be registered in application start up with Nanostack.
 * Example: eventOS_event_handler_create(endpoint_tasklet,ARM_LIB_TASKLET_INIT_EVENT);
 */
void endpoint_tasklet(arm_event_s *event);

/**
 * Start endpoint tasklet.
 */
void send_signal_start_endpoint(void);

#endif /* ROUTER_H */

