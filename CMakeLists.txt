cmake_minimum_required(VERSION 3.1)
project(zzdicom)

set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL)
find_package(GLUT)
find_library(LIBUUID_LIBRARIES uuid)

enable_testing()

find_library(CHARLS_LIB NAMES charls CharLS)
find_path(CHARLS_INCLUDE NAMES charls.h PATHS /usr/include/charls /usr/include/CharLS)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(ZZ_FLAGS -g -O2 -Wall -Wformat=2 -Werror -Wno-unused-but-set-variable -Wno-unused-variable)
else()
	set(ZZ_FLAGS -g -O2 -Wall -Wformat=2 -Werror -Wno-unused-variable)
endif()

add_library(zzdicom zzio.c zzio.h zz.c zzwrite.c zzsql.c zzverify.c zz_priv.h zzwrite.h zzsql.h zztexture.c zztexture.h)
target_link_libraries(zzdicom ${OPENGL_LIBRARIES} ${LIBUUID_LIBRARIES} ${CHARLS_LIB})
set_target_properties(zzdicom PROPERTIES COMPILE_FLAGS "-I /usr/local/include")
target_include_directories(zzdicom PRIVATE ${CHARLS_INCLUDE})
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	target_compile_options(zzdicom PRIVATE -Wno-nonnull-compare ${ZZ_FLAGS})
endif()

add_executable(zzanon zzanon.c)
target_link_libraries(zzanon zzdicom)
target_compile_options(zzanon PRIVATE ${ZZ_FLAGS})

add_executable(zzpixel zzpixel.c part6.c part6.h)
target_link_libraries(zzpixel zzdicom)
target_compile_options(zzpixel PRIVATE ${ZZ_FLAGS})

add_executable(zzdump zzdump.c part6.c part6.h)
target_link_libraries(zzdump zzdicom)
target_compile_options(zzdump PRIVATE ${ZZ_FLAGS})

add_executable(zzcopy zzcopy.c part6.c part6.h)
target_link_libraries(zzcopy zzdicom ${CHARLS_LIB})
target_include_directories(zzcopy PRIVATE ${CHARLS_INCLUDE})
target_compile_options(zzcopy PRIVATE ${ZZ_FLAGS})

add_executable(zzread zzread.c)
target_link_libraries(zzread zzdicom sqlite3)
target_compile_options(zzread PRIVATE ${ZZ_FLAGS})

add_executable(zzstudies zzstudies.c)
target_link_libraries(zzstudies zzdicom sqlite3)
target_compile_options(zzstudies PRIVATE ${ZZ_FLAGS})

add_executable(zzprune zzprune.c)
target_link_libraries(zzprune zzdicom sqlite3)
target_compile_options(zzprune PRIVATE ${ZZ_FLAGS})

add_executable(zzfix zzgroupfix.c)
target_link_libraries(zzfix zzdicom)
target_compile_options(zzfix PRIVATE ${ZZ_FLAGS})

add_executable(zzmkrandom zzmkrandom.c)
target_link_libraries(zzmkrandom zzdicom)
target_compile_options(zzmkrandom PRIVATE ${ZZ_FLAGS})

add_executable(zzechoscp zzechoscp.c zznet.c zznet.h zznetwork.c zznetwork.h)
target_link_libraries(zzechoscp zzdicom)
target_compile_options(zzechoscp PRIVATE ${ZZ_FLAGS} -Wno-unused-function)

add_subdirectory(nifti)
add_subdirectory(tests)
add_test(zz1 ${CMAKE_CURRENT_BINARY_DIR}/tests/zz1 Hello)

install(TARGETS zzanon zzpixel zzdump zzcopy zzprune zzfix zzmkrandom RUNTIME DESTINATION bin)
