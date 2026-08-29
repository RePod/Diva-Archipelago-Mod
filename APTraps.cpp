#include "APClient.h"
#include "APTraps.h"
#include "APGUI.h"

namespace APTraps
{
	bool &devMode = APClient::devMode;

	// Config

	float trapDuration = 15.0f;
	float iconInterval = 60.0f;
	bool trapOverlap = false;
	bool randomizeGlyphs = false;
	int slowTarget = 33;

	bool trap_link = false; // Is Trap Link enabled?
	bool trap_link_others = false; // Handle known traps from other games?
	std::vector<std::string> trap_link_tags = { "TrapLink" }; // inb4 Trap Link Groups
	bool stutterQueued = false;

	// Traps native to this game. Should map 1:1 with TrapID.
	std::unordered_map<std::string, std::vector<TrapID>> trapMap = {
		{ "Sudden Trap", { TrapID::Sudden } },
		{ "Hidden Trap", { TrapID::Hidden } },
		{ "Stutter Trap", { TrapID::Stutter } },
		{ "Icon Trap", { TrapID::Icon } },
		{ "Slow Trap", { TrapID::Slow } },
	};

	// Known traps from other games, when trap_link_others is true
	// Hopefully kept updated: https://docs.google.com/spreadsheets/d/1yoNilAzT5pSU9c2hYK7f2wHAe9GiWDiHFZz8eMe1oeQ/edit?usp=sharing
	std::unordered_map<std::string, std::vector<TrapID>> trapMapExt = {
		{ "Bullet Time Trap",		{ TrapID::Slow } },
		{ "Chaos Control Trap",		{ TrapID::Stutter } },
		{ "Chaos Trap",				{ TrapID::Icon } },
		{ "Chart Modifier Trap",	{ TrapID::Icon } },
		{ "Confuse Trap",			{ TrapID::Icon } },
		{ "Confound Trap",			{ TrapID::Icon } },
		{ "Confusion Trap",			{ TrapID::Icon } },
		{ "Cutscene Trap",			{ TrapID::Slow } },
		{ "Extreme Chaos Mode",		{ TrapID::Stutter, TrapID::Slow, TrapID::Hidden, TrapID::Sudden, TrapID::Icon } },
		{ "Fake Transition",		{ TrapID::Hidden, TrapID::Sudden } },
		{ "Fear Trap",				{ TrapID::Sudden } },
		{ "Frame Slime Trap",		{ TrapID::Slow } },
		{ "Freeze Trap",			{ TrapID::Stutter } },
		{ "Frost Trap",				{ TrapID::Stutter } },
		{ "Frozen Trap",			{ TrapID::Stutter } },
		{ "Fuzzy Trap",				{ TrapID::Icon } },
		{ "Gadget Shuffle Trap",	{ TrapID::Icon } },
		{ "Ghost",					{ TrapID::Hidden, TrapID::Sudden } },
		{ "Hiccup Trap",			{ TrapID::Stutter } },
		{ "Ice Trap",				{ TrapID::Stutter } },
		{ "Input Sequence Trap",	{ TrapID::Icon } },
		{ "Invisibility Trap",		{ TrapID::Hidden } },
		{ "Invisible Trap",			{ TrapID::Hidden } },
		{ "Iron Boots Trap",		{ TrapID::Slow } },
		{ "Paralysis Trap",			{ TrapID::Stutter } },
		{ "Paralyze Trap",			{ TrapID::Stutter } },
		{ "Paratoad Trap",			{ TrapID::Stutter } },
		{ "PowerPoint Trap",		{ TrapID::Slow } },
		{ "Shuffle Trap",			{ TrapID::Icon } },
		{ "Sleep Trap",				{ TrapID::Stutter } },
		{ "Slowness Trap",			{ TrapID::Slow } },
		{ "Spooky Time",			{ TrapID::Hidden, TrapID::Sudden } },
		{ "Stun Trap",				{ TrapID::Stutter } },
		{ "Swap Trap",				{ TrapID::Icon } },
	};

	const uint64_t DivaGameControlConfig = 0x1401D6520;
	const uint64_t PvControllerGlyphBase = 0x141133D30; // Copy of GCC Icon on load (0-12), original caller returns base glyph (0-2).
	//const uint64_t DivaGameModifier = PvPlayData + 0x2D120;
	const uint64_t DivaGameTimer = PvPlayData + 0x2D33C;

	// Internal

	uint8_t savedIcon = 39;
	bool isSudden = false; // Had trouble with this as a bool(timestamp > 0)
	bool isHidden = false; // Had trouble with this as a bool(timestamp > 0)
	bool isSlow = false;

