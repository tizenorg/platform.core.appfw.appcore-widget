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


#ifndef __APPCORE_WIDGET_PRIVATE_H__
#define __APPCORE_WIDGET_PRIVATE_H__

#include <glib.h>

#include <Elementary.h>
#include <widget_app.h>
#include <app_common.h>
#include <widget_errno.h>

#define FEATURE_SHELL_APPWIDGET "http://tizen.org/feature/shell.appwidget"

struct _widget_class {
	void *user_data;
	widget_instance_lifecycle_callback_s ops;
	char *classid;
	struct _widget_class *next;
	struct _widget_class *prev;
};

struct _widget_context {
	char *id;
	struct _widget_class *provider;
	int state;
	void *tag;
	Evas_Object *win;
	int win_id;
	char *content;
	widget_instance_lifecycle_callback_s ops;
};

typedef struct _widget_context widget_context_s;

GList *_widget_app_get_contexts();
int _widget_app_add_context(widget_context_s *wc);
int _widget_app_remove_context(widget_context_s *wc);
int _widget_app_set_viewer_endpoint(char *viewer_endpoint);
char *_widget_app_get_viewer_endpoint();
int _widget_app_free_viewer_endpoint();
int _set_i18n(const char *domainname);
void _update_lang(void);
void _update_region(void);
int widget_app_error(widget_error_e error, const char *function,
		const char *description);

#endif /* __APPCORE_WIDGET_PRIVATE_H__ */

