aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} vb_warp_src)
pybind11_add_module(vb_warp ${vb_warp_src})

target_link_libraries(vb_warp PRIVATE
  vb_libcompiler
  vb_libruntime
  vb_libutils
  vb_lib_disassembler
  vb_extensions
)
