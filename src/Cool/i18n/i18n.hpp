#pragma once
#include <string>
#include <string_view>

/// Localization.
/// The English text is used as the key, and translations are loaded from "res/i18n/<language>.json" (both from Cool's res and the app's res, the latter taking precedence).
/// If a translation is missing, the English text is returned unchanged.
///
/// Translation files have the following sections, all optional:
/// {
///     "ui":         { "English text": "Translation" },
///     "categories": { "Category name": "Translation" },
///     "nodes":      { "Category name/Node name": { "name": "Translation", "keywords": "words used by the search bar" } },
///     "inputs":     { "Input name": "Translation" },
///     "tasks":      { "task_id": { "name": "Translation", "keywords": "..." } }
/// }
namespace Cool::i18n {

enum class Language {
    Russian,
    English,
};

auto current_language() -> Language;
void set_language(Language);
auto language_code(Language) -> const char*;
auto language_from_code(std::string_view code) -> Language;
/// Returns true iff the language was changed
auto imgui_language_selector() -> bool;

/// Translates a piece of UI text. Returns the text itself if there is no translation.
auto tr(const char* text) -> const char*;
/// NB: the returned reference might be the one you passed in, so don't keep it longer than the input string
auto tr(std::string const& text) -> std::string const&;

auto tr_category(std::string const& category_name) -> std::string const&;
auto tr_node(std::string const& category_name, std::string const& node_name) -> std::string const&;
/// Additional words that the search bar uses to find the node (can be empty)
auto node_keywords(std::string const& category_name, std::string const& node_name) -> std::string const&;
auto tr_input(std::string const& input_name) -> std::string const&;
auto tr_task(std::string const& task_id) -> std::string const&;
auto task_keywords(std::string const& task_id) -> std::string const&;

/// Use this for the labels of ImGui widgets: the visible text gets translated but the ImGui ID stays the same.
/// Supports labels that already contain an ID (e.g. "Delete##preset").
auto label(std::string_view text) -> std::string;

/// Use this for ImGui windows: the visible title gets translated but the ImGui ID stays the same, so that the docking layout is kept when changing language.
auto window_title(std::string const& title) -> std::string;

/// Lower-cases a UTF-8 string, including Cyrillic letters (used to do case-insensitive searches).
auto to_lower_utf8(std::string_view) -> std::string;

/// Reloads the translation files (called automatically when the language changes).
void reload();

} // namespace Cool::i18n

namespace Cool {
using i18n::tr;
}
