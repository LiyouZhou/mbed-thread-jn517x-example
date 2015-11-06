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
/*
 * Static configuration file for embedded NanoRouter
 *
 * This file SHOULD be included only once
 * This file SHOULD be included only after defining struct conf_t
 *
 */

static conf_t static_config[] = {
	/* NAME,	STRING_VALUE,	INT_VALUE */
#if defined(TARGET_JN517X)
	{ "NAME",	"517X-%M",				0},
	{ "MODEL",	"517X",				0},
	{ "MANUFACTURER","NXP",		0},
#else
	{ "NAME",	"MBED-%M",				0},
	{ "MODEL",	"MBED",				0},
	{ "MANUFACTURER","ARM",		0},
#endif

	{ "NETWORKID",		"NETWORK000000000",				0},
	{ "PREFIX",		"fd00:ff1:ce0b:a5e0::1",			0},
	{ "CHANNEL_LIST", NULL, 0x01000000},

	{ "NSP", "fd00:ff1:ce0b:a5e0::1", 0 },
	{ "NSP_PORT", NULL, 5683U },
	{ "NSP_LIFETIME", NULL, 120 },

	/* Array must end on NULL,NULL,0 field */
	{ NULL, NULL, 0}
};

conf_t *global_config = static_config;
