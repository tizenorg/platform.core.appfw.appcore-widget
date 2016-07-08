/*
 * Copyright (c) 2015 - 2016 Samsung Electronics Co., Ltd All Rights Reserved
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


#include <string.h>
#include <libintl.h>

#include <dlog.h>
#include <widget_errno.h>

#include "widget-private.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "CAPI_WIDGET_APPLICATIO"

static const char *widget_app_error_to_string(widget_error_e error)
{
	switch (error) {
	case WIDGET_ERROR_NONE:
		return "NONE";
	case WIDGET_ERROR_INVALID_PARAMETER:
		return "INVALID_PARAMETER";
	case WIDGET_ERROR_OUT_OF_MEMORY:
		return "OUT_OF_MEMORY"; /* LCOV_EXCL_LINE */
	case WIDGET_ERROR_RESOURCE_BUSY:
		return "RESOURCE_BUSY"; /* LCOV_EXCL_LINE */
	case WIDGET_ERROR_PERMISSION_DENIED:
		return "PERMISSION_DENIED"; /* LCOV_EXCL_LINE */
	case WIDGET_ERROR_CANCELED:
		return "CANCELED"; /* LCOV_EXCL_LINE */
	case WIDGET_ERROR_IO_ERROR:
		return "IO_ERROR"; /* LCOV_EXCL_LINE */
	case WIDGET_ERROR_TIMED_OUT:
		return "TIMED_OUT"; /* LCOV_EXCL_LINE */
	case WIDGET_ERROR_NOT_SUPPORTED:
		return "NOT_SUPPORTED"; /* LCOV_EXCL_LINE */
	case WIDGET_ERROR_FILE_NO_SPACE_ON_DEVICE:
		return "FILE_NO_SPACE_ON_DEVICE"; /* LCOV_EXCL_LINE */
	case WIDGET_ERROR_FAULT:
		return "FAULT"; /* LCOV_EXCL_LINE */
	case WIDGET_ERROR_ALREADY_EXIST:
		return "ALREADY_EXIST"; /* LCOV_EXCL_LINE */
	case WIDGET_ERROR_ALREADY_STARTED:
		return "ALREADY_STARTED"; /* LCOV_EXCL_LINE */
	case WIDGET_ERROR_NOT_EXIST:
		return "NOT_EXIST"; /* LCOV_EXCL_LINE */
	case WIDGET_ERROR_DISABLED:
		return "DISABLED"; /* LCOV_EXCL_LINE */
	default:
		return "UNKNOWN"; /* LCOV_EXCL_LINE */
	}
}

int widget_app_error(widget_error_e error, const char *function,
		const char *description)
{
	if (description) {
		LOGE("[%s] %s(0x%08x) : %s", function,
				widget_app_error_to_string(error), error,
				description);
	} else {
		LOGE("[%s] %s(0x%08x)", function,
				widget_app_error_to_string(error), error);
	}

	return error;
}

