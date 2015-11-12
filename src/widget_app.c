/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
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

#include <bundle.h>
#include <aul.h>
#include <dlog.h>
#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <app_control.h>
#include <app_control_internal.h>
#include <Elementary.h>
#include <widget_errno.h>
#include <widget_instance.h>
#include <widget_service_internal.h>

#include "widget_app.h"
#include "widget-log.h"
#include "widget-private.h"
#include "widget_app_internal.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define STR_MAX_BUF 128
#define LOG_TAG "CAPI_WIDGET_APPLICATION"
#define K_REASON    "__WC_K_REASON__"

typedef enum _widget_obj_state_e {
	WC_READY = 0,
	WC_RUNNING = 1,
	WC_PAUSED = 2,
	WC_TERMINATED = 3
} widget_obj_state_e;

struct app_event_handler {
	app_event_type_e type;
	app_event_cb cb;
	void *data;
};

struct app_event_info {
	app_event_type_e type;
	void *value;
};

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
	bundle *content;
	widget_instance_lifecycle_callback_s ops;
};

typedef struct _widget_class widget_class_s;
typedef struct _widget_context widget_context_s;

static int caller_pid = 0;
static widget_app_lifecycle_callback_s *app_ops;
static void *app_user_data = NULL;
static char *appid = NULL;
static widget_class_h class_provider = NULL;
static GList *contexts = NULL;

static gint __comp_by_id(gconstpointer a, gconstpointer b)
{
	widget_context_s *wc = (widget_context_s*)a;

 	return strcmp(wc->id, (const char*)b);
}

static widget_context_s* __find_context_by_id(const char *id)
{
	GList* ret = g_list_find_custom(contexts, id, __comp_by_id);

	if (ret == NULL)
		return NULL;

	return ret->data;
}

static int __send_update_status(const char *class_id, const char *instance_id,
	int status, bundle *extra, int internal_only)
{
	return aul_update_widget_status(class_id, instance_id, status, extra, internal_only ? caller_pid : 0);
}

static int __instance_create(widget_class_h handle, const char *id, bundle *b)
{
	widget_context_s *wc = NULL;
	int w = 0, h = 0;
	char *w_str = NULL, *h_str = NULL;
	char *remain = NULL;
	int ret = 0;

	wc = (widget_context_s *)malloc(sizeof(widget_context_s));
	if (!wc)
		return WIDGET_ERROR_OUT_OF_MEMORY;

	wc->state = WC_READY;
	wc->id = g_strdup(id);
	wc->provider = handle;
	wc->win = NULL;

	wc->content = bundle_dup(b);
	bundle_get_str(b, WIDGET_K_WIDTH, &w_str);
	bundle_get_str(b, WIDGET_K_HEIGHT, &h_str);

	if (w_str) {
		w = (int)g_ascii_strtoll(w_str, &remain, 10);
	}

	if (h_str) {
		h = (int)g_ascii_strtoll(h_str, &remain, 10);
	}

	contexts = g_list_append(contexts, wc);

	handle->ops.create(wc, b, w, h, handle->user_data);

	ret = __send_update_status(handle->classid, wc->id, WIDGET_INSTANCE_EVENT_CREATE, b, 0);

	return ret;
}

