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


#include <stdlib.h>

#include <bundle.h>
#include <bundle_internal.h>
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
#include <widget_service.h>
#include <widget_service_internal.h>
#include <aul_app_com.h>
#include <Ecore_Wayland.h>
#include <system_info.h>
#include <vconf.h>
#include <vconf-internal-keys.h>

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

enum {
	UPDATE_LOCAL = 0,
	UPDATE_ALL = 1,
};

struct _widget_class {
	void *user_data;
	widget_instance_lifecycle_callback_s ops;
	char *classid;
	struct _widget_class *next;
	struct _widget_class *prev;
};

struct app_event_handler {
	app_event_type_e type;
	app_event_cb cb;
	void *data;
};

struct app_event_info {
	app_event_type_e type;
	void *value;
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

typedef struct _widget_class widget_class_s;
typedef struct _widget_context widget_context_s;

#define WIDGET_APP_EVENT_MAX 5
static GList *handler_list[WIDGET_APP_EVENT_MAX] = {NULL, };

static int caller_pid;
static widget_app_lifecycle_callback_s *app_ops;
static void *app_user_data;
static char *appid;
static widget_class_h class_provider;
static GList *contexts;
static char *viewer_endpoint;
static int exit_called;

static void _widget_core_set_appcore_event_cb(void);
static void _widget_core_unset_appcore_event_cb(void);

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

static inline bool _is_widget_feature_enabled(void)
{
	static bool feature = false;
	static bool retrieved = false;
	int ret;

	if (retrieved == true)
		return feature;

	ret = system_info_get_platform_bool(
			"http://tizen.org/feature/shell.appwidget", &feature);
	if (ret != SYSTEM_INFO_ERROR_NONE) {
		_E("failed to get system info"); /* LCOV_EXCL_LINE */
		return false; /* LCOV_EXCL_LINE */
	}

	retrieved = true;

	return feature;
}

static gint __comp_by_id(gconstpointer a, gconstpointer b)
{
	widget_context_s *wc = (widget_context_s *)a;

	return strcmp(wc->id, (const char *)b);
}

static widget_context_s *__find_context_by_id(const char *id)
{
	GList *ret;

	if (id == NULL)
		return NULL;

	ret = g_list_find_custom(contexts, id, __comp_by_id);
	if (ret == NULL)
		return NULL;

	return ret->data;
}

static gint __comp_by_win(gconstpointer a, gconstpointer b)
{
	int win = GPOINTER_TO_INT(b);
	widget_context_s *wc = (widget_context_s *)a;

	return (wc && wc->win_id == win) ? 0 : -1;
}

static widget_context_s *__find_context_by_win(int win)
{
	GList *ret = g_list_find_custom(contexts, GINT_TO_POINTER(win), __comp_by_win);

	if (ret == NULL)
		return NULL;

	return ret->data;
}

static int __send_lifecycle_event(const char *class_id, const char *instance_id,
	int status)
{
	bundle *b = bundle_create();
	int ret;

	if (b == NULL) {
		_E("out of memory"); /* LCOV_EXCL_LINE */
		return -1; /* LCOV_EXCL_LINE */
	}

	bundle_add_str(b, AUL_K_WIDGET_ID, class_id);
	bundle_add_str(b, AUL_K_WIDGET_INSTANCE_ID, instance_id);
	bundle_add_byte(b, AUL_K_WIDGET_STATUS, &status, sizeof(int));

	_D("send lifecycle %s(%d)", instance_id, status);
	ret = aul_app_com_send("widget.status", b);
	if (ret < 0)
		_E("send lifecycle error:%d", ret); /* LCOV_EXCL_LINE */

	bundle_free(b);

	return ret;
}

static int __send_update_status(const char *class_id, const char *instance_id,
	int status, bundle *extra)
{
	bundle *b;
	int lifecycle = -1;
	bundle_raw *raw = NULL;
	int len;

	b = bundle_create();
	if (!b) {
		_E("out of memory"); /* LCOV_EXCL_LINE */
		return -1; /* LCOV_EXCL_LINE */
	}

	bundle_add_str(b, AUL_K_WIDGET_ID, class_id);
	bundle_add_str(b, AUL_K_WIDGET_INSTANCE_ID, instance_id);
	bundle_add_byte(b, AUL_K_WIDGET_STATUS, &status, sizeof(int));

	if (extra) {
		bundle_encode(extra, &raw, &len);
		bundle_add_str(b, WIDGET_K_CONTENT_INFO, (const char *)raw);
		aul_widget_instance_add(class_id, instance_id);
	}

	_D("send update %s(%d) to %s", instance_id, status, viewer_endpoint);
	aul_app_com_send(viewer_endpoint, b);

	switch (status) {
	case WIDGET_INSTANCE_EVENT_CREATE:
		lifecycle = WIDGET_LIFE_CYCLE_EVENT_CREATE;
		break;
	case WIDGET_INSTANCE_EVENT_DESTROY:
		lifecycle = WIDGET_LIFE_CYCLE_EVENT_DESTROY;
		break;
	case WIDGET_INSTANCE_EVENT_PAUSE:
		lifecycle = WIDGET_LIFE_CYCLE_EVENT_PAUSE;
		break;
	case WIDGET_INSTANCE_EVENT_RESUME:
		lifecycle = WIDGET_LIFE_CYCLE_EVENT_RESUME;
		break;
	}

	if (lifecycle > -1)
		__send_lifecycle_event(class_id, instance_id, lifecycle);

	bundle_free(b);
	if (raw)
		free(raw);

	return 0;
}

static int __instance_resume(widget_class_h handle, const char *id, int send_update)
{
	widget_context_s *wc = __find_context_by_id(id);
	int ret;

	if (!wc) {
		_E("context not found: %s", id); /* LCOV_EXCL_LINE */
		return -1; /* LCOV_EXCL_LINE */
	}

	if (wc->state == WC_RUNNING) {
		_D("%s is already in running state", id); /* LCOV_EXCL_LINE */
		return 0; /* LCOV_EXCL_LINE */
	}

	if (wc->state == WC_TERMINATED) {
		_D("%s is in terminated state", id); /* LCOV_EXCL_LINE */
		return 0; /* LCOV_EXCL_LINE */
	}

	if (handle->ops.resume)
		handle->ops.resume(wc, handle->user_data);

	wc->state = WC_RUNNING;
	_D("%s is resumed", id);
	if (send_update) {
		ret = __send_update_status(handle->classid, wc->id,
			WIDGET_INSTANCE_EVENT_RESUME, NULL);
	} else {
		ret = 0;
	}

	return ret;
}

static int __instance_pause(widget_class_h handle, const char *id, int send_update)
{
	widget_context_s *wc = __find_context_by_id(id);
	int ret;

	if (!wc) {
		_E("context not found: %s", id); /* LCOV_EXCL_LINE */
		return -1; /* LCOV_EXCL_LINE */
	}

	if (wc->state == WC_PAUSED) {
		_D("%s is already in paused state", id); /* LCOV_EXCL_LINE */
		return 0; /* LCOV_EXCL_LINE */
	}

	if (wc->state == WC_TERMINATED) {
		_D("%s is in terminated state", id); /* LCOV_EXCL_LINE */
		return 0; /* LCOV_EXCL_LINE */
	}

	if (handle->ops.pause)
		handle->ops.pause(wc, handle->user_data);

	wc->state = WC_PAUSED;
	_D("%s is paused", id);
	if (send_update) {
		ret = __send_update_status(handle->classid, wc->id,
			WIDGET_INSTANCE_EVENT_PAUSE, NULL);
	} else {
		ret = 0;
	}

	return ret;
}

static int __instance_resize(widget_class_h handle, const char *id, int w, int h)
{
	widget_context_s *wc = __find_context_by_id(id);
	int ret;

	if (!wc) {
		_E("context not found: %s", id); /* LCOV_EXCL_LINE */
		return -1; /* LCOV_EXCL_LINE */
	}

	if (handle->ops.resize)
		handle->ops.resize(wc, w, h, handle->user_data);

	_D("%s is resized to %dx%d", id, w, h);
	ret = __send_update_status(handle->classid, wc->id,
		WIDGET_INSTANCE_EVENT_SIZE_CHANGED, NULL);

	return ret;
}

/* LCOV_EXCL_START */
static int __instance_update_all(widget_class_h handle, int force, const char *content)
{
	widget_context_s *wc;
	int ret = 0;
	bundle *b = NULL;
	GList *context = contexts;

	if (content)
		b = bundle_decode((const bundle_raw *)content, strlen(content));

	if (handle->ops.update) {
		while (context) {
			wc = (widget_context_s *)context->data;
			handle->ops.update(wc, b, force, handle->user_data);
			ret = __send_update_status(handle->classid, wc->id,
				WIDGET_INSTANCE_EVENT_UPDATE, NULL);
			_D("updated:%s", wc->id);
			context = context->next;
		}
	}

	if (b)
		bundle_free(b);

	return ret;
}
/* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */
static int __instance_update(widget_class_h handle, const char *id, int force, const char *content)
{
	widget_context_s *wc = __find_context_by_id(id);
	int ret = 0;
	bundle *b = NULL;
	if (!wc) {
		_E("context not found: %s", id);
		return -1;
	}

	if (content)
		b = bundle_decode((const bundle_raw *)content, strlen(content));

	if (handle->ops.update) {
		handle->ops.update(wc, b, force, handle->user_data);
		ret = __send_update_status(handle->classid, wc->id,
			WIDGET_INSTANCE_EVENT_UPDATE, NULL);
		_D("updated:%s", id);
	}

	if (b)
		bundle_free(b);

	return ret;
}
/* LCOV_EXCL_STOP */

static int __instance_create(widget_class_h handle, const char *id, const char *content, int w, int h)
{
	widget_context_s *wc = NULL;
	int ret = 0;
	bundle *content_info = NULL;

	wc = (widget_context_s *)calloc(1, sizeof(widget_context_s));
	if (!wc)
		return WIDGET_ERROR_OUT_OF_MEMORY;

	wc->state = WC_READY;
	wc->id = g_strdup(id);
	wc->provider = handle;
	wc->win = NULL;
	wc->win_id = -1;

	if (content) {
		wc->content = strdup(content);
		content_info = bundle_decode((const bundle_raw *)content, strlen(content));
	}

	contexts = g_list_append(contexts, wc);

	ret = handle->ops.create(wc, content_info, w, h, handle->user_data);
	if (ret < 0) {
		/* TODO send abort */
	} else {
		ret = __send_update_status(handle->classid, wc->id,
			WIDGET_INSTANCE_EVENT_CREATE, NULL);

		if (content == NULL)
			content = "NULL";

		aul_widget_instance_add(handle->classid, id);
	}

	if (content_info)
		bundle_free(content_info);

	return ret;
}

static int __instance_destroy(widget_class_h handle, const char *id,
		widget_app_destroy_type_e reason, int send_update)
{
	widget_context_s *wc = __find_context_by_id(id);
	int ret = 0;
	int event = WIDGET_INSTANCE_EVENT_TERMINATE;
	bundle *content_info;

	if (!wc) {
		_E("could not find widget obj: %s", id); /* LCOV_EXCL_LINE */
		return WIDGET_ERROR_INVALID_PARAMETER; /* LCOV_EXCL_LINE */
	}

	wc->state = WC_TERMINATED;
	if (wc->content)
		content_info = bundle_decode((const bundle_raw *)wc->content, strlen(wc->content));
	else
		content_info = bundle_create();

	handle->ops.destroy(wc, reason, content_info,
			handle->user_data);

	if (reason == WIDGET_APP_DESTROY_TYPE_PERMANENT) {
		event = WIDGET_INSTANCE_EVENT_DESTROY;
	} else {
		ret = __send_update_status(handle->classid, id,
				WIDGET_INSTANCE_EVENT_EXTRA_UPDATED, content_info);
	}

	aul_widget_instance_del(handle->classid, id);

	if (content_info)
		bundle_free(content_info);

	ret = __send_update_status(handle->classid, id, event, NULL);

	contexts = g_list_remove(contexts, wc);

	if (wc->id)
		free(wc->id);

	if (wc->content)
		free(wc->content);

	free(wc);

	if (contexts == NULL && !exit_called) /* all instance destroyed */
		widget_app_exit();

	return ret;
}

static widget_class_h __find_class_handler(const char *class_id,
		widget_class_h handle)
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

/* LCOV_EXCL_START */
static void __resize_window(char *id, int w, int h)
{
	widget_context_s *wc = __find_context_by_id(id);

	if (!wc) {
		_E("can not find instance: %s", id);
		return;
	}

	if (wc->win)
		evas_object_resize(wc->win, w, h);
	else
		_E("unable to find window of %d", wc->id);
}
/* LCOV_EXCL_STOP */

static void __control(bundle *b)
{
	char *class_id = NULL;
	char *id = NULL;
	char *operation = NULL;
	char *content = NULL;
	char *w_str = NULL;
	char *h_str = NULL;
	int w = 0;
	int h = 0;
	char *remain = NULL;
	int force;
	char *force_str = NULL;
	widget_class_h handle = NULL;

	bundle_get_str(b, WIDGET_K_CLASS, &class_id);
	/* for previous version compatibility, use appid for default class id */
	if (class_id == NULL)
		class_id = appid;

	bundle_get_str(b, AUL_K_WIDGET_INSTANCE_ID, &id);
	bundle_get_str(b, WIDGET_K_OPERATION, &operation);

	handle = __find_class_handler(class_id, class_provider);
	if (!handle) {
		_E("no handle provided: %s", class_id); /* LCOV_EXCL_LINE */
		goto error;
	}

	if (!operation) {
		_E("no operation provided");
		goto error;
	}

	bundle_get_str(b, WIDGET_K_FORCE, &force_str);

	if (force_str && strcmp(force_str, "true") == 0)
		force = 1;
	else
		force = 0;

	bundle_get_str(b, WIDGET_K_CONTENT_INFO, &content);
	bundle_get_str(b, WIDGET_K_WIDTH, &w_str);
	bundle_get_str(b, WIDGET_K_HEIGHT, &h_str);
	if (w_str)
		w = (int)g_ascii_strtoll(w_str, &remain, 10);

	if (h_str)
		h = (int)g_ascii_strtoll(h_str, &remain, 10);

	if (strcmp(operation, "create") == 0) {
		__instance_create(handle, id, content, w, h);
	} else if (strcmp(operation, "resize") == 0) {
		__resize_window(id, w, h);
	} else if (strcmp(operation, "update") == 0) {
		if (id)
			__instance_update(handle, id, force, content);
		else
			__instance_update_all(handle, force, content);

	} else if (strcmp(operation, "destroy") == 0) {
		__instance_destroy(handle, id, WIDGET_APP_DESTROY_TYPE_PERMANENT, UPDATE_ALL);
	} else if (strcmp(operation, "resume") == 0) {
		__instance_resume(handle, id, UPDATE_ALL);
	} else if (strcmp(operation, "pause") == 0) {
		__instance_pause(handle, id, UPDATE_ALL);
	} else if (strcmp(operation, "terminate") == 0) {
		__instance_destroy(handle, id, WIDGET_APP_DESTROY_TYPE_TEMPORARY, UPDATE_ALL);
	}

	return;
error:
	LOGD("error on control");
	return;
}

static void __pause_all(int send_update)
{
	GList *iter = g_list_first(contexts);

	while (iter != NULL) {
		widget_context_s *cxt = (widget_context_s *)iter->data;

		switch (cxt->state) {
		case WC_READY:
			__instance_resume(cxt->provider, cxt->id, send_update);
			__instance_pause(cxt->provider, cxt->id, send_update);
			break;
		case WC_RUNNING:
			__instance_pause(cxt->provider, cxt->id, send_update);
			break;
		}
		iter = g_list_next(iter);
	}
}

/* LCOV_EXCL_START */
static void __resume_all(int send_update)
{
	GList *iter = g_list_first(contexts);

	while (iter != NULL) {
		widget_context_s *cxt = (widget_context_s *)iter->data;

		switch (cxt->state) {
		case WC_READY:
			__instance_resume(cxt->provider, cxt->id, send_update);
			break;
		case WC_PAUSED:
			__instance_resume(cxt->provider, cxt->id, send_update);
			break;
		}
		iter = g_list_next(iter);
	}
}
/* LCOV_EXCL_STOP */

static void __destroy_all(int reason, int send_update)
{
	GList *iter = g_list_first(contexts);

	__pause_all(send_update);

	while (iter != NULL) {
		widget_context_s *cxt = (widget_context_s *)iter->data;

		switch (cxt->state) {
		case WC_PAUSED:
			__instance_destroy(cxt->provider, cxt->id, reason, send_update);
			break;
		}
		iter = g_list_next(iter);
	}
}


static Eina_Bool __show_cb(void *data, int type, void *event)
{
	Ecore_Wl_Event_Window_Show *ev = event;
	widget_context_s *cxt = __find_context_by_win(ev->win);

	LOGD("show %d %d", (unsigned int)ev->win, (unsigned int)ev->data[0]);

	if (cxt)
		__instance_resume(cxt->provider, cxt->id, UPDATE_ALL);
	else
		LOGE("unknown window error: %d", ev->win); /* LCOV_EXCL_LINE */

	return ECORE_CALLBACK_RENEW;
}

static Eina_Bool __hide_cb(void *data, int type, void *event)
{
	Ecore_Wl_Event_Window_Hide *ev = event;
	widget_context_s *cxt = __find_context_by_win(ev->win);


	LOGD("hide %d", (unsigned int)ev->win);

	if (cxt)
		__instance_pause(cxt->provider, cxt->id, UPDATE_ALL);
	else
		LOGE("unknown window error: %d", ev->win); /* LCOV_EXCL_LINE */

	return ECORE_CALLBACK_RENEW;
}

static Eina_Bool __visibility_cb(void *data, int type, void *event)
{
	Ecore_Wl_Event_Window_Visibility_Change *ev = event;
	widget_context_s *cxt = __find_context_by_win(ev->win);

	LOGD("visiblity change: %d %d", (unsigned int)ev->win,  (unsigned int)ev->fully_obscured);

	if (!cxt) {
		LOGE("unknown window error: %d", ev->win);
		return ECORE_CALLBACK_RENEW;
	}

	if (cxt->state == WC_PAUSED && ev->fully_obscured == 0) {
		__instance_resume(cxt->provider, cxt->id, UPDATE_ALL);
	} else if (cxt->state == WC_RUNNING && ev->fully_obscured == 1) {
		__instance_pause(cxt->provider, cxt->id, UPDATE_ALL);
	} else {
		LOGD("cxt:%s state:%d obscured:%d", cxt->id, cxt->state, ev->fully_obscured);
	}

	return ECORE_CALLBACK_RENEW;
}

/* LCOV_EXCL_START */
static Eina_Bool __lower_cb(void *data, int type, void *event)
{
	LOGD("lower");
	return ECORE_CALLBACK_RENEW;
}
/* LCOV_EXCL_STOP */

static Eina_Bool __configure_cb(void *data, int type, void *event)
{
	Ecore_Wl_Event_Window_Configure *ev = event;
	widget_context_s *cxt = __find_context_by_win(ev->win);

	LOGD("configure: %d %d", ev->w, ev->h);

	if (!cxt) {
		LOGE("unknown window error: %d", ev->win); /* LCOV_EXCL_LINE */
		return ECORE_CALLBACK_RENEW; /* LCOV_EXCL_LINE */
	}

	if (cxt->state == WC_PAUSED || cxt->state == WC_RUNNING)
		__instance_resize(cxt->provider, cxt->id, ev->w, ev->h);
	LOGD("cxt:%s resized to %dx%d", cxt->id, ev->w, ev->h);

	return ECORE_CALLBACK_RENEW;
}

static void __add_climsg()
{
	ecore_event_handler_add(ECORE_WL_EVENT_WINDOW_SHOW, __show_cb, NULL);
	ecore_event_handler_add(ECORE_WL_EVENT_WINDOW_HIDE, __hide_cb, NULL);
	ecore_event_handler_add(ECORE_WL_EVENT_WINDOW_VISIBILITY_CHANGE, __visibility_cb, NULL);
	ecore_event_handler_add(ECORE_WL_EVENT_WINDOW_LOWER, __lower_cb, NULL);
	ecore_event_handler_add(ECORE_WL_EVENT_WINDOW_CONFIGURE, __configure_cb, NULL);
}

static int __aul_handler(aul_type type, bundle *b, void *data)
{
	char *caller = NULL;
	char *remain = NULL;

	switch (type) {
	case AUL_START:
		if (b) {
			bundle_get_str(b, WIDGET_K_CALLER, &caller);
			if (caller) {
				caller_pid = g_ascii_strtoll(caller, &remain,
						10);
			} else {
				/* using caller appid and query pid using caller appid? */
				_E("no caller pid");
			}
		}

		__control(b);
		break;
	case AUL_RESUME:
		__resume_all(UPDATE_ALL);
		break;
	case AUL_TERMINATE:
		widget_app_exit();
		break;
	default:
		break;
	}

	return 0;
}

static char *__get_domain_name(char *appid)
{
	char *name_token;

	if (appid == NULL) {
		_E("appid is NULL"); /* LCOV_EXCL_LINE */
		return NULL; /* LCOV_EXCL_LINE */
	}

	name_token = strrchr(appid, '.');

	if (name_token == NULL) {
		_E("appid is invalid"); /* LCOV_EXCL_LINE */
		return appid; /* LCOV_EXCL_LINE */
	}

	name_token++;

	return name_token;
}

/* LCOV_EXCL_START */
static void __on_poweroff(keynode_t *key, void *data)
{
	int val;

	val = vconf_keynode_get_int(key);
	switch (val) {
	case VCONFKEY_SYSMAN_POWER_OFF_DIRECT:
	case VCONFKEY_SYSMAN_POWER_OFF_RESTART:
		_I("power off changed: %d", val);
		widget_app_exit();
		break;
	case VCONFKEY_SYSMAN_POWER_OFF_NONE:
	case VCONFKEY_SYSMAN_POWER_OFF_POPUP:
	default:
		/* DO NOTHING */
		break;
	}
}
/* LCOV_EXCL_STOP */

extern int _set_i18n(const char *name);

static int __before_loop(int argc, char **argv)
{
	int r;
	bundle *kb = NULL;
	char *wayland_display = NULL;
	char *xdg_runtime_dir = NULL;
	char *name;

#if !(GLIB_CHECK_VERSION(2, 36, 0))
	g_type_init();
#endif

	kb = bundle_import_from_argv(argc, argv);
	if (kb) {
		bundle_get_str(kb, AUL_K_WAYLAND_WORKING_DIR, &xdg_runtime_dir);
		bundle_get_str(kb, AUL_K_WAYLAND_DISPLAY, &wayland_display);
		bundle_get_str(kb, WIDGET_K_ENDPOINT, &viewer_endpoint);
		if (viewer_endpoint) {
			_E("viewer endpoint :%s", viewer_endpoint);
			viewer_endpoint = strdup(viewer_endpoint);
		} else {
			_E("endpoint is missing");
		}

		if (xdg_runtime_dir)
			setenv("XDG_RUNTIME_DIR", xdg_runtime_dir, 1);

		_D("xdg_runtime_dir:%s", xdg_runtime_dir);

		if (wayland_display)
			setenv("WAYLAND_DISPLAY", wayland_display, 1);

		_D("wayland_display:%s", wayland_display);

		bundle_free(kb);
		kb = NULL;
	} else {
		_E("failed to get launch argv"); /* LCOV_EXCL_LINE */
	}

	elm_init(argc, argv);

	r = aul_launch_init(__aul_handler, NULL);
	if (r < 0) {
		/* LCOV_EXCL_START */
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__,
				"Fail to call the aul_launch_init");
		/* LCOV_EXCL_STOP */
	}

