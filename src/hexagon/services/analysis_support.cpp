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
using xray::hexagon::model::IncludeResolutionResult;
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
    if (candidate_path.is_absolute()) return candidate_path.lexically_normal();
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

std::filesystem::path compile_commands_base_directory(std::string_view compile_commands_path) {
    const auto base = std::filesystem::absolute(std::filesystem::path{compile_commands_path})
                          .lexically_normal()
                          .parent_path();
    return base.empty() ? std::filesystem::current_path() : base;
}

std::string display_compile_commands_path(std::string_view compile_commands_path) {
    return std::filesystem::path{compile_commands_path}.lexically_normal().generic_string();
}

std::string normalize_path(const std::filesystem::path& path) {
    return std::filesystem::absolute(path).lexically_normal().generic_string();
}

std::string make_display_path(const std::string& normalized_path,
                              const std::filesystem::path& base_directory) {
    const auto absolute_path = std::filesystem::path{normalized_path}.lexically_normal();
    const auto absolute_base =
        std::filesystem::absolute(base_directory).lexically_normal();

    if (path_is_within(absolute_path, absolute_base)) {
        return absolute_path.lexically_relative(absolute_base).generic_string();
    }

    return absolute_path.generic_string();
}

std::vector<TranslationUnitObservation> build_translation_unit_observations(
    const std::vector<CompileEntry>& entries, std::string_view compile_commands_path) {
    const auto base_directory = compile_commands_base_directory(compile_commands_path);

    std::vector<TranslationUnitObservation> observations;
    observations.reserve(entries.size());

    for (const auto& entry : entries) {
        observations.push_back(build_translation_unit_observation(entry, base_directory));
    }

    return observations;
}

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
        });
    }

    std::sort(ranked.begin(), ranked.end(), ranked_translation_unit_less);

    for (std::size_t index = 0; index < ranked.size(); ++index) {
        ranked[index].rank = index + 1;
    }

    return ranked;
}

std::vector<model::IncludeHotspot> build_include_hotspots(
    const std::vector<TranslationUnitObservation>& observations,
    const IncludeResolutionResult& include_resolution, std::string_view compile_commands_path) {
    const auto base_directory = compile_commands_base_directory(compile_commands_path);

    std::unordered_map<std::string, TranslationUnitReference> references_by_key;
    references_by_key.reserve(observations.size());
    for (const auto& observation : observations) {
        references_by_key.emplace(observation.reference.unique_key, observation.reference);
    }

    std::map<std::string, std::vector<TranslationUnitReference>> hotspots_by_header;

    for (const auto& resolved : include_resolution.translation_units) {
        const auto reference_it = references_by_key.find(resolved.translation_unit_key);
        if (reference_it == references_by_key.end()) continue;

        for (const auto& header : resolved.headers) {
            hotspots_by_header[header].push_back(reference_it->second);
        }
    }

    std::vector<model::IncludeHotspot> hotspots;
    for (auto& [header_key, affected] : hotspots_by_header) {
        std::sort(affected.begin(), affected.end(), translation_unit_reference_less);
        affected.erase(std::unique(affected.begin(), affected.end(),
                                   [](const auto& lhs, const auto& rhs) {
                                       return lhs.unique_key == rhs.unique_key;
                                   }),
                       affected.end());

        if (affected.size() < 2) continue;

        hotspots.push_back({
            .header_path = make_display_path(header_key, base_directory),
            .affected_translation_units = std::move(affected),
            .diagnostics = {},
        });
    }

    std::sort(hotspots.begin(), hotspots.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.affected_translation_units.size() != rhs.affected_translation_units.size()) {
            return lhs.affected_translation_units.size() > rhs.affected_translation_units.size();
        }
        return lhs.header_path < rhs.header_path;
    });

    return hotspots;
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
