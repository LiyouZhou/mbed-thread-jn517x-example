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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "randLIB.h"

extern void arm_random_module_init(void)
{

}

extern uint32_t arm_random_seed_get(void)
{
	return 0;
}

void arm_aes_block_encode(uint8_t * key_ptr , uint8_t * Ai_ptr, uint8_t * Si_ptr)
{
	(void)key_ptr;
	(void)Ai_ptr;
	(void)Si_ptr;
	// TBD
}
