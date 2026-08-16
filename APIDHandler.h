#pragma once
#include "pch.h"
#include <vector>

struct TrackerItem
{
	int64_t songID = 0;
	int receivedIndex = 0;
	int checksAvailable = 0;
	std::string name = "";
};

namespace APIDHandler
{
	void config(const toml::table& settings);
	void save(toml::table& settings);

	void lock();
	void unlock();
	bool check(std::string& line);
	void reset();

	void updateTrackerLine();
	void ImGuiTab();
}
