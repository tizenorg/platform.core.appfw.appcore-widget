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
#include <app_control.h>
#include <app_control_internal.h>
#include <widget.h>
#include <widget_service.h>
#include <widget_service_internal.h>
#include <widget_provider_app.h>
#include <widget_provider_app_internal.h>
#include <Elementary.h>
#include <vconf.h>
#include <widget_errno.h>
#include <system_info.h>

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

#define ELM_WIN_TIZEN_WIDGET -1
/* TODO
 * This definition is intended to prevent the build break.
 * This should be removed after adding ELM_WIN_TIZEN_WIDGET in Elm_Win_Type.
 */

#define WIDGET_APP_EVENT_MAX 5
static GList *handler_list[WIDGET_APP_EVENT_MAX] = {NULL, };

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
	widget_instance_lifecycle_callback_s ops;
	widget_obj_private_ops_s ops_private;
};

struct _widget_context {
	char *id;
	int state;
	void *tag;
	widget_instance_lifecycle_callback_s ops;
	widget_obj_private_ops_s ops_private;
};

typedef struct _widget_class widget_class_s;
typedef struct _widget_context widget_context_s;

static widget_app_lifecycle_callback_s *app_ops = NULL;
static void *app_user_data = NULL;
static widget_class_factory_full_s factory;
static widget_class_s *widget_class = NULL;
static widget_class_s widget_class_tmp;
static GList *contexts = NULL;
static int is_init_provider = 0;
static char *appid = NULL;
static int is_background = -1;

static gint __comp_by_id(gconstpointer a, gconstpointer b)
{
	widget_context_s *wc = (widget_context_s*)a;

	return strcmp(wc->id, (const char*)b);
}

static void __check_status_for_cgroup(void)
{
	GList *iter = g_list_first(contexts);

	while (iter != NULL) {
		widget_context_s *cxt = (widget_context_s*) iter->data;

		if (cxt->state != WC_PAUSED) {
			if (is_background != 0) {
				is_background = 0;
				_I("enter foreground group");
				//TODO: Do something to enter foreground group
			}
			return;
		}
		iter = g_list_next(iter);
	}

	if (g_list_length(contexts) > 0 && is_background == 0) {
		is_background = 1;
		_I("enter background group");
		//TODO: DO something to enter background group
	}
}

static widget_context_s* __find_context_by_id(const char *id)
{
	GList* ret = g_list_find_custom(contexts, id, __comp_by_id);

	if ( ret == NULL)
		return NULL;

	return ret->data;
}

static int __provider_create_cb(const char *id, const char *content, int w,
                                int h,
                                void *data)
{
	int ret = WIDGET_ERROR_FAULT;
	widget_context_s *wc = (widget_context_s*)malloc(sizeof(widget_context_s));
	if (wc == NULL)
		return WIDGET_ERROR_OUT_OF_MEMORY;

	wc->id = strdup(id);
	wc->state = WC_READY;
	wc->tag = NULL;
	wc->ops = widget_class->ops;
	wc->ops_private = widget_class->ops_private;
	contexts = g_list_append(contexts, wc);

	if (wc->ops.create) {
		bundle *b = bundle_decode((const bundle_raw*)content, strlen(content));
		ret = wc->ops.create(wc, b,  w, h);
		bundle_free(b);
	}
	_I("widget obj was created");

	return ret;
}

static int __provider_resize_cb(const char *id, int w, int h, void *data)
{
	int ret = WIDGET_ERROR_FAULT;
	widget_context_s *cxt = __find_context_by_id(id);

	if (cxt) {
		if (cxt->ops.resize)
			ret = cxt->ops.resize(cxt, w, h);
		_I("received resizing signal");
	} else {
		_E("could not find widget obj : %s", __FUNCTION__);
	}

	return ret;
}

