#include "EnginePCH.h"
#include "Editor/EditorUI/ImGuiRenderer.h"

#include "Core/Window.h"
#include "Input/InputSystem.h"
#include "Core/Misc/Paths.h"

#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <d3dcompiler.h>
#include <cstring>
#pragma comment(lib, "d3dcompiler.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
// imgui_impl_dx11.cpp에 정의만 있고 헤더에는 아직 선언이 없다.
void ImGui_ImplDX11_SetSwapChainDescs(const DXGI_SWAP_CHAIN_DESC* desc_templates, int desc_templates_count);

namespace
{
	// FWindow가 ImGui를 직접 알지 않도록 메시지 처리기를 훅으로 등록한다.
	bool ImGuiWndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam) != 0;
	}

    // UE5 에디터 구조(어둡고 평평하고 촘촘한 레이아웃) + 분홍 머리 / 파란 눈 / 노란 핀 팔레트.
    // 포인트 색은 선택/활성 상태에만 쓰고, 평상시 위젯은 자두빛이 살짝 도는 회색으로 둔다.
    void ApplyDefaultStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        auto WithAlpha = [](const ImVec4& C, float A) { return ImVec4(C.x, C.y, C.z, A); };

        // ------------------------------------------------
        // Palette
        // ------------------------------------------------

        // 배경 단계 (붉은 기가 살짝 도는 다크 그레이)
        const ImVec4 Bg0   = ImVec4(0.071f, 0.059f, 0.067f, 1.00f); // #120F11 타이틀바, 비활성 탭
        const ImVec4 Bg1   = ImVec4(0.102f, 0.090f, 0.098f, 1.00f); // #1A1719 창 배경
        const ImVec4 Bg2   = ImVec4(0.141f, 0.125f, 0.137f, 1.00f); // #242023 팝업, 입력 hover
        const ImVec4 Bg3   = ImVec4(0.196f, 0.173f, 0.188f, 1.00f); // #322C30 버튼, 테이블 헤더
        const ImVec4 Bg4   = ImVec4(0.255f, 0.224f, 0.243f, 1.00f); // #41393E hover
        const ImVec4 Input = ImVec4(0.055f, 0.047f, 0.051f, 1.00f); // #0E0C0D 입력 칸
        const ImVec4 Line  = ImVec4(0.165f, 0.145f, 0.157f, 1.00f); // #2A2528 테두리/구분선

        // 텍스트 (크림빛 흰색)
        const ImVec4 Text    = ImVec4(0.949f, 0.925f, 0.933f, 1.00f); // #F2ECEE
        const ImVec4 TextDim = ImVec4(0.541f, 0.498f, 0.522f, 1.00f); // #8A7F85

        // 분홍 (머리카락) - 주 포인트: 선택/활성
        const ImVec4 Pink      = ImVec4(0.941f, 0.604f, 0.722f, 1.00f); // #F09AB8
        const ImVec4 PinkLight = ImVec4(0.969f, 0.737f, 0.816f, 1.00f); // #F7BCD0
        const ImVec4 PinkDeep  = ImVec4(0.769f, 0.369f, 0.518f, 1.00f); // #C45E84 흰 글자가 올라가는 활성 배경
        const ImVec4 PinkMuted = ImVec4(0.400f, 0.235f, 0.310f, 1.00f); // #663C4F 선택 행, 카테고리 헤더

        // 파랑 (눈) - 보조 포인트: 도킹/드래그/링크/네비게이션
        const ImVec4 Blue     = ImVec4(0.298f, 0.706f, 0.918f, 1.00f); // #4CB4EA
        const ImVec4 BlueDeep = ImVec4(0.165f, 0.525f, 0.784f, 1.00f); // #2A86C8

        // 노랑 (머리핀) - 작은 강조 표시
        const ImVec4 Yellow = ImVec4(0.949f, 0.824f, 0.294f, 1.00f); // #F2D24B

        // ------------------------------------------------
        // Colors
        // ------------------------------------------------

        // Text
        colors[ImGuiCol_Text]           = Text;
        colors[ImGuiCol_TextDisabled]   = TextDim;
        colors[ImGuiCol_TextLink]       = Blue;
        colors[ImGuiCol_TextSelectedBg] = WithAlpha(BlueDeep, 0.45f);

        // Window Background
        colors[ImGuiCol_WindowBg]       = Bg1;
        colors[ImGuiCol_ChildBg]        = WithAlpha(Bg2, 0.00f);
        colors[ImGuiCol_PopupBg]        = Bg2;
        colors[ImGuiCol_DockingEmptyBg] = Bg0;

        // Borders
        colors[ImGuiCol_Border]       = Line;
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        // Input Frames (UE5처럼 패널보다 어둡게 파인 칸)
        colors[ImGuiCol_FrameBg]         = Input;
        colors[ImGuiCol_FrameBgHovered]  = Bg2;
        colors[ImGuiCol_FrameBgActive]   = Bg3;
        colors[ImGuiCol_InputTextCursor] = Pink;

        // Title & Menu
        colors[ImGuiCol_TitleBg]          = Bg0;
        colors[ImGuiCol_TitleBgActive]    = Bg0;
        colors[ImGuiCol_TitleBgCollapsed] = Bg0;
        colors[ImGuiCol_MenuBarBg]        = Bg0;

        // Scrollbar
        colors[ImGuiCol_ScrollbarBg]          = WithAlpha(Bg1, 0.00f);
        colors[ImGuiCol_ScrollbarGrab]        = Bg3;
        colors[ImGuiCol_ScrollbarGrabHovered] = Bg4;
        colors[ImGuiCol_ScrollbarGrabActive]  = PinkDeep;

        // Checkbox
        colors[ImGuiCol_CheckboxSelectedBg] = PinkDeep;
        colors[ImGuiCol_CheckMark]          = PinkLight;

        // Slider
        colors[ImGuiCol_SliderGrab]       = Pink;
        colors[ImGuiCol_SliderGrabActive] = PinkLight;

        // Button (평상시 무채색, 누를 때만 분홍)
        colors[ImGuiCol_Button]        = Bg3;
        colors[ImGuiCol_ButtonHovered] = Bg4;
        colors[ImGuiCol_ButtonActive]  = PinkDeep;

        // Header, TreeNode, Selectable (선택 행과 CollapsingHeader가 같이 쓴다)
        colors[ImGuiCol_Header]        = PinkMuted;
        colors[ImGuiCol_HeaderHovered] = WithAlpha(Pink, 0.22f);
        colors[ImGuiCol_HeaderActive]  = PinkDeep;

        // Separator
        colors[ImGuiCol_Separator]        = Line;
        colors[ImGuiCol_SeparatorHovered] = WithAlpha(Pink, 0.60f);
        colors[ImGuiCol_SeparatorActive]  = Pink;

        // Resize Grip
        colors[ImGuiCol_ResizeGrip]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ResizeGripHovered] = WithAlpha(Pink, 0.50f);
        colors[ImGuiCol_ResizeGripActive]  = Pink;

        // Tabs (선택 탭은 창 배경과 이어지고 위에 분홍 라인)
        colors[ImGuiCol_Tab]                       = Bg0;
        colors[ImGuiCol_TabHovered]                = Bg4;
        colors[ImGuiCol_TabSelected]               = Bg1;
        colors[ImGuiCol_TabSelectedOverline]       = Pink;
        colors[ImGuiCol_TabDimmed]                 = Bg0;
        colors[ImGuiCol_TabDimmedSelected]         = Bg1;
        colors[ImGuiCol_TabDimmedSelectedOverline] = WithAlpha(Pink, 0.35f);

        // Docking
        colors[ImGuiCol_DockingPreview] = WithAlpha(Blue, 0.55f);

        // Graphs
        colors[ImGuiCol_PlotLines]            = Blue;
        colors[ImGuiCol_PlotLinesHovered]     = Pink;
        colors[ImGuiCol_PlotHistogram]        = Yellow;
        colors[ImGuiCol_PlotHistogramHovered] = Pink;

        // Tables
        colors[ImGuiCol_TableHeaderBg]     = Bg3;
        colors[ImGuiCol_TableBorderStrong] = Line;
        colors[ImGuiCol_TableBorderLight]  = WithAlpha(Line, 0.60f);
        colors[ImGuiCol_TableRowBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]     = ImVec4(1.00f, 1.00f, 1.00f, 0.025f);

        // Misc
        colors[ImGuiCol_TreeLines]             = Line;
        colors[ImGuiCol_DragDropTarget]        = Blue;
        colors[ImGuiCol_DragDropTargetBg]      = WithAlpha(Blue, 0.15f);
        colors[ImGuiCol_UnsavedMarker]         = Yellow;
        colors[ImGuiCol_NavCursor]             = Blue;
        colors[ImGuiCol_NavWindowingHighlight] = Blue;
        colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
        colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);

        // ------------------------------------------------
        // Layout & Spacing (UE5처럼 촘촘하게)
        // ------------------------------------------------

        // Padding
        style.WindowPadding    = ImVec2(8.0f, 8.0f);
        style.FramePadding     = ImVec2(6.0f, 3.0f);
        style.ItemSpacing      = ImVec2(6.0f, 4.0f);
        style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
        style.CellPadding      = ImVec2(4.0f, 2.0f);
        style.IndentSpacing    = 14.0f;

        // Widget Sizes
        style.GrabMinSize   = 8.0f;
        style.ScrollbarSize = 10.0f;

        // Rounding (거의 각지게)
        style.WindowRounding    = 0.0f;
        style.ChildRounding     = 0.0f;
        style.FrameRounding     = 2.0f;
        style.PopupRounding     = 2.0f;
        style.GrabRounding      = 2.0f;
        style.ScrollbarRounding = 2.0f;
        style.TabRounding       = 2.0f;

        // Borders (입력 칸/탭 테두리 없음, 창/팝업만 얇게)
        style.WindowBorderSize   = 1.0f;
        style.ChildBorderSize    = 1.0f;
        style.PopupBorderSize    = 1.0f;
        style.FrameBorderSize    = 0.0f;
        style.TabBorderSize      = 0.0f;
        style.TabBarOverlineSize = 2.0f;

        // Minimum Window Size
        style.WindowMinSize = ImVec2(160.0f, 100.0f);
    }

}

