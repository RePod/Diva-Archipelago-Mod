#pragma once
#include <chrono>
#include <random>
#include <stdint.h>
#include "pch.h"

class Trap;

namespace APTraps
{
	enum struct TrapID : int64_t {
		None = 0,
		Random = 1, // TODO: Client specific Trap ID. Use to roll valid native traps.

		// Datapackage's Trap IDs begin at 30. Up to that can be used internally.
		Hidden = 30,	// trap_sudden_hidden
		Sudden = 31,	// trap_sudden_hidden
		HiSpeed = 32,   // trap_hispeed
		Slow = 33,		// trap_slow
		Stutter = 34,	// trap_slow
		Icon = 35,		// trap_icon
		PSP = 36,		// trap_psp
		SFX = 37,		// trap_sfx
		NoSpeed = 38,	// trap_hispeed
		Lane = 39,		// trap_hispeed
	};

	extern bool& devMode;
	extern std::mt19937 mt;
	extern float trapDuration;
	extern bool trap_link;
	extern std::vector<std::string> trap_link_tags;

	void registerTrap(Trap* trap);

	void config(const toml::table& settings);
	void save(toml::table& settings);

	void reset();
	void resetQueue(); // Empty Trap Queue, usually on disconnect.
	void runFrame(); // Ran OnFrame for traps that need to run when not in game, usually to disable themselves. Future hook?
	void runQueue();
	void run(); // Ran during gameplay via hook to expire traps.

	bool canRecv(const int64_t itemID);
	void trapRecv(const int64_t itemID, const bool notify); // Trap received from the multiworld as an item

	float getTrapEndTime(float& timestampTrap);

	void linkSend(const std::string& trapName);
	void linkRecv(const std::string& trapName); // Trap received via bounce packet

	float& getGameTime();

	void ImGuiTab();
}