static int __provider_destroy_cb(const char *id, widget_destroy_type_e reason,
                                 void *data)
{
	int ret = WIDGET_ERROR_FAULT;
	widget_context_s *cxt = __find_context_by_id(id);

	if (cxt) {
		cxt->state = WC_TERMINATED;
		if (cxt->ops.destroy) {
			bundle *b  = bundle_create();
			ret = cxt->ops.destroy(cxt,(widget_app_destroy_type_e)reason, b);

			bundle_raw *raw = NULL;
			int len;

			bundle_encode(b, &raw, &len);
			if (raw) {
				widget_provider_app_send_extra_info(id, (const char*)raw, NULL);
				free(raw);
			}

			bundle_free(b);
			contexts = g_list_remove(contexts, cxt);

			free(cxt->id);
			free(cxt);
		}
		_I("widget obj was deleted");
	} else {
		_E("could not find widget obj : %s", __FUNCTION__);
	}

	return ret;
}

static int __provider_update_cb(const char *id, const char *content, int force,
                                void *data)
{
	int ret = WIDGET_ERROR_FAULT;
	widget_context_s *cxt = __find_context_by_id(id);

	if (cxt) {
		if (cxt->ops.update) {
			bundle *b = bundle_decode((const bundle_raw*)content, strlen(content));
			ret = cxt->ops.update(cxt, b, force);
			bundle_free(b);
		}
		_I("received updating signal");
	} else {
		_E("could not find widget obj : %s", __FUNCTION__);
	}

	return ret;
}

static int __provider_pause_cb(const char *id, void *data)
{
	int ret = WIDGET_ERROR_FAULT;
	widget_context_s *cxt = __find_context_by_id(id);

	if (cxt) {
		if (cxt->ops.pause)
			ret = cxt->ops.pause(cxt);
		cxt->state = WC_PAUSED;
		_I("widget obj was paused");
	} else {
		_E("could not find widget obj : %s", __FUNCTION__);
	}

	__check_status_for_cgroup();
	return ret;
}

static int __provider_resume_cb(const char *id, void *data)
{
	int ret = WIDGET_ERROR_FAULT;
	widget_context_s *cxt = __find_context_by_id(id);

	if (cxt) {
		if (cxt->ops.resume)
			ret = cxt->ops.resume(cxt);
		cxt->state = WC_RUNNING;
		_I("widget obj was resumed");
	} else {
		_E("could not find widget obj : %s", __FUNCTION__);
	}

	__check_status_for_cgroup();
	return ret;
}

static int __provider_text_signal_cb(const char *id, const char *signal_name,
                                     const char *source, struct widget_event_info *info, void *data)
{
	int ret = WIDGET_ERROR_FAULT;
	widget_context_s *cxt = __find_context_by_id(id);

	if (cxt) {
		if (cxt->ops_private.text_signal) {
			ret = cxt->ops_private.text_signal(cxt, signal_name, source,
			                                   (widget_obj_event_info_s*)info);
		}
		_I("received text signal");
	} else {
		_E("could not find widget obj : %s", __FUNCTION__);
	}

	return ret;
}

static const widget_class_factory_full_s*
__widget_class_factory_override_text_signal(widget_instance_text_signal_cb op)
{
	widget_class_tmp.ops_private.text_signal = op;
	return &factory;
}

static void __free_handler_cb(gpointer data)
{
	if (data)
		free(data);
}

static void __free_handler_list(void)
{
	int i;

	for (i = 0; i < WIDGET_APP_EVENT_MAX; i++) {
		g_list_free_full(handler_list[i], __free_handler_cb);
		handler_list[i] = NULL;
	}
}

