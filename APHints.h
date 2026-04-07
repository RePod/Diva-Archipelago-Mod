#pragma once
#include "pch.h"

namespace APHints
{
    extern std::vector<int> HintedIDs;

    void reset();
    void handleHintMessage(const AP_HintMessage&);

    void ImGuiTab();
}

