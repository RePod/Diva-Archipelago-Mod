#pragma once
#include <chrono>
#include <random>
#include <stdint.h>
#include "pch.h"

namespace fs = std::filesystem;

namespace APTraps
{
	enum struct TrapID : int64_t {
		None = 0,
		Hidden = 4,
		Sudden = 5,
		Stutter = 8,
		Icon = 9,
	};

	extern bool isSudden;
	extern bool isHidden;
	extern bool stutterQueued;
	extern bool trap_link;

	void config(const toml::table& settings);
	void save(toml::table& settings);

	int reset();
	void resetIcon();
	void run();

	void touchSudden();
	void touchHidden();
	void touchStutter();
	void touchIcon();
	void linkSend(const std::string trapName);
	void linkRecv(const std::string trapName);

	uint64_t getGameControlConfig();
	uint64_t getIconAddress();

	uint8_t getCurrentIcon();
	void rollIcon();

	void checkStutter();

	void ImGuiTab();
}
