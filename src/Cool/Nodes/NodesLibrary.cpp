#include "NodesLibrary.h"
#include <Cool/String/String.h>
#include <wafl/wafl.hpp>
#include <filesystem>
#include <fstream>
#include "Cool/ImGui/ImGuiExtras.h"
#include "Cool/Path/Path.h"
#include "Cool/i18n/i18n.hpp"
#include "nlohmann/json.hpp"

namespace Cool {

namespace internal {
auto name_matches_filter(std::string const& name, std::string const& filter) -> bool
{
    return wafl::similarity_match({.input = filter, .reference = name}) >= wafl::Matches::Strongly;
}
} // namespace internal

auto NodesLibrary::get_category(std::string const& category_name) const -> NodesCategory const*
{
    auto const it = std::find_if(_categories.begin(), _categories.end(), [&](NodesCategory const& cat) {
        return cat.name() == category_name;
    });

    if (it != _categories.end())
        return &*it;
    return nullptr;
}

auto NodesLibrary::get_category(std::string const& category_name) -> NodesCategory*
{
    auto const it = std::find_if(_categories.begin(), _categories.end(), [&](NodesCategory const& cat) {
        return cat.name() == category_name;
    });

    if (it != _categories.end())
        return &*it;
    return nullptr;
}

namespace {

/// Describes how the categories are organized in the nodes menu. Loaded from "res/nodes_menu.json".
struct NodesMenuLayout {
    struct TypeGroup {
        std::string              name{}; // English name, translated with tr()
        std::vector<std::string> categories{};
    };
    struct Task {
        std::string              id{}; // Translated with tr_task()
        std::vector<std::string> nodes{};      // "Category/Node name"
        std::vector<std::string> categories{}; // All the nodes of these categories belong to the task
    };
    std::vector<TypeGroup> type_groups{};
    std::vector<Task>      tasks{};
};

auto load_nodes_menu_layout() -> NodesMenuLayout
{
    auto       layout = NodesMenuLayout{};
    auto const path   = Path::root() / "res/nodes_menu.json";
    if (!std::filesystem::exists(path))
        return layout;
    try
    {
        auto       file = std::ifstream{path};
        auto const json = nlohmann::json::parse(file, nullptr, true /*allow_exceptions*/, true /*ignore_comments*/);
        auto const get_strings = [](nlohmann::json const& j, const char* key) {
            auto res = std::vector<std::string>{};
            if (auto const it = j.find(key); it != j.end() && it->is_array())
            {
                for (auto const& str : *it)
                {
                    if (str.is_string())
                        res.push_back(str.get<std::string>());
                }
            }
            return res;
        };
        for (auto const& group : json.value("by_type", nlohmann::json::array()))
            layout.type_groups.push_back({.name = group.value("name", ""), .categories = get_strings(group, "categories")});
        for (auto const& task : json.value("by_task", nlohmann::json::array()))
            layout.tasks.push_back({.id = task.value("id", ""), .nodes = get_strings(task, "nodes"), .categories = get_strings(task, "categories")});
    }
    catch (std::exception const& e)
    {
        Log::internal_warning("Nodes menu", fmt::format("Failed to load \"{}\":\n{}", path.string(), e.what()));
    }
    return layout;
}

auto nodes_menu_layout() -> NodesMenuLayout const&
{
    static auto const layout = load_nodes_menu_layout();
    return layout;
}

auto task_contains(NodesMenuLayout::Task const& task, std::string const& category_name, std::string const& node_name) -> bool
{
    if (std::find(task.categories.begin(), task.categories.end(), category_name) != task.categories.end())
        return true;
    auto const key = category_name + '/' + node_name;
    return std::find(task.nodes.begin(), task.nodes.end(), key) != task.nodes.end();
}

enum class MenuTab : uint8_t {
    ByType,
    ByTask,
};

struct MenuState {
    MenuTab tab{MenuTab::ByType};
    bool    show_incompatible_nodes{false};
};

auto menu_state() -> MenuState&
{
    static auto state = MenuState{};
    return state;
}

/// Popups and sub-menus must never be bigger than the screen, otherwise you can't reach their content.
void constrain_next_menu_size()
{
    auto const max_height = ImGui::GetMainViewport()->WorkSize.y * 0.7f;
    ImGui::SetNextWindowSizeConstraints(ImVec2{0.f, 0.f}, ImVec2{FLT_MAX, max_height});
}

struct NodeEntry {
    NodeDefinition const*            def;
    NodesCategory const*             category;
    std::optional<NodeCompatibility> compatibility;
};

class MenuContext {
public:
    MenuContext(NodesMenuParams const& params, std::vector<NodesCategory> const& categories)
        : _params{params}
        , _categories{categories}
    {}

