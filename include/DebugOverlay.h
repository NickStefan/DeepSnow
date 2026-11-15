#pragma once

// Deep Snow - TEMPORARY debug overlay (upper right): the texture under the player and each check on the way to a
// depth band, passing or failing, beside SkiingInSkyrim's powder share for comparison. Not for the upstream PR.

#include <atomic>

namespace DebugOverlay
{
	inline std::atomic<bool> show{ true };

	void Register();         // after the menu framework is known to be installed
	void __stdcall Render();  // render thread
}
