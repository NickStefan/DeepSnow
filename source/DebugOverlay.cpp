#include "PCH.h"

#include "DebugOverlay.h"

#include "SKSEMenuFramework.h"

#include "Depth.h"
#include "Movement.h"
#include "Settings.h"

#include "utils/Logger.h"

#include <format>
#include <string>
#include <vector>

namespace DebugOverlay
{
	namespace
	{
		using ImGuiMCP::ImU32;
		using ImGuiMCP::ImVec2;
		namespace Draw = ImGuiMCP::ImDrawListManager;

		constexpr float kMargin = 20.0F;
		constexpr float kWidth = 520.0F;
		constexpr float kPad = 8.0F;
		constexpr float kTagWidth = 46.0F;

		const ImU32 kBackground = IM_COL32(0, 0, 0, 160);
		const ImU32 kText = IM_COL32(230, 230, 230, 255);
		const ImU32 kFaint = IM_COL32(230, 230, 230, 150);
		const ImU32 kPass = IM_COL32(110, 220, 110, 255);
		const ImU32 kFail = IM_COL32(255, 90, 80, 255);
		const ImU32 kInfo = IM_COL32(90, 170, 255, 255);

		enum class Tag { kNone, kPass, kFail, kInfo };

		struct Line
		{
			Tag tag = Tag::kNone;
			std::string text;
			ImU32 color = kText;
		};

		const char* MaterialName(std::uint32_t a_id)
		{
			using M = RE::MATERIAL_ID;
			switch (static_cast<M>(a_id))
			{
			case M::kNone: return "none";
			case M::kSnow: return "snow";
			case M::kSnowStairs: return "snow stairs";
			case M::kIce: return "ice";
			case M::kGrass: return "grass";
			case M::kGravel: return "gravel";
			case M::kMud: return "mud";
			case M::kSand: return "sand";
			case M::kStoneBroken: return "stone broken";
			case M::kStoneHeavy: return "stone heavy";
			case M::kStoneStairs: return "stone stairs";
			case M::kStoneAsStairs: return "stone as stairs";
			case M::kBoulderSmall: return "boulder small";
			case M::kBoulderLarge: return "boulder large";
			case M::kWood: return "wood";
			case M::kWater: return "water";
			case M::kAsh: return "ash";
			default: break;
			}
			// Dirt and stone share a few IDs CommonLib does not name; show the number.
			static thread_local std::string other;
			other = std::to_string(a_id);
			return other.c_str();
		}

