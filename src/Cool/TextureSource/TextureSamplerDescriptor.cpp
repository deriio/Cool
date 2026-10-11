#include "TextureSamplerDescriptor.h"
#include "Cool/i18n/i18n.hpp"
#include "Cool/ImGui/ImGuiExtras.h"
#include "Cool/NfdFileFilter/NfdFileFilter.h"

namespace Cool {

auto TextureSamplerDescriptor::imgui_widget() -> bool
{
    bool b = false;
    b |= ImGui::Combo(Cool::i18n::label("Repeat Mode").c_str(), reinterpret_cast<int*>(&repeat_mode), "None\0Mirror\0Mosaic\0Clamp\0\0");

    int mode = interpolation_mode == glpp::Interpolation::NearestNeighbour ? 0 : 1;
    if (ImGui::Combo(Cool::i18n::label("Interpolation Mode").c_str(), &mode, "Nearest Neighbour\0Linear\0\0"))
    {
        interpolation_mode = mode == 0
                                 ? glpp::Interpolation::NearestNeighbour
                                 : glpp::Interpolation::Linear;

        b = true;
    }
    return b;
}

} // namespace Cool