#include "APClient.h"
#include "APHints.h"
#include "APIDHandler.h"
#include "APReload.h"

namespace APIDHandler
{
	// Internal
	bool exists = false;
	bool freeplay = false;
	bool hide_checked = true;
	bool reloading = false;

	auto &CheckedLocations = APClient::CheckedLocations;
	auto &seedIDs = APClient::seedIDs;
	auto &recvIDs = APClient::recvIDs;
	auto &missingIDs = APClient::missingIDs;
	auto &item_ap_id_to_name = APClient::item_ap_id_to_name;
	int availableLocs = 0; // Calculated on reload

	std::string trackerLine; // Holds the formatted Tracker line

	auto &HintedIDs = APHints::HintedIDs;

	void config(const toml::table& settings)
	{
		toml::table section;
		if (settings.contains("tracker") && settings["tracker"].is_table())
			section = *settings["tracker"].as_table();

		hide_checked = section["hide_checked"].value_or(true);
		APLogger::print("hide_checked: %d\n", hide_checked);
	}

	void save(toml::table& settings)
	{
		toml::table config;
		config.insert("hide_checked", hide_checked);

		settings.insert("tracker", config);
	}

	bool check(std::string& line)
	{
		if (missingIDs.size() == 0 || line.find("pv_") != 0 /*|| AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated*/)
			return true;

		size_t diff_pos = line.find(".difficulty.");
		size_t len_pos = line.rfind(".length");

		if (diff_pos == std::string::npos || len_pos == std::string::npos)
			return true;

		// Naively restrict to pv_#.difficulty.easy.length (difficulty and length already confirmed in the string)
		if (3 != std::count(line.begin(), line.end(), '.'))
			return true;

		size_t start = line.find_first_of("_");
		int pvID = std::stoi(line.substr(start + 1, line.find_first_of(".") - start - 1));

		// Always enabled to prevent softlocks or crashing.
		if (144 == pvID || 700 == pvID)
			return true;

		auto begin = freeplay ? missingIDs.begin() : recvIDs.begin();
		auto end = freeplay ? missingIDs.end() : recvIDs.end();
		auto contains = std::find(begin, end, pvID) != end;

		if (!freeplay && contains && hide_checked)
		{
			for (const auto& songID : recvIDs) {
				auto loc1checked = std::find(CheckedLocations.begin(), CheckedLocations.end(), pvID * 10) != CheckedLocations.end();
				auto loc2checked = std::find(CheckedLocations.begin(), CheckedLocations.end(), (pvID * 10) + 1) != CheckedLocations.end();
				if (loc1checked && loc2checked)
					return false;
			}
		}

		return freeplay ? !contains : contains;
	}

	void reset()
	{
		//APLogger::print("IDHandler reset\n");
		freeplay = false;
		unlock();
	}

	void lock()
	{
		reloading = true;
	}

	void unlock()
	{
		reloading = false;
	}

	void updateTrackerLine()
	{
		// TODO: Update from relevant send/recv callbacks

		int64_t totalLocs = (seedIDs.size() - 1) * 2;

		std::ostringstream trackerStream;
		trackerStream << "Songs: " << recvIDs.size() << "/" << seedIDs.size() << " | ";
		trackerStream << "Locs: " << min(static_cast<int64_t>(CheckedLocations.size()), totalLocs) << "/" << totalLocs << " | ";
		trackerStream << "Logic: " << availableLocs << " | ";
		trackerStream << "Leeks: " << APClient::leekHave << "/" << APClient::leekNeed;

		trackerLine = trackerStream.str();

		// Dump for i.e. stream overlay
		//std::ofstream tracker("stats.txt");
		//tracker << trackerLine;
	}

	void ImGuiTab()
	{
		updateTrackerLine();
		ImGui::Text(trackerLine.c_str());

		if (ImGui::BeginTable("tableTrackerOptions", 2, ImGuiTableFlags_SizingStretchSame))
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			if (ImGui::Checkbox("Freeplay", &freeplay))
				APReload::run();
			ImGui::SameLine();
			HelpMarker("The entire song list will be available except for songs that have not been received yet.");

			ImGui::TableSetColumnIndex(1);

			if (ImGui::Checkbox("Hide checked", &hide_checked))
				APReload::run();
			ImGui::SameLine();
			HelpMarker("When not in Freeplay, the song list will only show songs that have checks.");

			ImGui::EndTable();
		}

		if (ImGui::BeginTable("tableTracker", 2,
			ImGuiTableFlags_BordersInner | ImGuiTableFlags_Hideable | ImGuiTableFlags_HighlightHoveredColumn |
			ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit
		))
		{
			ImGui::TableSetupColumn("Checks");
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();

			int _availableLocs = 0;
			for (const auto& songID : recvIDs) {
				auto loc1checked = std::find(CheckedLocations.begin(), CheckedLocations.end(), songID * 10) != CheckedLocations.end();
				auto loc2checked = std::find(CheckedLocations.begin(), CheckedLocations.end(), (songID * 10) + 1) != CheckedLocations.end();

				int available = (int)!loc1checked + (int)!loc2checked;

				if (hide_checked && available == 0)
					continue;

				_availableLocs += available;

				ImGui::PushID(static_cast<int>(songID));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				std::string label = (available > 0) ? std::to_string(available) : " ";
				label = (songID == APClient::victoryID / 10) ? "GOAL" : label;

				CenterText(label);
				ImGui::Text("%s", label.c_str());

				ImGui::TableSetColumnIndex(1);
				std::string name = item_ap_id_to_name[songID * 10];
				if (name.empty())
					name = "ID " + std::to_string(songID) + " (not in datapackage)";

				if (*(bool*)PvPlayData && songID == static_cast<int64_t>(*(int*)(PvPlayData + 0x10)))
					name = "NP: " + name;

				bool isHinted = std::find(HintedIDs.begin(), HintedIDs.end(), songID) != HintedIDs.end();

				if (isHinted)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

				ImGui::Text("%s", name.c_str());

				if (isHinted)
					ImGui::PopStyleColor();

				if (APClient::devMode)
				{
					if (ImGui::BeginPopupContextItem("##xx"))
					{
						if (ImGui::MenuItem("Cheat##xx"))
							APClient::LocationSend(songID);

						ImGui::EndPopup();
					}
				}

				ImGui::PopID();
			}

			if (_availableLocs == 0)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				CenterText("BK");
				ImGui::Text("BK");
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("Waiting for songs...");
			}

			availableLocs = _availableLocs;

			ImGui::EndTable();
		}
	}
}
