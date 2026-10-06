#pragma once
#include "forgeconvert/document/model.hpp"
#include "forgeconvert/core/result.hpp"
namespace forgeconvert::docx {
Result<std::string> write(const Document& document, std::size_t max_bytes=128*1024*1024);
}
