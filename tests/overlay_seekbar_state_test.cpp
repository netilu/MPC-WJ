#include "../src/apps/mplayerc/OverlaySeekBarState.h"
#include <cassert>
#include <climits>
#include <iostream>
#include <limits>

int main()
{
	using State = OverlaySeekBarState;
	State state;
	assert(!state.UpdateHover(false, 0));
	assert(state.UpdateHover(true, 0));
	assert(state.UpdateHover(false, 0));
	assert(state.UpdateHover(false, 499));
	assert(!state.UpdateHover(false, 500));
	assert(state.UpdateHover(true, 600));
	assert(state.UpdateHover(false, 700));
	assert(state.UpdateHover(true, 1199)); // Re-enter before the deadline.
	assert(state.UpdateHover(false, 1200));
	assert(!state.UpdateHover(false, 1700));

	state.UpdateProgress(25, 100);
	state.dragging = true;
	assert(state.UpdateHover(false, 10000)); // Dragging outside retains the bar.
	assert(!state.UpdateProgress(2, 100)); // Stale playback polling cannot snap back.
	assert(state.position == 25);
	state.dragging = false;
	assert(state.UpdateHover(false, 20000));
	assert(!state.UpdateHover(false, 20500));
	assert(state.UpdateProgress(50, 100));
	assert(state.position == 50);
	state.dragging = true;
	state.Hide(); // Closing media, switching preset, losing activation.
	assert(!state.dragging && !state.visible && !state.leaving);
	state.UpdateProgress(50, 0);
	assert(state.position == 0 && state.duration == 0);
	state.UpdateProgress(-10, 100);
	assert(state.position == 0);
	state.UpdateProgress(110, 100);
	assert(state.position == 100);

	assert(State::PositionAtPixel(-50, 800, 600000000) == 0);
	assert(State::PositionAtPixel(400, 800, 600000000) == 300000000);
	assert(State::PositionAtPixel(1000, 800, 600000000) == 600000000);
	assert(State::PositionAtPixel(1, 0, 600000000) == 0);
	assert(State::PositionAtPixel(1, 1, 0) == 0);
	const auto longest = std::numeric_limits<std::int64_t>::max();
	assert(State::PositionAtPixel(INT_MAX, INT_MAX, longest) == longest);
	assert(State::PositionAtPixel(1, 1, longest) == longest);
	std::int64_t previous = 0;
	for (int x = 0; x <= 4000; ++x) {
		const auto position = State::PositionAtPixel(x, 4000, longest);
		assert(position >= previous && position <= longest);
		previous = position;
	}
	assert(State::AlphaFromTransparency(0) == 255);
	assert(State::AlphaFromTransparency(30) == 179);
	assert(State::AlphaFromTransparency(90) == 26);
	assert(State::AlphaFromTransparency(-10) == 255);
	assert(State::AlphaFromTransparency(100) == 26);
	std::cout << "PASS: overlay visibility, drag retention, time mapping, transparency.\n";
}
