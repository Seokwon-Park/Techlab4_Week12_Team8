#include "EnginePCH.h"
#include "Editor/EditorUI/EditorIcons.h"

#include "Core/Misc/Paths.h"

#include <imgui_internal.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#pragma warning(push, 0)
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg.h>
#include <nanosvgrast.h>
#pragma warning(pop)

namespace
{
	using Microsoft::WRL::ComPtr;

	ImU32 ApplyStyleAlpha(ImU32 Color)
	{
		ImVec4 C = ImGui::ColorConvertU32ToFloat4(Color);
		C.w *= ImGui::GetStyle().Alpha;   // BeginDisabled일 때 흐리게
		return ImGui::ColorConvertFloat4ToU32(C);
	}

	// EEditorIcon 순서대로 EngineContent/Editor/Icons/<이름>.svg
	constexpr const char* IconFileNames[] = {
		nullptr,          // None
		"Save",
		"BrowseContent",
		"AddActor",
		"Cinematics",
		"Play",
		"Pause",
		"Step",
		"Stop",
		"MoreVertical",
		"ChevronDown",
		"Settings",
		"Folder",
		"NewWindow",
		"Check",
		"SelectMode",
		"Eye",
		"EyeClosed",
		"Translate",
		"Rotate",
		"Scale",
		"World",
		"Local",
		"Search",
		"Reset",
		"Camera",
		"CameraSpeed",
		"Lit",
		"LayoutQuad",
		"LayoutSingle",
		"Select",
		"ViewTop",
		"ViewBottom",
		"ViewLeft",
		"ViewRight",
		"ViewFront",
		"ViewBack",
		"Plus",
		"Import",
		"ArrowLeft",
		"ArrowRight",
		"ChevronRight",
		"Filter",
		"Sort",
		"Dock",
		"Texture",
		"SkeletalMesh",
		"Animation",
		"Particle",
		"Level",
		"Font",
		"Sound",
		"Script",
		"File",
		"Widget",
		"ViewWireframe",
		"ViewDepth",
		"ViewNormal",
		"ViewLightCulling",
		"ViewCascade",
		"ViewShadowMask",
		"ShowPrimitives",
		"ShowDecal",
		"ShowFog",
		"ShowAntiAliasing",
		"ShowVignette",
		"ShowCameraOverlay",
	};
	static_assert(IM_ARRAYSIZE(IconFileNames) == static_cast<int>(EEditorIcon::Count), "아이콘 파일 이름 표를 enum과 맞춰야 한다");

	// UE5 Slate가 SVG 브러시를 그리는 방식과 같다.
	// 시작할 때 SVG를 파싱만 해 두고, (아이콘, 픽셀 크기)가 처음 쓰일 때 그 크기로 래스터화해 아틀라스 한 칸에 올린다.
	// 아이콘은 흰색으로 그려 두고 그릴 때 정점 색으로 칠한다. DPI가 바뀌면 새 크기가 자동으로 새 칸에 구워진다.
	class FIconAtlas
	{
	public:
		struct FSlot
		{
			ImVec2 UV0;
			ImVec2 UV1;
		};

		bool Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
		{
			Device = InDevice;
			Context = InContext;

			D3D11_TEXTURE2D_DESC Desc{};
			Desc.Width = AtlasSize;
			Desc.Height = AtlasSize;
			Desc.MipLevels = 1;
			Desc.ArraySize = 1;
			Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			Desc.SampleDesc.Count = 1;
			Desc.Usage = D3D11_USAGE_DEFAULT;
			Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			// 빈 칸은 투명이어야 선형 샘플링이 칸 바깥을 섞어도 티가 안 난다.
			std::vector<uint32> Clear(static_cast<size_t>(AtlasSize) * AtlasSize, 0u);
			D3D11_SUBRESOURCE_DATA Init{ Clear.data(), AtlasSize * 4u, 0 };
			if (FAILED(Device->CreateTexture2D(&Desc, &Init, Texture.GetAddressOf())) ||
				FAILED(Device->CreateShaderResourceView(Texture.Get(), nullptr, SRV.GetAddressOf())))
			{
				Shutdown();
				return false;
			}

			Rasterizer = nsvgCreateRasterizer();
			const std::filesystem::path Dir = FPaths::EngineContentDir() / "Editor" / "Icons";
			for (int32 I = 1; I < static_cast<int32>(EEditorIcon::Count); ++I)
				Images[I] = LoadSvg(Dir / (std::string(IconFileNames[I]) + ".svg"));
			return true;
		}

