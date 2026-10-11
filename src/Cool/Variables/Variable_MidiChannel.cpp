#include "Variable_MidiChannel.h"
#include "Cool/i18n/i18n.hpp"
#include <imgui.h>
#include "Cool/ImGui/IcoMoonCodepoints.h"
#include "Cool/ImGui/ImGuiExtras.h"
#include "Cool/Midi/MidiManager.h"

namespace Cool {

auto imgui_widget(Variable<MidiChannel>& var) -> bool
{
    if (ImGuiExtras::button_with_text_icon(ICOMOON_COG))
        midi_manager().open_config_window();
    ImGui::SetItemTooltip("%s", Cool::tr("Open MIDI config to find out which Index corresponds to which knob / slider / button on your controller."));

    bool b = false;
    b |= ImGui::InputScalar(fmt::format("{} {}###{} Index", i18n::tr_input(var.name()), tr("Index"), var.name()).c_str(), ImGuiDataType_S32, &var.value().index);
    b |= imgui_widget(fmt::format("{} {}###{} Kind", i18n::tr_input(var.name()), tr("Kind"), var.name()).c_str(), var.value().kind);
    return b;
}

auto imgui_widget(VariableMetadata<MidiChannel>&) -> bool
{
    return false;
}

} // namespace Cool