    [[nodiscard]] auto is_filtering_by_compatibility() const -> bool
    {
        return _params.check_compatibility && *_params.check_compatibility && !menu_state().show_incompatible_nodes;
    }

    /// Returns nullopt if the node should not be shown
    [[nodiscard]] auto make_entry(NodeDefinition const& def, NodesCategory const& category) const -> std::optional<NodeEntry>
    {
        auto entry = NodeEntry{.def = &def, .category = &category, .compatibility = std::nullopt};
        if (_params.check_compatibility && *_params.check_compatibility)
        {
            entry.compatibility = (*_params.check_compatibility)(def, category);
            if (!entry.compatibility && is_filtering_by_compatibility())
                return std::nullopt;
        }
        return entry;
    }

    template<typename Predicate>
    [[nodiscard]] auto entries(Predicate&& predicate) const -> std::vector<NodeEntry>
    {
        auto res = std::vector<NodeEntry>{};
        for (auto const& category : _categories)
        {
            for (auto const& def : category.definitions())
            {
                if (!predicate(def, category))
                    continue;
                if (auto entry = make_entry(def, category))
                    res.push_back(*entry);
            }
        }
        // Nodes that match exactly come first
        std::stable_sort(res.begin(), res.end(), [](NodeEntry const& a, NodeEntry const& b) {
            return needs_conversion(a) < needs_conversion(b);
        });
        return res;
    }

    static auto needs_conversion(NodeEntry const& entry) -> bool
    {
        return entry.compatibility.has_value() && entry.compatibility->needs_conversion;
    }

    /// Returns true iff the node was selected
    auto selectable(NodeEntry const& entry, bool show_category) -> bool
    {
        auto const& name     = i18n::tr_node(entry.category->name(), entry.def->name());
        bool        selected = false;
        ImGui::PushID(entry.def);
        Cool::ImGuiExtras::disabled_if(_params.maybe_disable(*entry.def, *entry.category), [&]() {
            if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns /* HACK to work around a bug in ImGui (https://github.com/ocornut/imgui/issues/8203)*/))
                selected = true;
            if (show_category)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", i18n::tr_category(entry.category->name()).c_str());
            }
        });
        ImGui::PopID();
        if (selected)
            select(entry);
        return selected;
    }

    void select(NodeEntry const& entry)
    {
        _selection = NodesMenuSelection{
            .def_and_cat   = NodeDefinitionAndCategoryName{*entry.def, entry.category->name()},
            .compatibility = entry.compatibility,
        };
    }

    /// Shows the list of nodes, with exact matches first, then a separator, then the nodes that need a conversion.
    void list(std::vector<NodeEntry> const& entries, bool show_category)
    {
        bool separator_done = false;
        for (auto const& entry : entries)
        {
            if (!separator_done && needs_conversion(entry))
            {
                separator_done = true;
                ImGuiExtras::separator_text(tr("With conversion"));
            }
            if (selectable(entry, show_category))
                return;
        }
    }

    [[nodiscard]] auto selection() const -> std::optional<NodesMenuSelection> const& { return _selection; }
    [[nodiscard]] auto params() const -> NodesMenuParams const& { return _params; }
    [[nodiscard]] auto categories() const -> std::vector<NodesCategory> const& { return _categories; }

private:
    NodesMenuParams const&            _params;
    std::vector<NodesCategory> const& _categories;
    std::optional<NodesMenuSelection> _selection{};
};

void imgui_category_nodes(MenuContext& ctx, NodesCategory const& category)
{
    auto const entries = ctx.entries([&](NodeDefinition const&, NodesCategory const& cat) { return &cat == &category; });
    if (entries.empty())
        return;
    ImGui::PushStyleColor(ImGuiCol_Text, category.config().color().as_ImColor().Value);
    ImGuiExtras::separator_text(i18n::tr_category(category.name()));
    ImGui::PopStyleColor();
    ImGui::PushID(&category);
    ctx.list(entries, false);
    ImGui::PopID();
}

auto category_has_visible_nodes(MenuContext const& ctx, NodesCategory const& category) -> bool
{
    return std::any_of(category.definitions().begin(), category.definitions().end(), [&](NodeDefinition const& def) {
        return ctx.make_entry(def, category).has_value();
    });
}

