#include "APClient.h"
#include "APIDHandler.h"
#include <sstream>

namespace APIDHandler
{
	// Internal
	bool exists = false;
	bool freeplay = false;
	bool reload_needed = true;
	bool reloading = false;

	std::vector<int> seedIDs = { }; // All IDs known to the seed, from slot data
	std::vector<int> toggleIDs = { };

	std::vector<int> &checked = APClient::CheckedLocations;

	void cacheExists()
	{
		exists = toggleIDs.size() > 0;
	}

	bool checkNC()
	{
		if (!reload_needed)
			return false;

		HMODULE hModule = GetModuleHandle(L"NewClassics.dll");

		if (hModule != NULL) {
			APLogger::print("IDH: New Classics suspected, reload recommended\n");
			return true;
		}

		reload_needed = false;
		return false;
	}

	bool check(std::string& line)
	{
		if (reload_needed || AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated || line.find("pv_") != 0)
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
		if (144 == pvID || 700 == pvID || 701 == pvID)
			return true;

		bool c = contains(pvID);

		return freeplay ? !c : c;
	}

	void reset()
	{
		//APLogger::print("IDHandler reset\n");
		freeplay = false;
		toggleIDs.clear();
		unlock();
	}

	void add(int newID)
	{
		if (reload_needed) {
			APLogger::print("IDHandler < Attempted to add %d but a reload is needed\n", newID);
			return;
		}

		newID = abs(newID);

		if (144 == newID || 700 == newID || 701 == newID)
			return;

		if (!contains(newID))
			toggleIDs.push_back(newID);
	}

	bool contains(int songID)
	{
		// Potentially very slow as the song list grows.
		return std::find(toggleIDs.begin(), toggleIDs.end(), songID) != toggleIDs.end();
	}

	void lock()
	{
		cacheExists();
		reloading = true;
	}

	void unlock()
	{
		cacheExists();
		reloading = false;
	}
}
