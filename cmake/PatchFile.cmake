# cmake/PatchFile.cmake
# FetchContent PATCH_COMMAND tarafından çağrılır.
# Belirtilen dosyada OLD_STR'yi NEW_STR ile değiştirir.
# Kaynak ağacına DOKUNULMAZ; sadece _deps/xxx-src/ içinde çalışır.
#
# Kullanım (Dependencies.cmake içinden):
#   cmake -DFILE=path -DOLD_STR="..." -DNEW_STR="..." -P PatchFile.cmake

if(NOT DEFINED FILE)
    message(FATAL_ERROR "PatchFile.cmake: FILE tanımlı değil")
endif()

if(NOT EXISTS "${FILE}")
    message(WARNING "PatchFile.cmake: Dosya bulunamadı: ${FILE} — patch atlanıyor")
    return()
endif()

file(READ "${FILE}" _content)

# Zaten yamalanmışsa tekrar uygulama
if("${_content}" MATCHES "${NEW_STR}")
    message(STATUS "PatchFile: '${FILE}' zaten yamalı, atlanıyor.")
    return()
endif()

string(REPLACE "${OLD_STR}" "${NEW_STR}" _patched "${_content}")

if("${_patched}" STREQUAL "${_content}")
    message(STATUS "PatchFile: Değiştirme deseni '${OLD_STR}' bulunamadı — patch atlanıyor.")
else()
    file(WRITE "${FILE}" "${_patched}")
    message(STATUS "PatchFile: '${FILE}' yamalandı: '${OLD_STR}' -> '${NEW_STR}'")
endif()
