CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(appcore-widget C)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "\${prefix}")
SET(INCLUDEDIR "\${prefix}/include")
SET(VERSION_MAJOR 1)
SET(VERSION "${VERSION_MAJOR}.1")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

ADD_DEFINITIONS("-DPREFIX=\"${PREFIX}\"")

SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -fvisibility=hidden")
SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -g -Wall")
SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -Werror")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")

SET(CMAKE_SKIP_BUILD_RPATH TRUE)

#################################################################
# Build appcore-widget Library
# ------------------------------
SET(APPCORE_WIDGET "capi-appfw-widget-application")
SET(SRCS_widget
		src/widget-i18n.c
		src/widget_app.c
		src/widget_error.c
		)
SET(HEADERS_widget widget_app.h widget_app_efl.h widget_app_internal.h)

INCLUDE(FindPkgConfig)
pkg_check_modules(pkg_widget REQUIRED
		aul
		dlog
		capi-appfw-app-control
		capi-appfw-app-common
		vconf
		elementary
		vconf-internal-keys
		widget_service
		capi-system-info
		)
FOREACH(flag ${pkg_widget_CFLAGS})
	SET(EXTRA_CFLAGS_widget "${EXTRA_CFLAGS_widget} ${flag}")
ENDFOREACH(flag)

ADD_LIBRARY(${APPCORE_WIDGET} SHARED ${SRCS_widget})
SET_TARGET_PROPERTIES(${APPCORE_WIDGET} PROPERTIES SOVERSION ${VERSION_MAJOR})
SET_TARGET_PROPERTIES(${APPCORE_WIDGET} PROPERTIES VERSION ${VERSION})
SET_TARGET_PROPERTIES(${APPCORE_WIDGET} PROPERTIES COMPILE_FLAGS ${EXTRA_CFLAGS_widget})
TARGET_LINK_LIBRARIES(${APPCORE_WIDGET} ${pkg_widget_LDFLAGS} "-ldl")

INSTALL(TARGETS ${APPCORE_WIDGET} DESTINATION ${LIB_INSTALL_DIR} COMPONENT RuntimeLibraries)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${APPCORE_WIDGET}.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)
FOREACH(hfile ${HEADERS_widget})
	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/${hfile} DESTINATION include/appfw)
ENDFOREACH(hfile)

SET(WIDGET_LOADER "widget-loader")
PKG_CHECK_MODULES(${WIDGET_LOADER} REQUIRED
	dlog
	aul
	ecore
	launchpad
	elementary
	bundle
	)

FOREACH(flag ${${WIDGET_LOADER}_CFLAGS})
	SET(EXTRA_CFLAGS_loader "${EXTRA_CFLAGS_loader} ${flag}")
ENDFOREACH(flag)

SET(${WIDGET_LOADER}_SOURCE_FILES launchpad_src/widget_loader.c)
ADD_EXECUTABLE(${WIDGET_LOADER} ${${WIDGET_LOADER}_SOURCE_FILES})
SET_TARGET_PROPERTIES(${WIDGET_LOADER} PROPERTIES COMPILE_FLAGS ${EXTRA_CFLAGS_loader})
TARGET_LINK_LIBRARIES(${WIDGET_LOADER} ${${WIDGET_LOADER}_LDFLAGS} "-ldl")

INSTALL(TARGETS ${WIDGET_LOADER} DESTINATION bin)
