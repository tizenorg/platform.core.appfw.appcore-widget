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

#ifndef __TIZEN_APPFW_WIDGET_APP_INTERNAL_H__
#define __TIZEN_APPFW_WIDGET_APP_INTERNAL_H__

#include <widget_app.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * For in-house applications
 *
 */
typedef struct {
	struct __pointer {
		double x;
		double y;
		int down;
	} pointer;

	struct __part {
		double sx;
		double sy;
		double ex;
		double ey;
	} part;

} widget_obj_event_info_s;

typedef int (*widget_instance_text_signal_cb)(widget_context_h context,
		const char *signal_name, const char *source,
		widget_obj_event_info_s * info, void *user_data);

typedef struct _widget_obj_private_ops {
	widget_instance_text_signal_cb text_signal;
} widget_obj_private_ops_s;

typedef struct _widget_class_factory_full widget_class_factory_full_s;
typedef const widget_class_factory_full_s *(*widget_class_factory_override_text_signal)(widget_instance_text_signal_cb op);
typedef widget_class_h (*_widget_class_factory_operation_make)(widget_instance_lifecycle_callback_s callback);

struct _widget_class_factory_full {
	widget_class_factory_override_text_signal override_text_signal;
};

const widget_class_factory_full_s *widget_app_get_class_factory(void);

#ifdef __cplusplus
}
#endif
#endif
