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
#include <tzplatform_config.h>

#include "widget-log.h"
#include "widget-private.h"

#define PKGID_MAX 256
#define PATH_APP_ROOT tzplatform_getenv(TZ_USER_APP)
#define PATH_SYS_RO_APP_ROOT tzplatform_getenv(TZ_SYS_RO_APP)
#define PATH_SYS_RW_APP_ROOT tzplatform_getenv(TZ_SYS_RW_APP)
#define PATH_RES "/res"
#define PATH_LOCALE "/locale"

void _update_lang(void)
{
	char *r;
	char *lang = vconf_get_str(VCONFKEY_LANGSET);
	if (lang) {
		setenv("LANG", lang, 1);
		setenv("LC_MESSAGES", lang, 1);

		r = setlocale(LC_ALL, "");
		if (r == NULL) {
			r = setlocale(LC_ALL, lang);
			if (r)
				_D("*****appcore setlocale=%s\n", r);
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
		if (r != NULL)
			_D("*****appcore setlocale=%s\n", r);

		free(region);
	} else {
		_E("failed to get current region format for set region env");
	}
}

static int __get_locale_resource_dir(char *locale_dir, int size)
{
	int r;
	char pkgid[PKGID_MAX];
	const char *root_path;

	root_path = aul_get_preinit_root_path();
	if (root_path) {
		snprintf(locale_dir, size, "%s" PATH_RES PATH_LOCALE,
				root_path);
		if (access(locale_dir, R_OK) == 0)
			return 0;
	}

	r = aul_app_get_pkgid_bypid(getpid(), pkgid, sizeof(pkgid));
	if (r != AUL_R_OK)
		return -1;

	snprintf(locale_dir, size, "%s/%s" PATH_RES PATH_LOCALE,
			PATH_APP_ROOT, pkgid);
	if (access(locale_dir, R_OK) == 0)
		return 0;

	snprintf(locale_dir, size, "%s/%s" PATH_RES PATH_LOCALE,
			PATH_SYS_RO_APP_ROOT, pkgid);
	if (access(locale_dir, R_OK) == 0)
		return 0;

	snprintf(locale_dir, size, "%s/%s" PATH_RES PATH_LOCALE,
			PATH_SYS_RW_APP_ROOT, pkgid);

	return 0;
}

static int __set_i18n(const char *domain)
{
	char *r;
	char locale_dir[PATH_MAX];
	char *lang;

	if (domain == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (__get_locale_resource_dir(locale_dir, sizeof(locale_dir)) < 0) {
		_E("Failed to get locale resource directory");
		return -1;
	}
	_D("locale dir: %s", locale_dir);

	r = setlocale(LC_ALL, "");
	/* if locale is not set properly, try again to set as language base */
	if (r == NULL) {
		lang = vconf_get_str(VCONFKEY_LANGSET);
		r = setlocale(LC_ALL, lang);
		if (r)
			_D("*****appcore setlocale=%s\n", r);

		if (lang)
			free(lang);
	}
	if (r == NULL)
		_E("appcore: setlocale() error");

	r = bindtextdomain(domain, locale_dir);
	if (r == NULL)
		_E("appcore: bindtextdomain() error");

	r = textdomain(domain);
	if (r == NULL)
		_E("appcore: textdomain() error");

	return 0;
}

int _set_i18n(const char *domainname)
{
	_update_lang();
	_update_region();

	return __set_i18n(domainname);
}

