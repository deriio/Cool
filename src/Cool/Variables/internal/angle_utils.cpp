#include "angle_utils.h"
#include "Cool/i18n/i18n.hpp"
#include "Cool/ImGui/ImGuiExtras.h"
#include "imgui.h"

namespace Cool::internal {

auto angle_metadata_widget(int& number_of_snaps, float& snaps_offset, bool& always_snap) -> bool
{
    bool b = false;

    b |= ImGui::InputInt(Cool::i18n::label("Number of snaps").c_str(), &number_of_snaps, 0);
    if (number_of_snaps < 1)
        number_of_snaps = 1;

    b |= ImGuiExtras::angle_slider("Snaps offset", &snaps_offset);

    b |= ImGui::Checkbox(Cool::i18n::label("Always snap").c_str(), &always_snap);

    return b;
}

} // namespace Cool::internal