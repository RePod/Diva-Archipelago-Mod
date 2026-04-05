#include "APClient.h"
#include "APDeathLink.h"
#include "APIDHandler.h"
#include "APReload.h"
#include "APTraps.h"

namespace APClient
{
    const char* GameName = "Hatsune Miku Project Diva Mega Mix+";

    // Any char where a string makes sense is for ImGui::InputText without using ImGui's stdlib string.

    char slotName[17] = "Player1"; // Slot names cap at 16 characters + terminator
    char slotServer[128] = "archipelago.gg:38281";
    char slotPassword[128] = ""; // No password cap?

    char say[256] = ""; // Client -> Server
    std::string APLog = ""; // Various memory management concerns.

    // TODO: ID remaps
    AP_RoomInfo RoomInfo;
    std::string DatapackageChecksum;
    bool datapackageLoaded = false;

    std::filesystem::path LocalPath = std::filesystem::current_path();
    nlohmann::json_abi_v3_12_0::json datapackageJSON;
    std::unordered_map<uint32_t, std::string> item_ap_id_to_name;
    std::vector<int> CheckedLocations = {}; // Love is War [1] = 10, 11.

    // TODO: Relocate?
    bool autoRemove = true;

    int clearGrade = 2; // Standard
    char diffs[5][10] = {"Cheap", "Standard", "Great", "Excellent", "Perfect"};

    int victoryID = 0;

    int leekHave = 0;
    int leekNeed = 0;

    int &progHPHave = APDeathLink::HPnumerator;
    int &progHPTotal = APDeathLink::HPdenominator;

    void config(toml::v3::ex::parse_result& data)
    {
        std::string config_name = data["slot_name"].value_or("Player1");
        std::string config_server = data["slot_server"].value_or("archipelago.gg:38281");
        std::string config_pass = data["slot_password"].value_or("");

        strncpy(slotName, config_name.c_str(), config_name.size() + 1);
        strncpy(slotServer, config_server.c_str(), config_server.size() + 1);
        strncpy(slotPassword, config_pass.c_str(), config_pass.size() + 1);
    }

    void connect()
    {
        AP_Shutdown();

        if (AP_GetConnectionStatus() == AP_ConnectionStatus::Disconnected)
        {
            AP_Init(slotServer, GameName, slotName, slotPassword);

            // Requires `death_link` in slot data, not `deathLink`, etc.
            AP_SetDeathLinkSupported(true);
            AP_SetDeathLinkRecvCallback(RecvDeath);

            AP_SetItemClearCallback(ItemClear);
            AP_SetItemRecvCallback(ItemRecv);
            AP_SetLocationCheckedCallback(LocationChecked);

            AP_RegisterSlotDataIntCallback("victoryID", FromSlot_victoryID);
            AP_RegisterSlotDataIntCallback("scoreGradeNeeded", FromSlot_scoreGradeNeeded);
            AP_RegisterSlotDataIntCallback("leekWinCount", FromSlot_leekWinCount);
            AP_RegisterSlotDataIntCallback("progHP", FromSlot_progHP);

            // int slot data
            // autoRemove bool / Handle internally
            // deathLink -> death_link type? / Needs a rename for APCpp
            // deathLink_Amnesty -> death_link_amnesty int / Needs a rename and/or handle internally

            // raw slot data (mix of strings and lists)
            // finalSongIDs list[int] / To know all relevant locations
            // modData dict[str, list[str,int]] / Possibly not needed (for the mod) since dynamic datapackage? Universal Tracker still needs it
            // modRemap
            // victoryLocation str / Possibly not needed in slot data anymore?

            AP_Start();
        }
    }

    void reset()
    {
        datapackageLoaded = false;
        CheckedLocations.clear();

        say[0] = '\0';
        APLog = "";

        clearGrade = 2;
        victoryID = 0;

        leekHave = 0;
        leekNeed = 0;

        progHPHave = 1;
        progHPTotal = 1;

        APIDHandler::reset();
    }

    // Slot data callbacks

    void FromSlot_victoryID(int val)
    {
        // TODO: Not multiply in the APWorld
        victoryID = val / 10;
    }

    void FromSlot_scoreGradeNeeded(int val)
    {
        clearGrade = val;
    }

    void FromSlot_leekWinCount(int val)
    {
        APReload::run();
        leekNeed = val;
    }

    void FromSlot_progHP(int val)
    {
        progHPTotal = val;
    }

    // Server messages

    void ItemClear()
    {
        APLogger::print("Client: reset\n");
        reset();
    }

    void ItemRecv(int64_t itemID, bool notify)
    {
        switch (itemID) {
            case 1:
                leekHave += 1;
                if (leekHave >= leekNeed)
                    APIDHandler::add(victoryID);
                break;
            case 2:
                break; // Filler
            case 3:
                progHPHave += 1;
                break;
            case 4:
                APTraps::touchHidden();
                break;
            case 5:
                APTraps::touchSudden();
                break;
            case 9:
                APTraps::touchIcon();
                break;
            default:
                if (itemID >= 10)
                    APIDHandler::add(itemID / 10);
        }
    }

    void LocationChecked(int64_t locationID)
    {
        CheckedLocations.push_back(locationID);
    }