		void Shutdown()
		{
			for (NSVGimage*& Image : Images)
			{
				if (Image) nsvgDelete(Image);
				Image = nullptr;
			}
			if (Rasterizer) nsvgDeleteRasterizer(Rasterizer);
			Rasterizer = nullptr;
			Slots.clear();
			SRV.Reset();
			Texture.Reset();
			Context.Reset();
			Device.Reset();
		}

		ImTextureID GetTextureId() const { return (ImTextureID)SRV.Get(); }

		// 없으면 지금 Pixels 크기로 구워 올린다. 실패하면 nullptr.
		const FSlot* FindOrBake(EEditorIcon Icon, int32 Pixels)
		{
			const int32 Index = static_cast<int32>(Icon);
			if (!SRV || Index <= 0 || Index >= static_cast<int32>(EEditorIcon::Count) || !Images[Index])
				return nullptr;
			Pixels = ImClamp(Pixels, 4, 256);

			const uint32 Key = (static_cast<uint32>(Index) << 16) | static_cast<uint32>(Pixels);
			if (auto It = Slots.find(Key); It != Slots.end())
				return &It->second;

			int32 X = 0, Y = 0;
			if (!Allocate(Pixels, X, Y))
			{
				// 가득 차면 처음부터 다시 쌓는다 (크기를 계속 바꾸는 경우에만 일어난다).
				Slots.clear();
				PenX = PenY = RowHeight = 0;
				if (!Allocate(Pixels, X, Y))
					return nullptr;
			}

			NSVGimage* Image = Images[Index];
			const float Scale = Pixels / ImMax(Image->width, Image->height);
			const float OffsetX = (Pixels - Image->width * Scale) * 0.5f;
			const float OffsetY = (Pixels - Image->height * Scale) * 0.5f;
			Pixels32.assign(static_cast<size_t>(Pixels) * Pixels, 0u);
			nsvgRasterize(Rasterizer, Image, OffsetX, OffsetY, Scale,
				reinterpret_cast<unsigned char*>(Pixels32.data()), Pixels, Pixels, Pixels * 4);

			// 같은 즉시 컨텍스트라 이번 프레임 ImGui 그리기보다 먼저 반영된다.
			const D3D11_BOX Box{ static_cast<UINT>(X), static_cast<UINT>(Y), 0,
				static_cast<UINT>(X + Pixels), static_cast<UINT>(Y + Pixels), 1 };
			Context->UpdateSubresource(Texture.Get(), 0, &Box, Pixels32.data(), Pixels * 4, 0);

			const float Inv = 1.0f / AtlasSize;
			FSlot Slot{ ImVec2(X * Inv, Y * Inv), ImVec2((X + Pixels) * Inv, (Y + Pixels) * Inv) };
			return &Slots.emplace(Key, Slot).first->second;
		}

	private:
		static constexpr int32 AtlasSize = 1024;
		static constexpr int32 Padding = 1;   // 선형 샘플링이 옆 칸을 섞지 않게 비워 두는 테두리

		static NSVGimage* LoadSvg(const std::filesystem::path& Path)
		{
			std::ifstream File(Path, std::ios::binary);
			if (!File)
			{
				HTR_LOG(Warning, "Editor icon not found: {}", FPaths::ToUtf8(Path));
				return nullptr;
			}
			std::stringstream Stream;
			Stream << File.rdbuf();
			std::string Text = Stream.str();   // nsvgParse가 버퍼를 고쳐 쓰므로 복사본을 넘긴다
			NSVGimage* Image = nsvgParse(Text.data(), "px", 96.0f);
			if (Image && (Image->width <= 0.0f || Image->height <= 0.0f))
			{
				nsvgDelete(Image);
				Image = nullptr;
			}
			if (!Image)
				HTR_LOG(Warning, "Failed to parse editor icon: {}", FPaths::ToUtf8(Path));
			return Image;
		}

		// 선반(shelf) 방식으로 왼쪽 위부터 채운다.
		bool Allocate(int32 Pixels, int32& OutX, int32& OutY)
		{
			const int32 Cell = Pixels + Padding * 2;
			if (PenX + Cell > AtlasSize)
			{
				PenX = 0;
				PenY += RowHeight;
				RowHeight = 0;
			}
			if (PenY + Cell > AtlasSize)
				return false;
			OutX = PenX + Padding;
			OutY = PenY + Padding;
			PenX += Cell;
			RowHeight = ImMax(RowHeight, Cell);
			return true;
		}

		ComPtr<ID3D11Device> Device;
		ComPtr<ID3D11DeviceContext> Context;
		ComPtr<ID3D11Texture2D> Texture;
		ComPtr<ID3D11ShaderResourceView> SRV;
		NSVGrasterizer* Rasterizer = nullptr;
		NSVGimage* Images[static_cast<int32>(EEditorIcon::Count)] = {};
		std::unordered_map<uint32, FSlot> Slots;
		std::vector<uint32> Pixels32;
		int32 PenX = 0;
		int32 PenY = 0;
		int32 RowHeight = 0;
	};

