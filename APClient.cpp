#include "APClient.h"
#include "APDeathLink.h"
#include "APGUI.h"
#include "APHints.h"
#include "APIDHandler.h"
#include "APReload.h"
#include "APSettings.h"
#include "APTraps.h"

namespace APClient
{
    const char* GameName = "Hatsune Miku Project Diva Mega Mix+";

    bool devMode = false;

    // Any char where a string makes sense is for ImGui::InputText without using ImGui's stdlib string.

    char slotName[17] = "Player1"; // Slot names cap at 16 characters + terminator
    char slotServer[128] = "archipelago.gg:38281";
    bool hideServer = false;
    char slotPassword[128] = ""; // No password cap?

    char say[256] = ""; // Client -> Server
    std::string APLog = ""; // Various memory management concerns.
    bool APLogCopyMode = false;

    // Hold server data messaging
    std::vector<std::pair<AP_GetServerDataRequest, std::function<void(std::string raw)>>> DataRequests;

    // Datapackage
    std::string DatapackageChecksum;
    bool datapackageLoaded = false;

    json datapackageJSON;
    std::unordered_map<std::string, int64_t> item_name_to_ap_id;
    std::unordered_map<int64_t, std::string> item_ap_id_to_name;
    std::unordered_map<std::string, int64_t> location_name_to_id;
    std::unordered_map<int64_t, std::string> location_id_to_name;

    // TODO: Relocate?
    int clearGrade = 2;
    char diffs[5][10] = {"Cheap", "Standard", "Great", "Excellent", "Perfect"};

    // Archipelago state

    AP_RoomInfo RoomInfo;

    json slotData;
    std::vector<int64_t> seedIDs = {}; // Song IDs (Love is War [1] = 1) that are part of the seed
    std::vector<int64_t> recvIDs = {}; // Song IDs (Love is War [1] = 1) received as items
    std::vector<int64_t> missingIDs = {}; // Song IDs (Love is War [1] = 1) not yet received
    std::vector<int64_t> CheckedLocations = {}; // Love is War [1] = 10, 11

    int64_t victoryID = 0; // Song ID * 10, Love is War [1] = 10
    GoalMode goalMode = GoalMode::Leeks;
    int leekHave = 0;
    int leekNeed = 0;

    int &progHPReceived = APDeathLink::HPreceived;
    int &progHPtemp = APDeathLink::HPtemp;
    int &progHPTotal = APDeathLink::HPdenominator;

    void config(const toml::table& settings)
    {
        if (AP_GetConnectionStatus() != AP_ConnectionStatus::Disconnected)
            return;

        toml::table section;
        if (settings.contains("client") && settings["client"].is_table())
            section = *settings["client"].as_table();

        std::string config_name = section["slot_name"].value_or("Player1");
        std::string config_server = section["slot_server"].value_or("archipelago.gg:38281");
        std::string config_pass = section["slot_password"].value_or("");

        std::size_t slotName_len = min(config_name.size(), sizeof(slotName) - 1);
        std::size_t slotServer_len = min(config_server.size(), sizeof(slotServer) - 1);
        std::size_t slotPassword_len = min(config_pass.size(), sizeof(slotPassword) - 1);

        strncpy(slotName, config_name.c_str(), slotName_len);
        strncpy(slotServer, config_server.c_str(), slotServer_len);
        hideServer = section["slot_server_hide"].value_or(false);
        strncpy(slotPassword, config_pass.c_str(), slotPassword_len);

        slotName[slotName_len] = '\0';
        slotServer[slotServer_len] = '\0';
        slotPassword[slotPassword_len] = '\0';
    }

    void save(toml::table& settings)
    {
        toml::table config;
        config.insert("slot_name", slotName);
        config.insert("slot_server", slotServer);
        config.insert("slot_server_hide", hideServer);
        config.insert("slot_password", slotPassword);

        settings.insert("client", config);
    }

    char* getSlotName()
    {
        return slotName;
    }

    void SlotData_LeekHave(int leekWinCount)
    {
        leekNeed = leekWinCount;
    }

    void SlotData_ProgHP(int progHP)
    {
        progHPTotal = 1 + progHP;
    }

    void SlotData_Grade(int grade)
    {
        clearGrade = grade;
    }

    void SlotData_VictoryID(int id)
    {
        victoryID = id;
    }

    void SlotData_FinalSongs(std::string raw)
    {
        auto final = json::parse(raw);
        if (final.is_array())
        {
            seedIDs = final.get<std::vector<int64_t>>();
            std::sort(seedIDs.begin(), seedIDs.end());
        }

        ImGui::SetWindowFocus("Client");
        UpdateMissing();
        UpdateTags();
        APReload::run();
        APTraps::reset();
    }

