static void ReportDialogSnapshot(const char* label) {
    const bool open = DialogMgr::IsDialogOpen();
    const uint32_t sender = DialogMgr::GetDialogSenderAgentId();
    const uint32_t buttonCount = DialogMgr::GetButtonCount();
    FroggyFeatureReport("  %s: dialogOpen=%d sender=%u buttons=%u", label, open ? 1 : 0, sender, buttonCount);
    for (uint32_t i = 0; i < buttonCount && i < 6; ++i) {
        const auto* button = DialogMgr::GetButton(i);
        if (!button) continue;
        FroggyFeatureReport("    dialogButton[%u]: dialog_id=0x%X icon=%u skill=%u", i, button->dialog_id, button->button_icon, button->skill_id);
    }
}
