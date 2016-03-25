/*
 * Copyright (c) 2015 - 2016 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 *
 * @ingroup CAPI_WIDGET_FRAMEWORK
 * @defgroup CAPI_WIDGET_APP_MODULE Widget Application
 * @brief Widget application API
 *
 * @section CAPI_WIDGET_APP_MODULE_HEADER Required Header
 *   \#include <widget_app.h>
 *   \#include <widget_app_efl.h>
 * @section CAPI_WIDGET_APP_MODULE_OVERVIEW Overview
 * The @ref CAPI_WIDGET_APP_MODULE API provides functions for handling Tizen widget application state changes or system events. Tizen widget application can be shown in the home screen.
 * This APIs support making multiple widget instances per an application.
 *
 * This API provides interfaces for the following categories:
 * - Starting or exiting the main event loop
 * - Registering callbacks for application state change events
 * - Registering callbacks for basic system events
 * - Registering callbacks for instance state change events
 *
 * @subsection CAPI_WIDGET_APP_MODULE_STATE_CHANGE_EVENT Registering Callbacks for Application State Change Events
 * As for Tizen widget application states, it is very simple and somewhat similer to Tizen service application states.
 *
 * <p>
 * <table>
 * <tr>
 *   <th> Callback </th>
 *   <th> Description </th>
 * </tr>
 * <tr>
 *   <td>widget_app_create_cb()</td>
 *   <td>Hook to take necessary actions before the main event loop starts.
 *   Your UI generation code should be placed here so that you do not miss any events from your application UI.
 *   Please make sure that you should make a class handle and return it. It will be used when the event for creating widget instance is received.
 *   You can initialize shared resources for widget instances in this callback function as well.
 * </td>
 * </tr>
 * <tr>
 *  <td> widget_app_terminate_cb() </td>
 *  <td> Hook to take necessary actions when your application is terminating.
 *   Your application should release all resources, especially any
 *   allocations and shared resources must be freed here so other running applications can fully use these shared resources.
 *  </td>
 * </tr>
 * </table>
 * </p>
 *
 * Please refer to the following state diagram to see the possible transitions and callbacks that are called while transition.
 * @image html widget_app_lifecycle.png "Widget Application States"
 *
 * @subsection CAPI_WIDGET_APP_MODULE_SYSTEM_EVENT Registering Callbacks for System Events
 * Tizen widget applications can receive system events with widget_app_add_event_handler() api.
 * The type of system events that can be received are same as Tizen UI applications except for APP_EVENT_DEVICE_ORIENTATION_CHANGED.
 * See @ref CAPI_APPLICATION_MODULE.
 * The event for APP_EVENT_DEVICE_ORIENTATION_CHANGED is not supported in this module.
 *
 * @subsection CAPI_WIDGET_APP_INSTNACE_STATE_CHANGE_EVENT Registering callbacks for instance state change events
 * As for Tizen widget instance states, it is somewhat similer to Tizen application states.
 *
 * <p>
 * <table>
 * <tr>
 *   <th> Callback </th>
 *   <th> Description </th>
 * </tr>
 * <tr>
 *   <td> widget_instance_create_cb() </td>
 *   <td> Called after widget instance is created.
 *    In this callback, you can initialize resources for this instance.
 *    If parameter 'content' is not NULL, You should restore the pervious status.
 *   </td>
 * </tr>
 * <tr>
 *  <td> widget_instance_destroy_cb() </td>
 *  <td> Called before widget instance is destroyed.
 *   In this callback, you can finalize resources for this instance.
 *  </td>
 * </tr>
 * <tr>
 *  <td> widget_instance_pause_cb() </td>
 *  <td> Called when the widget is invisible.
 *   The paused instance may be destroyed by framework
 *  </td>
 * </tr>
 * <tr>
 *  <td> widget_instance_resume_cb() </td>
 *  <td> Called when the widget is visible.
 *   The callback function is called when the widget is visible.
 *  </td>
 * </tr>
 * <tr>
 *  <td> widget_instance_resize_cb() </td>
 *  <td> Called before the widget size is changed.
 *   The callback function is called before the widget size is changed.
 *  </td>
 * </tr>
 * <tr>
 *  <td> widget_instance_update_cb() </td>
 *  <td> Called when the event for updating widget is received.
 *   The callback function is called when the event for updating widget is received.
 *  </td>
 * </tr>
 * </table>
 * </p>
 *
 * Please refer to the following state diagram to see the possible transitions and callbacks that are called while transition.
 * @image html widget_obj_lifecycle.png "Widget Instance States"
 *
 * @section CAPI_WIDGET_APP_MODULE_RELATED_FEATURES Related Features
 * This API is related with the following feature:
 * - http://tizen.org/feature/shell.appwidget
 *
 *
 */
