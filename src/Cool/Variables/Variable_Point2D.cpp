#include "Variable_Point2D.h"
#include "Cool/i18n/i18n.hpp"

namespace Cool {

auto imgui_widget(Variable<Point2D>& var) -> bool
{
    return imgui_widget(var.label().c_str(), var.value(), var.metadata().drag_speed);
}

auto imgui_widget(VariableMetadata<Point2D>& meta) -> bool
{
    return ImGui::DragFloat(Cool::i18n::label("Drag speed").c_str(), &meta.drag_speed, 0.0000005f, 0.000001f, FLT_MAX / static_cast<float>(INT_MAX), "%.6f");
}

} // namespace Cool