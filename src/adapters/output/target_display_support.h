#pragma once

#include <string>
#include <vector>

namespace xray::adapters::output {

// Single (display_text, identity_key) entry for the target-display
// disambiguation helper. Adapter-side type, on purpose: M6 AP 1.2 Tranche A.3
// shares the same helper across console, markdown and html, but it deals
// purely with rendering, never with hexagon model invariants.
struct TargetDisplayEntry {
    std::string display_text;
    std::string identity_key;
};

// Disambiguate target display strings against name collisions inside a single
// rendered list, per docs/plan-M6-1-2.md "Adapter-Implementierungs-Hinweise"
// and docs/report-html.md "Disambiguierung kollidierender Target-Display-
// Namen". Operates on pairs so the same helper covers File-API targets,
// "<external>::*" edge targets and mixed collisions where an internal target
// `foo` and an external target with raw_id `foo` share the same display_text
// but differ in identity_key.
//
// Output rules:
//   - If `display_text` is unique inside `entries` (only one distinct
//     `identity_key` carries it), the helper returns it unchanged.
//   - If two or more distinct `identity_key` values share the same
//     `display_text`, every colliding entry is rewritten as
//     `<display_text> [key: <identity_key>]`.
//   - Entries that share both `display_text` and `identity_key` count as one
//     for the collision check; AP 1.1's `sort_target_graph` already dedups
//     such pairs, so the helper does not need to.
//
// The helper does NOT HTML-escape the `<` and `>` inside `<external>::*`
// keys. Each adapter owns its escaping pass; HTML adapters call this helper
// first and escape the result, while console and markdown adapters render
// the suffix verbatim per their respective contracts.
//
// Returned strings are aligned positionally with `entries`.
std::vector<std::string> disambiguate_target_display_names(
    const std::vector<TargetDisplayEntry>& entries);

}  // namespace xray::adapters::output