	float lastRun = 0.0f; // For delta time against APTraps::DivaGameTimer
	float timestampSudden = 0.0f;
	float timestampHidden = 0.0f;
	float timestampIconStart = 0.0f;
	float timestampIconLast = 0.0f;
	float timestampSlow = 0.0f;

	std::mt19937 mt;
	std::uniform_int_distribution<int> dist(0, 4);
	std::uniform_int_distribution<int> glyph(0, 2);

	void config(const toml::table& settings)
	{
		toml::table section;
		if (settings.contains("traps") && settings["traps"].is_table())
			section = *settings["traps"].as_table();

		float config_duration = section["duration"].value_or(trapDuration);
		trapDuration = std::clamp(config_duration, 0.0f, 300.0f);
		APLogger::print("trap duration: %.02f (config: %.02f)\n", trapDuration, config_duration);

		float config_iconinterval = section["icon_interval"].value_or(iconInterval);
		iconInterval = std::clamp(config_iconinterval, 0.0f, 60.0f);
		APLogger::print("trap icon_interval: %.02f (config: %.02f)\n", iconInterval, config_iconinterval);

		int config_slow_target = section["slow_target"].value_or(32);
		slowTarget = std::clamp(config_slow_target, 15, 40);
		APLogger::print("slow_target: %i (config: %i)\n", slowTarget, config_slow_target);

		trapOverlap = section["overlap"].value_or(false);
		APLogger::print("trap overlap: %d\n", trapOverlap);

		trap_link = section["trap_link"].value_or(false);
		APLogger::print("trap_link: %d\n", trap_link);

		trap_link_others = section["trap_link_others"].value_or(false);
		APLogger::print("trap_link_others: %d\n", trap_link_others);

		randomizeGlyphs = section["icon_glyphs"].value_or(false);
		APLogger::print("trap icon_glyphs: %d\n", randomizeGlyphs);
	}

	void save(toml::table& settings)
	{
		toml::table config;
		config.insert("duration", trapDuration);
		config.insert("icon_interval", iconInterval);
		config.insert("icon_glyphs", randomizeGlyphs);
		config.insert("overlap", trapOverlap);
		config.insert("slow_target", slowTarget);
		config.insert("trap_link", trap_link);
		config.insert("trap_link_others", trap_link_others);

		settings.insert("traps", config);
	}

	int reset()
	{
		APLogger::print("Traps: reset\n");

		std::random_device rd;
		mt.seed(rd());

		resetIcon();
		timestampSudden = 0.0f;
		timestampHidden = 0.0f;
		timestampIconStart = 0.0f;
		timestampIconLast = 0.0f;
		timestampSlow = 0.0f;
		isHidden = false;
		isSudden = false;
		isSlow = false;
		lastRun = 0.0f;

		return 0;
	}

	void resetIcon()
	{
		if (savedIcon == 39)
			return;

		int restoredIcon = ((savedIcon <= 12 && savedIcon >= 0) ? savedIcon : 4);
		//if (getCurrentIcon() != restoredIcon) {
			WRITE_MEMORY(getIconAddress(), uint8_t, (uint8_t)restoredIcon);
			WRITE_MEMORY(PvControllerGlyphBase, uint8_t, (uint8_t)restoredIcon);
			APLogger::print("Traps: Icons restored to %d\n", restoredIcon);
		//}
		savedIcon = 39;
	}

	float getGameTime()
	{
		return *(float*)DivaGameTimer;
	}

	void touchSudden()
	{
		float now = getGameTime();
		float expires = (trapDuration > 0.0f) ? now + trapDuration : 0.0f;

		APLogger::print("[%6.2f] Trap < Sudden (expires: %.2f)\n", now, expires);
		timestampSudden = now;
		isSudden = true;

		if (!trapOverlap && isHidden) {
			APLogger::print("[%6.2f] Trap < Hidden -> Sudden (expires: %.2f)\n", now, expires);
			timestampHidden = 0.0f;
			isHidden = false;
		}
	}

	void touchHidden()
	{
		float now = getGameTime();
		float expires = (trapDuration > 0.0f) ? now + trapDuration : 0.0f;

		APLogger::print("[%6.2f] Trap < Hidden (expires: %.2f)\n", now, expires);
		timestampHidden = now;
		isHidden = true;

		if (!trapOverlap && isSudden) {
			APLogger::print("[%6.2f] Trap < Sudden -> Hidden (expires: %.2f)\n", now, expires);
			timestampSudden = 0.0f;
			isSudden = false;
		}
	}