	r = aul_launch_argv_handler(argc, argv);
	if (r < 0) {
		/* LCOV_EXCL_START */
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__,
				"Fail to call the aul_launch_argv_handler");
		/* LCOV_EXCL_STOP */
	}

	r = app_get_id(&appid);
	if (r != APP_ERROR_NONE)
		return r;

	name = __get_domain_name(appid);

	if (name == NULL) {
		/* LCOV_EXCL_START */
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__,
				"Fail to call __get_domain_name");
		/* LCOV_EXCL_STOP */
	}

	r = _set_i18n(name);

	if (r < 0) {
		/* LCOV_EXCL_START */
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__,
				"Fail to call _set_i18n");
		/* LCOV_EXCL_STOP */
	}

	__add_climsg();

	_widget_core_set_appcore_event_cb();

	class_provider = app_ops->create(app_user_data);
	if (class_provider == NULL) {
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__, "widget_class is NULL");
	}

	vconf_notify_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, __on_poweroff, NULL);

	return WIDGET_ERROR_NONE;
}

static void __after_loop()
{
	exit_called = 1;
	vconf_ignore_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, __on_poweroff);

	__pause_all(UPDATE_LOCAL);
	__destroy_all(WIDGET_APP_DESTROY_TYPE_TEMPORARY, UPDATE_ALL);

	if (app_ops->terminate)
		app_ops->terminate(app_user_data);

	if (viewer_endpoint)
		free(viewer_endpoint);

	_widget_core_unset_appcore_event_cb();
	__free_handler_list();
	elm_shutdown();
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
		event.value = (void *)&val;

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
		event.value = (void *)&val;

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
	event.value = (void *)val;

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
	event.value = (void *)val;

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
		vconf_notify_key_changed(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, __on_low_battery, NULL);
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

