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

#include <dlfcn.h>
#include <glib.h>
#include <dlog.h>
#include <Ecore.h>
#include <Elementary.h>
#include <bundle_internal.h>
#include <aul.h>
#include <launchpad.h>

#undef LOG_TAG
#define LOG_TAG "WIDGET_LOADER"

struct loader_s {
	int argc;
	char** argv;
	Ecore_Fd_Handler *fd_handler;
	loader_receiver_cb receiver;
};

static void __init_wayland(bundle *kb)
{
	const char *wayland_disp;
	const char *xdg_working_dir;

	if (kb == NULL) {
		LOGE("failed to get bundle");
		return;
	}

	wayland_disp = bundle_get_val(kb, AUL_K_WAYLAND_DISPLAY);
	xdg_working_dir = bundle_get_val(kb, AUL_K_WAYLAND_WORKING_DIR);

	if (wayland_disp == NULL || xdg_working_dir == NULL) {
		LOGE("failed to get argv: %s %s", wayland_disp, xdg_working_dir);
		return;
	}

	setenv("WAYLAND_DISPLAY", wayland_disp, 1);
	setenv("XDG_WORKING_DIR", xdg_working_dir, 1);
}

static void __loader_create_cb(bundle *extra, int type, void *user_data)
{
	int elm_init_cnt = 0;
	struct loader_s *data = (struct loader_s *)user_data;

	__init_wayland(extra);

	elm_init_cnt = elm_init(data->argc, data->argv);
	LOGD("[candidate] elm init, returned: %d", elm_init_cnt);
}

static int __loader_launch_cb(int argc, char **argv, const char *app_path,
			const char *appid, const char *pkgid, const char *pkg_type, void *user_data)
{
	return 0;
}

static int __loader_terminate_cb(int argc, char **argv, void *user_data)
{
#define MAX_LOCAL_BUFSZ 128
	void *handle = NULL;
	int res;
	int (*dl_main)(int, char **);

	LOGD("[candidate] Launch real application (%s)", argv[0]);
	handle = dlopen(argv[0], RTLD_LAZY | RTLD_GLOBAL);
	if (handle == NULL) {
		LOGE("dlopen failed(%s). Please complile with -fPIE and link with -pie flag",
			dlerror());
		goto do_exec;
	}

	dlerror();

	dl_main = dlsym(handle, "main");
	if (dl_main != NULL)
		res = dl_main(argc, argv);
	else {
		LOGE("dlsym not founded(%s). Please export 'main' function", dlerror());
		dlclose(handle);
		goto do_exec;
	}

	dlclose(handle);
	return res;

do_exec:
	if (access(argv[0], F_OK | R_OK)) {
		char err_str[MAX_LOCAL_BUFSZ] = { 0, };

		LOGE("access() failed for file: \"%s\", error: %d (%s)",
			argv[0], errno, strerror_r(errno, err_str, sizeof(err_str)));
	} else {
		LOGD("[candidate] Exec application (%s)", argv[0]);
		if (execv(argv[0], argv) < 0) {
			char err_str[MAX_LOCAL_BUFSZ] = { 0, };

			LOGE("execv() failed for file: \"%s\", error: %d (%s)",
				argv[0], errno, strerror_r(errno, err_str, sizeof(err_str)));
		}
	}

	return -1;

}

static Eina_Bool __process_fd_handler(void* user_data, Ecore_Fd_Handler *handler)
{
	struct loader_s *data = (struct loader_s *)user_data;
	int fd = ecore_main_fd_handler_fd_get(handler);

	if (fd == -1) {
		LOGD("[candidate] ECORE_FD_GET");
		exit(-1);
	}

	if (ecore_main_fd_handler_active_get(handler, ECORE_FD_ERROR)) {
		LOGD("[candidate] ECORE_FDLOGERROR");
		close(fd);
		exit(-1);
	}

	if (ecore_main_fd_handler_active_get(handler, ECORE_FD_READ)) {
		if (data && data->receiver)
			data->receiver(fd);
	}

	return ECORE_CALLBACK_CANCEL;
}

static void __adapter_loop_begin(void *user_data)
{
	ecore_main_loop_begin();
}

static void __adapter_loop_quit(void *user_data)
{
	ecore_main_loop_quit();
}

static void __adapter_add_fd(void *user_data, int fd,
                             loader_receiver_cb receiver)
{
	struct loader_s *data = (struct loader_s *)user_data;

	if (data == NULL)
		return;

	data->fd_handler = ecore_main_fd_handler_add(fd,
			(Ecore_Fd_Handler_Flags)(ECORE_FD_READ | ECORE_FD_ERROR),
			__process_fd_handler, data, NULL, NULL);
	if (data->fd_handler == NULL) {
		LOGD("fd_handler is NULL");
		close(fd);
		exit(-1);
	}

	data->receiver = receiver;
}

static void __adapter_remove_fd(void *user_data, int fd)
{
	struct loader_s *data = (struct loader_s *)user_data;
	if (data && data->fd_handler) {
		ecore_main_fd_handler_del(data->fd_handler);
		data->fd_handler = NULL;
		data->receiver = NULL;
	}
}

int main(int argc, char **argv)
{
	struct loader_s data;

	loader_lifecycle_callback_s callbacks = {
		.create = __loader_create_cb,
		.launch = __loader_launch_cb,
		.terminate = __loader_terminate_cb
	};

	loader_adapter_s adapter = {
		.loop_begin = __adapter_loop_begin,
		.loop_quit = __adapter_loop_quit,
		.add_fd = __adapter_add_fd,
		.remove_fd = __adapter_remove_fd
	};

	data.argc = argc;
	data.argv = argv;

	return launchpad_loader_main(argc, argv, &callbacks, &adapter, &data);
}