void imgui_tab_by_type(MenuContext& ctx)
{
    auto const& layout          = nodes_menu_layout();
    auto        used_categories = std::vector<NodesCategory const*>{};

    auto const group_menu = [&](std::string const& name, std::vector<NodesCategory const*> const& categories) {
        if (std::none_of(categories.begin(), categories.end(), [&](NodesCategory const* cat) { return category_has_visible_nodes(ctx, *cat); }))
            return;
        constrain_next_menu_size();
        if (ImGui::BeginMenu(name.c_str()))
        {
            for (auto const* category : categories)
            {
                imgui_category_nodes(ctx, *category);
                if (ctx.selection())
                    break;
            }
            ImGui::EndMenu();
        }
    };

    for (auto const& group : layout.type_groups)
    {
        auto categories = std::vector<NodesCategory const*>{};
        for (auto const& category_name : group.categories)
        {
            auto const it = std::find_if(ctx.categories().begin(), ctx.categories().end(), [&](NodesCategory const& cat) { return cat.name() == category_name; });
            if (it == ctx.categories().end())
                continue;
            categories.push_back(&*it);
            used_categories.push_back(&*it);
        }
        group_menu(tr(group.name), categories);
        if (ctx.selection())
            return;
    }

    // Categories that are not listed in the layout file (e.g. new categories created by the user) each get their own menu
    for (auto const& category : ctx.categories())
    {
        if (std::find(used_categories.begin(), used_categories.end(), &category) != used_categories.end())
            continue;
        group_menu(i18n::tr_category(category.name()), {&category});
        if (ctx.selection())
            return;
    }
}

void imgui_tab_by_task(MenuContext& ctx)
{
    for (auto const& task : nodes_menu_layout().tasks)
    {
        auto const entries = ctx.entries([&](NodeDefinition const& def, NodesCategory const& category) {
            return task_contains(task, category.name(), def.name());
        });
        if (entries.empty())
            continue;
        constrain_next_menu_size();
        if (ImGui::BeginMenu(i18n::tr_task(task.id).c_str()))
        {
            ImGui::PushID(&task);
            ctx.list(entries, true);
            ImGui::PopID();
            ImGui::EndMenu();
        }
        if (ctx.selection())
            return;
    }
}

auto split_words(std::string const& str) -> std::vector<std::string>
{
    auto res  = std::vector<std::string>{};
    auto word = std::string{};
    for (char const c : str)
    {
        if (c == ' ' || c == ',' || c == '/' || c == '(' || c == ')' || c == '-' || c == '_')
        {
            if (!word.empty())
                res.push_back(std::move(word));
            word.clear();
            continue;
        }
        word += c;
    }
    if (!word.empty())
        res.push_back(std::move(word));
    return res;
}

/// Lower is better. nullopt if it doesn't match at all.
auto search_score(std::vector<std::string> const& query_words, std::string const& filter, NodeDefinition const& def, NodesCategory const& category) -> std::optional<int>
{
    auto const name            = i18n::to_lower_utf8(i18n::tr_node(category.name(), def.name()));
    auto const english_name    = i18n::to_lower_utf8(def.name());
    auto const category_name   = i18n::to_lower_utf8(i18n::tr_category(category.name()) + " " + category.name());
    auto       other_words     = i18n::to_lower_utf8(i18n::node_keywords(category.name(), def.name()));
    for (auto const& task : nodes_menu_layout().tasks)
    {
        if (task_contains(task, category.name(), def.name()))
            other_words += " " + i18n::to_lower_utf8(i18n::tr_task(task.id) + " " + i18n::task_keywords(task.id));
    }

    auto const lower_filter = i18n::to_lower_utf8(filter);
    if (name.starts_with(lower_filter) || english_name.starts_with(lower_filter))
        return 0;
    if (name.find(lower_filter) != std::string::npos || english_name.find(lower_filter) != std::string::npos)
        return 1;

    // Every word of the query must be found somewhere
    bool all_words_in_name = true;
    bool all_words_found   = true;
    for (auto const& word : query_words)
    {
        bool const in_name = name.find(word) != std::string::npos || english_name.find(word) != std::string::npos;
        all_words_in_name &= in_name;
        all_words_found &= in_name || category_name.find(word) != std::string::npos || other_words.find(word) != std::string::npos;
    }
    if (all_words_in_name)
        return 2;
    if (all_words_found)
        return 3;

    // Tolerate typos in the English name
    if (internal::name_matches_filter(def.name(), filter))
        return 4;
    return std::nullopt;
}

