#pragma once
#include "pch.h"
#include <vector>

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
