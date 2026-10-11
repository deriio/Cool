#include "i18n.hpp"
#include <filesystem>
#include <fstream>
#include <array>
#include <optional>
#include <unordered_map>
#include "Cool/ImGui/ImGuiExtras.h"
#include "Cool/Path/Path.h"
#include "Cool/UserSettings/UserSettings.h"
#include "imgui.h"
#include "nlohmann/json.hpp"

namespace Cool::i18n {

namespace {

struct Translations {
    std::unordered_map<std::string, std::string> ui{};
    std::unordered_map<std::string, std::string> categories{};
    std::unordered_map<std::string, std::string> nodes{};
    std::unordered_map<std::string, std::string> nodes_keywords{};
    std::unordered_map<std::string, std::string> inputs{};
    std::unordered_map<std::string, std::string> tasks{};
    std::unordered_map<std::string, std::string> tasks_keywords{};
};

void load_string_map(nlohmann::json const& json, const char* section, std::unordered_map<std::string, std::string>& map)
{
    auto const it = json.find(section);
    if (it == json.end() || !it->is_object())
        return;
    for (auto const& [key, value] : it->items())
    {
        if (value.is_string())
            map[key] = value.get<std::string>();
    }
}

void load_named_map(nlohmann::json const& json, const char* section, std::unordered_map<std::string, std::string>& names, std::unordered_map<std::string, std::string>& keywords)
{
    auto const it = json.find(section);
    if (it == json.end() || !it->is_object())
        return;
    for (auto const& [key, value] : it->items())
    {
        if (value.is_string())
        {
            names[key] = value.get<std::string>();
            continue;
        }
        if (!value.is_object())
            continue;
        if (auto const name = value.find("name"); name != value.end() && name->is_string())
            names[key] = name->get<std::string>();
        if (auto const kw = value.find("keywords"); kw != value.end() && kw->is_string())
            keywords[key] = kw->get<std::string>();
    }
}

void load_file(std::filesystem::path const& path, Translations& translations)
{
    if (!std::filesystem::exists(path))
        return;
    try
    {
        auto       file = std::ifstream{path};
        auto const json = nlohmann::json::parse(file, nullptr, true /*allow_exceptions*/, true /*ignore_comments*/);
        load_string_map(json, "ui", translations.ui);
        load_string_map(json, "categories", translations.categories);
        load_named_map(json, "nodes", translations.nodes, translations.nodes_keywords);
        load_string_map(json, "inputs", translations.inputs);
        load_named_map(json, "tasks", translations.tasks, translations.tasks_keywords);
    }
    catch (std::exception const& e)
    {
        Log::internal_warning("Translations", fmt::format("Failed to load \"{}\":\n{}", path.string(), e.what()));
    }
}

bool needs_reload{true}; // NOLINT(*-avoid-non-const-global-variables)

auto get() -> Translations const&
{
    static auto translations = Translations{};
    if (needs_reload)
    {
        needs_reload         = false;
        translations         = Translations{};
        auto const file_name = fmt::format("i18n/{}.json", language_code(current_language()));
        load_file(Path::cool_res() / file_name, translations);
        load_file(Path::root() / "res" / file_name, translations); // App translations override Cool ones
    }
    return translations;
}

auto find(std::unordered_map<std::string, std::string> const& map, std::string const& key) -> std::string const*
{
    auto const it = map.find(key);
    if (it == map.end() || it->second.empty())
        return nullptr;
    return &it->second;
}

auto empty_string() -> std::string const&
{
    static auto const str = std::string{};
    return str;
}

} // namespace

auto language_code(Language language) -> const char*
{
    switch (language)
    {
    case Language::Russian:
        return "ru";
    case Language::English:
        return "en";
    }
    return "ru";
}

auto language_from_code(std::string_view code) -> Language
{
    if (code == "en")
        return Language::English;
    return Language::Russian; // Russian is the default language
}

auto current_language() -> Language
{
    return language_from_code(user_settings().language);
}

void set_language(Language language)
{
    if (language == current_language())
        return;
    user_settings().language = language_code(language);
    user_settings().save();
    reload();
}

void reload()
{
    needs_reload = true;
}

auto imgui_language_selector() -> bool
{
    static constexpr auto languages = std::array{Language::Russian, Language::English};
    auto const            label     = [](Language language) -> const char* {
        switch (language)
        {
        case Language::Russian:
            return "Русский";
        case Language::English:
            return "English";
        }
        return "";
    };

    bool b = false;
    if (ImGui::BeginCombo(tr("Language"), label(current_language())))
    {
        for (auto const language : languages)
        {
            if (ImGui::Selectable(label(language), language == current_language()))
            {
                set_language(language);
                b = true;
            }
        }
        ImGui::EndCombo();
    }
    return b;
}

auto tr(const char* text) -> const char*
{
    auto const* res = find(get().ui, text);
    return res ? res->c_str() : text;
}

auto tr(std::string const& text) -> std::string const&
{
    auto const* res = find(get().ui, text);
    return res ? *res : text;
}

auto tr_category(std::string const& category_name) -> std::string const&
{
    auto const* res = find(get().categories, category_name);
    return res ? *res : category_name;
}

static auto node_key(std::string const& category_name, std::string const& node_name) -> std::string
{
    return category_name + '/' + node_name;
}

auto tr_node(std::string const& category_name, std::string const& node_name) -> std::string const&
{
    auto const* res = find(get().nodes, node_key(category_name, node_name));
    return res ? *res : node_name;
}

auto node_keywords(std::string const& category_name, std::string const& node_name) -> std::string const&
{
    auto const* res = find(get().nodes_keywords, node_key(category_name, node_name));
    return res ? *res : empty_string();
}

auto tr_input(std::string const& input_name) -> std::string const&
{
    if (auto const* res = find(get().inputs, input_name))
        return *res;
    return tr(input_name); // Many inputs have generic names that are already translated for the UI
}

auto tr_task(std::string const& task_id) -> std::string const&
{
    auto const* res = find(get().tasks, task_id);
    return res ? *res : task_id;
}

auto task_keywords(std::string const& task_id) -> std::string const&
{
    auto const* res = find(get().tasks_keywords, task_id);
    return res ? *res : empty_string();
}

auto label(std::string_view text) -> std::string
{
    auto const id_pos  = text.find("##");
    auto const visible = std::string{text.substr(0, id_pos)};
    auto const& translated = tr(visible);
    if (&translated == &visible) // No translation
        return std::string{text};
    if (id_pos != std::string_view::npos && text.substr(id_pos).starts_with("###"))
        return fmt::format("{}{}", translated, text.substr(id_pos));
    return fmt::format("{}###{}", translated, text);
}

auto window_title(std::string const& title) -> std::string
{
    if (title.find("###") != std::string::npos) // Already has a stable ID
        return title;
    // Titles created with icon_fmt() look like " <icon> Text": only translate the text part
    auto prefix = std::string{};
    auto text   = std::string_view{title};
    if (text.starts_with(' '))
    {
        auto const pos = text.find(' ', 1);
        if (pos != std::string_view::npos)
        {
            prefix = std::string{text.substr(0, pos + 1)};
            text   = text.substr(pos + 1);
        }
    }
    return fmt::format("{}{}###{}", prefix, tr(std::string{text}), title);
}

auto to_lower_utf8(std::string_view str) -> std::string
{
    auto res = std::string{};
    res.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i)
    {
        auto const c = static_cast<unsigned char>(str[i]);
        if (c < 0x80)
        {
            res += static_cast<char>(std::tolower(c));
            continue;
        }
        // Cyrillic letters are encoded on 2 bytes: 0xD0 or 0xD1 followed by a continuation byte
        if ((c == 0xD0 || c == 0xD1) && i + 1 < str.size())
        {
            auto const c2        = static_cast<unsigned char>(str[i + 1]);
            auto       codepoint = static_cast<uint32_t>(((c & 0x1F) << 6) | (c2 & 0x3F));
            if (codepoint >= 0x0410 && codepoint <= 0x042F) // А-Я
                codepoint += 0x20;
            else if (codepoint >= 0x0400 && codepoint <= 0x040F) // Ѐ-Џ (including Ё)
                codepoint += 0x50;
            if (codepoint == 0x0451) // Treat ё as е, so that searches work regardless of how people type it
                codepoint = 0x0435;
            res += static_cast<char>(0xC0 | (codepoint >> 6));
            res += static_cast<char>(0x80 | (codepoint & 0x3F));
            ++i;
            continue;
        }
        res += static_cast<char>(c);
    }
    return res;
}

} // namespace Cool::i18n
