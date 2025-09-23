set(CMAKE_CXX_STANDARD 20)

file(GLOB SRC_FILES
  ${CMAKE_CURRENT_LIST_DIR}/*.cpp
)

add_library(vb_extensions ${SRC_FILES})

EnableClangTidyMSVS(vb_extensions)
