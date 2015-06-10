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

#include <locale.h>
#include <libintl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <linux/limits.h>
#include <glib.h>
#include <fcntl.h>

#include <aul.h>
#include <vconf.h>

#include "widget-log.h"
#include "widget-private.h"

#define BASE_LANG "en_GB.UTF-8"
#define CSC_DEFAULT_INI "/opt/system/csc-default/data/csc-default-data.ini"

#define PKGNAME_MAX 256
#define PATH_APP_ROOT "/opt/usr/apps"
#define PATH_RO_APP_ROOT "/usr/apps"
#define PATH_RES "/res"
#define PATH_LOCALE "/locale"

void _update_lang(void)
{
	char *lang = vconf_get_str(VCONFKEY_LANGSET);
	if (lang) {
		setenv("LANG", lang, 1);
		setenv("LC_MESSAGES", lang, 1);

		char *r = setlocale(LC_ALL, "");
		if (r == NULL) {
			r = setlocale(LC_ALL, lang);
			if (r) {
				_D("*****appcore setlocale=%s\n", r);
			}
		}
		free(lang);
	} else {
		_E("failed to get current language for set lang env");
	}
}

void _update_region(void)
{
	char *region;
	char *r;

	region = vconf_get_str(VCONFKEY_REGIONFORMAT);
	if (region) {
		setenv("LC_CTYPE", region, 1);
		setenv("LC_NUMERIC", region, 1);
		setenv("LC_TIME", region, 1);
		setenv("LC_COLLATE", region, 1);
		setenv("LC_MONETARY", region, 1);
		setenv("LC_PAPER", region, 1);
		setenv("LC_NAME", region, 1);
		setenv("LC_ADDRESS", region, 1);
		setenv("LC_TELEPHONE", region, 1);
		setenv("LC_MEASUREMENT", region, 1);
		setenv("LC_IDENTIFICATION", region, 1);
		r = setlocale(LC_ALL, "");
		if (r != NULL) {
			_D("*****appcore setlocale=%s\n", r);
		}
		free(region);
	} else {
		_E("failed to get current region format for set region env");
	}
}

static int __read_proc(const char *path, char *buf, int size)
{
	int fd;
	int ret;

	if (buf == NULL || path == NULL)
		return -1;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;

	ret = read(fd, buf, size - 1);
	if (ret <= 0) {
		close(fd);
		return -1;
	} else
		buf[ret] = 0;

	close(fd);

	return ret;
}

static char* __proc_get_cmdline()
{
#define MAX_CMD_BUFSZ 1024

	char buf[MAX_CMD_BUFSZ];
	int ret;

	snprintf(buf, sizeof(buf), "/proc/%d/cmdline", getpid());
	ret = __read_proc(buf, buf, sizeof(buf));
	if (ret <= 0)
		return NULL;

	return strdup(buf);
}

static void __get_dir_name(char *dirname, int maxlen)
{
	const char *path = NULL;

	path = aul_get_app_root_path();
	snprintf(dirname, maxlen, "%s/res/locale", path);
	free(path);
}


static int __set_i18n(const char *domain)
{
	char *r;
	char dirname[PATH_MAX] = {0, };

	if (domain == NULL) {
		errno = EINVAL;
		return -1;
	}

	__get_dir_name(dirname, PATH_MAX - 1);
	_D("locale dir: %s", dirname);

	r = setlocale(LC_ALL, "");
	/* if locale is not set properly, try again to set as language base */
	if (r == NULL) {
		char *lang = vconf_get_str(VCONFKEY_LANGSET);
		r = setlocale(LC_ALL, lang);
		if (r) {
			_D("*****appcore setlocale=%s\n", r);
		}
		if (lang) {
			free(lang);
		}
	}
	if (r == NULL) {
		_E("appcore: setlocale() error");
	}

	r = bindtextdomain(domain, dirname);
	if (r == NULL) {
		_E("appcore: bindtextdomain() error");
	}

	r = textdomain(domain);
	if (r == NULL) {
		_E("appcore: textdomain() error");
	}

	return 0;
}

int _set_i18n(const char *domainname)
{
	_update_lang();
	_update_region();

	return __set_i18n(domainname);
}

