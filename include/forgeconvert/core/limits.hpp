#pragma once
#include <cstddef>
namespace forgeconvert {
struct ResourceLimits {
    std::size_t input_bytes = 64 * 1024 * 1024;
    std::size_t stream_bytes = 16 * 1024 * 1024;
    std::size_t total_stream_bytes = 64 * 1024 * 1024;
    std::size_t objects = 100000;
    std::size_t pages = 1000;
    std::size_t depth = 64;
    std::size_t steps = 2000000;
    std::size_t string_bytes = 1024 * 1024;
    std::size_t output_bytes = 128 * 1024 * 1024;
    std::size_t expansion_ratio = 100; // reserved until compressed streams are implemented
};
}
