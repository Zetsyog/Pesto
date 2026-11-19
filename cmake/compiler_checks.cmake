# Keep instrumentation on PESTO's own targets; external polyhedral libraries
# retain their configured build flags and C++ ABI.
option(PESTO_ENABLE_SANITIZERS "Instrument PESTO with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

foreach(pesto_checked_target IN ITEMS pesto pesto-cli pesto_gtest)
    target_compile_definitions(${pesto_checked_target} PRIVATE $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)
    if(PESTO_ENABLE_SANITIZERS)
        if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            message(FATAL_ERROR "PESTO_ENABLE_SANITIZERS requires GCC or Clang")
        endif()
        target_compile_definitions(${pesto_checked_target} PRIVATE _GLIBCXX_ASSERTIONS)
        target_compile_options(${pesto_checked_target} PRIVATE
            -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -UNDEBUG)
        target_link_options(${pesto_checked_target} PRIVATE -fsanitize=address,undefined)
    endif()
endforeach()