static void __control(bundle *b)
{
	app_control_h app_control;

	if (is_init_provider) {
		_E("already initialized");
		return;
	}

	if (app_control_create_event(b, &app_control) != 0) {
		_E("failed to get the app_control handle");
		return;
	}

	char *op = NULL;
	app_control_get_operation(app_control, &op);

	if (op && strcmp(op, "http://tizen.org/appcontrol/operation/main") == 0) {
		static struct widget_provider_event_callback cb = {
			.create = __provider_create_cb,
			.resize = __provider_resize_cb,
			.destroy = __provider_destroy_cb,

			.update = __provider_update_cb,
			.text_signal = __provider_text_signal_cb,

			.pause = __provider_pause_cb,
			.resume = __provider_resume_cb,

			.connected = NULL,
			.disconnected = NULL,

			.data = NULL,
		};

		if (widget_provider_app_init(app_control, &cb) == 0) {
			is_init_provider = 1;
		}
	}

	app_control_destroy(app_control);
	if (op)
		free(op);
}

static void __pause_all()
{
	GList *iter = g_list_first(contexts);

	while (iter != NULL) {
		widget_context_s *cxt = (widget_context_s*) iter->data;
		const char *id = cxt->id;

		switch (cxt->state) {
		case WC_READY:
			__provider_resume_cb(id, NULL);
			__provider_pause_cb(id, NULL);
			break;

		case WC_RUNNING:
			__provider_pause_cb(id, NULL);
			break;
		}
		iter = g_list_next(iter);
	}
}

static int __aul_handler(aul_type type, bundle *b, void *data)
{
	switch (type) {
	case AUL_START:
		__control(b);
		break;
	case AUL_RESUME:
		break;
	case AUL_TERMINATE:
		widget_app_exit();
		break;
	default:
		break;
	}

	return 0;
}

static char* __get_domain_name(const char *aid)
{
	char *name_token = NULL;

	if (aid == NULL) {
		_E("appid is NULL");
		return NULL;
	}

	// com.vendor.name -> name
	name_token = strrchr(aid, '.');

	if (name_token == NULL) {
		_E("appid is invalid");
		return strdup(aid);
	}

	name_token++;

	return strdup(name_token);
}

static int __before_loop(int argc, char **argv)
{
	int r;

#if !(GLIB_CHECK_VERSION(2, 36, 0))
	g_type_init();
#endif

	elm_init(argc, argv);

	factory.override_text_signal = __widget_class_factory_override_text_signal;

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

	char *name = __get_domain_name(appid);

	if (name == NULL) {
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__,
		                        "Fail to call __get_domain_name");
	}

	r = _set_i18n(name);

	free(name);
	if (r < 0) {
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__,
		                        "Fail to call _set_i18n");
	}

	widget_provider_app_create_app();

	memset(&widget_class_tmp, 0, sizeof(widget_class_tmp));
	widget_class = app_ops->create(app_user_data);
	if (widget_class == NULL) {
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__,
		                        "widget_class is NULL");
	}

	return WIDGET_ERROR_NONE;
}

static void __after_loop()
{
	__pause_all();
	widget_provider_app_terminate_app(WIDGET_DESTROY_TYPE_TEMPORARY, 1);

	if (app_ops->terminate)
		app_ops->terminate(app_user_data);

	if (is_init_provider) {
		widget_provider_app_fini();
		is_init_provider = 0;
	}
	__free_handler_list();
	elm_shutdown();
	if (appid)
		free(appid);
	if (contexts)
		g_list_free(contexts);
	contexts = NULL;
}

static gboolean __finish_event_cb(gpointer user_data)
{
	if (user_data == NULL)
		return FALSE;

	widget_context_s *wc = (widget_context_s*) user_data;
	const char* id = wc->id;

	switch (wc->state) {
	case WC_READY:
		__provider_resume_cb(id, NULL);
		__provider_pause_cb(id, NULL);
		__provider_destroy_cb(id, WIDGET_DESTROY_TYPE_DEFAULT, NULL);
		break;

	case WC_RUNNING:
		__provider_pause_cb(id, NULL);
		__provider_destroy_cb(id, WIDGET_DESTROY_TYPE_DEFAULT, NULL);
		break;

	case WC_PAUSED:
		__provider_destroy_cb(id, WIDGET_DESTROY_TYPE_DEFAULT, NULL);
		break;
	}

	widget_provider_app_send_deleted(id);
	return FALSE;
}

