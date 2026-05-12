#include "services/compile_db_fingerprint.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "model/translation_unit.h"

namespace xray::hexagon::services {

namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
    0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
    0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
    0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

std::uint32_t rotate_right(std::uint32_t value, std::uint32_t amount) {
    return (value >> amount) | (value << (32U - amount));
}

using Sha256State = std::array<std::uint32_t, 8>;
using Sha256Words = std::array<std::uint32_t, 64>;

std::vector<std::uint8_t> sha256_padded_bytes(std::string_view input) {
    std::vector<std::uint8_t> bytes(input.begin(), input.end());
    const auto bit_len = static_cast<std::uint64_t>(bytes.size()) * 8U;
    bytes.push_back(0x80U);
    while ((bytes.size() % 64U) != 56U) bytes.push_back(0U);
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<std::uint8_t>((bit_len >> shift) & 0xffU));
    }
    return std::vector<std::uint8_t>(std::move(bytes));
}

Sha256Words sha256_words_for_chunk(const std::vector<std::uint8_t>& bytes,
                                   std::size_t chunk) {
    Sha256Words words{};
    for (std::size_t i = 0; i < 16U; ++i) {
        const auto offset = chunk + (i * 4U);
        words[i] = (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
                   (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
                   (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
                   static_cast<std::uint32_t>(bytes[offset + 3U]);
    }
    for (std::size_t i = 16U; i < words.size(); ++i) {
        const auto s0 = rotate_right(words[i - 15U], 7U) ^
                        rotate_right(words[i - 15U], 18U) ^ (words[i - 15U] >> 3U);
        const auto s1 = rotate_right(words[i - 2U], 17U) ^
                        rotate_right(words[i - 2U], 19U) ^ (words[i - 2U] >> 10U);
        words[i] = words[i - 16U] + s0 + words[i - 7U] + s1;
    }
    return words;
}

void sha256_compress_chunk(Sha256State& hash, const Sha256Words& words) {
    auto a = hash[0];
    auto b = hash[1];
    auto c = hash[2];
    auto d = hash[3];
    auto e = hash[4];
    auto f = hash[5];
    auto g = hash[6];
    auto h = hash[7];
    for (std::size_t i = 0; i < words.size(); ++i) {
        const auto sum1 = rotate_right(e, 6U) ^ rotate_right(e, 11U) ^ rotate_right(e, 25U);
        const auto choose = (e & f) ^ ((~e) & g);
        const auto temp1 = h + sum1 + choose + kRoundConstants[i] + words[i];
        const auto sum0 = rotate_right(a, 2U) ^ rotate_right(a, 13U) ^ rotate_right(a, 22U);
        const auto majority = (a & b) ^ (a & c) ^ (b & c);
        const auto temp2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
    hash[4] += e;
    hash[5] += f;
    hash[6] += g;
    hash[7] += h;
}

std::string sha256_state_to_hex(const Sha256State& hash) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto word : hash) out << std::setw(8) << word;
    return out.str();
}

std::string sha256_hex(std::string_view input) {
    const auto bytes = sha256_padded_bytes(input);
    Sha256State hash{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                     0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    for (std::size_t chunk = 0; chunk < bytes.size(); chunk += 64U) {
        sha256_compress_chunk(hash, sha256_words_for_chunk(bytes, chunk));
    }
    return sha256_state_to_hex(hash);
}

std::vector<std::string> split_path(std::string_view path) {
    std::vector<std::string> parts;
    std::string current;
    for (const char ch : path) {
        if (ch == '/') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

void append_json_string(std::ostringstream& out, std::string_view value) {
    out << '"';
    for (const unsigned char ch : value) {
        switch (ch) {
            case '"':
                out << "\\\"";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (ch < 0x20U) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(ch) << std::dec;
                } else {
                    out << static_cast<char>(ch);
                }
                break;
        }
    }
    out << '"';
}

std::string canonical_payload(const std::set<std::string>& source_paths) {
    std::ostringstream out;
    out << "{\"kind\":\"cmake-xray.compile-db-project-identity\",";
    out << "\"version\":1,\"source_paths\":[";
    bool first = true;
    for (const auto& path : source_paths) {
        if (!first) out << ',';
        append_json_string(out, path);
        first = false;
    }
    out << "]}";
    return out.str();
}

void lowercase_windows_drive_letter(std::string& path) {
    if (path.size() >= 2U && path[1] == ':' && path[0] >= 'A' && path[0] <= 'Z') {
        path[0] = static_cast<char>(path[0] - 'A' + 'a');
    }
}

std::vector<std::string> normalized_path_parts(std::string_view path) {
    std::vector<std::string> normalized;
    for (const auto& part : split_path(path)) {
        if (part.empty() || part == ".") continue;
        normalized.push_back(part);
    }
    return std::vector<std::string>(std::move(normalized));
}

std::string join_path_parts(const std::vector<std::string>& parts, bool absolute) {
    std::string out = absolute ? "/" : "";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0U) out.push_back('/');
        out += parts[i];
    }
    return std::string(std::move(out));
}

bool is_unc_path(std::string_view path) {
    return path.size() >= 2U && path[0] == '/' && path[1] == '/' &&
           (path.size() == 2U || path[2] != '/');
}

bool is_drive_rooted_path(std::string_view path) {
    return path.size() >= 3U && path[1] == ':' && path[2] == '/';
}

bool is_rooted_identity_path(std::string_view path) {
    return is_unc_path(path) || (!path.empty() && path.front() == '/') ||
           is_drive_rooted_path(path);
}

std::vector<std::string> parent_parts(const std::vector<std::string>& parts) {
    if (parts.empty()) return {};
    return {parts.begin(), parts.end() - 1};
}

std::vector<std::string> common_prefix(std::vector<std::string> prefix,
                                       const std::vector<std::string>& parts) {
    const auto mismatch = std::mismatch(prefix.begin(), prefix.end(), parts.begin(), parts.end());
    prefix.erase(mismatch.first, prefix.end());
    return prefix;
}

std::vector<std::string> common_parent_prefix(
    const std::vector<std::vector<std::string>>& paths) {
    if (paths.empty()) return {};
    auto prefix = parent_parts(paths.front());
    for (std::size_t i = 1; i < paths.size(); ++i) {
        prefix = common_prefix(std::move(prefix), parent_parts(paths[i]));
    }
    return std::vector<std::string>(std::move(prefix));
}

std::string join_relative_suffix(const std::vector<std::string>& parts,
                                 const std::vector<std::string>& prefix) {
    if (prefix.empty()) return join_path_parts(parts, false);
    std::vector<std::string> suffix(parts.begin() + static_cast<std::ptrdiff_t>(prefix.size()),
                                    parts.end());
    return join_path_parts(suffix, false);
}

std::set<std::string> relativize_absolute_paths_for_fingerprint(
    const std::set<std::string>& normalized_paths) {
    const bool all_rooted =
        std::all_of(normalized_paths.begin(), normalized_paths.end(), is_rooted_identity_path);
    if (!all_rooted) return normalized_paths;

    std::vector<std::vector<std::string>> path_parts;
    path_parts.reserve(normalized_paths.size());
    for (const auto& path : normalized_paths) {
        path_parts.push_back(normalized_path_parts(path));
    }
    const auto prefix = common_parent_prefix(path_parts);
    if (prefix.empty()) return normalized_paths;

    std::set<std::string> relative_paths;
    for (const auto& parts : path_parts) {
        const auto relative = join_relative_suffix(parts, prefix);
        if (!relative.empty()) relative_paths.insert(relative);
    }
    return relative_paths.empty() ? normalized_paths : relative_paths;
}

}  // namespace

std::string normalize_project_identity_path(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    lowercase_windows_drive_letter(path);

    const bool unc = is_unc_path(path);
    const bool absolute = !unc && !path.empty() && path.front() == '/';
    if (unc) return "//" + join_path_parts(normalized_path_parts(path), false);
    return join_path_parts(normalized_path_parts(path), absolute);
}

std::string compile_db_project_identity_from_source_paths(
    const std::vector<std::string>& source_paths) {
    std::set<std::string> normalized_paths;
    for (const auto& path : source_paths) {
        auto normalized = normalize_project_identity_path(path);
        if (!normalized.empty()) normalized_paths.insert(std::move(normalized));
    }
    if (normalized_paths.empty()) return {};

    return "compile-db:" +
           sha256_hex(canonical_payload(relativize_absolute_paths_for_fingerprint(
               normalized_paths)));
}

std::string compile_db_project_identity_from_ranked_units(
    const std::vector<xray::hexagon::model::RankedTranslationUnit>& translation_units) {
    std::vector<std::string> source_paths;
    source_paths.reserve(translation_units.size());
    for (const auto& unit : translation_units) {
        source_paths.push_back(unit.reference.source_path);
    }
    return compile_db_project_identity_from_source_paths(source_paths);
}

}  // namespace xray::hexagon::services
