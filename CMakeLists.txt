cmake_minimum_required(VERSION 2.8)
project(zzdicom)

find_package(OpenGL)
find_package(GLUT)
find_library(LIBUUID_LIBRARIES uuid)

enable_testing()

add_definitions(-Wall -Werror -Wformat=2 -Wno-unused)

add_library(zzdicom zzio.c zzio.h zz.c zzwrite.c zzsql.c zzverify.c zz_priv.h zzwrite.h zzsql.h zztexture.c zztexture.h)
target_link_libraries(zzdicom ${OPENGL_LIBRARIES} ${LIBUUID_LIBRARIES})
set_target_properties(zzdicom PROPERTIES COMPILE_FLAGS "-I /usr/local/include")
target_compile_options(zzdicom PRIVATE -Wno-nonnull-compare)

add_executable(zzanon zzanon.c)
target_link_libraries(zzanon zzdicom)

add_executable(zzdump zzdump.c part6.c part6.h)
target_link_libraries(zzdump zzdicom)

add_executable(zzcopy zzcopy.c part6.c part6.h)
target_link_libraries(zzcopy zzdicom CharLS)

add_executable(zzread zzread.c)
target_link_libraries(zzread zzdicom sqlite3)

add_executable(zzstudies zzstudies.c)
target_link_libraries(zzstudies zzdicom sqlite3)

add_executable(zzprune zzprune.c)
target_link_libraries(zzprune zzdicom sqlite3)

add_executable(zzfix zzgroupfix.c)
target_link_libraries(zzfix zzdicom)

add_executable(zzmkrandom zzmkrandom.c)
target_link_libraries(zzmkrandom zzdicom)

add_executable(zzechoscp zzechoscp.c zznet.c zznet.h zznetwork.c zznetwork.h)
target_link_libraries(zzechoscp zzdicom)

add_subdirectory(nifti)
add_subdirectory(tests)
add_test(zz1 ${CMAKE_CURRENT_BINARY_DIR}/tests/zz1 Hello)
