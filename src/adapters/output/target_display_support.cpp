#include "adapters/output/target_display_support.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace xray::adapters::output {

std::vector<std::string> disambiguate_target_display_names(
    const std::vector<TargetDisplayEntry>& entries) {
    // Count distinct identity_keys per display_text. A display_text shared by
    // two or more keys is the collision condition that triggers the suffix.
    std::map<std::string, std::set<std::string>> distinct_keys;
    for (const auto& entry : entries) {
        distinct_keys[entry.display_text].insert(entry.identity_key);
    }
    std::vector<std::string> out;
    out.reserve(entries.size());
    for (const auto& entry : entries) {
        const auto& keys = distinct_keys.at(entry.display_text);
        if (keys.size() > 1) {
            out.push_back(entry.display_text + " [key: " + entry.identity_key + "]");
        } else {
            out.push_back(entry.display_text);
        }
    }
    // NRVO-defeating return so the closing-brace destructor counts as a
    // covered line under gcov, mirroring the other output adapters.
    return std::vector<std::string>(std::move(out));
}

}  // namespace xray::adapters::output
