# Normalize platform detection into ENGINE_PLATFORM + family flags.
# Order matters: Emscripten also sets UNIX, iOS also sets APPLE.

if(EMSCRIPTEN)
    set(ENGINE_PLATFORM "Web")
elseif(ANDROID)
    set(ENGINE_PLATFORM "Android")
elseif(IOS)
    set(ENGINE_PLATFORM "iOS")
elseif(WIN32)
    set(ENGINE_PLATFORM "Windows")
elseif(APPLE)
    set(ENGINE_PLATFORM "macOS")
elseif(UNIX)
    set(ENGINE_PLATFORM "Linux")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

set(ENGINE_FAMILY_DESKTOP FALSE)
set(ENGINE_FAMILY_MOBILE  FALSE)
set(ENGINE_FAMILY_WEB     FALSE)

if(ENGINE_PLATFORM MATCHES "^(Windows|macOS|Linux)$")
    set(ENGINE_FAMILY_DESKTOP TRUE)
elseif(ENGINE_PLATFORM MATCHES "^(Android|iOS)$")
    set(ENGINE_FAMILY_MOBILE TRUE)
else()
    set(ENGINE_FAMILY_WEB TRUE)
endif()

message(STATUS "Engine platform: ${ENGINE_PLATFORM}")
