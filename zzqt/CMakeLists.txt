set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

qt_add_executable(zzqt
	main.cpp
	mainwindow.cpp
	mainwindow.h
	mainwindow.ui
	${PROJECT_SOURCE_DIR}/part6.c
)

target_include_directories(zzqt PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/zzqt)
target_link_libraries(zzqt PRIVATE
	zzdicom
	Qt6::Core
	Qt6::Gui
	Qt6::Widgets
	Qt6::OpenGL
	Qt6::OpenGLWidgets
	Qt6::Xml
)

if (DEFINED ZZ_FLAGS)
	target_compile_options(zzqt PRIVATE ${ZZ_FLAGS})
endif()

add_custom_command(TARGET zzqt POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_CURRENT_SOURCE_DIR}/shader.frag
		$<TARGET_FILE_DIR:zzqt>/shader.frag
)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/raycast.frag")
	add_custom_command(TARGET zzqt POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/raycast.frag
			$<TARGET_FILE_DIR:zzqt>/raycast.frag
	)
endif()
