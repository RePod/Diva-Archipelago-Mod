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
		Random = 1, // TODO: Client specific Trap ID. Use to roll valid native traps.

		// Datapackage's Trap IDs begin at 30. Up to that can be used for whatever.
		Hidden = 30,
		Sudden = 31,
		//HiSpeed = 32,
		Slow = 33,
		Stutter = 34,
		Icon = 35,
		PSP = 36, // Changes actual resolution, minor flicker, may mess with recording software (Steam).
	};

	struct Resolution {
		int width = 0; // Original width + X padding * 2
		int height = 0; // Original height + Y padding * 2

		void clear() {
			*this = {};
		}

		void update() {
			static uintptr_t* res = reinterpret_cast<uint64_t*>(0x141148218);
			width = *(int*)(*(res)+0x40) + (2 * *(int*)(*(res)+0x48));
			height = *(int*)(*(res)+0x44) + (2 * *(int*)(*(res)+0x4c));
		}
	};

	extern bool isSudden;
	extern bool isHidden;
	extern bool isSlow;
	extern bool trap_link;

	void config(const toml::table& settings);
	void save(toml::table& settings);

	int reset();
	void resetIcon();
	void run();
	void runSlow();

	bool canRecv(const int64_t itemID);
	void trapRecv(const int64_t itemID, const bool notify);

	void menuOpened();
	void touchSudden();
	void touchSudden(bool force);
	void touchHidden();
	void touchHidden(bool force);
	void touchStutter();
	void touchIcon();
	void touchSlow();
	void touchPSP();
	void linkSend(const std::string& trapName);
	void linkRecv(const std::string& trapName);

	uint64_t getGameControlConfig();
	uint64_t getIconAddress();

	int getCurrentIcon();
	void rollIcon();

	void ImGuiTab();
}
