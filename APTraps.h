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
		Random = 1, // Client specific Trap ID. Use to roll valid native traps.
		// Datapackage's Trap IDs begin at 30. Up to that can be used for whatever.
		Hidden = 30,
		Sudden = 31,
		//HiSpeed = 32,
		Slow = 33,
		Stutter = 34,
		Icon = 35,
	};

	extern bool isSudden;
	extern bool isHidden;
	extern bool isSlow;
	extern bool stutterQueued;
	extern bool trap_link;

	void config(const toml::table& settings);
	void save(toml::table& settings);

	int reset();
	void resetIcon();
	void run();
	void runSlow();

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