static void _widget_core_set_appcore_event_cb(void)
{
	__register_event(APP_EVENT_LANGUAGE_CHANGED);
	__register_event(APP_EVENT_REGION_FORMAT_CHANGED);
}

static void _widget_core_unset_appcore_event_cb(void)
{
	__unregister_event(APP_EVENT_LANGUAGE_CHANGED);
	__unregister_event(APP_EVENT_REGION_FORMAT_CHANGED);
}

EXPORT_API int widget_app_main(int argc, char **argv,
		widget_app_lifecycle_callback_s *callback, void *user_data)
{
	int r;

	if (!_is_widget_feature_enabled()) {
		_E("not supported"); /* LCOV_EXCL_LINE */
		return WIDGET_ERROR_NOT_SUPPORTED; /* LCOV_EXCL_LINE */
	}

	if (argc <= 0 || argv == NULL || callback == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__, NULL);

	if (callback->create == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__,
				"widget_app_create_cb() callback must be "
				"registered");

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
	if (!_is_widget_feature_enabled()) {
		_E("not supported"); /* LCOV_EXCL_LINE */
		return WIDGET_ERROR_NOT_SUPPORTED; /* LCOV_EXCL_LINE */
	}

	if (exit_called)
		return WIDGET_ERROR_NONE;

	exit_called = 1;

	ecore_main_loop_quit();

	return WIDGET_ERROR_NONE;
}