void imgui_search_results(MenuContext& ctx)
{
    auto const& filter      = ctx.params().filter;
    auto const  query_words = split_words(i18n::to_lower_utf8(filter));

    struct Result {
        NodeEntry entry;
        int       score;
    };
    auto results = std::vector<Result>{};
    for (auto const& category : ctx.categories())
    {
        for (auto const& def : category.definitions())
        {
            auto const score = search_score(query_words, filter, def, category);
            if (!score)
                continue;
            if (auto entry = ctx.make_entry(def, category))
                results.push_back({*entry, *score});
        }
    }
    std::stable_sort(results.begin(), results.end(), [](Result const& a, Result const& b) {
        auto const conv_a = MenuContext::needs_conversion(a.entry);
        auto const conv_b = MenuContext::needs_conversion(b.entry);
        if (conv_a != conv_b)
            return conv_a < conv_b;
        return a.score < b.score;
    });

    if (results.empty())
    {
        ImGui::TextDisabled("%s", tr("No node found"));
        return;
    }

    if (ctx.params().select_first)
    {
        ctx.select(results.front().entry);
        return;
    }

    auto entries = std::vector<NodeEntry>{};
    entries.reserve(results.size());
    for (auto const& result : results)
        entries.push_back(result.entry);

    auto const max_height = ImGui::GetMainViewport()->WorkSize.y * 0.5f;
    auto const height     = std::min(max_height, static_cast<float>(entries.size()) * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y);
    if (ImGui::BeginChild("##search_results", ImVec2{std::max(ImGui::GetContentRegionAvail().x, 300.f), height}))
        ctx.list(entries, true);
    ImGui::EndChild();
}

} // namespace

auto NodesLibrary::imgui_nodes_menu(NodesMenuParams const& params) const -> std::optional<NodesMenuSelection>
{
    auto ctx = MenuContext{params, _categories};

    if (params.menu_just_opened)
        menu_state().show_incompatible_nodes = false;

    if (params.check_compatibility && *params.check_compatibility)
    {
        ImGuiExtras::toggle(tr("Show all nodes"), &menu_state().show_incompatible_nodes);
        ImGuiExtras::help_marker(tr("By default, only the nodes that can be plugged into the link you are dragging are shown."));
    }

    if (!params.filter.empty())
    {
        imgui_search_results(ctx);
        return ctx.selection();
    }

    if (ImGui::BeginTabBar("##nodes_menu_tabs"))
    {
        auto const tab = [&](const char* label, MenuTab which) {
            auto const flags = params.menu_just_opened && menu_state().tab == which ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if (ImGui::BeginTabItem(label, nullptr, flags))
            {
                if (!params.menu_just_opened)
                    menu_state().tab = which;
                if (which == MenuTab::ByType)
                    imgui_tab_by_type(ctx);
                else
                    imgui_tab_by_task(ctx);
                ImGui::EndTabItem();
            }
        };
        tab(i18n::window_title("By type").c_str(), MenuTab::ByType);
        tab(i18n::window_title("By task").c_str(), MenuTab::ByTask);
        ImGui::EndTabBar();
    }

    return ctx.selection();
}

void NodesLibrary::add_definition(
    NodeDefinition const&                 definition,
    std::string const&                    category_name,
    std::function<NodesCategory()> const& make_category // We take a function because we want to delay the creation of the category until we are sure we need to create one, which is rare (and otherwise would cause warning when two folders correspond to the same category, but only one of them contains the _category_config.json, which happens when reading nodes from both the app's root folder and the user-data folder)
)
{
    // Add definition to the corresponding category if it exists
    for (auto& category : _categories)
    {
        if (category.name() != category_name)
            continue;

        category.definitions().push_back(definition);
        category.sort();
        return;
    }

    // Add new category if not found
    _categories.push_back(make_category());
    _categories.back().definitions().push_back(definition);
    std::sort(
        _categories.begin(), _categories.end(),
        [](NodesCategory const& c1, NodesCategory const& c2) {
            return c1.order() < c2.order();
        }
    );
}

void NodesLibrary::remove_definition(NodeDefinitionIdentifier const& identifier)
{
    auto* category = get_category(identifier.category_name);
    if (!category)
        return;

    // Remove the node from the definitions
    std::erase_if(category->definitions(), [&](NodeDefinition const& def) { return def.name() == identifier.definition_name; });

    // Remove the category from _categories if is now empty.
    if (category->definitions().empty())
    {
        std::erase_if(_categories, [&](NodesCategory const& category) {
            return category.name() == identifier.category_name;
        });
    }
}

auto NodesLibrary::is_empty() const -> bool
{
    return _categories.empty();
}

} // namespace Cool