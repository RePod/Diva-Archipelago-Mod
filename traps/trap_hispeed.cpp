#include "trap_hispeed.h"

namespace TrapHiSpeed
{
	const char* lanePos[4] = { "Left", "Right", "Top", "Bottom" };

	void _TrapHiSpeed::config(const toml::table& settings)
	{
		hiSpeedFactor = std::clamp(settings["hispeed_factor"].value_or(hiSpeedFactor), -10.0f, 10.0f);
		APLogger::print("hispeed_factor: %i\n", hiSpeedFactor);

		lanePosition = std::clamp(settings["lane_position"].value_or(lanePosition), 0, 4);
		APLogger::print("lane_position: %i\n", lanePosition);

		flatAmplitudes = settings["flat_amplitudes"].value_or(flatAmplitudes);
		APLogger::print("flat_amplitudes: %i\n", flatAmplitudes);
	}

	void _TrapHiSpeed::save(toml::table& settings)
	{
		settings.insert("hispeed_factor", hiSpeedFactor);
		settings.insert("lane_position", lanePosition);
		settings.insert("flat_amplitudes", flatAmplitudes);
	}

	void _TrapHiSpeed::resetHiSpeed()
	{
		Trap::reset();
	}

	void _TrapHiSpeed::resetNoSpeed()
	{
		isNoSpeed = false;
		timestampNoSpeed = 0.0f;
	}

	void _TrapHiSpeed::resetLanes()
	{
		isLanes = false;
		timestampLanes = 0.0f;
	}

	void _TrapHiSpeed::reset()
	{
		resetHiSpeed();
		resetNoSpeed();
		resetLanes();
	}

	bool _TrapHiSpeed::isRunningNoSpeed() const
	{
		return isNoSpeed;
	}

	bool _TrapHiSpeed::isRunningLanes() const
	{
		return isLanes;
	}

	void _TrapHiSpeed::touchHiSpeed()
	{
		resetNoSpeed();

		timestamp = APTraps::getTrapEndTime(timestamp);
		running = true;

		APLogger::print("[%6.2f] Trap < HiSpeed (expires: %.2f)\n", now, timestamp);
	}

	void _TrapHiSpeed::touchNoSpeed()
	{
		resetHiSpeed();

		timestampNoSpeed = APTraps::getTrapEndTime(timestampNoSpeed);
		isNoSpeed = true;

		APLogger::print("[%6.2f] Trap < NoSpeed (expires: %.2f)\n", now, timestampNoSpeed);
	}

	void _TrapHiSpeed::touchLanes()
	{
		timestampLanes = APTraps::getTrapEndTime(timestampLanes);
		isLanes = true;

		APLogger::print("[%6.2f] Trap < Lanes (expires: %.2f)\n", now, timestampLanes);
	}

	void _TrapHiSpeed::touch()
	{
		touchHiSpeed();
	}

	void _TrapHiSpeed::tick()
	{
		if (!running && !isNoSpeed && !isLanes) return;

		if (isLanes && now >= timestampLanes) {
			APLogger::print("[%6.2f] Trap > Lanes expired\n", now);
			resetLanes();
			return;
		}

		if (isNoSpeed && now >= timestampNoSpeed) {
			APLogger::print("[%6.2f] Trap > NoSpeed expired\n", now);
			resetNoSpeed();
			return;
		}

		if (running && now >= timestamp) {
			APLogger::print("[%6.2f] Trap > HiSpeed expired\n", now);
			resetHiSpeed();
			return;
		}
	}

	void _TrapHiSpeed::ImGuiConfig()
	{
		if (ImGui::CollapsingHeader("HiSpeed / NoSpeed / Lanes")) {
			static std::string fmt;
			fmt = std::format("{:.3f} x {} = {:.3f}", hiSpeedFactor, getHighSpeedRate(), hiSpeedFactor * getHighSpeedRate());

			if (ImGui::SliderFloat("HiSpeed Factor", &hiSpeedFactor, 0.5, 5.0, fmt.c_str()))
				hiSpeedFactor = std::clamp(hiSpeedFactor, -10.0f, 10.0f);
			HelpMarker("Fine tune the song's default high speed rate.\nThe second operand is from the current song played.\n\n0 is equivalent to NoSpeed.\nNegative flips the trajectory.\nTry extremely small numbers close to 0!");

			ImGui::Combo("Lane edge", &lanePosition, lanePos, IM_COUNTOF(lanePos));

			ImGui::Checkbox("Lanes: Flat amplitudes", &flatAmplitudes);
			HelpMarker("Flight paths will be flat instead of wavy.\nUncheck for more visual flair.");
		}
	}

	void _TrapHiSpeed::ImGuiStatus()
	{
		if (running)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("HiSpeed");
			ImGui::TableNextColumn();
			ImGui::Text("%.02f", timestamp - now);
		}