/* LCOV_EXCL_START */
static gboolean __finish_event_cb(gpointer user_data)
{
	if (user_data == NULL)
		return FALSE;

	widget_context_s *wc = (widget_context_s *)user_data;

	switch (wc->state) {
	case WC_READY:
		__instance_resume(wc->provider, wc->id, UPDATE_LOCAL);
	case WC_RUNNING:
		__instance_pause(wc->provider, wc->id, UPDATE_LOCAL);
	case WC_PAUSED:
		__instance_destroy(wc->provider, wc->id,
				WIDGET_DESTROY_TYPE_TEMPORARY, UPDATE_ALL);
		break;
	default:
		break;
	}

	return FALSE;
}
/* LCOV_EXCL_STOP */

EXPORT_API int widget_app_terminate_context(widget_context_h context)
{
	if (!_is_widget_feature_enabled()) {
		_E("not supported"); /* LCOV_EXCL_LINE */
		return WIDGET_ERROR_NOT_SUPPORTED; /* LCOV_EXCL_LINE */
	}

	if (context == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__, NULL);

	g_idle_add(__finish_event_cb, context);
	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_foreach_context(widget_context_cb cb, void *data)
{
	GList *list;
	widget_context_s *wc;

	if (!_is_widget_feature_enabled()) {
		_E("not supported"); /* LCOV_EXCL_LINE */
		return WIDGET_ERROR_NOT_SUPPORTED; /* LCOV_EXCL_LINE */
	}

	if (!cb)
		return WIDGET_ERROR_INVALID_PARAMETER;

	list = g_list_first(contexts);

	while (list) {
		wc = (widget_context_s *)list->data;
		if (wc) {
			if (!cb(wc, data))
				break;
		}
		list = list->next;
	}

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_add_event_handler(app_event_handler_h *event_handler,
					app_event_type_e event_type, app_event_cb callback,
					void *user_data)
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
		return widget_app_error(WIDGET_ERROR_OUT_OF_MEMORY, __FUNCTION__, NULL); /* LCOV_EXCL_LINE */

	if (g_list_length(handler_list[event_type]) == 0)
		__register_event(event_type);

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

EXPORT_API const char *widget_app_get_id(widget_context_h context)
{
	if (!_is_widget_feature_enabled()) {
		_E("not supported"); /* LCOV_EXCL_LINE */
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED); /* LCOV_EXCL_LINE */
		return NULL; /* LCOV_EXCL_LINE */
	}

	if (!context) {
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	set_last_result(WIDGET_ERROR_NONE);
	return context->id;
}

EXPORT_API int widget_app_get_elm_win(widget_context_h context,
					Evas_Object **win)
{
	widget_context_s *cxt = (widget_context_s *)context;
	Evas_Object *ret_win;
	Ecore_Wl_Window *wl_win;

	if (!_is_widget_feature_enabled()) {
		_E("not supported"); /* LCOV_EXCL_LINE */
		return WIDGET_ERROR_NOT_SUPPORTED; /* LCOV_EXCL_LINE */
	}

	if (context == NULL || win == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__, NULL);

	ret_win = elm_win_add(NULL, cxt->id, ELM_WIN_BASIC);
	if (ret_win == NULL) {
		_E("failed to create window"); /* LCOV_EXCL_LINE */
		return WIDGET_ERROR_FAULT; /* LCOV_EXCL_LINE */
	}

	wl_win = elm_win_wl_window_get(ret_win);
	if (wl_win == NULL) {
		_E("failed to get wayland window"); /* LCOV_EXCL_LINE */
		evas_object_del(ret_win); /* LCOV_EXCL_LINE */
		return WIDGET_ERROR_FAULT; /* LCOV_EXCL_LINE */
	}

	ecore_wl_window_class_name_set(wl_win, cxt->id);

	*win = ret_win;
	cxt->win = ret_win;
	cxt->win_id = ecore_wl_window_id_get(wl_win);

	_D("window created: %d", cxt->win_id);

	return WIDGET_ERROR_NONE;
}

widget_class_h _widget_class_create(widget_class_s *prev, const char *class_id,
		widget_instance_lifecycle_callback_s callback, void *user_data)
{
	widget_class_s *wc;

	if (!_is_widget_feature_enabled()) {
		_E("not supported"); /* LCOV_EXCL_LINE */
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED); /* LCOV_EXCL_LINE */
		return NULL;
	}

	if (class_id == NULL) {
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	wc = (widget_class_s *)calloc(1, sizeof(widget_class_s));
	if (wc == NULL) {
		_E("failed to calloc : %s", __FUNCTION__); /* LCOV_EXCL_LINE */
		set_last_result(WIDGET_ERROR_OUT_OF_MEMORY); /* LCOV_EXCL_LINE */
		return NULL; /* LCOV_EXCL_LINE */
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

EXPORT_API widget_class_h widget_app_class_add(widget_class_h widget_class,
		const char *class_id,
		widget_instance_lifecycle_callback_s callback, void *user_data)
{
	return _widget_class_create(widget_class, class_id, callback,
			user_data);
}

EXPORT_API widget_class_h widget_app_class_create(
		widget_instance_lifecycle_callback_s callback, void *user_data)
{
	return _widget_class_create(class_provider, appid, callback, user_data);
}

EXPORT_API int widget_app_context_set_tag(widget_context_h context, void *tag)
{
	if (!_is_widget_feature_enabled()) {
		_E("not supported"); /* LCOV_EXCL_LINE */
		return WIDGET_ERROR_NOT_SUPPORTED; /* LCOV_EXCL_LINE */
	}

	if (context == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__, NULL);

	context->tag = tag;

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_context_get_tag(widget_context_h context, void **tag)
{
	if (!_is_widget_feature_enabled()) {
		_E("not supported"); /* LCOV_EXCL_LINE */
		return WIDGET_ERROR_NOT_SUPPORTED; /* LCOV_EXCL_LINE */
	}

	if (context == NULL || tag == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__, NULL);

	*tag = context->tag;

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_context_set_content_info(widget_context_h context,
		bundle *content_info)
{
	const char *class_id = NULL;
	int ret = 0;
	bundle_raw *raw = NULL;
	int len;

	if (!_is_widget_feature_enabled()) {
		_E("not supported"); /* LCOV_EXCL_LINE */
		return WIDGET_ERROR_NOT_SUPPORTED; /* LCOV_EXCL_LINE */
	}

	if (context == NULL || content_info == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__, NULL);

	if (context->provider == NULL)
		return widget_app_error(WIDGET_ERROR_INVALID_PARAMETER,
				__FUNCTION__, NULL);

	class_id = context->provider->classid;
	if (class_id == NULL)
		return widget_app_error(WIDGET_ERROR_FAULT, __FUNCTION__, NULL);

	ret = __send_update_status(class_id, context->id,
			WIDGET_INSTANCE_EVENT_EXTRA_UPDATED, content_info);

	if (context->content)
		free(context->content);

	bundle_encode(content_info, &raw, &len);
	if (raw)
		context->content = strdup((const char *)raw);
	else
		context->content = NULL;

	if (ret < 0) {
		/* LCOV_EXCL_START */
		_E("failed to send content info: %s of %s (%d)", context->id,
				class_id, ret);
		return widget_app_error(WIDGET_ERROR_IO_ERROR, __FUNCTION__,
				NULL);
		/* LCOV_EXCL_STOP */
	}

	return WIDGET_ERROR_NONE;
}

EXPORT_API int widget_app_context_set_title(widget_context_h context,
		const char *title)
{
	if (!_is_widget_feature_enabled()) {
		_E("not supported"); /* LCOV_EXCL_LINE */
		return WIDGET_ERROR_NOT_SUPPORTED; /* LCOV_EXCL_LINE */
	}

	if (!context || !title)
		return WIDGET_ERROR_INVALID_PARAMETER;

	if (context->win)
		elm_win_title_set(context->win, title);

	return WIDGET_ERROR_NONE;
}

