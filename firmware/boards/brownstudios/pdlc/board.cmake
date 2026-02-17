# Requires OpenOCD 0.12.0-7 or newer
# Get it here: https://xpack-dev-tools.github.io/openocd-xpack/
find_program(OPENOCD openocd REQUIRED)
unset(OPENOCD_DEFAULT_PATH)
find_file(OPENOCD_DEFAULT_PATH scripts PATHS ${OPENOCD}/../../openocd NO_DEFAULT_PATH REQUIRED)

board_runner_args(stm32cubeprogrammer "--port=swd" "--reset-mode=hw")

include(${ZEPHYR_BASE}/boards/common/stm32cubeprogrammer.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd-stm32.board.cmake)
