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

#ifndef __TIZEN_APPFW_WIDGET_APP_EFL_H__
#define __TIZEN_APPFW_WIDGET_APP_EFL_H__

#include <widget_app.h>
#include <Evas.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup CAPI_WIDGET_APP_MODULE
 * @{
 */

/**
 * @brief Gets a evas object for the widget
 * @since_tizen 2.3.1
 *
 * @param[in] context The context for widget instance
 * @param[out] win Evas object for window
 *
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfull
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #WIDGET_ERROR_FAULT Failed to make evas object
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 */
int widget_app_get_elm_win(widget_context_h context, Evas_Object **win);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
