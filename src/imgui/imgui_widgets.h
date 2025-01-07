#pragma once

namespace ImGui
{

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool Link(const char* label, const char* desc)
{
    ImVec2 size = ImGui::CalcTextSize(label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 p2 = ImGui::GetCursorPos();
    bool result = ImGui::InvisibleButton(label, size);
    ImGui::SetCursorPos(p2);

    if (ImGui::IsItemHovered())
    {
        if (desc != nullptr)
        {
            if (ImGui::BeginItemTooltip())
            {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 80.0f);
                ImGui::TextUnformatted(desc);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y + size.y), ImVec2(p.x + size.x, p.y + size.y), ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_CheckMark]));
    }

    ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_CheckMark], "%s", label);

    return result;
}

} // namespace ImGui
