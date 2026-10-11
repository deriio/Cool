#pragma once

#include <utility>
#include <vector>
#include "Cool/ImGui/ImGuiExtras.h"
#include "Cool/Nodes/NodesCategoryConfig.h"
#include "NodeDefinition.h"
#include "NodeDefinitionIdentifier.h"
#include "NodesCategoryConfig.h"
#include "imgui.h"
#include "scope_guard/scope_guard.hpp"

namespace Cool {

namespace internal {
auto name_matches_filter(std::string const& _name, std::string const& filter) -> bool;
} // namespace internal

class NodesCategory {
public:
    NodesCategory(std::string name, NodesCategoryConfig config, int order)
        : _config{std::move(config)}
        , _name{std::move(name)}
        , _order{order}
    {}

    auto definitions() const -> auto const& { return _definitions; }
    auto definitions() -> auto& { return _definitions; }
    auto config() const -> auto const& { return _config; }
    auto config() -> auto& { return _config; }
    auto name() const -> auto const& { return _name; }
    auto order() const -> int { return _order; }
    void sort()
    {
        std::sort(_definitions.begin(), _definitions.end(), [](NodeDefinition const& d1, NodeDefinition const& d2) {
            return d1.name() < d2.name();
        });
    }

private:
    std::vector<NodeDefinition> _definitions{};
    NodesCategoryConfig         _config;
    std::string                 _name{};
    int                         _order{};
};

struct NodeDefinitionAndCategoryName {
    NodeDefinition def;
    std::string    category_name;
};

/// Returns a message if the NodeDefinition should be disabled, explaining why it is disabled. Or nullopt if it is not disabled.
using MaybeDisableNodeDefinition = std::function<std::optional<const char*>(NodeDefinition const&, NodesCategory const&)>;

/// How a node that we are about to create can be plugged into the pin that the user is dragging a link from.
struct NodeCompatibility {
    /// Index of the pin of the new node that should be linked to the dragged pin.
    /// It is an input pin when dragging from an output pin, and an output pin when dragging from an input pin.
    size_t pin_index{0};
    /// True iff the types don't match exactly, and an implicit conversion will be needed.
    bool needs_conversion{false};
};
/// Returns nullopt if the node can't be plugged into the pin we are dragging a link from.
using CheckNodeCompatibility = std::function<std::optional<NodeCompatibility>(NodeDefinition const&, NodesCategory const&)>;

struct NodesMenuSelection {
    NodeDefinitionAndCategoryName    def_and_cat;
    std::optional<NodeCompatibility> compatibility{};
};

struct NodesMenuParams {
    std::string const&                   filter;
    MaybeDisableNodeDefinition const&    maybe_disable;
    CheckNodeCompatibility const*        check_compatibility{nullptr}; // nullptr when we are not dragging a link
    bool                                 select_first{false};
    bool                                 menu_just_opened{false};
};

class NodesLibrary {
public:
    auto get_definition(Cool::NodeDefinitionIdentifier const& id_names) const -> NodeDefinition const* { return internal_get_definition(*this, id_names); }
    auto get_definition(Cool::NodeDefinitionIdentifier const& id_names) -> NodeDefinition* { return internal_get_definition(*this, id_names); }

    auto get_category(std::string const& category_name) const -> NodesCategory const*;
    auto get_category(std::string const& category_name) -> NodesCategory*;

    auto imgui_nodes_menu(NodesMenuParams const&) const -> std::optional<NodesMenuSelection>;
    auto categories() const -> std::vector<NodesCategory> const& { return _categories; }

    void add_definition(NodeDefinition const& definition, std::string const& category_name, std::function<NodesCategory()> const& make_category);

    void remove_definition(NodeDefinitionIdentifier const&);

    auto is_empty() const -> bool;

private:
    template<typename T>
    static auto internal_get_definition(T&& nodes_library, Cool::NodeDefinitionIdentifier const& id_names) -> NodeDefinition*
    {
        for (auto&& category : nodes_library._categories)
        {
            if (category.name() != id_names.category_name)
                continue;

            auto const it = std::find_if(category.definitions().begin(), category.definitions().end(), [&](NodeDefinition const& def) {
                return def.name() == id_names.definition_name;
            });

            if (it != category.definitions().end())
                return &*it;
        }
        return nullptr;
    }

private:
    mutable std::vector<NodesCategory> _categories;
};

} // namespace Cool