FImGuiRenderer::~FImGuiRenderer()
{

}

bool FImGuiRenderer::Init(HWND WindowHandle, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui_ImplWin32_Init(WindowHandle);
	ImGui_ImplDX11_Init(Device, DeviceContext);

	// 분리된 ImGui 창의 스왑체인을 메인 창(FSwapchain)과 같은 플립 모델로 맞춘다.
	// 백엔드 기본값(DISCARD, 버퍼 1개)을 플립 모델 메인 창과 섞어 Present하면 Present가 끝나지 않고 에디터가 멈췄다.
	DXGI_SWAP_CHAIN_DESC ViewportSwapChainDesc{};
	ViewportSwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ViewportSwapChainDesc.SampleDesc.Count = 1;
	ViewportSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	ViewportSwapChainDesc.BufferCount = 2;
	ViewportSwapChainDesc.Windowed = TRUE;
	ViewportSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	ImGui_ImplDX11_SetSwapChainDescs(&ViewportSwapChainDesc, 1);
	// Reuse ImGui's vertex shader and draw commands; only array-depth sampling differs.
	ComPtr<ID3DBlob> Bytecode;
	ComPtr<ID3DBlob> Errors;
	HRESULT PreviewResult = D3DCompileFromFile(FPaths::EngineFile(L"Resources/Shader/ShadowPreview.hlsl").c_str(), nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainPS", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
		Bytecode.GetAddressOf(), Errors.GetAddressOf());
	if (FAILED(PreviewResult))
	{
		if (Errors) OutputDebugStringA(static_cast<const char*>(Errors->GetBufferPointer()));
	}
	else
	{
		PreviewResult = Device->CreatePixelShader(Bytecode->GetBufferPointer(), Bytecode->GetBufferSize(),
			nullptr, ShadowPreviewPS.GetAddressOf());
		D3D11_BUFFER_DESC BufferDesc{};
		BufferDesc.ByteWidth = 16;
		BufferDesc.Usage = D3D11_USAGE_DEFAULT;
		BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (SUCCEEDED(PreviewResult)) PreviewResult = Device->CreateBuffer(&BufferDesc, nullptr, ShadowPreviewCB.GetAddressOf());
		D3D11_SAMPLER_DESC SamplerDesc{};
		SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		if (SUCCEEDED(PreviewResult)) PreviewResult = Device->CreateSamplerState(&SamplerDesc, ShadowPreviewSampler.GetAddressOf());
	}
	if (FAILED(PreviewResult))
	{
		ShadowPreviewPS.Reset();
		ShadowPreviewCB.Reset();
		ShadowPreviewSampler.Reset();
	}
	FWindow::SetWndProcHook(&ImGuiWndProcHook);

	ApplyDefaultStyle();

	//// 프로세스가 Per-Monitor DPI Aware라 창이 실제 픽셀 크기로 잡힌다. 모니터 배율(예: 150%)만큼 UI를 키운다.
	//// 1.92+ 동적 폰트 아틀라스라 FontScaleDpi로 키워도 글자가 해당 크기로 다시 래스터화되어 흐려지지 않는다.
	const float DpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(WindowHandle);
	ImGuiStyle& Style = ImGui::GetStyle();
	Style.ScaleAllSizes(DpiScale);     // 패딩·간격·스크롤바 등 크기 (ApplyDefaultStyle 이후에 적용)
	Style.FontScaleDpi = DpiScale;     // 글자 크기
	//// 다른 배율의 모니터로 창을 옮기면 글자 배율을 자동으로 갱신한다.
	io.ConfigDpiScaleFonts = true;
	io.ConfigDpiScaleViewports = true;

	return true;
}

