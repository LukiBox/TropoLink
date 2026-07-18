# Warnings-as-errors discipline for first-party targets.
# Third-party code (vcpkg, vendored ITM) is exempt via /external and per-target opt-out.

function(tropolink_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /Zc:preprocessor
            /external:anglebrackets
            /external:W0
            /wd4127          # conditional expression is constant — fires on legitimate if constexpr fallbacks
        )
        if(TROPOLINK_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
        if(TROPOLINK_SANITIZE)
            target_compile_options(${target} PRIVATE /fsanitize=address)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(TROPOLINK_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(TROPOLINK_SANITIZE)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()