    void ItemClear()
    {
        APLogger::print("Client: reset\n");
        reset();
    }

    void RecvBounce(AP_Bounce bouncePacket)
    {
        json data = json::parse(bouncePacket.data);

        if (bouncePacket.tags->front() == "TrapLink") {
            std::string trap = data.value("trap_name", "");
            APTraps::recvTrapLink(trap);
        }
        else if (bouncePacket.tags->front() == "DeathLink") {
            RecvDeath(data.value("source", ""), data.value("cause", ""));
        }
    }

    void ItemRecv(int64_t itemID, bool notify)
    {
        switch (itemID) {
        case 1:
            leekHave += 1;
            UpdateMissing();
            break;
        case 2:
            break; // Filler
        case 3:
            APDeathLink::recvHP();
            break;
        case static_cast<int64_t>(APTraps::TrapID::Hidden):
            if (!notify) return;
            APTraps::touchHidden();
            APTraps::sendTrapLink("Hidden Trap");
            break;
        case static_cast<int64_t>(APTraps::TrapID::Sudden):
            if (!notify) return;
            APTraps::touchSudden();
            APTraps::sendTrapLink("Sudden Trap");
            break;
        case static_cast<int64_t>(APTraps::TrapID::Stutter):
            if (!notify) return;
            APTraps::touchStutter();
            APTraps::sendTrapLink("Stutter Trap");
            break;
        case static_cast<int64_t>(APTraps::TrapID::Icon):
            if (!notify) return;
            APTraps::touchIcon();
            APTraps::sendTrapLink("Icon Trap");
            break;
        default:
            if (itemID >= 10) {
                PushRecvID(itemID / 10);
                APHints::updateByItemName(item_ap_id_to_name[itemID]);
            }
        }

        APIDHandler::updateTrackerLine();
    }

    void LocationChecked(int64_t locationID)
    {
        if (std::find(CheckedLocations.begin(), CheckedLocations.end(), locationID) != CheckedLocations.end())
            return;

        CheckedLocations.push_back(locationID);
        APIDHandler::updateTrackerLine();
    }

    void connect()
    {
        AP_Shutdown();

        if (AP_GetConnectionStatus() == AP_ConnectionStatus::Disconnected)
        {
            AP_Init(slotServer, GameName, slotName, slotPassword);
            AP_RegisterBouncedCallback(RecvBounce);

            AP_SetItemClearCallback(ItemClear);
            AP_SetItemRecvCallback(ItemRecv);
            AP_SetLocationCheckedCallback(LocationChecked);

            AP_RegisterSlotDataIntCallback("victoryID", SlotData_VictoryID);
            AP_RegisterSlotDataIntCallback("scoreGradeNeeded", SlotData_VictoryID);
            AP_RegisterSlotDataIntCallback("leekWinCount", SlotData_LeekHave);
            AP_RegisterSlotDataIntCallback("progHP", SlotData_ProgHP);
            AP_RegisterSlotDataRawCallback("finalSongIDs", SlotData_FinalSongs);

            AP_Start();
        }
    }

    void reset()
    {
        datapackageLoaded = false;

        DataRequests.clear();

        slotData.clear();

        seedIDs.clear();
        recvIDs.clear();
        missingIDs.clear();
        CheckedLocations.clear();

        say[0] = '\0';
        APLog.clear();

        clearGrade = 2;
        victoryID = 0;

        goalMode = GoalMode::Leeks;
        leekHave = 0;
        leekNeed = 0;

        progHPReceived = 1;
        progHPtemp = 0;
        progHPTotal = 1;

        APIDHandler::reset();
        APHints::reset();
    }

    void PushRecvID(int64_t songID)
    {
        if (std::find(recvIDs.begin(), recvIDs.end(), songID) != recvIDs.end() ||
            std::find(seedIDs.begin(), seedIDs.end(), songID) == seedIDs.end())
            return;

        recvIDs.push_back(songID);

        UpdateMissing();
    }

    void UpdateMissing()
    {
        if (victoryID != 0 && leekHave >= leekNeed)
            PushRecvID(victoryID / 10);

        // TODO: Works from a copy to preserve receive order for the Tracker.
        // Tracking the order can be moved higher to APClient::ItemRecv.
        // set_symmetric_difference items need to be presorted. If missingIDs is wrong Freeplay breaks.
        auto _recvIDs = recvIDs;
        std::sort(_recvIDs.begin(), _recvIDs.end());

        missingIDs.clear();
        std::set_symmetric_difference(
            seedIDs.begin(), seedIDs.end(),
            _recvIDs.begin(), _recvIDs.end(),
            std::back_inserter(missingIDs)
        );
    }

