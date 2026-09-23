/*
 * (C) 2026 see Authors.txt
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cstdint>

// UI-independent timing and time mapping, shared with the regression tests.
struct OverlaySeekBarState
{
	std::int64_t duration = 0;
	std::int64_t position = 0;
	bool dragging = false;
	bool visible = false;
	bool leaving = false;
	std::uint64_t leaveTime = 0;

	void Hide()
	{
		visible = dragging = leaving = false;
	}

	bool UpdateProgress(std::int64_t newPosition, std::int64_t newDuration)
	{
		if (dragging) {
			return false;
		}
		newDuration = std::max<std::int64_t>(0, newDuration);
		newPosition = std::clamp<std::int64_t>(newPosition, 0, newDuration);
		const bool changed = position != newPosition || duration != newDuration;
		position = newPosition;
		duration = newDuration;
		return changed;
	}

	bool UpdateHover(bool inHotZone, std::uint64_t now)
	{
		if (dragging || inHotZone) {
			visible = true;
			leaving = false;
		} else if (visible) {
			if (!leaving) {
				leaveTime = now;
				leaving = true;
			} else if (now - leaveTime >= 500) {
				Hide();
			}
		}
		return visible;
	}

	static std::int64_t PositionAtPixel(std::int64_t offset, int width, std::int64_t duration)
	{
		if (width <= 0 || duration <= 0) {
			return 0;
		}
		offset = std::clamp<std::int64_t>(offset, 0, width);
		// Split the multiplication to retain 64-bit time precision without overflow.
		return (duration / width) * offset + ((duration % width) * offset + width / 2) / width;
	}

	static unsigned char AlphaFromTransparency(int percent)
	{
		return (unsigned char)((255 * (100 - std::clamp(percent, 0, 90)) + 50) / 100);
	}
};
