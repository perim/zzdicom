add_executable(zznifti2dcm zznifti2dcm.c nifti1.h znzlib.h nifti1_io.h znzlib.c nifti1_io.c)
target_link_libraries(zznifti2dcm zzdicom z m)
set_target_properties(zznifti2dcm PROPERTIES COMPILE_FLAGS "-DHAVE_ZLIB -DHAVE_FDOPEN -I${PROJECT_SOURCE_DIR} -I${PROJECT_SOURCE_DIR}/nifti")

add_executable(zzdcm2nifti zzdcm2nifti.c nifti1.h znzlib.h nifti1_io.h znzlib.c nifti1_io.c)
target_link_libraries(zzdcm2nifti zzdicom z m)
set_target_properties(zzdcm2nifti PROPERTIES COMPILE_FLAGS "-DHAVE_ZLIB -DHAVE_FDOPEN -I${PROJECT_SOURCE_DIR} -I${PROJECT_SOURCE_DIR}/nifti")

add_executable(zzniftistitch zzniftistitch.c nifti1.h znzlib.h nifti1_io.h znzlib.c nifti1_io.c)
target_link_libraries(zzniftistitch zzdicom z m)
set_target_properties(zzniftistitch PROPERTIES COMPILE_FLAGS "-DHAVE_ZLIB -DHAVE_FDOPEN -I${PROJECT_SOURCE_DIR} -I${PROJECT_SOURCE_DIR}/nifti")

add_test(zznifti1 ${CMAKE_CURRENT_BINARY_DIR}/zznifti2dcm "${CMAKE_SOURCE_DIR}/samples/minimal.nii")
add_test(zznifti2 ${CMAKE_CURRENT_BINARY_DIR}/zznifti2dcm "${CMAKE_SOURCE_DIR}/samples/minimal.hdr" "${CMAKE_SOURCE_DIR}/samples/minimal.img")
