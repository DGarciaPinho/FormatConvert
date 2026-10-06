#pragma once
#include <string>
#include <string_view>
#include "forgeconvert/core/result.hpp"
namespace forgeconvert::xml {
Result<std::string> escape(std::string_view utf8);
std::string utf8(unsigned codepoint);
}
