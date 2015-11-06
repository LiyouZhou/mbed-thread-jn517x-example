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

#include "socket_api.h"
#include "ns_address.h"
#include "nsdynmemLIB.h"
#include "router.h"
#include "sn_nsdl.h"
#include "sn_coap_header.h"
#include "sn_coap_protocol.h"
#include "sn_nsdl_lib.h"
#include "nsdl_helper.h"
#include "endpoint.h"

/**
 * \brief Create a static resource.
 * Fills the structure for libNSDL to register this static resource.
 * \param resource_structure resource root structure for libNSDL.
 * \param path Resource path. Null terminated string
 * \param type resource type. Null terminated.
 * \param value Static value for resource. null terminated string.
 * \return 0 if ok. Nonzero on failure.
 */
int create_static_resource(const char *path, const char *type, const char *value)
{
	sn_nsdl_resource_info_s *resource_structure;
	int rc;
	resource_structure = (sn_nsdl_resource_info_s *)own_malloc(sizeof(sn_nsdl_resource_info_s));
	if (NULL == resource_structure)
		return 1;
	resource_structure->resource_parameters_ptr = (sn_nsdl_resource_parameters_s*)own_malloc(sizeof(sn_nsdl_resource_parameters_s));
	if (NULL == resource_structure->resource_parameters_ptr) {
		free(resource_structure);
		return 1;
	}
	resource_structure->access = SN_GRS_GET_ALLOWED;
	resource_structure->mode = SN_GRS_STATIC;
	resource_structure->pathlen = strlen(path);
	resource_structure->path = (uint8_t *)path;
	resource_structure->resource_parameters_ptr->resource_type_len = strlen(type);
	resource_structure->resource_parameters_ptr->resource_type_ptr = (uint8_t *)type;
	resource_structure->resource = (uint8_t *)value;
	resource_structure->resourcelen = strlen(value);
	rc = sn_nsdl_create_resource(nsdl_handle, resource_structure);
	free(resource_structure->resource_parameters_ptr);
	free(resource_structure);
	return rc;
}


/**
 * \brief Create a dynamic resource.
 * \param resource_structure resource tree stucture.
 * \param path Resource path, null terminated.
 * \param type Resource type, null terminated.
 * \param flags Bitmask of sn_grs_resource_acl_e flags, to select if GET/POST etc is allowed.
 * \param is_observable nonzero value is observable resource.
 * \param callback pointer to callback function.
 * \return 0 on successs, nonzero on failure.
 */
int create_dynamic_resource(const char *path, const char *type, int flags, char is_observable, coap_callback *callback)
{
	sn_nsdl_resource_info_s *resource_structure;
	int rc;
	resource_structure = (sn_nsdl_resource_info_s *)own_malloc(sizeof(sn_nsdl_resource_info_s));
	if (NULL == resource_structure)
		return 1;
	resource_structure->resource_parameters_ptr = (sn_nsdl_resource_parameters_s*)own_malloc(sizeof(sn_nsdl_resource_parameters_s));
	if (NULL == resource_structure->resource_parameters_ptr) {
		free(resource_structure);
		return 1;
	}
	resource_structure->access = (sn_grs_resource_acl_e)flags;
	resource_structure->resource = 0;
	resource_structure->resourcelen = 0;
	resource_structure->sn_grs_dyn_res_callback = callback;
	resource_structure->mode = SN_GRS_DYNAMIC;
	resource_structure->pathlen = strlen(path);
	resource_structure->path = (uint8_t *)path;
	resource_structure->resource_parameters_ptr->resource_type_len = strlen(type);
	resource_structure->resource_parameters_ptr->resource_type_ptr = (uint8_t *)type;
	resource_structure->resource_parameters_ptr->observable = is_observable;
	rc = sn_nsdl_create_resource(nsdl_handle, resource_structure);
	free(resource_structure->resource_parameters_ptr);
	free(resource_structure);
	return rc;
}

static sn_nsdl_ep_parameters_s *create_endpoint_struct(const char *name, const char *type_name, const char *lifetime)
{
	sn_nsdl_ep_parameters_s *endpoint_structure = (sn_nsdl_ep_parameters_s *)own_malloc(sizeof(sn_nsdl_ep_parameters_s));
	if (NULL == endpoint_structure)
		return NULL;
	endpoint_structure->endpoint_name_ptr = (uint8_t *)name;
	endpoint_structure->endpoint_name_len = strlen(name);
	endpoint_structure->type_ptr = (uint8_t *)type_name;
	endpoint_structure->type_len =  strlen(type_name);
	endpoint_structure->lifetime_ptr = (uint8_t *)lifetime;
	endpoint_structure->lifetime_len =  strlen(lifetime);
	return endpoint_structure;
}

/**
 * \brief Register endpoint.
 * \param name Endpoint name. Null terminated string.
 * \param typename Endpoint type. Null terminated string.
 * \param lifetime Endpoint lifetime in seconds.
 * \return nonzero on failure. zero on success.
 */
int register_endpoint(const char *name, const char *type_name, unsigned int lifetime)
{
	sn_nsdl_ep_parameters_s *endp;
	char t[10];
	int rc;

	if (snprintf(t, 10,"%u", lifetime) >= 9) /* Output truncated */
		return 1;

	endp = create_endpoint_struct(name, type_name, t);
	rc = sn_nsdl_register_endpoint(nsdl_handle, endp);
	free(endp);
	return rc;
}

void *own_malloc(uint16_t len)
{
	void* tmp = malloc(len);
	if(tmp)
		memset(tmp, 0, len);
	return tmp;
}

void own_free(void *ptr)
{
	if(ptr)
		free(ptr);
	return;
}