static void __on_low_memory(keynode_t *key, void *data)
{
	int val;

	val = vconf_keynode_get_int(key);
	if (val == VCONFKEY_SYSMAN_LOW_MEMORY_SOFT_WARNING) {
		app_event_handler_h handler;
		struct app_event_info event;

		_I("widget_app_low_memory");

		event.type = APP_EVENT_LOW_MEMORY;
		event.value = (void*)&val;

		GList *iter = g_list_first(handler_list[APP_EVENT_LOW_MEMORY]);

		while (iter) {
			handler = (app_event_handler_h) iter->data;
			handler->cb(&event, handler->data);
			iter = g_list_next(iter);
		}
	}
}

static void __on_low_battery(keynode_t *key, void *data)
{
	int val;

	val = vconf_keynode_get_int(key);
	if (val <= VCONFKEY_SYSMAN_BAT_CRITICAL_LOW) {
		app_event_handler_h handler;
		struct app_event_info event;

		_I("widget_app_low_battery");

		event.type = APP_EVENT_LOW_BATTERY;
		event.value = (void*)&val;

		GList *iter = g_list_first(handler_list[APP_EVENT_LOW_BATTERY]);

		while (iter) {
			handler = (app_event_handler_h) iter->data;
			handler->cb(&event, handler->data);
			iter = g_list_next(iter);
		}
	}
}

static void __on_lang_changed(keynode_t *key, void *data)
{
	char *val;

	_update_lang();
	val = vconf_keynode_get_str(key);

	app_event_handler_h handler;
	struct app_event_info event;

	_I("widget_app_lang_changed");

	event.type = APP_EVENT_LANGUAGE_CHANGED;
	event.value = (void*)val;

	GList *iter = g_list_first(handler_list[APP_EVENT_LANGUAGE_CHANGED]);

	while (iter) {
		handler = (app_event_handler_h) iter->data;
		handler->cb(&event, handler->data);
		iter = g_list_next(iter);
	}
}

static void __on_region_changed(keynode_t *key, void *data)
{
	char *val;

	_update_region();
	val = vconf_keynode_get_str(key);

	app_event_handler_h handler;
	struct app_event_info event;

	_I("widget_app_region_changed");

	event.type = APP_EVENT_REGION_FORMAT_CHANGED;
	event.value = (void*)val;

	GList *iter = g_list_first(handler_list[APP_EVENT_REGION_FORMAT_CHANGED]);

	while (iter) {
		handler = (app_event_handler_h) iter->data;
		handler->cb(&event, handler->data);
		iter = g_list_next(iter);
	}
}

static void __register_event(int event_type)
{
	switch (event_type) {
	case APP_EVENT_LOW_MEMORY:
		vconf_notify_key_changed(VCONFKEY_SYSMAN_LOW_MEMORY, __on_low_memory, NULL);
		break;

	case APP_EVENT_LOW_BATTERY:
		vconf_notify_key_changed(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, __on_low_battery,
		                         NULL);
		break;

	case APP_EVENT_LANGUAGE_CHANGED:
		vconf_notify_key_changed(VCONFKEY_LANGSET, __on_lang_changed, NULL);
		break;

	case APP_EVENT_REGION_FORMAT_CHANGED:
		vconf_notify_key_changed(VCONFKEY_REGIONFORMAT, __on_region_changed, NULL);
		break;
	}
}

static void __unregister_event(int event_type)
{
	switch (event_type) {
	case APP_EVENT_LOW_MEMORY:
		vconf_ignore_key_changed(VCONFKEY_SYSMAN_LOW_MEMORY, __on_low_memory);
		break;

	case APP_EVENT_LOW_BATTERY:
		vconf_ignore_key_changed(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, __on_low_battery);
		break;

	case APP_EVENT_LANGUAGE_CHANGED:
		vconf_ignore_key_changed(VCONFKEY_LANGSET, __on_lang_changed);
		break;

	case APP_EVENT_REGION_FORMAT_CHANGED:
		vconf_ignore_key_changed(VCONFKEY_REGIONFORMAT, __on_region_changed);
		break;
	}
}

