#include "APClient.h"
#include "APHints.h"

namespace APHints
{
    // Currently missing most of the networking to do hints properly
    // The potential expensive operation is the initial !hint due to server notices instead of JSON
    // Hopefully in the future use _read_hints_{team}_{slot}
    //  https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/network%20protocol.md#get
    //  https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/network%20protocol.md#Hint

    bool devMode = APClient::devMode;

    bool init = false; // If !hint needs to be sent
    bool hintHideChecked = true;
    bool hintOwnLocationsOnly = false;

    // AP_HintMessage passes players as stringified names instead of player ID. Good and bad.
    std::vector<AP_HintMessage> Hints;
    std::vector<int> HintedIDs;

    auto& recvIDs = APClient::recvIDs;
    auto& item_ap_id_to_name = APClient::item_ap_id_to_name;
    auto& location_name_to_id = APClient::location_name_to_id;

    bool operator==(const AP_HintMessage& hintA, const AP_HintMessage& hintB)
    {
        // Exclude "checked"
        return (hintA.item == hintB.item && hintA.location == hintB.location
                && hintA.sendPlayer == hintB.sendPlayer && hintA.recvPlayer == hintB.recvPlayer);
    }

    void reset()
    {
        init = false;
        Hints.clear();
        HintedIDs.clear();
    }

    void handleHintMessage(const AP_HintMessage& recvHint)
    {
        if (recvHint.sendPlayer != APClient::getSlotName() && recvHint.recvPlayer != APClient::getSlotName())
            return;

        for (AP_HintMessage &hint : Hints)
        {
            if (hint == recvHint)
            {
                hint.checked = recvHint.checked;
                return;
            }
        }

        if (recvHint.sendPlayer == APClient::getSlotName())
        {
            // TODO: ID Remaps
            auto itemID = location_name_to_id[recvHint.location] / 10;

            if (std::find(HintedIDs.begin(), HintedIDs.end(), itemID) == HintedIDs.end())
                HintedIDs.push_back(itemID);
        }

        Hints.push_back(recvHint);
    }

    void refreshHints()
    {
        // On item receive -> checked
        // On location checked -> checked
    }

    std::string getHintedForID(int songID)
    {
        std::string out = "";


    }

    void ImGuiTab()
    {
        //if (!APClient::devMode) return;

        if (ImGui::BeginTabItem("Hints")) {
            if (!init)
            {
                AP_Say("!hint");
                init = true;
            }

            ImGui::Checkbox("Hide checked", &hintHideChecked);
            ImGui::Checkbox("Only show my checks", &hintOwnLocationsOnly);

            ImGui::Text("%i Hints", Hints.size());

            //if (ImGui::BeginChild("tableHintsContainer", ImVec2(0, 300))) {
            if (ImGui::BeginTable("tableHints", 5,
                ImGuiTableFlags_BordersInner | ImGuiTableFlags_Hideable | ImGuiTableFlags_HighlightHoveredColumn |
                ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit
            ))
            {
                ImGui::TableSetupColumn(" ");
                ImGui::TableSetupColumn("Finder");
                ImGui::TableSetupColumn("Receiver");
                ImGui::TableSetupColumn("Item");
                ImGui::TableSetupColumn("Location");
                ImGui::TableHeadersRow();

                int uid = 0;
                for (const AP_HintMessage& hint : Hints)
                {
                    if (hintHideChecked && hint.checked)
                        continue;

                    bool isMyCheck = hint.sendPlayer.compare(APClient::getSlotName()) == 0;

                    if (hintOwnLocationsOnly && !isMyCheck)
                        continue;

                    ImGui::TableNextRow();

                    uid += 1;
                    ImGui::PushID(uid);

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", hint.checked ? "X" : " ");

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", hint.sendPlayer.c_str());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%s", hint.recvPlayer.c_str());

                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%s", hint.item.c_str());

                    ImGui::TableSetColumnIndex(4);

                    // TODO: ID Remaps
                    auto locID = location_name_to_id[hint.location.c_str()];
                    auto itemName = item_ap_id_to_name[(locID / 10) * 10];

                    bool haveItem = isMyCheck && std::find(recvIDs.begin(), recvIDs.end(), locID / 10) != recvIDs.end();

                    if (haveItem)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

                    ImGui::Text("%s", hint.location.c_str());

                    if (haveItem)
                        ImGui::PopStyleColor();

                    if (isMyCheck && !haveItem)
                    {
                        if (ImGui::BeginPopupContextItem("##xx"))
                        {
                            if (ImGui::MenuItem("Hint this song##xx"))
                                AP_Say("!hint " + itemName);

                            ImGui::EndPopup();
                        }
                    }

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

            //ImGui::EndChild(); }

            ImGui::EndTabItem();
        }
    }
}