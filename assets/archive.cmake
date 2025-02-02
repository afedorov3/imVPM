function(archive_target TARGET_DIR TARGET_NAME TARGET_VERSION TARGET_FLAVOR)
  find_program(FILE_PROG file)
  if(FILE STREQUAL "FILE-NOTFOUND")
    message(WARNING "file program not found, skipping release archive creation")
    return()
  endif()
  execute_process(COMMAND file -b ${TARGET_NAME} OUTPUT_VARIABLE FILEINFO ERROR_VARIABLE FILEINFO
    WORKING_DIRECTORY ${TARGET_DIR}
  )
  string(REGEX MATCH "^PE.*" PEFILE "${FILEINFO}")
  string(REGEX MATCH "^ELF.*" ELFILE "${FILEINFO}")
  if(NOT PEFILE STREQUAL "")
    find_program(SEVENZIP 7z)
    if(SEVENZIP STREQUAL "SEVENZIP-NOTFOUND")
      message(WARNING "7z not found, skipping release archive creation")
      return()
    endif()
    string(REGEX REPLACE "^.*, for M*S* *([^, ]+).*$" "\\1" TARGET_SYSTEM "${FILEINFO}")
    string(REGEX REPLACE "^[^,]* ([^,]+),.*" "\\1" TARGET_ARCH "${FILEINFO}")
    string(TOLOWER "${TARGET_SYSTEM}" TARGET_SYSTEM)
    string(REPLACE "-" "_" TARGET_ARCH "${TARGET_ARCH}")
    string(REGEX REPLACE "\\.[^.]*$" "" TARGET "${TARGET_NAME}")
    set(ARCHIVE_NAME "${TARGET}-${TARGET_VERSION}-${TARGET_SYSTEM}-${TARGET_FLAVOR}-${TARGET_ARCH}.zip")
    message(STATUS "Creating ${ARCHIVE_NAME}")
    execute_process(
      COMMAND ${SEVENZIP} a ${ARCHIVE_NAME} ${TARGET_NAME} -mx9 -bso0
      WORKING_DIRECTORY ${TARGET_DIR}
    )
  elseif(NOT ELFILE STREQUAL "")
    string(REGEX REPLACE "^.*, for ([^, ]+).*$" "\\1" TARGET_SYSTEM "${FILEINFO}")
    string(REGEX REPLACE "^[^,]*, *([^,]+),.*" "\\1" TARGET_ARCH "${FILEINFO}")
    string(TOLOWER "${TARGET_SYSTEM}" TARGET_SYSTEM)
    string(REGEX REPLACE ".*/" "" TARGET_SYSTEM "${TARGET_SYSTEM}")
    string(REPLACE "-" "_" TARGET_ARCH "${TARGET_ARCH}")
    set(ARCHIVE_NAME "${TARGET_NAME}-${TARGET_VERSION}-${TARGET_SYSTEM}-${TARGET_FLAVOR}-${TARGET_ARCH}.tar.gz")
    message(STATUS "Creating ${ARCHIVE_NAME}")
    execute_process(
      COMMAND tar czf ${ARCHIVE_NAME} --owner 0 --group 0 ${TARGET_NAME}
      WORKING_DIRECTORY ${TARGET_DIR}
    )
  else()
    message(WARNING "can't determine file type, skipping release archive creation")
  endif()
endfunction()