void FImGuiRenderer::Begin()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();
}

void FImGuiRenderer::End()
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();
	// Detached Details windows consume their draw commands above as well.
	PreviewTextures.clear();
}

bool FImGuiRenderer::DrawShadowMap(ID3D11ShaderResourceView* AtlasSRV, unsigned int AtlasIndex,
	ImVec2 Size, ImVec2 UV0, ImVec2 UV1, float Contrast)
{
	if (!AtlasSRV || !ShadowPreviewPS || !ShadowPreviewCB || !ShadowPreviewSampler) return false;
	PreviewTextures.emplace_back(AtlasSRV);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddCallback(BindShadowPreview, this);
	
    // Copy the constants into ImGui-owned storage until actual GPU draws.
	unsigned int Constants[4] = { AtlasIndex, 0, 0, 0 };
	static_assert(sizeof(Contrast) == sizeof(Constants[1]));
	
    std::memcpy(&Constants[1], &Contrast, sizeof(Contrast));
	
    DrawList->AddCallback(SetShadowPreviewSlice, Constants, sizeof(Constants));
	ImGui::Image((ImTextureID)AtlasSRV, Size, UV0, UV1);
	
    DrawList->AddCallback(RestoreShadowPreview, this);
	DrawList->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState);
	return true;
}

