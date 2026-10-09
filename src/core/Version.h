#pragma once
#include <cstdint>

// Demo builds override this to exercise the election; release builds never define it.
#ifndef B21UI_FRAMEWORK_VERSION
#define B21UI_FRAMEWORK_VERSION 11u
#endif

namespace b21ui {
    inline constexpr std::uint32_t kFrameworkVersion = B21UI_FRAMEWORK_VERSION;
}
