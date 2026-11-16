#include "PCH.h"

#include "DebugOverlay.h"

#include "SKSEMenuFramework.h"

#include "Movement.h"

#include "utils/Logger.h"

#include <format>
#include <string>

namespace DebugOverlay
{
	namespace
	{
		using ImGuiMCP::ImU32;
		using ImGuiMCP::ImVec2;
		namespace Draw = ImGuiMCP::ImDrawListManager;

		constexpr float kMargin = 20.0F;
		constexpr float kWidth = 380.0F;
		constexpr float kPad = 8.0F;
		constexpr float kLabelColumn = 150.0F;
		constexpr float kValueColumn = 110.0F;

		const ImU32 kBackground = IM_COL32(0, 0, 0, 160);
		const ImU32 kText = IM_COL32(230, 230, 230, 255);
		const ImU32 kFaint = IM_COL32(230, 230, 230, 150);
		const ImU32 kPass = IM_COL32(110, 220, 110, 255);
		const ImU32 kFail = IM_COL32(255, 90, 80, 255);

		bool InGame()
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) { return false; }
			auto* player = RE::PlayerCharacter::GetSingleton();
			return player && player->Is3DLoaded();
		}
	}

	void Register()
	{
		constexpr const char* required[] = { "igGetForegroundDrawList_Nil", "igGetIO", "igCalcTextSize", "ImDrawList_AddRectFilled", "ImDrawList_AddText_Vec2" };
		for (const char* name : required)
		{
			if (!GetMenuFrameworkFunction<void*>(name))
			{
				logger::warn("[overlay] the menu framework does not export \"{}\"; no debug overlay", name);
				return;
			}
		}
		SKSEMenuFramework::AddHudElement(Render);
		logger::info("[overlay] follow debug overlay registered (upper right)");
	}

	void __stdcall Render()
	{
		if (!show.load() || !InGame()) { return; }
		auto* list = ImGuiMCP::GetForegroundDrawList();
		const ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
		if (!list || !io) { return; }

		const movement::Snapshot s = movement::Get();
		if (!s.havePlayer) { return; }

		const float lineHeight = ImGuiMCP::CalcTextSize("Ag").y + 2.0F;
		const ImVec2 origin{ io->DisplaySize.x - kWidth - kMargin, kMargin };
		Draw::AddRectFilled(list, origin, { origin.x + kWidth, origin.y + kPad * 2.0F + lineHeight * 5.0F }, kBackground, 6.0F, 0);

		float y = origin.y + kPad;
		const auto row = [&](const std::string& a_label, const std::string& a_applied, const std::string& a_feet, ImU32 a_color) {
			const float x = origin.x + kPad;
			Draw::AddText(list, { x, y }, a_color, a_label.c_str());
			Draw::AddText(list, { x + kLabelColumn, y }, a_color, a_applied.c_str());
			Draw::AddText(list, { x + kLabelColumn + kValueColumn, y }, a_color, a_feet.c_str());
			y += lineHeight;
		};
		const bool pass = s.largestAppliedStep <= s.largestFeetStep;
		row("Deep Snow follow", "applied", "feet alone", kFaint);
		row("depth", std::format("{:.1f}%", s.depthPercent), std::format("{:.1f}%", s.feetDepth), kText);
		row("band", std::to_string(s.groundBand), std::to_string(s.feetBand), kText);
		row("largest step (10 s)", std::to_string(s.largestAppliedStep), std::to_string(s.largestFeetStep), pass ? kPass : kFail);
		row(pass ? "PASS" : "FAIL", "", "", pass ? kPass : kFail);
	}
}