	void touchStutter()
	{
		float now = getGameTime();
		APLogger::print("[%6.2f] Trap < Stutter\n", now);

		stutterQueued = true;
	}

	void touchIcon()
	{
		float now = getGameTime();
		float expires = (trapDuration > 0.0f) ? now + trapDuration : 0.0f;

		APLogger::print("[%6.2f] Trap < Icon (expires: %.2f)\n", now, expires);
		timestampIconStart = now;
		rollIcon();

		if (timestampIconStart == now)
			return;
	}

	void touchSlow()
	{
		float now = getGameTime();
		float expires = (trapDuration > 0.0f) ? now + trapDuration : 0.0f;

		APLogger::print("[%6.2f] Trap < Slow (expires: %.2f)\n", now, expires);
		timestampSlow = now;
		isSlow = true;
	}

	void linkSend(const std::string trapName)
	{
		if (!trap_link || !APGUI::isInGame()) return;

		AP_Bounce bounce;
		bounce.tags = &trap_link_tags;
		json data;
		std::chrono::time_point<std::chrono::system_clock> timestamp = std::chrono::system_clock::now();
		data["time"] = (int64_t)std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count();
		data["source"] = APClient::getSlotName();
		data["trap_name"] = trapName;

		bounce.data = to_string(data);

		AP_SendBounce(bounce);
	}

	void linkRecv(const std::string trapName)
	{
		if (!trap_link || !APGUI::isInGame()) return;

		auto trap = trapMap.find(trapName);

		if (trap_link_others && trap == trapMap.end())
			trap = trapMapExt.find(trapName);

		if (trap == trapMap.end())
			return;

		auto traps = trap->second;
		float now = getGameTime();
		APLogger::print("[%6.2f] Trap < Linked: %s\n", now, trapName.c_str());

		for (auto& trapID : traps) {
			switch (trapID)
			{
			case TrapID::Hidden:
				touchHidden();
				break;
			case TrapID::Sudden:
				touchSudden();
				break;
			case TrapID::Stutter:
				touchStutter();
				break;
			case TrapID::Icon:
				touchIcon();
				break;
			case TrapID::Slow:
				touchSlow();
				break;
			}
		}
	}

	void run()
	{
		float now = getGameTime();

		if (now == 0.0f && lastRun > 0.0f) {
			reset();
			return;
		}

		APTraps::runSlow();

		if (now - lastRun < 0.1f)
			return;

		lastRun = now;

		if (isSudden) {
			auto deltaSudden = now - timestampSudden;
			if (trapDuration > 0.0f && deltaSudden >= trapDuration) {
				APLogger::print("[%6.2f] Trap > Sudden expired\n", now);
				timestampSudden = 0.0f;
				isSudden = false;
			}
		}

		if (isHidden) {
			auto deltaHidden = now - timestampHidden;
			if (trapDuration > 0.0f && deltaHidden >= trapDuration) {
				APLogger::print("[%6.2f] Trap > Hidden expired\n", now);
				timestampHidden = 0.0f;
				isHidden = false;
			}
		}

		if (savedIcon <= 12) {
			float deltaStart = now - timestampIconStart;
			float deltaLast = now - timestampIconLast;
			if (trapDuration == 0.0f || deltaStart < trapDuration) {
				if (iconInterval > 0.0f && deltaLast >= iconInterval) {
					timestampIconLast = now;
					rollIcon();
				}
			}
			else if (trapDuration > 0.0f) {
				APLogger::print("[%6.2f] Trap > Icon expired\n", now);
				resetIcon();
			}
		}

		// More authetnic than from OnFrame, but not truly authentic.
		checkStutter();
	}

	void runSlow()
	{
		if (APGUI::isInGame() && isSlow) {
			static std::chrono::time_point<std::chrono::system_clock> timestamp = std::chrono::system_clock::now();

			float now = getGameTime();
			std::this_thread::sleep_for(std::chrono::microseconds(1'000'000 / slowTarget));
			auto post_sleep = std::chrono::system_clock::now();
			timestamp = post_sleep;

			auto deltaSlow = now - timestampSlow;

			if (trapDuration > 0.0f && deltaSlow >= trapDuration) {
				APLogger::print("[%6.2f] Trap > Slow expired\n", now);
				timestampSlow = 0.0f;
				isSlow = false;
			}
		}
	}

	uint64_t getGameControlConfig()
	{
		uint64_t GCC = reinterpret_cast<uint64_t(__fastcall*)(void)>(DivaGameControlConfig)();
		return GCC;
	}

