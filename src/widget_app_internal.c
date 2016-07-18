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
#include <dlog.h>

#include "widget_app.h"
#include "widget-log.h"
#include "widget-private.h"
#include "widget_app_internal.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "CAPI_WIDGET_APPLICATION"

EXPORT_API int widget_app_restart(char *appid)
{
	int ret;

	ret =  aul_widget_app_restart(getpid());
	if (ret != AUL_R_OK) {
		_E("failed to kill app");
		return WIDGET_ERROR_IO_ERROR;
	}

	return WIDGET_ERROR_NONE;
}
