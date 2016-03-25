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


#ifndef __APPFW_WIDGET_LOG_H__
#define __APPFW_WIDGET_LOG_H__

#include <dlog.h>

#define _E(fmt, arg...) LOGE(fmt, ##arg)
#define _I(fmt, arg...) LOGI(fmt, ##arg)
#define _D(fmt, arg...) LOGD(fmt, ##arg)
#define _W(fmt, arg...) LOGW(fmt, ##arg)

#ifndef EXPORT_API
#define EXPORT_API __attribute__ ((visibility("default")))
#endif

#ifndef _E
#define _E(fmt, arg...) LOGE(fmt, ##arg)
#endif

#ifndef _I
#define _I(...) LOGI(__VA_ARGS__)
#endif

#ifndef _D
#define _D(...) LOGD(__VA_ARGS__)
#endif

#ifndef _W
#define _W(...) LOGW(__VA_ARGS__)
#endif

#define _warn_if(expr, fmt, arg...) do { \
	if (expr) { \
		_ERR(fmt, ##arg); \
	} \
} while (0)

#define _ret_if(expr) do { \
	if (expr) { \
		return; \
	} \
} while (0)

#define _retv_if(expr, val) do { \
	if (expr) { \
		return (val); \
	} \
} while (0)

#define _retm_if(expr, fmt, arg...) do { \
	if (expr) { \
		_ERR(fmt, ##arg); \
		return; \
	} \
} while (0)

#define _retvm_if(expr, val, fmt, arg...) do { \
	if (expr) { \
		_ERR(fmt, ##arg); \
		return (val); \
	} \
} while (0)

#endif /* __APPFW_WIDGET_LOG_H_ */
