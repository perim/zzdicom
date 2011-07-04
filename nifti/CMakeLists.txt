add_executable(zznifti2dcm zznifti2dcm.c nifti1.h znzlib.h nifti1_io.h znzlib.c nifti1_io.c)
target_link_libraries(zznifti2dcm zzdicom z)
set_target_properties(zznifti2dcm PROPERTIES COMPILE_FLAGS "-DHAVE_ZLIB -DHAVE_FDOPEN -I. -I..")