EXPORT_API int widget_app_main(int argc, char **argv,
                               widget_app_lifecycle_callback_s *callback, void *user_data)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0)
		return WIDGET_ERROR_FAULT;

	if (!feature)
		return WIDGET_ERROR_NOT_SUPPORTED;

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
	//aul_status_update(STATUS_DYING);
	__after_loop();

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_exit(void)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0)
		return WIDGET_ERROR_FAULT;

	if (!feature)
		return WIDGET_ERROR_NOT_SUPPORTED;

	ecore_main_loop_quit();

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_terminate_context(widget_context_h context)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0)
		return WIDGET_ERROR_FAULT;

	if (!feature)
		return WIDGET_ERROR_NOT_SUPPORTED;

	if (context == NULL) {
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__,
		                        "obj is NULL");
	}

	g_idle_add(__finish_event_cb, context);
	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_foreach_context(widget_context_cb cb, void *data)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0)
		return WIDGET_ERROR_FAULT;

	if (!feature)
		return WIDGET_ERROR_NOT_SUPPORTED;

	if (cb == NULL)
		return WIDGET_ERROR_INVALID_PARAMETER;

	GList *iter = g_list_first(contexts);

	while (iter != NULL) {
		widget_context_s *cxt = (widget_context_s*) iter->data;
		if ( !cb(cxt, data)) {
			return WIDGET_ERROR_CANCELED;
		}

		iter = g_list_next(iter);
	}

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_add_event_handler(app_event_handler_h *event_handler,
        app_event_type_e event_type, app_event_cb callback, void *user_data)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0)
		return WIDGET_ERROR_FAULT;

	if (!feature)
		return WIDGET_ERROR_NOT_SUPPORTED;

	app_event_handler_h handler;

	if (event_handler == NULL || callback == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	if (event_type < APP_EVENT_LOW_MEMORY
	    || event_type > APP_EVENT_REGION_FORMAT_CHANGED)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	if (event_type == APP_EVENT_DEVICE_ORIENTATION_CHANGED)
		return widget_app_error(WIDGET_ERROR_NOT_SUPPORTED, __FUNCTION__, NULL);

	GList *iter = g_list_first(handler_list[event_type]);

	while (iter) {
		handler = (app_event_handler_h) iter->data;

		if (handler->cb == callback)
			return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);
		iter = g_list_next(iter);
	}

	handler = calloc(1, sizeof(struct app_event_handler));
	if (!handler)
		return widget_app_error(WIDGET_ERROR_OUT_OF_MEMORY, __FUNCTION__, NULL);

	if (g_list_length(handler_list[event_type]) == 0) {
		__register_event(event_type);
	}

	handler->type = event_type;
	handler->cb = callback;
	handler->data = user_data;
	handler_list[event_type] = g_list_append(handler_list[event_type], handler);

	*event_handler = handler;

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_remove_event_handler(app_event_handler_h
        event_handler)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0)
		return WIDGET_ERROR_FAULT;

	if (!feature)
		return WIDGET_ERROR_NOT_SUPPORTED;

	app_event_type_e type;

	if (event_handler == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	type = event_handler->type;
	if (type < APP_EVENT_LOW_MEMORY || type > APP_EVENT_REGION_FORMAT_CHANGED)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);

	handler_list[type] = g_list_remove(handler_list[type], event_handler);
	free(event_handler);

	if (g_list_length(handler_list[type]) == 0)
		__unregister_event(type);

	return WIDGET_ERROR_NONE;
}

EXPORT_API const char* widget_app_get_id(widget_context_h context)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0) {
		set_last_result(WIDGET_ERROR_FAULT);
		return NULL;
	}

	if (!feature) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return NULL;
	}

	if (context == NULL) {
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	widget_context_s *cxt = (widget_context_s*)context;

	set_last_result(WIDGET_ERROR_NONE);
	return cxt->id;
}

