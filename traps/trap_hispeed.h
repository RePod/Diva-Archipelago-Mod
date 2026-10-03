#pragma once
#include "trap.h"

namespace TrapHiSpeed
{
	struct Note {
		int type;
		float pos_x;
		float pos_y;
		float origin_x;
		float origin_y;
		float amp;
		int freq;
		bool slide_start;
		bool slide_end;
		char unk[6];
	};

	void installHooks();

	class _TrapHiSpeed : public Trap {
	public:
		_TrapHiSpeed() : Trap() {
			installHooks();
		};

		void config(const toml::table& settings);
		void save(toml::table& settings);
		void resetHiSpeed();
		void resetNoSpeed();
		void resetLanes();
		void reset();
		bool isRunningNoSpeed() const;
		bool isRunningLanes() const;
		void touchHiSpeed();
		void touchNoSpeed();
		void touchLanes();
		void touch();
		void tick();
		void ImGuiConfig();
		void ImGuiStatus();
		void ImGuiExpose();

		const float& getHiSpeedFactor() const;
		const bool& getFlatAmplitudes() const;
		const int& getLanePosition() const;

	private:
		float hiSpeedFactor = 1.0f; // Extra mult on base high speed rate

		bool isNoSpeed = false;
		float timestampNoSpeed = 0.0f;

		bool isLanes = false;
		float timestampLanes = 0.0f;
		int lanePosition = 0;
		bool flatAmplitudes = false; // True: set amplitude to 0
	};

	int getHighSpeedRate();

	extern _TrapHiSpeed trap;
}
