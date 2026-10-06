#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "forgeconvert/core/result.hpp"
namespace forgeconvert::codecs {
std::uint32_t crc32(std::string_view bytes);
std::uint32_t adler32(std::string_view bytes);
struct ZipEntry { std::string name, data; };
Result<std::string> zip_store(const std::vector<ZipEntry>& entries, std::size_t max_bytes=128*1024*1024);
}
