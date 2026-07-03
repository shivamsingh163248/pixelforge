# cli_smoke.cmake -- end-to-end test for the pixelforge CLI.
#
# Generates a tiny BMP via Python, runs the CLI through a 2-filter
# pipeline (grayscale -> invert), then verifies the output PNG exists
# and has plausible content (file size > 50 bytes).
#
# Required vars (passed via -D):
#   PIXELFORGE_CLI -- absolute path to the CLI binary
#   PLUGIN_DIR     -- directory containing pf_*.dll / pf_*.so
#   TMP_DIR        -- writable directory for intermediate files

if(NOT EXISTS "${PIXELFORGE_CLI}")
    message(FATAL_ERROR "CLI not found: ${PIXELFORGE_CLI}")
endif()

file(MAKE_DIRECTORY "${TMP_DIR}")
set(IN_BMP  "${TMP_DIR}/cli_in.bmp")
set(OUT_PNG "${TMP_DIR}/cli_out.png")
file(REMOVE "${OUT_PNG}")

find_program(PYTHON_EXE NAMES python3 python)
if(NOT PYTHON_EXE)
    message(FATAL_ERROR "Python is required to generate the test BMP")
endif()

# Make a 4x4 24-bit BMP using Python's struct module.
execute_process(
    COMMAND "${PYTHON_EXE}" -c "
import struct, sys, pathlib
W, H = 4, 4
pixels = bytearray()
for y in range(H):
    for x in range(W):
        # BMP is BGR.
        pixels += bytes((x*60 & 255, y*60 & 255, 100))
file_size  = 54 + len(pixels)
header = b'BM' + struct.pack('<IHHI', file_size, 0, 0, 54)
dib    = struct.pack('<IiiHHIIiiII', 40, W, H, 1, 24, 0, len(pixels),
                     2835, 2835, 0, 0)
pathlib.Path(sys.argv[1]).write_bytes(header + dib + pixels)
" "${IN_BMP}"
    RESULT_VARIABLE _rc
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "Failed to generate input BMP")
endif()

execute_process(
    COMMAND "${PIXELFORGE_CLI}"
            "${IN_BMP}"
            --filter grayscale
            --filter invert
            --output "${OUT_PNG}"
            --plugin-dir "${PLUGIN_DIR}"
    RESULT_VARIABLE CLI_RC
    OUTPUT_VARIABLE CLI_OUT
    ERROR_VARIABLE  CLI_ERR
)
message(STATUS "CLI stdout: ${CLI_OUT}")
if(NOT CLI_RC EQUAL 0)
    message(FATAL_ERROR "pixelforge CLI failed (rc=${CLI_RC}): ${CLI_ERR}")
endif()
if(NOT EXISTS "${OUT_PNG}")
    message(FATAL_ERROR "CLI did not produce ${OUT_PNG}")
endif()
file(SIZE "${OUT_PNG}" OUT_SIZE)
if(OUT_SIZE LESS 50)
    message(FATAL_ERROR "Output PNG suspiciously small: ${OUT_SIZE} bytes")
endif()
message(STATUS "CLI smoke test passed: ${OUT_PNG} (${OUT_SIZE} bytes)")
