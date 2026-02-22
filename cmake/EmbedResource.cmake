# EmbedResource.cmake
# Generates a C header containing a static const unsigned char[] array
# from a binary file, matching the format used in Sans.ttf.h.
#
# Usage:
#   embed_resource(<input_file> <output_header> <array_name>)

function(embed_resource INPUT_FILE OUTPUT_HEADER ARRAY_NAME)
    file(READ "${INPUT_FILE}" FILE_HEX HEX)
    string(LENGTH "${FILE_HEX}" HEX_LEN)
    math(EXPR BYTE_COUNT "${HEX_LEN} / 2")

    # Build include guard from array name
    string(TOUPPER "${ARRAY_NAME}" GUARD_NAME)
    string(APPEND GUARD_NAME "_H")

    set(OUTPUT "#ifndef ${GUARD_NAME}\n#define ${GUARD_NAME}\n\n")
    string(APPEND OUTPUT "static const unsigned char ${ARRAY_NAME}[] = {\n")

    set(LINE "")
    set(BYTES_ON_LINE 0)
    set(POS 0)

    while(POS LESS HEX_LEN)
        string(SUBSTRING "${FILE_HEX}" ${POS} 2 BYTE)
        math(EXPR POS "${POS} + 2")
        math(EXPR BYTES_REMAINING "(${HEX_LEN} - ${POS}) / 2")

        if(BYTES_ON_LINE EQUAL 0)
            string(APPEND LINE "\t")
        endif()

        string(APPEND LINE "0x${BYTE}")

        if(BYTES_REMAINING GREATER 0)
            string(APPEND LINE ", ")
        endif()

        math(EXPR BYTES_ON_LINE "${BYTES_ON_LINE} + 1")

        if(BYTES_ON_LINE EQUAL 12)
            string(APPEND OUTPUT "${LINE}\n")
            set(LINE "")
            set(BYTES_ON_LINE 0)
        endif()
    endwhile()

    # Flush remaining bytes on the last line
    if(BYTES_ON_LINE GREATER 0)
        string(APPEND OUTPUT "${LINE}\n")
    endif()

    string(APPEND OUTPUT "};\n\n")
    string(APPEND OUTPUT "static const unsigned int ${ARRAY_NAME}_len = ${BYTE_COUNT};\n\n")
    string(APPEND OUTPUT "#endif // ${GUARD_NAME}\n")

    file(WRITE "${OUTPUT_HEADER}" "${OUTPUT}")
    message(STATUS "Embedded ${INPUT_FILE} -> ${OUTPUT_HEADER} (${BYTE_COUNT} bytes)")
endfunction()