	uint64_t getIconAddress()
	{
		return getGameControlConfig() + 0x28;
	}

	uint8_t getCurrentIcon()
	{
		return *(uint8_t*)getIconAddress();
	}

	void rollIcon()
	{
		int currentIcon = getCurrentIcon();
		int nextIcon = dist(mt);

		if (savedIcon > 12)
			savedIcon = currentIcon;

		while (currentIcon == nextIcon) {
			nextIcon = dist(mt);

			if (nextIcon == 4 && currentIcon == 4)
				nextIcon = savedIcon;
			else if (currentIcon >= 9)
				nextIcon += 9;
			else if (currentIcon >= 5)
				nextIcon += 5;
		}

		if (randomizeGlyphs) {
			int out = 1 + (4 * glyph(mt));
			WRITE_MEMORY(PvControllerGlyphBase, uint8_t, (uint8_t)out);
		}

		WRITE_MEMORY(getIconAddress(), uint8_t, (uint8_t)nextIcon);
	}

	void checkStutter()
	{
		if (!stutterQueued)
			return;

		stutterQueued = false;
		std::this_thread::sleep_for(std::chrono::milliseconds(100 * ((rand() % 7) + 1)));
	}

	void ImGuiTab()
	{
		char buf[32];
		float songLength = *(float*)(PvPlayData + 0x2D338);
		sprintf(buf, "%.03f / %.03f", getGameTime(), songLength);
		ImGui::ProgressBar(getGameTime() / songLength, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f), buf);

		ImGui::SliderFloat("Trap Duration", &trapDuration, 0.0f, 300.0f, "%.1f seconds");
		ImGui::SameLine();
		HelpMarker("Seconds until individual traps expire.\n0 to not expire for current attempt.");

		ImGui::SliderFloat("Icon Reroll", &iconInterval, 0.0f, 60.0f, "%.1f seconds");
		ImGui::SameLine();
		HelpMarker("Seconds between icon rerolls while Icon trap is active.\n0 to only reroll once.");

		ImGui::SliderInt("Slow target", &slowTarget, 15, 40);

		ImGui::Checkbox("Allow Sudden and Hidden to overlap", &trapOverlap);
		ImGui::Checkbox("Icon Trap: Random controller glyphs", &randomizeGlyphs);

		if (ImGui::Checkbox("Trap Link", &trap_link))
			APClient::UpdateTags();
		ImGui::SameLine();
		HelpMarker("Share traps with other Trap Link players.\nLinked traps will only apply during play instead of queuing up.");

		if (trap_link) {
			ImGui::Checkbox("Traps from other games", &trap_link_others);
			ImGui::SameLine();
			HelpMarker("Handle known traps of a similar effect from other games.\nExample: Ice Trap = Stutter Trap");
			ImGui::SameLine();
			ImGui::TextLinkOpenURL("(?)", "https://docs.google.com/spreadsheets/d/1yoNilAzT5pSU9c2hYK7f2wHAe9GiWDiHFZz8eMe1oeQ/edit?gid=811965759");
		}

		if (devMode) {
			ImGui::Separator();

			if (ImGui::Button("Sudden"))
				touchSudden();
			ImGui::SameLine();
			if (ImGui::Button("Hidden"))
				touchHidden();
			ImGui::SameLine();
			if (ImGui::Button("Stutter"))
				touchStutter();
			ImGui::SameLine();
			if (ImGui::Button("Icon"))
				touchIcon();
			ImGui::SameLine();
			if (ImGui::Button("Slow"))
				touchSlow();

			static char tl[20];
			ImGui::InputText("##xx", tl, sizeof(tl));
			ImGui::SameLine();
			if (ImGui::Button("Trap Link##xx"))
				linkRecv(std::string(tl));

			if (ImGui::BeginTable("tableTraps", 2))
			{
				if (isSudden)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Sudden");
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%.02f", trapDuration + timestampSudden - getGameTime());
				}

				if (isHidden)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Hidden");
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%.02f", trapDuration + timestampHidden - getGameTime());
				}

				if (isSlow)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Slow");
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%.02f", trapDuration + timestampSlow - getGameTime());
				}

				if (savedIcon <= 12)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Icon");
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%.02f %i / %i", trapDuration + timestampIconStart - getGameTime(), getCurrentIcon(),
								*reinterpret_cast<uint8_t*>(PvControllerGlyphBase));
				}

				ImGui::EndTable();
			}
		}
	}
}
