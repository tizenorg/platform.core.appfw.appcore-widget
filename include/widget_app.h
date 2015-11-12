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


#ifndef __TIZEN_APPFW_WIDGET_APP_H__
#define __TIZEN_APPFW_WIDGET_APP_H__

#include <tizen.h>
#include <app_control.h>
#include <app_common.h>
#include <bundle.h>
#include <widget_errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup CAPI_WIDGET_APP_MODULE
 * @{
 */

/**
 * @brief Destroy type of widget instance
 * @since_tizen 2.3.1
 */
typedef enum widget_app_destroy_type {
	WIDGET_APP_DESTROY_TYPE_DEFAULT = 0x00,   /**< Deleted */
	WIDGET_APP_DESTROY_TYPE_UPGRADE = 0x01,   /**< Deleted for upgrading */
	WIDGET_APP_DESTROY_TYPE_UNINSTALL = 0x02, /**< Deleted by uninstalling */
	WIDGET_APP_DESTROY_TYPE_TERMINATE = 0x03, /**< Deleted for reboot device */
	WIDGET_APP_DESTROY_TYPE_FAULT = 0x04,     /**< Deleted by system-fault */
	WIDGET_APP_DESTROY_TYPE_TEMPORARY = 0x05, /**< Temporarly deleted, will be created again */
	WIDGET_APP_DESTROY_TYPE_UNKNOWN = 0x06    /**< Undefined reason */
} widget_app_destroy_type_e; /**< Delete type */

/**
 * @brief The handle for widget class.
 * @since_tizen 2.3.1
 */
typedef struct _widget_class* widget_class_h;

/**
 * @brief The handle for widget context
 * @since_tizen 2.3.1
 */
typedef struct _widget_context* widget_context_h;

/**
 * @brief Called for each widget class provider.
 * @since_tizen 3.0
 * @param[in] class_id The class id of class provider
 * @param[in] widget_class The handle of widget class provider
 * @param[in] user_data The user data passed from widget_app_foreach_class_provider function
 *
 * @return @c true to continue with the next iteration of the loop,
 *         otherwise @c false to break out of the loop
 *
 */
typedef int (*widget_class_provider_cb)(const char *class_id, widget_class_h widget_class, void *user_data);

/**
 * @brief Called when the widget instance starts.
 * @since_tizen 2.3.1
 *
 * @details The callback function is called after widget instance is created.
 *          In this callback, you can initialize resources for this instance.
 *
 * @param[in] context The context of widget instance.
 * @param[in] content The data set for the previous status
 * @param[in] w The pixel value for widget width
 * @param[in] h The pixel value for widget height
 * @param[in] user_data The user data passed from widget_app_class_create function
 *
 * @return #WIDGET_ERROR_NONE on success,
 *         otherwise an error code (see WIDGET_ERROR_XXX) on failure
 */
typedef int (*widget_instance_create_cb)(widget_context_h context, bundle *content, int w, int h, void *user_data);

/**
 * @brief Called before the widget instance is destroyed.
 * @since_tizen 2.3.1
 *
 * @details The callback function is called before widget instance is destroyed.
 *          In this callback, you can finalize resources for this instance.
 *          If reason is not #WIDGET_APP_DESTROY_TYPE_DEFAULT, It should store the current status by using incoming bundle.
 *
 * @param[in] context The context of widget instance.
 * @param[in] reason The reason for destruction
 * @param[in,out] content The data set to save
 * @param[in] user_data The user data passed from widget_app_class_create function
 * @remark Note that the parameter 'content' is used to save the status of the widget instance.
 *         As a input parameter, content contains the saved status of the widget instance.
 *         You can fill the content parameter with the current status in this callback,
 *         then the framework will save the content by receiving it as a output parameter.
 *         Consequently, you should not use widget_app_context_set_content_info() api in this callback.
 *         The content will be overwritten after this callback returns with the 'content' parameter.
 *
 * @return #WIDGET_ERROR_NONE on success,
 *         otherwise an error code (see WIDGET_ERROR_XXX) on failure
 */
typedef int (*widget_instance_destroy_cb)(widget_context_h context, widget_app_destroy_type_e reason, bundle *content, void *user_data);

/**
 * @brief Called when the widget is invisible.
 * @since_tizen 2.3.1
 *
 * @details The callback function is called when the widget is invisible.
 *          The paused instance may be destroyed by framework
 *
 * @param[in] context The context of widget instance.
 * @param[in] user_data The user data passed from widget_app_class_create function
 * @return #WIDGET_ERROR_NONE on success,
 *         otherwise an error code (see WIDGET_ERROR_XXX) on failure
 */
typedef int (*widget_instance_pause_cb)(widget_context_h context, void *user_data);

/**
 * @brief Called when the widget is visible.
 * @since_tizen 2.3.1
 *
 * @details The callback function is called when the widget is visible.
 *
 * @param[in] context The context of widget instance.
 * @param[in] user_data The user data passed from widget_app_class_create function
 * @return #WIDGET_ERROR_NONE on success,
 *         otherwise an error code (see WIDGET_ERROR_XXX) on failure
 */
typedef int (*widget_instance_resume_cb)(widget_context_h context, void *user_data);

/**
 * @brief Called before the widget size is changed.
 * @since_tizen 2.3.1
 *
 * @details The callback function is called before the widget size is changed.
 *
 * @param[in] context The context of widget instance.
 * @param[in] w The pixel value for widget width
 * @param[in] h The pixel value for widget height
 * @param[in] user_data The user data passed from widget_app_class_create function
 * @return #WIDGET_ERROR_NONE on success,
 *         otherwise an error code (see WIDGET_ERROR_XXX) on failure
 */
typedef int (*widget_instance_resize_cb)(widget_context_h context, int w, int h, void *user_data);

/**
 * @brief Called when the event for updating widget is received.
 * @since_tizen 2.3.1
 *
 * @details The callback function is called when the event for updating widget is received.
 *
 * @param[in] context The context of widget instance.
 * @param[in] content The data set for updating this widget. It will be provided by requester.
 *                    Requester can use widget_service_trigger_update().
 * @param[in] force Although the widget is paused, if it is TRUE, the widget can be updated.
 * @param[in] user_data The user data passed from widget_app_class_create function
 * @return #WIDGET_ERROR_NONE on success,
 *         otherwise an error code (see WIDGET_ERROR_XXX) on failure
 * @see widget_service_trigger_update
 */
typedef int (*widget_instance_update_cb)(widget_context_h context, bundle *content, int force, void *user_data);

/**
 * @brief The structure for lifecycle of a widget instance
 * @since_tizen 2.3.1
 */
typedef struct
{
	widget_instance_create_cb create; /**< The callback function is called after widget instance is created. */
	widget_instance_destroy_cb destroy; /**< The callback function is called before widget instance is destroyed. */
	widget_instance_pause_cb pause; /**< The callback function is called when the widget is invisible. */
	widget_instance_resume_cb resume; /**< The callback function is called when the widget is visible. */
	widget_instance_resize_cb resize; /**< The callback function is called before the widget size is changed. */
	widget_instance_update_cb update; /**< The callback function is called when the event for updating widget is received. */
} widget_instance_lifecycle_callback_s;

/**
 * @brief Called when the application starts.
 * @since_tizen 2.3.1
 *
 * @details The callback function is called before the main loop of the application starts.
 *          In this callback, you can initialize resources which can be shared among widget instances.
 *          This function should return the handle for widget class so that it will be used for making instances of widget.
 *
 * @param[in] user_data The user data passed from the callback registration function
 * @return The object of widget class
 * @see widget_app_main()
 * @code
 *
 * static widget_class_h __widget_app_created(void *user_data)
 * {
 *     widget_instance_lifecycle_callback_s callback = { .... };
 *
 *     return widget_app_class_create(callback);
 * }
 *
 * @endcode
 */
typedef widget_class_h (*widget_app_create_cb)(void *user_data);

/**
 * @brief Called when the application's main loop exits.
 * @details This callback function is called once after the main loop of the application exits.
 *          You should release the application's resources in this function.
 * @since_tizen 2.3.1
 *
 * @param[in] user_data The user data passed from the callback registration function
 * @see widget_app_main()
 */
typedef void (*widget_app_terminate_cb)(void *user_data);

/**
 * @brief The structure for lifecycle of a widget application
 * @since_tizen 2.3.1
 */
typedef struct
{
	widget_app_create_cb create; /**< The callback function is called before the main loop of the application starts. */
	widget_app_terminate_cb terminate; /**< This callback function is called once after the main loop of the application exits. */
} widget_app_lifecycle_callback_s;

/**
 * @brief Called for each widget context
 * @since_tizen 2.3.1
 *
 * @details This function will be called in the function of widget_app_foreach_context repeatedly.
 *
 * @param[in] context The context for widget instance
 * @param[in] data The data for caller
 * @return true to continue with the next iteration of the loop,
 *         otherwise false to break out of the loop.
 * @see widget_app_foreach_context
 */
typedef bool (*widget_context_cb)(widget_context_h context, void *data);

/**
 * @brief Runs the main loop of the application until widget_app_exit() is called.
 * @since_tizen 2.3.1
 *
 * @param[in] argc The argument count
 * @param[in] argv The argument vector
 * @param[in] callback The set of callback functions to handle application events
 * @param[in] user_data The user data to be passed to the callback functions
 *
 * @return @c 0 on success,
 *         otherwise a negative error value.
 * @retval #WIDGET_ERROR_NONE Successful
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 * @see widget_app_exit
 */
int widget_app_main(int argc, char **argv, widget_app_lifecycle_callback_s *callback, void *user_data);

/**
 * @brief Exits the main loop of the application.
 * @details The main loop of the application stops and widget_app_terminate_cb() is invoked.
 * @since_tizen 2.3.1
 *
 * @return @c 0 on success,
 *         otherwise a negative error value.
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 * @see widget_app_main
 * @see widget_app_terminate_cb
 */
int widget_app_exit(void);

/**
 * @brief Finishes context for the widget instance
 * @since_tizen 2.3.1
 *
 * @param[in] context The context for widget instance
 *
 * @return @c 0 on success, otherwise a negative error value.
 * @retval #WIDGET_ERROR_NONE Successfull
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 */
int widget_app_terminate_context(widget_context_h context);

/**
 * @brief Retrieves all widget contexts in this application
 * @since_tizen 2.3.1
 *
 * @param[in] callback The iteration callback function
 * @param[in] data The data for the callback function
 *
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfull
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #WIDGET_ERROR_CANCELED The iteration is canceled
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 * @see widget_app_foreach_context
 */
int widget_app_foreach_context(widget_context_cb callback, void *data);

/**
 * @brief Adds the system event handler
 * @since_tizen 2.3.1
 *
 * @param[out] event_handler The event handler
 * @param[in] event_type The system event type. APP_EVENT_DEVICE_ORIENTATION_CHANGED is not supported
 * @param[in] callback The callback function
 * @param[in] user_data The user data to be passed to the callback function
 *
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfull
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #WIDGET_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 * @see app_event_type_e
 * @see app_event_cb
 * @see watch_app_remove_event_handler
 */
int widget_app_add_event_handler(app_event_handler_h *event_handler, app_event_type_e event_type,
                                 app_event_cb callback, void *user_data);

/**
 * @brief Removes registered event handler
 * @since_tizen 2.3.1
 *
 * @param[in] event_handler The event handler
 *
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NONE Successfull
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 * @see watch_app_add_event_handler
 */
int widget_app_remove_event_handler(app_event_handler_h event_handler);

/**
 * @brief Gets a widget instance id
 * @since_tizen 2.3.1
 *
 * @param[in] context The context for widget instance
 *
 * @return Widget ID on success, otherwise NULL
 *         You can get the returned value using get_last_result()
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 * @remark You must not free returned Widget ID
 * @see get_last_result
 */
const char* widget_app_get_id(widget_context_h context);

/**
 * @brief Makes a class for widget instances.
 * @since_tizen 2.3.1
 *
 * @param[in] callback The set of lifecycle callbacks
 * @param[in] user_data The user data to be passed to the callback functions
 * @return The handle of class on success, otherwise NULL
 *         You can get the returned value using get_last_result()
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @see get_last_result
 */
widget_class_h widget_app_class_create(widget_instance_lifecycle_callback_s callback, void *user_data);

/**
 * @brief Sets a tag in the context
 * @since_tizen 2.3.1
 *
 * @param[in] context The context for widget instance
 * @param[in] tag The value to save
 *
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 */
int widget_app_context_set_tag(widget_context_h context, void *tag);

/**
 * @brief Gets the tag in the context
 * @since_tizen 2.3.1
 *
 * @param[in] context The context for widget instance
 * @param[out] tag The value to get
 *
 * @return 0 on success, otherwise a negative error value
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid parameter
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 */
int widget_app_context_get_tag(widget_context_h context, void **tag);

/**
 * @brief Sets the content info to the widget
 * @since_tizen 2.3.1
 * @param[in] context The context for widget instance
 * @param[in] content_info The data set to save
 * @return #WIDGET_ERROR_NONE on success,
 *          otherwise an error code (see WIDGET_ERROR_XXX) on failure
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 * @retval #WIDGET_ERROR_NONE Successfully sent
 */
int widget_app_context_set_content_info(widget_context_h context, bundle *content_info);

/**
 * @brief Sends the title to the widget
 * @since_tizen 2.3.1
 * @param[in] context The context for widget instance
 * @param[in] title When an accessibility mode is turned on, this string will be read.
 * @return #WIDGET_ERROR_NONE on success,
 *          otherwise an error code (see WIDGET_ERROR_XXX) on failure
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @retval #WIDGET_ERROR_OUT_OF_MEMORY Out of memory
 * @retval #WIDGET_ERROR_FAULT Unrecoverable error
 * @retval #WIDGET_ERROR_NONE Successfully sent
 */
int widget_app_context_set_title(widget_context_h context, const char *title);

/**
 * @brief Add a class provider for widget instance create.
 * since_tizen 3.0
 * @param[in] widget_class The class provider handle
 * @param[in] class_id The class id of provider
 * @param[in] callback The set of lifecycle callbacks
 * @param[in] user_data The user data to be passed to the callback functions
 * @return The handle of class on success, otherwise NULL
 *         You can get the returned value using get_last_result()
 * @retval #WIDGET_ERROR_NOT_SUPPORTED Not supported
 * @see get_last_result
 */
widget_class_h widget_app_add_class_provider(widget_class_h widget_class, const char *class_id,
		widget_instance_lifecycle_callback_s callback, void *user_data);

/**
 * @brief Get a class id in the class provider.
 * @since_tizen 3.0
 * @param[in] widget_class The handle for class provider
 * @param[out] class_id The class id of class provider.
 * @return #WIDGET_ERROR_NONE on success,
 *		otherwise an error code (see WIDGET_ERROR_XXX) on failure
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_NONE Success
 */
int widget_app_get_class_id(widget_class_h widget_class, char **class_id);

/**
 * @brief Retrieves all the class providers in this application.
 * @since_tizen 3.0
 * @param[in] callback The iterate callback function
 * @param[in] user_data The use data to be passed to the callback functions
 * retrun #WIDGET_ERROR_NONE on success,
 *		otherwise an error code (see WIDGET_ERROR_XXX) on failure
 * @retval #WIDGET_ERROR_INVALID_PARAMETER Invalid argument
 * @retval #WIDGET_ERROR_NONE Success
 */
int widget_app_foreach_class_provider(widget_class_provider_cb callback, void *user_data);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_APPFW_WIDGET_APP_H__ */