	FIconAtlas GIconAtlas;
}

namespace EditorIcons
{
	bool Initialize(ID3D11Device* Device, ID3D11DeviceContext* Context)
	{
		return GIconAtlas.Initialize(Device, Context);
	}

	void Shutdown()
	{
		GIconAtlas.Shutdown();
	}

	float GetScale()
	{
		return ImGui::GetFontSize() / 13.0f;
	}

	void Draw(ImDrawList* DrawList, EEditorIcon Icon, const ImVec2& Center, float Size, ImU32 Color)
	{
		const int32 Pixels = static_cast<int32>(std::lround(Size));
		const FIconAtlas::FSlot* Slot = GIconAtlas.FindOrBake(Icon, Pixels);
		if (!Slot)
			return;

		// 텍셀과 화면 픽셀을 1:1로 맞춰야 흐려지지 않는다.
		const float Half = Pixels * 0.5f;
		const ImVec2 Min(std::floor(Center.x - Half + 0.5f), std::floor(Center.y - Half + 0.5f));
		DrawList->AddImage(GIconAtlas.GetTextureId(), Min, ImVec2(Min.x + Pixels, Min.y + Pixels), Slot->UV0, Slot->UV1, Color);
	}

	void MenuNote(const char* Text, float WrapWidth)
	{
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + WrapWidth * GetScale());
		ImGui::TextDisabled("%s", Text);
		ImGui::PopTextWrapPos();
	}

	bool SearchBox(const char* Id, char* Buffer, size_t BufferSize, const char* Hint, float Width)
	{
		const float S = GetScale();
		const float IconSize = 12.0f * S;
		const float IconPad = IconSize + 10.0f * S;

		// 왼쪽 여백만 넓혀 돋보기 자리를 만든다.
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(IconPad, ImGui::GetStyle().FramePadding.y + 1.0f * S));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f * S);
		ImGui::SetNextItemWidth(Width > 0.0f ? Width : -FLT_MIN);
		const bool bChanged = ImGui::InputTextWithHint(Id, Hint, Buffer, BufferSize);
		ImGui::PopStyleVar(2);

		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		Draw(ImGui::GetWindowDrawList(), EEditorIcon::Search,
			ImVec2(Min.x + 6.0f * S + IconSize * 0.5f, (Min.y + Max.y) * 0.5f), IconSize,
			ImGui::GetColorU32(ImGuiCol_TextDisabled));
		return bChanged;
	}

	bool MatchesFilter(const char* Text, const char* Filter)
	{
		if (!Filter || !Filter[0])
			return true;
		if (!Text)
			return false;
		for (const char* Start = Text; *Start; ++Start)
		{
			const char* T = Start;
			const char* F = Filter;
			while (*T && *F && std::tolower(static_cast<unsigned char>(*T)) == std::tolower(static_cast<unsigned char>(*F)))
			{
				++T;
				++F;
			}
			if (!*F)
				return true;
		}
		return false;
	}

	float CalcButtonWidth(bool bHasIcon, const char* Label, bool bDropdown, float Height)
	{
		const float S = GetScale();
		const float H = Height > 0.0f ? Height : 28.0f * S;
		if (!Label && !bDropdown)
			return H;   // 아이콘만 있으면 정사각형

		const float IconSize = ImMin(18.0f * S, H - 8.0f * S);
		float Width = 6.0f * S;
		if (bHasIcon) Width += IconSize;
		if (Label) Width += (bHasIcon ? 6.0f * S : 0.0f) + ImGui::CalcTextSize(Label, nullptr, true).x;
		if (bDropdown) Width += 5.0f * S + 8.0f * S;
		return Width + 6.0f * S;
	}

	bool Button(const char* Id, EEditorIcon Icon, ImU32 IconColor, const char* Label,
		bool bDropdown, bool bSelected, float Height, ImGuiButtonFlags Flags)
	{
		const float S = GetScale();
		const float H = Height > 0.0f ? Height : 28.0f * S;
		const float IconSize = ImMin(18.0f * S, H - 8.0f * S);
		const float PadX = 6.0f * S;
		const float ChevronSize = 8.0f * S;
		const bool bHasIcon = Icon != EEditorIcon::None;
		const float LabelWidth = Label ? ImGui::CalcTextSize(Label, nullptr, true).x : 0.0f;
		const float Width = CalcButtonWidth(bHasIcon, Label, bDropdown, Height);

		const ImVec2 Min = ImGui::GetCursorScreenPos();
		const bool bPressed = ImGui::InvisibleButton(Id, ImVec2(Width, H), Flags);
		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = ImGui::IsItemActive();
		const ImVec2 Max(Min.x + Width, Min.y + H);

		ImDrawList* DL = ImGui::GetWindowDrawList();
		if (bHeld || bHovered || bSelected)
		{
			const ImU32 Bg = ImGui::GetColorU32(bHeld ? ImGuiCol_ButtonActive : bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Header);
			DL->AddRectFilled(Min, Max, Bg, 3.0f * S);
		}

		const float MidY = (Min.y + Max.y) * 0.5f;
		float X = Min.x + PadX;
		if (!Label && !bDropdown)
			X = Min.x + (Width - IconSize) * 0.5f;

		if (bHasIcon)
		{
			Draw(DL, Icon, ImVec2(X + IconSize * 0.5f, MidY), IconSize, ApplyStyleAlpha(IconColor));
			X += IconSize + 6.0f * S;
		}
		if (Label)
		{
			const float TextH = ImGui::GetFontSize();
			DL->AddText(ImVec2(X, MidY - TextH * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), Label, ImGui::FindRenderedTextEnd(Label));
			X += LabelWidth + 5.0f * S;
		}
		if (bDropdown)
		{
			if (!Label) X -= 2.0f * S;
			Draw(DL, EEditorIcon::ChevronDown, ImVec2(X + ChevronSize * 0.5f, MidY), ChevronSize,
				ImGui::GetColorU32(ImGuiCol_Text, 0.85f));
		}
		return bPressed;
	}

	void MenuSection(const char* Title)
	{
		const float S = GetScale();
		char Upper[64];
		size_t I = 0;
		for (; Title[I] && I + 1 < sizeof(Upper); ++I)
			Upper[I] = static_cast<char>(std::toupper(static_cast<unsigned char>(Title[I])));
		Upper[I] = '\0';

		ImGui::Dummy(ImVec2(0.0f, 2.0f * S));
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f * S);
		ImGui::TextDisabled("%s", Upper);
	}

	bool MenuRow(const char* Label, EEditorIcon Icon, bool bChecked, bool bEnabled, bool bKeepOpen,
		const char* Shortcut, bool bRadio)
	{
		const float S = GetScale();
		const float H = ImGui::GetFontSize() + 10.0f * S;
		const float CheckX = 12.0f * S, IconX = 34.0f * S, TextX = 54.0f * S;
		const float ShortcutWidth = Shortcut ? ImGui::CalcTextSize(Shortcut).x + 28.0f * S : 0.0f;
		const float RowWidth = TextX + ImGui::CalcTextSize(Label).x + 24.0f * S + ShortcutWidth;

		ImGui::PushID(Label);
		ImGui::BeginDisabled(!bEnabled);
		const ImVec2 Min = ImGui::GetCursorScreenPos();
		const bool bPressed = ImGui::Selectable("##Row", false,
			bKeepOpen ? ImGuiSelectableFlags_NoAutoClosePopups : ImGuiSelectableFlags_None,
			ImVec2(ImMax(RowWidth, ImGui::GetContentRegionAvail().x), H));
		const bool bHovered = ImGui::IsItemHovered();

		ImDrawList* DL = ImGui::GetWindowDrawList();
		const ImU32 TextCol = ImGui::GetColorU32(ImGuiCol_Text);
		const float MidY = Min.y + H * 0.5f;
		const float IconSize = 14.0f * S;
		if (bRadio)
			DL->AddCircleFilled(ImVec2(Min.x + CheckX, MidY), 3.5f * S,
				bChecked ? TextCol : ImGui::GetColorU32(ImGuiCol_WindowBg));
		else if (bChecked)
			Draw(DL, EEditorIcon::Check, ImVec2(Min.x + CheckX, MidY), 12.0f * S, TextCol);
		if (Icon != EEditorIcon::None)
			Draw(DL, Icon, ImVec2(Min.x + IconX, MidY), IconSize, ImGui::GetColorU32(ImGuiCol_Text, 0.8f));
		DL->AddText(ImVec2(Min.x + TextX, MidY - ImGui::GetFontSize() * 0.5f), TextCol, Label);
		if (Shortcut)
		{
			const float RowMaxX = ImGui::GetItemRectMax().x;
			DL->AddText(ImVec2(RowMaxX - 10.0f * S - ImGui::CalcTextSize(Shortcut).x, MidY - ImGui::GetFontSize() * 0.5f),
				ImGui::GetColorU32(ImGuiCol_TextDisabled), Shortcut);
		}

		ImGui::EndDisabled();
		ImGui::PopID();
		return bPressed;
	}
}
