set(CMAKE_CXX_STANDARD 20)

file(GLOB SRC_FILES
  ${CMAKE_CURRENT_LIST_DIR}/*.cpp
)

add_library(vb_extensions ${SRC_FILES})

target_link_libraries(vb_extensions PUBLIC vb_libWasmModule)

EnableClangTidyMSVS(vb_extensions)
