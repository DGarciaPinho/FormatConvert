#pragma once
#include <filesystem>
#include "forgeconvert/core/limits.hpp"
#include "forgeconvert/core/result.hpp"
#include "forgeconvert/document/model.hpp"
namespace forgeconvert {
struct ConversionOptions { bool strict=true, overwrite=false; ResourceLimits limits; };
enum class OutputFormat { docx, txt };
struct ConversionReport {
    std::size_t pages_processed=0;
    std::vector<std::string> warnings, discarded_resources, font_substitutions;
    std::string quality="editable text; heuristic single-column layout; visual pagination not guaranteed";
};
struct Inspection { Document document; ConversionReport report; };
Result<Inspection> inspect(const std::filesystem::path& input, const ConversionOptions& options={});
Result<ConversionReport> convert(const std::filesystem::path& input, const std::filesystem::path& output,
                                OutputFormat format, const ConversionOptions& options={});
std::string capabilities();
}
