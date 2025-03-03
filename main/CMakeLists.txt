idf_component_register(SRCS "main.cpp" "mac_ring.h" "mac_bloom.h"
                    INCLUDE_DIRS ".")
if(DEFINED ENV{POST_NOTIFY_URL})
    add_compile_definitions(POST_NOTIFY_URL="$ENV{POST_NOTIFY_URL}")
else()
    message(WARNING "POST_NOTIFY_URL not supplied. Using default one")
endif()

if(DEFINED ENV{TARGET_SSID})
    add_compile_definitions(_SSID="$ENV{TARGET_SSID}")
else()
    message(WARNING "POST_NOTIFY_URL not supplied. Using default one")
endif()

if(DEFINED ENV{FILTER_RANDOM_MAC})
    add_compile_definitions(FILTER_RANDOM_MAC)
endif()