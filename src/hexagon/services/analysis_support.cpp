#include "services/analysis_support.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>

namespace xray::hexagon::services {

namespace {

using xray::hexagon::model::CompileCommandSource;
using xray::hexagon::model::CompileEntry;
using xray::hexagon::model::Diagnostic;
using xray::hexagon::model::DiagnosticSeverity;
using xray::hexagon::model::IncludeDepthFilter;
using xray::hexagon::model::IncludeDepthKind;
using xray::hexagon::model::IncludeOrigin;
using xray::hexagon::model::IncludeResolutionResult;
using xray::hexagon::model::IncludeScope;
using xray::hexagon::model::RankedTranslationUnit;
using xray::hexagon::model::TranslationUnitObservation;
using xray::hexagon::model::TranslationUnitReference;

enum class CompileFlagKind {
    include,
    system,
    quote,
    define,
};

struct TokenizationResult {
    std::vector<std::string> tokens;
    std::vector<Diagnostic> diagnostics;
};

struct TokenizationRequest {
    std::string_view command_text;
    std::string_view translation_unit_path;
};

struct TokenizerState {
    TokenizationResult result;
    std::string current;
    bool in_single_quotes{false};
    bool in_double_quotes{false};
};

bool path_is_within(const std::filesystem::path& path, const std::filesystem::path& base) {
    const auto relative = path.lexically_relative(base);
    if (relative.empty()) return false;

    const auto as_text = relative.generic_string();
    return as_text != ".." && !as_text.starts_with("../");
}

std::filesystem::path resolve_path(const std::filesystem::path& base, std::string_view candidate) {
    const std::filesystem::path candidate_path{candidate};
    if (candidate_path.has_root_directory()) return candidate_path.lexically_normal();
    return (base / candidate_path).lexically_normal();
}

void flush_token(TokenizerState& state) {
    if (state.current.empty()) return;

    state.result.tokens.push_back(state.current);
    state.current.clear();
}

void handle_single_quoted_character(TokenizerState& state, char ch) {
    if (ch == '\'') {
        state.in_single_quotes = false;
        return;
    }

    state.current.push_back(ch);
}

void handle_double_quoted_character(const TokenizationRequest& request, TokenizerState& state,
                                    std::size_t& index) {
    const char ch = request.command_text[index];
    if (ch == '"') {
        state.in_double_quotes = false;
        return;
    }

    if (ch == '\\' && index + 1 < request.command_text.size()) {
        state.current.push_back(request.command_text[index + 1]);
        ++index;
        return;
    }

    state.current.push_back(ch);
}

void handle_unquoted_character(const TokenizationRequest& request, TokenizerState& state,
                               std::size_t& index) {
    const char ch = request.command_text[index];
    if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
        flush_token(state);
        return;
    }

    if (ch == '\'') {
        state.in_single_quotes = true;
        return;
    }

    if (ch == '"') {
        state.in_double_quotes = true;
        return;
    }

    if (ch == '\\' && index + 1 < request.command_text.size()) {
        state.current.push_back(request.command_text[index + 1]);
        ++index;
        return;
    }

    state.current.push_back(ch);
}

void finalize_tokenization(const TokenizationRequest& request, TokenizerState& state) {
    flush_token(state);

    if (state.in_single_quotes || state.in_double_quotes) {
        state.result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "command tokenization ended with an unmatched quote for " +
                 std::string(request.translation_unit_path) + "; metrics use best effort parsing"});
    }

    if (state.result.tokens.empty()) {
        state.result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "command tokenization produced no arguments for " +
                 std::string(request.translation_unit_path)});
    }
}

TokenizationResult tokenize_command(const TokenizationRequest& request) {
    TokenizerState state;

    for (std::size_t index = 0; index < request.command_text.size(); ++index) {
        if (state.in_single_quotes) {
            handle_single_quoted_character(state, request.command_text[index]);
            continue;
        }

        if (state.in_double_quotes) {
            handle_double_quoted_character(request, state, index);
            continue;
        }

        handle_unquoted_character(request, state, index);
    }

    finalize_tokenization(request, state);
    return state.result;
}

std::vector<Diagnostic> collect_resolution_diagnostics(
    const IncludeResolutionResult& include_resolution, std::string_view translation_unit_key) {
    for (const auto& resolved : include_resolution.translation_units) {
        if (resolved.translation_unit_key == translation_unit_key) return resolved.diagnostics;
    }
    return {};
}

