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
		void resetLane();
		void reset();
		bool isRunningNoSpeed() const;
		bool isRunningLane() const;
		void touchHiSpeed();
		void touchNoSpeed();
		void touchLane();
		void touch();
		void tick();
		void ImGuiConfig();
		void ImGuiStatus();
		void ImGuiExpose();

		const float& getHiSpeedFactor() const;
		const int& getLaneAmplitude() const;
		const int& getLanePosition() const;

	private:
		float hiSpeedFactor = 1.0f; // Extra mult on base high speed rate

		bool isNoSpeed = false;
		float timestampNoSpeed = 0.0f;

		bool isLane = false;
		float timestampLane = 0.0f;
		int lanePosition = 0;
		int laneAmplitude = 1;
	};

	int getHighSpeedRate();

	extern _TrapHiSpeed trap;
}
