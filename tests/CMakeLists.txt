add_executable(zziotest zziotest.c)
target_link_libraries(zziotest zzdicom)
set_target_properties(zziotest PROPERTIES COMPILE_FLAGS "-I..")
add_test(zziotest ${CMAKE_CURRENT_BINARY_DIR}/zziotest "IO layer")

add_executable(zz1 zz1.c)
target_link_libraries(zz1 zzdicom)
set_target_properties(zz1 PROPERTIES COMPILE_FLAGS "-I..")
add_test(zzw ${CMAKE_CURRENT_BINARY_DIR}/zz1 "Sanity")

if(APPLE)
  # fixme - trouble with charls
else(APPLE)
  add_executable(zzt zzt.c)
  target_link_libraries(zzt zzdicom ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES} CharLS)
  set_target_properties(zzt PROPERTIES COMPILE_FLAGS "-I..")
  add_test(zzt ${CMAKE_CURRENT_BINARY_DIR}/zzt "Textures")
endif(APPLE)

add_executable(zzw zzw.c)
target_link_libraries(zzw zzdicom)
set_target_properties(zzw PROPERTIES COMPILE_FLAGS "-I..")
add_test(zzw ${CMAKE_CURRENT_BINARY_DIR}/zzw "Basics")