bool ranked_translation_unit_less(const RankedTranslationUnit& lhs,
                                  const RankedTranslationUnit& rhs) {
    if (lhs.score() != rhs.score()) return lhs.score() > rhs.score();
    if (lhs.arg_count != rhs.arg_count) return lhs.arg_count > rhs.arg_count;
    if (lhs.include_path_count != rhs.include_path_count) {
        return lhs.include_path_count > rhs.include_path_count;
    }
    if (lhs.define_count != rhs.define_count) return lhs.define_count > rhs.define_count;
    return lhs.reference.unique_key < rhs.reference.unique_key;
}

bool translation_unit_reference_less(const TranslationUnitReference& lhs,
                                     const TranslationUnitReference& rhs) {
    return lhs.unique_key < rhs.unique_key;
}

std::string display_input_path(std::string_view raw_value, const std::string& resolved_value,
                               const std::filesystem::path& base_directory) {
    (void)raw_value;
    return make_display_path(resolved_value, base_directory);
}

TranslationUnitReference build_reference(const CompileEntry& entry,
                                         const std::filesystem::path& base_directory,
                                         const std::string& resolved_source_path,
                                         const std::string& resolved_directory) {
    return {
        .source_path = display_input_path(entry.file(), resolved_source_path, base_directory),
        .directory = display_input_path(entry.directory(), resolved_directory, base_directory),
        .source_path_key = resolved_source_path,
        .unique_key = resolved_source_path + "|" + resolved_directory,
    };
}

std::vector<std::string> collect_entry_tokens(const CompileEntry& entry,
                                              const TranslationUnitReference& reference,
                                              std::vector<Diagnostic>& diagnostics) {
    if (entry.command_source() == CompileCommandSource::arguments) return entry.arguments();

    auto tokenization = tokenize_command(
        {.command_text = entry.command(), .translation_unit_path = reference.source_path});
    diagnostics = std::move(tokenization.diagnostics);
    return std::move(tokenization.tokens);
}

void add_include_path(TranslationUnitObservation& observation, const std::string& resolved_directory,
                      std::string_view value, std::vector<std::string>& target) {
    ++observation.include_path_count;
    target.push_back(
        normalize_path(resolve_path(std::filesystem::path{resolved_directory}, value)));
}

void note_missing_flag_value(TranslationUnitObservation& observation, std::string_view flag_name) {
    observation.diagnostics.push_back(
        {DiagnosticSeverity::warning,
         std::string(flag_name) + " is missing a value for " +
             observation.reference.source_path});
}

void apply_explicit_flag(CompileFlagKind flag_kind, std::string_view value,
                         const std::string& resolved_directory,
                         TranslationUnitObservation& observation) {
    if (flag_kind == CompileFlagKind::define) {
        ++observation.define_count;
        return;
    }

    if (flag_kind == CompileFlagKind::include) {
        add_include_path(observation, resolved_directory, value, observation.include_paths);
        return;
    }

    if (flag_kind == CompileFlagKind::system) {
        add_include_path(observation, resolved_directory, value,
                         observation.system_include_paths);
        return;
    }

    add_include_path(observation, resolved_directory, value, observation.quote_include_paths);
}

std::optional<CompileFlagKind> split_flag_kind(std::string_view token) {
    if (token == "-I") return CompileFlagKind::include;
    if (token == "-isystem") return CompileFlagKind::system;
    if (token == "-iquote") return CompileFlagKind::quote;
    if (token == "-D") return CompileFlagKind::define;
    return std::nullopt;
}

bool try_apply_split_flag(const std::vector<std::string>& tokens, std::size_t& index,
                          const std::string& resolved_directory,
                          TranslationUnitObservation& observation) {
    const auto& token = tokens[index];
    const auto flag_kind = split_flag_kind(token);
    if (!flag_kind.has_value()) return false;

    if (index + 1 >= tokens.size()) {
        note_missing_flag_value(observation, token);
        return true;
    }

    apply_explicit_flag(*flag_kind, tokens[index + 1], resolved_directory, observation);
    ++index;
    return true;
}

bool try_apply_inline_flag(std::string_view token, const std::string& resolved_directory,
                           TranslationUnitObservation& observation) {
    const auto apply_if_prefixed = [&](std::string_view prefix, CompileFlagKind flag_kind) {
        if (token.rfind(prefix, 0) != 0 || token.size() <= prefix.size()) return false;

        apply_explicit_flag(flag_kind, token.substr(prefix.size()), resolved_directory, observation);
        return true;
    };

    return apply_if_prefixed("-isystem", CompileFlagKind::system) ||
           apply_if_prefixed("-iquote", CompileFlagKind::quote) ||
           apply_if_prefixed("-I", CompileFlagKind::include) ||
           apply_if_prefixed("-D", CompileFlagKind::define);
}