EXPORT_API int widget_app_get_elm_win(widget_context_h context,
                                      Evas_Object **win)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0)
		return WIDGET_ERROR_FAULT;

	if (!feature)
		return WIDGET_ERROR_NOT_SUPPORTED;

	if (context == NULL || win == NULL) {
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);
	}

	widget_context_s *cxt = (widget_context_s*)context;
	Evas *evas;
	Evas_Object *ret_win = NULL;

	evas = widget_get_evas(cxt->id);
	if (evas) {
		Evas_Object *widget_parent;
		widget_parent = evas_object_rectangle_add(evas);
		if (widget_parent) {
			ret_win = elm_win_add(widget_parent, cxt->id, ELM_WIN_TIZEN_WIDGET);
			evas_object_del(widget_parent);
			if (ret_win == NULL) {
				_E("win is NULL");
				return widget_app_error(WIDGET_ERROR_FAULT, __FUNCTION__, NULL);
			}
		} else {
			_E("Failed to get parent widget");
			return widget_app_error(WIDGET_ERROR_FAULT, __FUNCTION__, NULL);
		}
	} else {
		_E("parent evas object is NULL");
		return widget_app_error(WIDGET_ERROR_FAULT, __FUNCTION__, NULL);
	}

	*win = ret_win;
	return WIDGET_ERROR_NONE;
}

EXPORT_API widget_class_h widget_app_class_create(widget_instance_lifecycle_callback_s callback)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0) {
		set_last_result(WIDGET_ERROR_FAULT);
		return NULL;
	}

	if (!feature) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return NULL;
	}

	widget_class_s *wc = (widget_class_s*)malloc(sizeof(widget_class_s));

	if (wc == NULL) {
		_E("failed to malloc : %s", __FUNCTION__);
		set_last_result(WIDGET_ERROR_OUT_OF_MEMORY);
		return NULL;
	}

	wc->ops = callback;
	wc->ops_private = widget_class_tmp.ops_private;
	set_last_result(WIDGET_ERROR_NONE);
	return wc;
}

EXPORT_API int widget_app_context_set_tag(widget_context_h context, void *tag)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0)
		return WIDGET_ERROR_FAULT;

	if (!feature)
		return WIDGET_ERROR_NOT_SUPPORTED;

	if (context == NULL) {
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);
	}

	context->tag = tag;

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_context_get_tag(widget_context_h context, void **tag)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0)
		return WIDGET_ERROR_FAULT;

	if (!feature)
		return WIDGET_ERROR_NOT_SUPPORTED;

	if (context == NULL || tag == NULL) {
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER, __FUNCTION__, NULL);
	}

	*tag = context->tag;

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_context_set_content_info(widget_context_h context, bundle *content_info)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0)
		return WIDGET_ERROR_FAULT;

	if (!feature)
		return WIDGET_ERROR_NOT_SUPPORTED;

	if (content_info == NULL)
		return WIDGET_ERROR_INVALID_PARAMETER;

	bundle_raw *raw = NULL;
	int len;
	int ret = WIDGET_ERROR_FAULT;

	bundle_encode(content_info, &raw, &len);
	if (raw) {
		ret = widget_provider_app_send_extra_info(context->id, (const char*)raw, NULL);
		free(raw);
	}

	return ret;
}

EXPORT_API int widget_app_context_set_title(widget_context_h context, const char *title)
{
	int r;
	bool feature;

	r = system_info_get_platform_bool(FEATURE_SHELL_APPWIDGET, &feature);
	if (r < 0)
		return WIDGET_ERROR_FAULT;

	if (!feature)
		return WIDGET_ERROR_NOT_SUPPORTED;

	if (context == NULL)
		return WIDGET_ERROR_INVALID_PARAMETER;

	return widget_provider_app_send_extra_info(context->id, NULL, title);
}

// private API
EXPORT_API const widget_class_factory_full_s* widget_app_get_class_factory(void)
{
	return &factory;
}