    void LocationSend(int64_t pvID)
    {
        if (pvID == victoryID)
        {
            AP_StoryComplete();
        }
        else {
            // Song locations are in pairs
            AP_SendItem(pvID * 10);
            AP_SendItem((pvID * 10) + 1);
        }
    }

    void LogAppend(const std::string &text)
    {
        if (APLog.length() > 0)
            APLog += "\n";
        APLog += text;
    }

    void CheckMessages()
    {
        if (AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated)
            return;

        if (AP_IsMessagePending()) {
            AP_Message* msg = AP_GetLatestMessage();
            APLogger::print("%s\n", msg->text.c_str());

            LogAppend(msg->text);

            AP_ClearLatestMessage();
        }
    }

    void RecvDeath(std::string src, std::string cause)
    {
        APDeathLink::run(true);
    }

    void SendDeath()
    {
        if (APDeathLink::deathLinked)
            return;

        // TODO: Slot aliases?
        std::string msg = "The Disappearance of " + (std::string)slotName;

        AP_DeathLinkSend(msg);
    }

    bool LoadDatapackage()
    {
        // Dynamic datapackage lives on.

        if (datapackageLoaded)
            return true;

        if (AP_GetRoomInfo(&RoomInfo) != 0)
            return false;

        auto it = RoomInfo.datapackage_checksums.find(GameName);
        if (it != RoomInfo.datapackage_checksums.end()) {

            APLogger::print("Datapackage checksum: %s\n", it->second.c_str());

            if (DatapackageChecksum.compare(it->second))
            {
                APLogger::print("New datapackage checksum\n");
                item_ap_id_to_name.clear();
            }

            DatapackageChecksum = it->second;
        }
        else {
            APLogger::print("Could not find datapackage checksum in RoomInfo\n");
            return false;
        }

        std::ifstream datapackage(LocalPath / ".datapkg-cache" / ("HatsuneMikuProjectDivaMegaMix-" + DatapackageChecksum + ".json"));

        if (!datapackage.is_open())
            return false;

        // TODO: try catch?
        datapackageJSON = nlohmann::json::parse(datapackage);

        for (auto& el : datapackageJSON["item_name_to_id"].items())
            item_ap_id_to_name[(uint32_t)el.value()] = el.key();

        datapackageLoaded = true;
        return true;
    }

    void ImGuiTab()
    {
        if (ImGui::BeginTabItem("APClient")) {
            if (AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated)
            {
                ImGui::InputText("Slot Name", slotName, sizeof(slotName));
                ImGui::InputText("Server", slotServer, sizeof(slotServer));
                ImGui::InputText("Password", slotPassword, sizeof(slotPassword), ImGuiInputTextFlags_Password);

                if (ImGui::Button("Connect"))
                    connect();

                ImGui::SameLine();
                if (AP_GetConnectionStatus() == AP_ConnectionStatus::ConnectionRefused)
                    ImGui::Text("Wrong Name/Server/Password");

                if (AP_GetConnectionStatus() == AP_ConnectionStatus::Connected)
                    ImGui::Text("Connected");
            }
            else
            {
                LoadDatapackage();

                if (ImGui::Button("Disconnect"))
                {
                    reset();
                    AP_Shutdown();
                }

                ImGui::SameLine();
                ImGui::Text("Connected as %s", slotName);

                ImGui::SameLine();
                if (ImGui::Button("Reload"))
                    APReload::run();

                /*ImGui::SameLine();
                ImGui::Text("IDHandler: %d items/songs", APIDHandler::toggleIDs.size());*/

                /*static int pvID = 1;
                if (ImGui::Button("Send ID"))
                {
                    LocationSend(pvID);
                }
                ImGui::SameLine();
                ImGui::InputInt("##sendID", &pvID, 1, INT_MAX);*/

                /*if (ImGui::Button("Send Death"))
                    SendDeath();*/

                // TODO: Switch to TextWrapped, smart auto scroll to bottom
                ImGui::InputTextMultiline("##APlog", (char *)APLog.c_str(), sizeof(APLog), ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);
                ImGui::SetScrollHereY(1.0f);

                static bool refocus = false;
                if (refocus) {
                    refocus = false;
                    ImGui::SetKeyboardFocusHere();
                }

                if (ImGui::InputText("##APsay", say, sizeof(say), ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    AP_Say(std::string(say));
                    say[0] = '\0';

                    refocus = true;
                }

                ImGui::SameLine();
                ImGui::Text("%d / %d Leeks", leekHave, leekNeed);

                // TODO: Relocate
                static std::string goalTip = "Goal song: " + item_ap_id_to_name[victoryID * 10] +
                                             "\nClear grade needed: " + (std::string)diffs[clearGrade - 1];

                ImGui::SameLine();
                HelpMarker(goalTip.c_str());
            }

            ImGui::EndTabItem();
        }

        /*if (ImGui::BeginTabItem("Datapackage")) {
            if (ImGui::BeginChild("tableContainer", ImVec2(0, 300))) {
                if (ImGui::BeginTable("tableDatapackage", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableSetupColumn("ID");
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    for (const auto& [itemID, itemName] : item_ap_id_to_name) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", itemID);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text(itemName.c_str());
                    }

                    ImGui::EndTable();
                }

                ImGui::EndChild();
            }

            ImGui::EndTabItem();
        }*/
    }
}