static int __instance_destroy(widget_class_h handle, const char *id, widget_destroy_type_e reason, bundle *b)
{
	widget_context_s *wc = __find_context_by_id(id);
	int ret = 0;

	if (wc) {
		wc->state = WC_TERMINATED;
		handle->ops.destroy(wc, (widget_app_destroy_type_e)reason, b, handle->user_data);

		ret = __send_update_status(handle->classid, id, WIDGET_INSTANCE_EVENT_TERMINATE, b, 0);

		contexts = g_list_remove(contexts, wc);

		if (wc->id)
			free(wc->id);
		free(wc);
	} else {
		_E("could not find widget obj: %s", id);
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	return ret;
}

static widget_class_h __find_class_handler(const char *class_id, widget_class_h handle)
{
	if (!class_id || !handle)
		return NULL;

	widget_class_h head = handle;

	while (head) {
		if (head->classid && strcmp(head->classid, class_id) == 0)
			return head;

		head = head->next;
	}

	return NULL;
}

static void __control(bundle *b)
{
	char *class_id = NULL;
	char *id = NULL;
	char *operation = NULL;
	char *reason = NULL;
	char *remain = NULL;
	int destroy_type = WIDGET_DESTROY_TYPE_DEFAULT;

	widget_class_h handle = NULL;
	bundle_get_str(b, WIDGET_K_CLASS, &class_id);
	/* for previous version compatibility, use appid for default class id */
	if (class_id == NULL)
		class_id = appid;

	bundle_get_str(b, WIDGET_K_INSTANCE, &id);
	bundle_get_str(b, WIDGET_K_OPERATION, &operation);

	handle = __find_class_handler(class_id, class_provider);
	if (!handle) {
		_E("no handle provided: %s", class_id);
		goto error;
	}

	if (!operation) {
		_E("no operation provided");
		goto error;
	}

	if (strcmp(operation, "create") == 0) {
		__instance_create(handle, id, b);
	} else if (strcmp(operation, "resize") == 0) {
		/* TODO */
	} else if (strcmp(operation, "update") == 0) {
		/* TODO */
	} else if (strcmp(operation, "destroy") == 0) {
		bundle_get_str(b, WIDGET_K_REASON, &reason);
		if (reason)
			destroy_type = (int)g_ascii_strtoll(reason, &remain, 10);

		__instance_destroy(handle, id, destroy_type, b);
	} else if (strcmp(operation, "resume") == 0) {
		/* TODO */
	} else if (strcmp(operation, "pause") == 0) {
		/* TODO */
	}

	return;
error:
	LOGD("error on control");
	return;
}

static void __show_all()
{
	LOGD("resume");
}

static int __aul_handler(aul_type type, bundle *b, void *data)
{
	char *caller = NULL;
	char *remain = NULL;

	switch (type) {
	case AUL_START:
		if (b) {
			bundle_get_str(b, WIDGET_K_CALLER, &caller);
			if (caller)
				caller_pid = g_ascii_strtoll(caller, &remain, 10);
			else {
				/* using caller appid and query pid using caller appid? */
				_E("no caller pid");
			}
		}

		__control(b);
		break;
	case AUL_RESUME:
		__show_all();
		break;
	case AUL_TERMINATE:
		widget_app_exit();
		break;
	default:
		break;
	}

	return 0;
}


static int __before_loop(int argc, char **argv)
{
	int r;
	bundle *kb = NULL;
	char *wayland_display = NULL;
	char *xdg_runtime_dir = NULL;

#if !(GLIB_CHECK_VERSION(2, 36, 0))
	g_type_init();
#endif

	kb = bundle_import_from_argv(argc, argv);
	if (kb) {
		bundle_get_str(kb, AUL_K_WAYLAND_WORKING_DIR, &xdg_runtime_dir);
		bundle_get_str(kb, AUL_K_WAYLAND_DISPLAY, &wayland_display);

		if (xdg_runtime_dir)
			setenv("XDG_RUNTIME_DIR", xdg_runtime_dir, 1);

		if (wayland_display)
			setenv("WAYLAND_DISPLAY", wayland_display, 1);

		bundle_free(kb);
		kb = NULL;
	} else {
		_E("failed to get launch argv");
	}

	elm_init(argc, argv);

	r = aul_launch_init(__aul_handler, NULL);
	if (r < 0) {
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__,
					"Fail to call the aul_launch_init");
	}

	r = aul_launch_argv_handler(argc, argv);
	if (r < 0) {
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__,
					"Fail to call the aul_launch_argv_handler");
	}

	r = app_get_id(&appid);
	if (r != APP_ERROR_NONE)
		return r;

	class_provider = app_ops->create(app_user_data);
	if (class_provider == NULL) {
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__,
					"widget_class is NULL");
	}

	return WIDGET_ERROR_NONE;
}

static void __after_loop()
{
	if (app_ops->terminate)
		app_ops->terminate(app_user_data);

	elm_shutdown();
}