		bool InGame()
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) { return false; }
			auto* player = RE::PlayerCharacter::GetSingleton();
			return player && player->Is3DLoaded();
		}

		std::vector<Line> Build(const movement::Snapshot& a_s)
		{
			using K = surface::Kind;
			const surface::Reading& r = a_s.reading;
			std::vector<Line> out;
			const auto add = [&](Tag a_tag, std::string a_text, ImU32 a_color = kText) { out.push_back({ a_tag, std::move(a_text), a_color }); };
			const auto check = [&](bool a_pass, std::string a_text) { add(a_pass ? Tag::kPass : Tag::kFail, std::move(a_text)); };

			add(Tag::kNone, "Deep Snow debug", kFaint);
			if (!a_s.havePlayer) { add(Tag::kFail, "no player reading yet"); return out; }
			if (!a_s.enabled) { add(Tag::kFail, "Deep Snow is switched off"); }
			if (!a_s.effectsReady) { add(Tag::kFail, "DeepSnow.esp is not active"); }

			add(Tag::kNone, std::format("texture: {}", r.texture.empty() ? "(default land texture)" : r.texture));
			if (!r.landEditorID.empty() || r.landTexture) { add(Tag::kNone, std::format("  LTEX {:08X} {}", r.landTexture, r.landEditorID), kFaint); }
			add(Tag::kNone, std::format("material: texture {} | engine {} | underfoot {}", MaterialName(r.landMaterial), MaterialName(r.engineMaterial), MaterialName(r.material)));
			if (r.skiPowder >= 0.0F) { add(Tag::kInfo, std::format("SkiingInSkyrim powder: {:.0f}%", r.skiPowder * 100.0F)); }
			add(Tag::kNone, std::format("result: {} ({})", surface::Name(r.kind), r.why ? r.why : ""), kFaint);

			// The checks, in the order Surface.cpp and Depth.cpp apply them.
			check(r.kind != K::kNone, r.kind != K::kNone ? "ground read" : std::format("ground read: {}", r.why ? r.why : ""));
			check(r.kind != K::kIce, r.kind == K::kIce ? std::format("not ice: ice ({})", r.why ? r.why : "") : "not ice");
			check(!r.onMesh, r.onMesh ? std::format("on the landscape: on a mesh {}", r.model) : "on the landscape (no mesh under the feet)");
			check(r.kind != K::kRoad, r.kind == K::kRoad ? std::format("not a road: road by '{}'", r.roadBy) : "not a road");
			check(r.kind != K::kTrail, r.kind == K::kTrail ? "not a trail: on the trail list" : "not a trail");
			if (r.kind == K::kSnow) { add(Tag::kPass, std::string("snow: ") + (std::string_view(r.why ? r.why : "") == "snow material" ? "by material" : "by the snow list")); }
			else
			{
				const bool matSnow = surface::IsSnowMaterialID(r.landMaterial);
				add(Tag::kFail, std::format("snow: not on the snow list, texture material {}", matSnow ? "snow (an earlier row won)" : MaterialName(r.landMaterial)));
			}
			check(!r.sheltered, r.sheltered ? std::format("no roof: roof {:.0f} units up", r.roofDistance) : "no roof");
			if (r.kind == K::kSnow) { check(r.cover > 0.0F, std::format("snow cover in the blend: {:.0f}%", r.cover * 100.0F)); }
			check(a_s.packing < 0.5F, std::format("trail packing: {:.0f}%", a_s.packing * 100.0F));
			const bool sd = a_s.depthSource == 2;
			add(Tag::kInfo, sd ? std::format("depth source: Snow Deformation ({:.1f} units)", a_s.sdUnits) : std::string("depth source: Deep Snow's own model"));
			const float firstBand = settings::bands[0].depth.load();
			check(a_s.groundBand > 0, std::format("depth {:.0f}% -> band {} (band 1 starts at {:.0f}%)", a_s.depthPercent, a_s.groundBand, firstBand));
			if (a_s.forcedBand >= 0) { add(Tag::kInfo, std::format("band forced to {} by a test tool", a_s.forcedBand)); }

			if (!r.coverLayers.empty())
			{
				add(Tag::kNone, "blend at the nearest vertex (base first):", kFaint);
				for (std::size_t i = 0; i < r.coverLayers.size(); ++i)
				{
					const auto& l = r.coverLayers[i];
					add(Tag::kNone, std::format("  {} {:<22} {:>4.0f}%  {}{}", i == 0 ? "base " : "layer", l.stem.empty() ? "(default)" : l.stem, l.alpha * 100.0F, MaterialName(l.material),
						l.snow ? "  [snow]" : ""), l.snow ? kPass : kFaint);
				}
			}
			return out;
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
		logger::info("[overlay] debug overlay registered (upper right)");
	}

	void __stdcall Render()
	{
		if (!show.load() || !InGame()) { return; }
		auto* list = ImGuiMCP::GetForegroundDrawList();
		const ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
		if (!list || !io) { return; }

		const std::vector<Line> lines = Build(movement::Get());
		const float lineHeight = ImGuiMCP::CalcTextSize("Ag").y + 2.0F;
		const float height = kPad * 2.0F + lineHeight * static_cast<float>(lines.size());
		const ImVec2 origin{ io->DisplaySize.x - kWidth - kMargin, kMargin };
		Draw::AddRectFilled(list, origin, { origin.x + kWidth, origin.y + height }, kBackground, 6.0F, 0);

		float y = origin.y + kPad;
		for (const Line& l : lines)
		{
			float x = origin.x + kPad;
			if (l.tag != Tag::kNone)
			{
				const char* tag = l.tag == Tag::kPass ? "PASS" : l.tag == Tag::kFail ? "FAIL" : "INFO";
				const ImU32 color = l.tag == Tag::kPass ? kPass : l.tag == Tag::kFail ? kFail : kInfo;
				Draw::AddText(list, { x, y }, color, tag);
				x += kTagWidth;
			}
			Draw::AddText(list, { x, y }, l.color, l.text.c_str());
			y += lineHeight;
		}
	}
}