void FImGuiRenderer::BindShadowPreview(const ImDrawList*, const ImDrawCmd* Command)
{
	auto* Self = static_cast<FImGuiRenderer*>(Command->UserCallbackData);
	auto* State = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
	ID3D11DeviceContext* Context = State->DeviceContext;
	Context->PSGetConstantBuffers(0, 1, Self->PreviousPSConstants.ReleaseAndGetAddressOf());
	Context->PSSetShader(Self->ShadowPreviewPS.Get(), nullptr, 0);
	ID3D11Buffer* Buffer = Self->ShadowPreviewCB.Get();
	Context->PSSetConstantBuffers(0, 1, &Buffer);
	ID3D11SamplerState* Sampler = Self->ShadowPreviewSampler.Get();
	Context->PSSetSamplers(0, 1, &Sampler);
}

void FImGuiRenderer::SetShadowPreviewSlice(const ImDrawList*, const ImDrawCmd* Command)
{
	auto* State = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
	ComPtr<ID3D11Buffer> Buffer;
	State->DeviceContext->PSGetConstantBuffers(0, 1, Buffer.GetAddressOf());
	State->DeviceContext->UpdateSubresource(Buffer.Get(), 0, nullptr, Command->UserCallbackData, 0, 0);
}

void FImGuiRenderer::RestoreShadowPreview(const ImDrawList*, const ImDrawCmd* Command)
{
	auto* Self = static_cast<FImGuiRenderer*>(Command->UserCallbackData);
	auto* State = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
	ID3D11Buffer* Buffer = Self->PreviousPSConstants.Get();
	State->DeviceContext->PSSetConstantBuffers(0, 1, &Buffer);
	Self->PreviousPSConstants.Reset();
}

void FImGuiRenderer::Shutdown()
{
	PreviewTextures.clear();
	PreviousPSConstants.Reset();
	ShadowPreviewSampler.Reset();
	ShadowPreviewCB.Reset();
	ShadowPreviewPS.Reset();
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
}