void apply_compile_tokens(const std::vector<std::string>& tokens,
                          const std::string& resolved_directory,
                          TranslationUnitObservation& observation) {
    observation.arg_count = tokens.size();

    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (try_apply_split_flag(tokens, index, resolved_directory, observation)) continue;
        try_apply_inline_flag(tokens[index], resolved_directory, observation);
    }
}

TranslationUnitObservation build_translation_unit_observation(
    const CompileEntry& entry, const std::filesystem::path& base_directory) {
    const auto resolved_directory = normalize_path(resolve_path(base_directory, entry.directory()));
    const auto resolved_source_path =
        normalize_path(resolve_path(std::filesystem::path{resolved_directory}, entry.file()));

    TranslationUnitObservation observation;
    observation.reference =
        build_reference(entry, base_directory, resolved_source_path, resolved_directory);
    observation.resolved_source_path = resolved_source_path;
    observation.resolved_directory = resolved_directory;

    const auto tokens =
        collect_entry_tokens(entry, observation.reference, observation.diagnostics);
    apply_compile_tokens(tokens, resolved_directory, observation);

    return observation;
}

}  // namespace

namespace {

#ifdef _WIN32
constexpr bool kPlatformPathsCaseInsensitive = true;
#else
constexpr bool kPlatformPathsCaseInsensitive = false;
#endif

bool resolve_case_insensitivity(ReportPathCasePolicy policy) {
    if (policy == ReportPathCasePolicy::case_sensitive) return false;
    if (policy == ReportPathCasePolicy::case_insensitive) return true;
    return kPlatformPathsCaseInsensitive;
}

std::string maybe_case_fold(std::string value, bool case_insensitive) {
    if (!case_insensitive) return value;
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool paths_equal_with_policy(const std::filesystem::path& lhs,
                              const std::filesystem::path& rhs, bool case_insensitive) {
    return maybe_case_fold(lhs.generic_string(), case_insensitive) ==
           maybe_case_fold(rhs.generic_string(), case_insensitive);
}

bool report_path_under_base(const std::filesystem::path& path,
                            const std::filesystem::path& base, bool case_insensitive) {
    const auto path_text = maybe_case_fold(path.generic_string(), case_insensitive);
    const auto base_text = maybe_case_fold(base.generic_string(), case_insensitive);
    const std::filesystem::path folded_path{path_text};
    const std::filesystem::path folded_base{base_text};
    if (folded_path.root_name() != folded_base.root_name()) return false;
    const auto relative = folded_path.lexically_relative(folded_base);
    if (relative.empty()) return false;
    const auto text = relative.generic_string();
    return text != ".." && !text.starts_with("../");
}

std::string strip_base_prefix(const std::filesystem::path& display_path,
                               const std::filesystem::path& report_display_base) {
    const auto base_str = report_display_base.generic_string();
    auto strip_len = base_str.size();
    if (strip_len > 0 && base_str.back() != '/') ++strip_len;
    return display_path.generic_string().substr(strip_len);
}

std::string display_resolved_adapter_path(const std::filesystem::path& display_path,
                                          const std::filesystem::path& report_display_base,
                                          bool case_insensitive) {
    if (paths_equal_with_policy(display_path, report_display_base, case_insensitive)) return ".";
    if (!report_path_under_base(display_path, report_display_base, case_insensitive)) {
        return display_path.generic_string();
    }
    return strip_base_prefix(display_path, report_display_base);
}

}  // namespace

std::filesystem::path compile_commands_base_directory(
    std::string_view compile_commands_path, const std::filesystem::path& fallback_base) {
    const std::filesystem::path input{compile_commands_path};
    const std::filesystem::path resolved =
        input.has_root_directory()
            ? input.lexically_normal()
            : (fallback_base / input).lexically_normal();
    const auto base = resolved.parent_path();
    return base.empty() ? fallback_base.lexically_normal() : base;
}

std::optional<std::filesystem::path> source_root_from_build_model(
    const model::BuildModelResult& build_model) {
    if (build_model.source_root.empty()) return std::nullopt;
    return std::filesystem::path{build_model.source_root};
}

std::optional<std::string> to_report_display_path(
    ReportPathDisplayInput input, const std::filesystem::path& report_display_base,
    ReportPathCasePolicy case_policy) {
    if (!input.display_path.has_value()) return std::nullopt;
    const auto display = input.display_path->lexically_normal();

    if (input.kind == ReportPathDisplayKind::input_argument) {
        return display.generic_string();
    }
    if (!input.was_relative || !display.is_absolute()) {
        return display.generic_string();
    }
    return display_resolved_adapter_path(display, report_display_base.lexically_normal(),
                                          resolve_case_insensitivity(case_policy));
}

std::string normalize_path(const std::filesystem::path& path) {
    if (path.has_root_directory()) return path.lexically_normal().generic_string();
    return std::filesystem::absolute(path).lexically_normal().generic_string();
}

std::string make_display_path(const std::string& normalized_path,
                              const std::filesystem::path& base_directory) {
    const auto absolute_path = std::filesystem::path{normalized_path}.lexically_normal();
    const auto absolute_base = base_directory.has_root_directory()
                                   ? base_directory.lexically_normal()
                                   : std::filesystem::absolute(base_directory).lexically_normal();

    if (path_is_within(absolute_path, absolute_base)) {
        return absolute_path.lexically_relative(absolute_base).generic_string();
    }

    return absolute_path.generic_string();
}

std::vector<TranslationUnitObservation> build_translation_unit_observations(
    const std::vector<CompileEntry>& entries, const std::filesystem::path& base_directory) {
    std::vector<TranslationUnitObservation> observations;
    observations.reserve(entries.size());

    for (const auto& entry : entries) {
        observations.push_back(build_translation_unit_observation(entry, base_directory));
    }

    return observations; }

std::vector<RankedTranslationUnit> build_ranked_translation_units(
    const std::vector<TranslationUnitObservation>& observations,
    const IncludeResolutionResult& include_resolution) {
    std::vector<RankedTranslationUnit> ranked;
    ranked.reserve(observations.size());

    for (const auto& observation : observations) {
        auto diagnostics = observation.diagnostics;
        const auto resolution_diagnostics =
            collect_resolution_diagnostics(include_resolution, observation.reference.unique_key);
        diagnostics.insert(diagnostics.end(), resolution_diagnostics.begin(),
                           resolution_diagnostics.end());

        ranked.push_back({
            .reference = observation.reference,
            .rank = 0,
            .arg_count = observation.arg_count,
            .include_path_count = observation.include_path_count,
            .define_count = observation.define_count,
            .diagnostics = std::move(diagnostics),
            .targets = {},
        });
    }

    std::sort(ranked.begin(), ranked.end(), ranked_translation_unit_less);

    for (std::size_t index = 0; index < ranked.size(); ++index) {
        ranked[index].rank = index + 1;
    }

    return ranked;
}

namespace {

bool is_path_under(const std::filesystem::path& candidate, const std::filesystem::path& base) {
    const auto cand_norm = candidate.lexically_normal();
    const auto base_norm = base.lexically_normal();
    auto cand_it = cand_norm.begin();
    const auto cand_end = cand_norm.end();
    auto base_it = base_norm.begin();
    const auto base_end = base_norm.end();

    const auto skip_separators = [](auto& it, auto end) {
        while (it != end) {
            const auto piece = it->generic_string();
            if (!piece.empty() && piece != "/") return;
            ++it;
        }
    };

    skip_separators(base_it, base_end);
    if (base_it == base_end) return false;
    while (base_it != base_end) {
        skip_separators(cand_it, cand_end);
        if (cand_it == cand_end) return false;
        if (cand_it->generic_string() != base_it->generic_string()) return false;
        ++cand_it;
        ++base_it;
        skip_separators(base_it, base_end);
    }
    return true;
}

bool any_observation_path_under(
    const std::vector<TranslationUnitObservation>& observations,
    const std::filesystem::path& header,
    const std::vector<std::string> TranslationUnitObservation::*member) {
    for (const auto& obs : observations) {
        for (const auto& entry : obs.*member) {
            if (is_path_under(header, std::filesystem::path{entry})) return true;
        }
    }
    return false;
}

bool scope_matches(IncludeOrigin origin, IncludeScope scope) {
    if (scope == IncludeScope::all) return true;
    if (scope == IncludeScope::project) return origin == IncludeOrigin::project;
    if (scope == IncludeScope::external) return origin == IncludeOrigin::external;
    return origin == IncludeOrigin::unknown;
}

bool depth_matches(IncludeDepthKind kind, IncludeDepthFilter filter) {
    if (filter == IncludeDepthFilter::all) return true;
    if (filter == IncludeDepthFilter::direct) return kind == IncludeDepthKind::direct;
    return kind == IncludeDepthKind::indirect;
}

IncludeDepthKind aggregate_depth_kind(bool has_direct, bool has_indirect) {
    if (has_direct && has_indirect) return IncludeDepthKind::mixed;
    if (has_indirect) return IncludeDepthKind::indirect;
    return IncludeDepthKind::direct;
}

struct HotspotAccumulator {
    std::vector<TranslationUnitReference> affected_translation_units;
    bool has_direct{false};
    bool has_indirect{false};
};

std::map<std::string, HotspotAccumulator> accumulate_hotspot_entries(
    const std::vector<TranslationUnitObservation>& observations,
    const IncludeResolutionResult& include_resolution) {
    std::unordered_map<std::string, TranslationUnitReference> references_by_key;
    references_by_key.reserve(observations.size());
    for (const auto& observation : observations) {
        references_by_key.emplace(observation.reference.unique_key, observation.reference);
    }
    std::map<std::string, HotspotAccumulator> by_header;
    for (const auto& resolved : include_resolution.translation_units) {
        const auto reference_it = references_by_key.find(resolved.translation_unit_key);
        if (reference_it == references_by_key.end()) continue;
        for (const auto& entry : resolved.headers) {
            auto& acc = by_header[entry.header_path];
            acc.affected_translation_units.push_back(reference_it->second);
            if (entry.depth_kind == IncludeDepthKind::direct) acc.has_direct = true;
            if (entry.depth_kind == IncludeDepthKind::indirect) acc.has_indirect = true;
        }
    }
    return by_header;
}

}  // namespace

IncludeOrigin classify_include_origin(const std::string& header_path,
                                      const std::vector<TranslationUnitObservation>& observations,
                                      const std::filesystem::path& source_root) {
    const auto header = std::filesystem::path{header_path};
    if (!source_root.empty() && is_path_under(header, source_root)) {
        return IncludeOrigin::project;
    }
    if (any_observation_path_under(observations, header,
                                   &TranslationUnitObservation::system_include_paths)) {
        return IncludeOrigin::external;
    }
    if (any_observation_path_under(observations, header,
                                   &TranslationUnitObservation::quote_include_paths) ||
        any_observation_path_under(observations, header,
                                   &TranslationUnitObservation::include_paths)) {
        return IncludeOrigin::project;
    }
    return IncludeOrigin::unknown;
}

IncludeHotspotsBuildResult build_include_hotspots(
    const std::vector<TranslationUnitObservation>& observations,
    const IncludeResolutionResult& include_resolution,
    const std::filesystem::path& base_directory, const std::filesystem::path& source_root,
    IncludeHotspotFilters filters) {
    auto by_header = accumulate_hotspot_entries(observations, include_resolution);

    std::vector<model::IncludeHotspot> all_hotspots;
    for (auto& [header_key, acc] : by_header) {
        auto& affected = acc.affected_translation_units;
        std::sort(affected.begin(), affected.end(), translation_unit_reference_less);
        affected.erase(std::unique(affected.begin(), affected.end(),
                                   [](const auto& lhs, const auto& rhs) {
                                       return lhs.unique_key == rhs.unique_key;
                                   }),
                       affected.end());
        if (affected.size() < 2) continue;
        all_hotspots.push_back(model::IncludeHotspot{
            make_display_path(header_key, base_directory),
            std::move(affected),
            {},
            classify_include_origin(header_key, observations, source_root),
            aggregate_depth_kind(acc.has_direct, acc.has_indirect),
        });
    }

    std::sort(all_hotspots.begin(), all_hotspots.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.affected_translation_units.size() != rhs.affected_translation_units.size()) {
            return lhs.affected_translation_units.size() > rhs.affected_translation_units.size();
        }
        return lhs.header_path < rhs.header_path;
    });

    IncludeHotspotsBuildResult out;
    out.total_count = all_hotspots.size();
    for (auto& hotspot : all_hotspots) {
        if (!scope_matches(hotspot.origin, filters.scope)) {
            if (hotspot.origin == IncludeOrigin::unknown) ++out.excluded_unknown_count;
            continue;
        }
        if (!depth_matches(hotspot.depth_kind, filters.depth_filter)) {
            if (hotspot.depth_kind == IncludeDepthKind::mixed) ++out.excluded_mixed_count;
            continue;
        }
        out.hotspots.push_back(std::move(hotspot));
    }
    return out;
}

std::string resolve_changed_file_key(const std::filesystem::path& base_directory,
                                     const std::filesystem::path& changed_path) {
    return normalize_path(resolve_path(base_directory, changed_path.generic_string()));
}

std::string display_changed_file(const std::filesystem::path& base_directory,
                                 const std::string& changed_file_key) {
    return make_display_path(changed_file_key, base_directory);
}

}  // namespace xray::hexagon::services
