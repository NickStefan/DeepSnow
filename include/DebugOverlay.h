#pragma once

// Deep Snow - TEMPORARY debug overlay (upper right): the player's largest band change in the last 10 s with the depth
// follow against the reading at the feet alone. Not for the upstream PR.

#include <atomic>

namespace DebugOverlay
{
	inline std::atomic<bool> show{ true };

	void Register();         // after the menu framework is known to be installed
	void __stdcall Render();  // render thread
}
