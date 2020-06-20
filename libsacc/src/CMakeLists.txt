add_library(sacc SHARED libsacc.cpp xcl2.cpp)

add_definitions("-D_GLIBCXX_USE_CXX11_ABI=0")

if (${CMAKE_BUILD_TYPE} MATCHES "Release")
    add_definitions("-DNDEBUG")
endif (${CMAKE_BUILD_TYPE} MATCHES "Release")

find_library(LIB_XILINX_OPENCL NAMES xilinxopencl PATHS ${XILINX_XRT}/lib/ NO_DEFAULT_PATH REQUIRED)
find_library(LIB_TF NAMES tensorflow_framework PATHS ${CMAKE_SOURCE_DIR}/venv/lib/python3.6/site-packages/tensorflow NO_DEFAULT_PATH REQUIRED)

target_link_libraries(sacc PUBLIC ${LIB_TF})
target_link_libraries(sacc PUBLIC pthread)
target_link_libraries(sacc PUBLIC ${LIB_XILINX_OPENCL})

target_include_directories(sacc PRIVATE ${CMAKE_SOURCE_DIR}/inc/)
target_include_directories(sacc PUBLIC ${VIVADO_DIRECTORY}/include/)
target_include_directories(sacc PUBLIC ${XILINX_XRT}/include/)

target_include_directories(sacc PRIVATE ${CMAKE_SOURCE_DIR}/venv/lib/python3.6/site-packages/tensorflow/include/)

# for install operation
option(INSTALL_MODE "this is only an experimental function" ON)
if (INSTALL_MODE)
    set(CMAKE_INSTALL_PREFIX "/usr/local/")
    install(TARGETS sacc DESTINATION lib/sacc)
    install(FILES ${CMAKE_SOURCE_DIR}/inc/sacc.hpp DESTINATION include/sacc/ )
    install(FILES ${CMAKE_SOURCE_DIR}/inc/sacc_params.h DESTINATION include/sacc/ )
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/config DESTINATION lib/sacc)
    install(FILES ${CMAKE_SOURCE_DIR}/config/network.insts DESTINATION lib/sacc/config/)

    file(GLOB data_files "${CMAKE_SOURCE_DIR}/data/*.bin")
    foreach(file ${data_files})
        install(FILES ${file} DESTINATION lib/sacc/data)
    endforeach()
endif (INSTALL_MODE)