EXPORT_API int widget_app_main(int argc, char **argv,
				widget_app_lifecycle_callback_s *callback, void *user_data)
{
	int r;

	if (argc <= 0 || argv == NULL || callback == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	if (callback->create == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, "widget_app_create_cb() callback must be registered");

	app_ops = callback;
	app_user_data = user_data;
	r = __before_loop(argc, argv);
	if (r < 0)
		return r;

	ecore_main_loop_begin();
	aul_status_update(STATUS_DYING);
	__after_loop();

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_exit(void)
{
	ecore_main_loop_quit();

	return WIDGET_ERROR_NONE;
}

static gboolean __finish_event_cb(gpointer user_data)
{
	if (user_data == NULL)
		return FALSE;

	widget_context_s *wc = (widget_context_s *)user_data;

	switch (wc->state) {
	case WC_READY:

		break;
	case WC_RUNNING:

		break;
	case WC_PAUSED:

		break;
	default:
		break;
	}


	return FALSE;
}

EXPORT_API int widget_app_terminate_context(widget_context_h context)
{
	if (context == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	g_idle_add(__finish_event_cb, context);
	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_foreach_context(widget_context_cb cb, void *data)
{
	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_add_event_handler(app_event_handler_h *event_handler,
					app_event_type_e event_type, app_event_cb callback,
					void *user_data)
{
	/* TODO */
	return 0;
}

EXPORT_API int widget_app_remove_event_handler(app_event_handler_h
						event_handler)
{
	/* TODO */
	return 0;
}

EXPORT_API const char* widget_app_get_id(widget_context_h context)
{
	return context->id;
}

EXPORT_API int widget_app_get_elm_win(widget_context_h context,
					Evas_Object **win)
{
	if (context == NULL || win == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	widget_context_s *cxt = (widget_context_s*)context;
	Evas_Object *ret_win = NULL;

	ret_win = elm_win_add(NULL, cxt->id, ELM_WIN_BASIC);
	if (ret_win == NULL) {
		_E("failed to create window");
		return WIDGET_ERROR_FAULT;
	}
	elm_win_title_set(ret_win, cxt->id);

	*win = ret_win;
	cxt->win = ret_win;

	return WIDGET_ERROR_NONE;
}

widget_class_h _widget_class_create(widget_class_s *prev, const char *class_id,
		widget_instance_lifecycle_callback_s callback, void *user_data)
{
	widget_class_s *wc = (widget_class_s*)malloc(sizeof(widget_class_s));

	if (wc == NULL) {
		_E("failed to malloc : %s", __FUNCTION__);
		set_last_result(WIDGET_ERROR_OUT_OF_MEMORY);
		return NULL;
	}

	wc->classid = strdup(class_id);
	wc->user_data = user_data;
	wc->ops = callback;
	wc->next = prev;
	wc->prev = NULL;

	set_last_result(WIDGET_ERROR_NONE);

	if (prev)
		prev->prev = wc;

	return wc;
}

EXPORT_API widget_class_h widget_app_class_add(widget_class_h widget_class, const char *class_id,
		widget_instance_lifecycle_callback_s callback, void *user_data)
{
	if (class_id == NULL) {
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	return _widget_class_create(widget_class, class_id, callback, user_data);
}

EXPORT_API widget_class_h widget_app_class_create(widget_instance_lifecycle_callback_s callback, void *user_data)
{
	return _widget_class_create(class_provider, appid, callback, user_data);
}

EXPORT_API int widget_app_context_set_tag(widget_context_h context, void *tag)
{
	if (context == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	context->tag = tag;

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_context_get_tag(widget_context_h context, void **tag)
{
	if (context == NULL || tag == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	*tag = context->tag;

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_context_set_content_info(widget_context_h context, bundle *content_info)
{
	const char *class_id = NULL;
	int ret = 0;
	if (context == NULL || content_info == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	if (context->provider == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	class_id = context->provider->classid;

	if (class_id == NULL)
		return widget_app_error(WIDGET_ERROR_FAULT, __FUNCTION__, NULL);

	ret = __send_update_status(class_id, context->id, WIDGET_INSTANCE_EVENT_UPDATE, content_info, true);

	if (ret < 0) {
		_E("failed to send content info: %s of %s (%d)", context->id, class_id, ret);
		return widget_app_error(WIDGET_ERROR_IO_ERROR, __FUNCTION__, NULL);
	}

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_context_set_title(widget_context_h context, const char *title)
{
	/* TODO
	 may use widget status update, or use surface title set.
	 */
	return 0;
}

