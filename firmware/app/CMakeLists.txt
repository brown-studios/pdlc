cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(BOARD "brownstudios_pdlc")
set(BOARD_DEBUG_RUNNER openocd)
set(BOARD_FLASH_RUNNER openocd)

if(FEATURE_DEBUG)
    set(CONFIG_DEBUG y)
    set(CONFIG_DEBUG_THREAD_INFO y)
endif()

if(FEATURE_LOG)
    list(APPEND EXTRA_CONF_FILE log-deferred.conf)
    #list(APPEND EXTRA_CONF_FILE log-minimal.conf)
else()
    list(APPEND EXTRA_CONF_FILE log-none.conf)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pdlc)

add_custom_command(
    OUTPUT app/include/pdlc_tables.h
    COMMAND
    ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/gen_pdlc_tables.py
    -o app/include/pdlc_tables.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/gen_pdlc_tables.py
    VERBATIM
)
add_custom_target(gen_pdlc_tables DEPENDS app/include/pdlc_tables.h)
add_dependencies(app gen_pdlc_tables)

target_sources(app PRIVATE
    src/control.c
    src/main.c
    src/pdlc.c
    src/power_indicator.c
    src/status_indicator.c
)

target_include_directories(app PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR}/app/include
)