		if (isNoSpeed)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("NoSpeed");
			ImGui::TableNextColumn();
			ImGui::Text("%.02f", timestampNoSpeed - now);
		}

		if (isLanes)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Lanes");
			ImGui::TableNextColumn();
			ImGui::Text("%.02f", timestampLanes - now);
		}
	}

	void _TrapHiSpeed::ImGuiExpose()
	{
		if (ImGui::Button("HiSpeed"))
			touchHiSpeed();

		ImGui::SameLine();

		if (ImGui::Button("NoSpeed"))
			touchNoSpeed();

		ImGui::SameLine();

		if (ImGui::Button("Lanes"))
			touchLanes();
	}

	const float& _TrapHiSpeed::getHiSpeedFactor() const
	{
		return hiSpeedFactor;
	}

	const bool& _TrapHiSpeed::getFlatAmplitudes() const
	{
		return flatAmplitudes;
	}

	const int& _TrapHiSpeed::getLanePosition() const
	{
		return lanePosition;
	}

	HOOK(void, __fastcall, _NoteModifier, 0x14026E8E0, uintptr_t *a1, uintptr_t *note, float flying_time, int a4, int a5, int a6, long long *a7, long long a8, int noteTotal) {
		int& modifier = *reinterpret_cast<int*>(PvPlayData + 0x2D120); // TODO: Move to APTraps?
		if (modifier == 1 || note == nullptr || !trap.isRunningLanes() && !trap.isRunning() && !trap.isRunningNoSpeed()) {
			original_NoteModifier(a1, note, flying_time, a4, a5, a6, a7, a8, noteTotal);
			return;
		}

		// Due to traps being temporary and AP potentially being retry heavy the original note has to be preserved.
		// Functions this one calls out to could skip the backup and restore, but not all props are ready (freq).
		// A copy was done original, but this works off the copy instead of copying back.
		Note _note = *(Note*)note;
		uintptr_t* _note_ptr = reinterpret_cast<uintptr_t*>(&_note);

		if (trap.isRunningLanes()) {
			APLogger::print("%i %3.2f x %3.2f <- %3.2f x %3.2f / %i %i\n", _note.type, _note.pos_x, _note.pos_y, _note.origin_x, _note.origin_y, _note.slide_start, _note.slide_end);

			// Surely this is available somewhere else already to track steps through a multi note

			static int index = 0;
			static int last_type = -1;
			index += 1;

			static int mult_table[4] = { 1, 4, 3, 2 };
			static int mult = 0;

			float offset_x = 0.0f;
			float offset_y = 0.0f;

			if (_note.type >= 0 && _note.type <= 7)
			{
				mult = mult_table[_note.type % 4];
			}
			else if (_note.type >= 18 && _note.type <= 21) {
				mult = mult_table[(_note.type - 18) % 4];
			}
			else if (_note.type >= 29 && _note.type <= 36) {
				mult = mult_table[(_note.type - 29) % 4];
			}
			else if (_note.type == 12 || _note.type == 15 || _note.type == 23) { // Left chain/slide
				mult = 0;

				if (noteTotal == 2 && _note.type == last_type && index > 1) {
					mult = -1;
					offset_x = -24.0f;
				}
			}
			else if (_note.type == 13 || _note.type == 16 || _note.type == 24) { // Right chain/slide
				mult = 5;

				if (noteTotal == 2 && _note.type == last_type && index > 1) {
					mult = 6;
					offset_x = -24.0f;
				}
			}
			else if (_note.type >= 25 && _note.type <= 28) { // Rush notes
				mult = mult_table[(_note.type - 25) % 4];
			}
			else if (_note.type == 37 || _note.type == 39 || _note.type == 40 || _note.type == 41 || _note.type == 42) { // Star, Double, Chance, Link, Link End
				mult = 5;

				if (_note.type == 41 || _note.type == 42) {
					// Improve reading links
					static bool flipped = false;
					static bool step = true;

					step = !step;
					mult = flipped ? 0 : 5;

					if (!step) {
						offset_x = -24.0f;
						mult = flipped ? -1 : 6;
					}

					if (_note.type == 42) {
						step = false;
						flipped = !flipped;
					}
				}
			}

			auto distance = hypot(abs(_note.pos_x - _note.origin_x), abs(_note.pos_y - _note.origin_y));
			//distance *= 1.25;

			if (trap.getFlatAmplitudes())
				_note.amp = 0;

			int laneIndex = trap.getLanePosition();
			if (laneIndex == 0 || laneIndex == 1) {
				_note.pos_x = laneIndex == 0 ? 96.0f + offset_x : 384.0f;
				_note.origin_x = laneIndex == 0 ? 12.0f + 480.0f + offset_x : -12.0f;

				_note.pos_y = 77.0f + (24.0f * mult) + offset_y;
				_note.origin_y = _note.pos_y + offset_y;
			}
			else if (laneIndex == 2 || laneIndex == 3) {
				_note.pos_x = 168.0f + (24.0f * mult);
				_note.origin_x = _note.pos_x;

				_note.pos_y = laneIndex == 3 ? 270.0f - 48.0f : 48.0f;
				_note.origin_y = laneIndex == 3 ? -24.0f : 270.0f;
			}

			last_type = _note.type;

			if (index == noteTotal) {
				index = 0;

				if (_note.type != 41)
					last_type = -1;
			}
		}

		if (trap.isRunning()) {
			int rate = getHighSpeedRate();
			float factor = trap.getHiSpeedFactor();

			auto _x = abs(_note.pos_x - _note.origin_x) * rate * factor;
			auto _y = abs(_note.pos_y - _note.origin_y) * rate * factor;

			_note.freq *= max(1, static_cast<int>(rate * factor));
			_note.origin_x = _note.pos_x + (_note.pos_x > _note.origin_x ? _x * -1.0f : _x);
			_note.origin_y = _note.pos_y + (_note.pos_y > _note.origin_y ? _y * -1.0f : _y);
		}
		else if (trap.isRunningNoSpeed()) {
			_note.origin_x = _note.pos_x; // +-1 for a little movement
			_note.origin_y = _note.pos_y;
		}

		original_NoteModifier(a1, _note_ptr, flying_time, a4, a5, a6, a7, a8, noteTotal);
	}

	void installHooks()
	{
		INSTALL_HOOK(_NoteModifier);
	}

	int getHighSpeedRate()
	{
		static auto getHighSpeedRate = reinterpret_cast<int(__fastcall*)(uintptr_t PvPlayData)>(0x14024b630);
		return getHighSpeedRate(PvPlayData);
	}

	_TrapHiSpeed trap;
}