    void LocationSend(int64_t pvID)
    {
        // There is no current way to send an arbitrary ID so limit to received ones. Usually what's on the Tracker.
        // Specifically to prevent misfires of the AP and Tutorial songs but may benefit Freeplay.
        if (std::find(recvIDs.begin(), recvIDs.end(), pvID) == recvIDs.end() /*&& !devMode*/) {
            APLogger::print("Client: Skip location send for ID %i (not received)\n", pvID);
            return;
        }

        if (pvID == victoryID / 10)
        {
            APLogger::print("Client: Sending goal completion from ID %i\n", pvID);
            AP_StoryComplete();
        }
        else {
            APLogger::print("Client: Sending locations for ID %i\n", pvID);

            // Song locations are in pairs
            int64_t APID = pvID * 10;

            std::set<int64_t> locs{ APID, APID + 1 };
            AP_SendItem(locs);

            APHints::updateSentLocations(std::array<int64_t, 2>{ APID, APID + 1});
        }
    }

    void LogAppend(const std::string &text)
    {
        if (APLog.length() > 0)
            APLog += "\n";
        APLog += text;
    }

    // Server messages

    void DataRequest(const std::string key, std::function<void(std::string raw)> callback)
    {
        std::pair<AP_GetServerDataRequest, std::function<void(std::string raw)>> pair;

        AP_GetServerDataRequest req;
        req.key = key;
        req.value = new std::string;
        req.type = AP_DataType::Raw;
        req.status = AP_RequestStatus::Pending;

        pair.first = req;
        pair.second = callback;

        DataRequests.push_back(pair);

        AP_GetServerData(&DataRequests.back().first);
    }

    void CheckMessages()
    {
        if (AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated)
            return;

        if (DataRequests.size() > 0) {
            for (auto &[req, callback] : DataRequests) {
                if (req.status == AP_RequestStatus::Pending) // What about stuck pending?
                    continue;

                //if (req.status == AP_RequestStatus::Error)

                if (req.status == AP_RequestStatus::Done) {
                    std::string value = *(std::string*)req.value;
                    if (req.type == AP_DataType::Raw)
                        callback(value);
                }

                free(req.value);
                DataRequests.clear(); // TODO: conditional remove
            }
        }

        // No potential crashes here.
        LoadDatapackage();

        if (AP_IsMessagePending()) {
            AP_Message* msg = AP_GetLatestMessage();
            std::string hold_msg;

            // Not enough tangible info for recv/send
            /*if (msg->type == AP_MessageType::ItemRecv) {
                auto recv_msg = static_cast<AP_ItemRecvMessage*>(msg);
                hold_msg = recv_msg->sendPlayer + " sent " + recv_msg->item;
            }
            else if (msg->type == AP_MessageType::ItemSend) {
                auto send_msg = static_cast<AP_ItemSendMessage*>(msg);
                hold_msg = send_msg->recvPlayer + " received " + send_msg->item;
            }
            else*/
            if (msg->type == AP_MessageType::Hint)
            {
                AP_HintMessage* h_msg = static_cast<AP_HintMessage*>(msg);
                APHints::handleHintMessage(*h_msg);
                hold_msg = h_msg->text;
            }
            else {
                hold_msg = msg->text;
            }

            APLogger::print("%s\n", hold_msg.c_str());
            LogAppend(hold_msg);

            AP_ClearLatestMessage();
        }
    }

    void RecvDeath(std::string src, std::string cause)
    {
        APDeathLink::run(true);
    }

    void UpdateTags()
    {
        std::vector<std::string> tags;

        if (APDeathLink::death_link)
            tags.push_back("DeathLink");

        if (APTraps::trap_link)
            tags.push_back("TrapLink");

        AP_UpdateTags(tags);
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

            if (DatapackageChecksum.compare(it->second) != 0)
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

        std::ifstream datapackage(BasePath / ".datapkg-cache" / ("HatsuneMikuProjectDivaMegaMix-" + DatapackageChecksum + ".json"));

        if (!datapackage.is_open())
            return false;

        // TODO: try catch?
        datapackageJSON = json::parse(datapackage);

        item_name_to_ap_id = datapackageJSON["item_name_to_id"].get<std::unordered_map<std::string, int64_t>>();
        for (auto& el : datapackageJSON["item_name_to_id"].items())
            item_ap_id_to_name[(int64_t)el.value()] = el.key();

        location_name_to_id = datapackageJSON["location_name_to_id"].get<std::unordered_map<std::string, int64_t>>();
        for (auto& el : datapackageJSON["location_name_to_id"].items())
            location_id_to_name[(int64_t)el.value()] = el.key();

        datapackageLoaded = true;

        return true;
    }

