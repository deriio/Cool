#include "TextureDescriptor.h"
#include "Cool/i18n/i18n.hpp"

namespace Cool {

auto TextureDescriptor::imgui_widget() -> bool
{
    bool b = false;
    b |= ::Cool::imgui_widget(source);
    ImGui::NewLine();
    ImGui::SeparatorText(Cool::tr("Image options"));
    b |= sampler.imgui_widget();
    return b;
}

} // namespace Cool