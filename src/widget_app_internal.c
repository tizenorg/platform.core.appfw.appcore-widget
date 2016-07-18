/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <bundle.h>
#include <bundle_internal.h>
#include <aul.h>
#include <aul_app_com.h>
#include <dlog.h>

#include "widget_app.h"
#include "widget-log.h"
#include "widget-private.h"
#include "widget_app_internal.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "CAPI_WIDGET_APPLICATION"

static char *viewer_endpoint = NULL;

EXPORT_API int widget_app_restart()
{
	int ret;
	int status = AUL_WIDGET_INSTANCE_EVENT_APP_RESTART_REQUEST;
	bundle *kb;

	kb = bundle_create();
	bundle_add_byte(kb, AUL_K_WIDGET_STATUS, &status, sizeof(int));
	ret = aul_app_com_send(viewer_endpoint, kb);
	bundle_free(kb);
	if (ret != AUL_R_OK) {
		_E("failed to kill app");
		return WIDGET_ERROR_IO_ERROR;
	}
	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_set_viewer_endpoint(char *endpoint)
{
	if (endpoint == NULL)
		return WIDGET_ERROR_INVALID_PARAMETER;

	viewer_endpoint = strdup(endpoint);
	if (viewer_endpoint == NULL)
		return WIDGET_ERROR_OUT_OF_MEMORY;

	return WIDGET_ERROR_NONE;
}

EXPORT_API char *widget_app_get_viewer_endpoint()
{
	return viewer_endpoint;
}

EXPORT_API int widget_app_free_viewer_endpoint()
{
	if (viewer_endpoint)
		free(viewer_endpoint);

	return WIDGET_ERROR_NONE;
}