    void ImGuiTab()
    {
        if (AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated)
        {
            if (AP_IsInit())
                ImGui::BeginDisabled();

            ImGui::InputText("Slot Name", slotName, sizeof(slotName));
            ImGui::InputText("Server", slotServer, sizeof(slotServer), !hideServer ? 0 : ImGuiInputTextFlags_Password);
            if (ImGui::BeginPopupContextItem("##hideServer")) {
                ImGui::MenuItem("Hide server", nullptr, &hideServer);
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            HelpMarker(
                "Server address must have the port number.\nRight-click input to toggle visibility."
                "\n\nExample addresses:\n archipelago.gg:38281\n localhost:38281\n 127.0.0.1:38281"
            );

            ImGui::InputText("Password", slotPassword, sizeof(slotPassword), ImGuiInputTextFlags_Password);

            if (AP_IsInit())
                ImGui::EndDisabled();

            bool disconnected = AP_GetConnectionStatus() == AP_ConnectionStatus::Disconnected;
            bool refused = AP_GetConnectionStatus() == AP_ConnectionStatus::ConnectionRefused;

            if (disconnected || refused)
                if (!AP_IsInit()) {
                    if (ImGui::Button("Connect")) {
                        connect();
                        if (ImGui::GetIO().KeyShift)
                            APSettings::save();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Shift+Click to save connection information.");
                        ImGui::EndTooltip();
                    }
                }
                else {
                    if (ImGui::Button("Cancel"))
                        AP_Shutdown();
                    ImGui::SameLine();
                    ImGui::Text(refused ? "Wrong Name/Server/Password" : "Connecting...");
                }
        }
        else
        {
            if (ImGui::Button("Disconnect")) {
                AP_Shutdown();
                reset();

                if (!ImGui::GetIO().KeyShift)
                    APReload::run();
            }

            ImGui::SameLine();
            ImGui::Text("Connected as %s", slotName);

            ImGui::SameLine();
            if (ImGui::Button("Reload"))
                APReload::run();

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Reload key: %s", APReload::reloadVal);
                ImGui::EndTooltip();
            }

            ImGui::Separator();

            ImGui::BeginChild("APLog", ImVec2(0, ImGui::GetContentRegionAvail().y - (ImGui::GetFrameHeightWithSpacing() * 1.2f)));

            if (APLogCopyMode) {
                ImGui::InputTextMultiline(
                    "##APLogMulti",
                    (char*)APLog.c_str(),
                    APLog.size() + 1,
                    ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y),
                    ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap
                );
            }
            else {
                ImGui::BeginChild("APLogUnformatted");

                ImGui::PushTextWrapPos(0.0f);

                std::istringstream stream(APLog);
                std::string line;

                while (std::getline(stream, line)) {
                    ImGui::TextUnformatted(line.c_str());
                }

                ImGui::PopTextWrapPos();

                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
                    ImGui::SetScrollHereY(1.0f);

                ImGui::EndChild();
            }

            if (ImGui::BeginPopupContextItem("##xx")) {
                ImGui::MenuItem("Copy mode (no autoscroll)", nullptr, &APLogCopyMode);
                if (ImGui::MenuItem("Clear")) APLog.clear();
                ImGui::EndPopup();
            }

            ImGui::EndChild();

            ImGui::Separator();

            static bool refocus = false;
            if (refocus) {
                refocus = false;
                ImGui::SetKeyboardFocusHere();
            }

            if (ImGui::InputText("##APsay", say, sizeof(say), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                refocus = true;
                if (strlen(say) > 0) {
                    AP_Say(std::string(say));
                    say[0] = '\0';
                }
            }

            ImGui::SameLine();
            ImGui::Text("%d / %d Leeks", leekHave, leekNeed);

            // TODO: Relocate
            std::string goalTip = "Goal song: " + item_ap_id_to_name[victoryID] + "\n"
                                    "Clear grade needed: " + (std::string)diffs[clearGrade - 1];

            ImGui::SameLine();
            HelpMarker(goalTip.c_str());
        }
    }
}