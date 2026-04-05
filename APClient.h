#pragma once
#include "pch.h"
#include <Archipelago.h>

namespace APClient
{
    extern int clearGrade;
    extern std::vector<int> CheckedLocations;

    void reset();

    void FromSlot_victoryID(int);
    void FromSlot_scoreGradeNeeded(int);
    void FromSlot_leekWinCount(int);
    void FromSlot_progHP(int);

    void ItemClear();
    void ItemRecv(int64_t, bool);
    void LocationChecked(int64_t);
    void LocationSend(int64_t pvID);

    void LogAppend(const std::string& text);
    void CheckMessages();

    void RecvDeath(std::string, std::string);
    void SendDeath();


    bool LoadDatapackage();
    void ImGuiTab();
}

