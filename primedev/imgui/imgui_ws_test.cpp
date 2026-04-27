#include "imgui_ws_test.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "core/vanilla.h"
#include "core/tier1.h"
#include "engine/r2engine.h"
#include <mutex>

#define CINTERFACE
#include "dxgi.h"
#undef CINTERFACE
#include "d3d11.h"

// thread-local to allow for different threads to have different contexts,
// i.e. render thread for most menus, other threads for squirrel things
thread_local ImGuiContext* ImGuiThreadContext;

static ImGuiContext* ImGuiRenderThreadContext;
static ImGuiContext* ImGuiSquirrelThreadContext;

static bool isInited = false;

static ID3D11Device** device = nullptr; 
static ID3D11DeviceContext** deviceContext = nullptr;
static IDXGISwapChain** swapChain = nullptr;

static std::mutex sqRenderSnapshot_mutex;
static ImDrawDataSnapshot sqRenderSnapshot;

static ConVar* Cvar_imgui_mode = nullptr;

enum CursorCode
{
	dc_user,
	dc_none,
	dc_arrow,
	dc_ibeam,
	dc_hourglass,
	dc_waitarrow,
	dc_crosshair,
	dc_up,
	dc_sizenwse,
	dc_sizenesw,
	dc_sizewe,
	dc_sizens,
	dc_sizeall,
	dc_no,
	dc_hand,
	dc_blank,
	dc_last,
};

static bool engineCursorVisible = false;

class ISurface
{
public:
	struct VTable
	{
		void* unknown1[61];
		void (*SetCursor)(ISurface* surface, unsigned int cursor);
		bool (*IsCursorVisible)(ISurface* surface);
		void* unknown2[11];
		void (*UnlockCursor)(ISurface* surface);
		void (*LockCursor)(ISurface* surface);
	};

	VTable* m_vtable;
};

static ISurface* VGUI_Surface031 = nullptr;

static int GetImGuiMode()
{
	if (!Cvar_imgui_mode)
		return 0;

	// vanilla compat mode - don't allow drawing on the screen because I don't want people to get accidentally fairfight banned
	// they can still access the websocket version though
	if (g_pVanillaCompatibility->GetVanillaCompatibility())
		return 0;

	return Cvar_imgui_mode->GetInt();
}

// a menu for debugging the ImGui integration systems
static void RenderImGuiDebug()
{
	ImGui::Text("Engine cursor visible: %i", engineCursorVisible);
}

// a small panel for controlling the imgui_mode
static void RenderImGuiDebugControls()
{
	if (ImGui::Begin("Debug Controls", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking))
	{
		if (GetImGuiMode() == 2)
		{
			ImGui::Text("ImGui debug is in overlay mode");
			ImGui::NewLine();
			ImGui::Text("Enter 'imgui_mode 0' into the console to close ImGui");
			ImGui::Text("Enter 'imgui_mode 1' into the console to return to normal mode");

			ImGui::End();
			return;
		}

		if (ImGui::Button("Close"))
			Cvar_imgui_mode->SetValue(0);
		if (ImGui::Button("Overlay Mode"))
			Cvar_imgui_mode->SetValue(2);
	}
	ImGui::End();
}

void RenderScriptThing(const char* message)
{
	// context was set up on the other thread, so the TLS variable isnt set
	if (ImGui::GetCurrentContext() == nullptr)
		ImGui::SetCurrentContext(ImGuiSquirrelThreadContext);
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	for (int i = 0; i < 10; ++i)
	{
		ImGui::NewFrame();
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, {0.226, 0.16, 0.476, 1});

		if (ImGui::Begin("HELLO WORLD", 0, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextColored({0.4, 1, 0.4, 1}, "This text is green!");
			ImGui::TextColored({1, 0.4, 0.4, 1}, "This text is red!");
			ImGui::Separator();
			ImGui::Text("The following text is from UI script:\n\n%s", message);
		}
		else
		{
			spdlog::error("NO IMGUI MENU????");
		}
		ImGui::End();

		ImGui::PopStyleColor(1);

		ImGui::Render();

		const std::lock_guard<std::mutex> lock(sqRenderSnapshot_mutex);
		sqRenderSnapshot.SnapUsingSwap(ImGui::GetDrawData(), ImGui::GetTime());
	}
}

void BeginScriptFrame()
{
	// context was set up on the other thread, so the TLS variable isnt set
	if (ImGui::GetCurrentContext() == nullptr)
		ImGui::SetCurrentContext(ImGuiSquirrelThreadContext);

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();
}

void EndScriptFrame()
{
	ImGui::Render();

	const std::lock_guard<std::mutex> lock(sqRenderSnapshot_mutex);
	sqRenderSnapshot.SnapUsingSwap(ImGui::GetDrawData(), ImGui::GetTime());
}

void ImGuiDisplay::Start()
{
	IMGUI_CHECKVERSION();
	m_context = ImGui::CreateContext();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGuiRenderThreadContext = m_context;
	strcpy(ImGuiRenderThreadContext->ContextName, "Render");
	// ImGui::GetIO().MouseDrawCursor = true;

	// get the hwnd from directx
	DXGI_SWAP_CHAIN_DESC desc;
	(*swapChain)->lpVtbl->GetDesc(*swapChain, &desc);

	ImGui_ImplWin32_Init(desc.OutputWindow);
	ImGui_ImplDX11_Init(*device, *deviceContext);

	ImGui::StyleColorsDark();
	ImGui::GetStyle().AntiAliasedFill = false;
	ImGui::GetStyle().AntiAliasedLines = false;
	ImGui::GetStyle().WindowRounding = 0.0f;
	ImGui::GetStyle().ScrollbarRounding = 0.0f;

	// todo: move this to UI vm instantiation?
	if (!ImGuiSquirrelThreadContext)
	{
		auto* previousContext = ImGui::GetCurrentContext();
		ImGuiSquirrelThreadContext = ImGui::CreateContext();
		strcpy(ImGuiSquirrelThreadContext->ContextName, "Squirrel");

		ImGui::SetCurrentContext(ImGuiSquirrelThreadContext);
		ImGui_ImplWin32_Init(desc.OutputWindow);
		ImGui_ImplDX11_Init(*device, *deviceContext);
		ImGui::SetCurrentContext(previousContext);
	}

	isInited = true;
}

void ImGuiDisplay::Render()
{
	if (!isInited)
		Start();

	auto& io = ImGui::GetIO();

	ImGui::SetCurrentContext(m_context);
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// prevent the docking background from covering the entire game
	ImGui::SetNextWindowBgAlpha(0);
	// add a dock space over the entire screen
	ImGui::DockSpaceOverViewport(0, 0, ImGuiDockNodeFlags_PassthruCentralNode, 0);

	const bool isOverlay = GetImGuiMode() == 2;
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, isOverlay ? 0.75f : 1.f);

	if (GetImGuiMode())
		RenderImGuiDebugControls();

	for (auto& menu : m_menus)
		menu.Render();

	// dear imgui demo. please never delete this.
	if (m_showDemoWindow)
		ImGui::ShowDemoWindow(&m_showDemoWindow);

	// main menu bar
	if (ImGui::BeginMainMenuBar())
	{
		ImGui::MenuItem("DearImGui Demo", 0, &m_showDemoWindow);
		for (auto& menu : m_menus)
			menu.RenderMenuItem();

		ImGui::EndMainMenuBar();
	}

	// clear our overlay style
	ImGui::PopStyleVar();

	// generate ImDrawData
	ImGui::Render();

	auto* drawData = ImGui::GetDrawData();
	if (GetImGuiMode())
		ImGui_ImplDX11_RenderDrawData(drawData);
}

void ImGuiDisplay::Shutdown()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext(m_context);
	isInited = false;
	m_menus.clear();
}

void ImGuiDisplay::RegisterMenu(const char* name, ImGuiRenderCallback callback, const char* shortcut)
{
	m_menus.emplace_back(name, callback, shortcut);
}

ImGuiDisplay& ImGuiDisplay::GetInstance()
{
	static ImGuiDisplay* display = nullptr;

	if (display == nullptr)
	{
		display = new ImGuiDisplay();
		// add the ImGui debug menu always first
		display->RegisterMenu("ImGui Debug", RenderImGuiDebug, nullptr);
	}

	return *display;
}

static HRESULT (*o_pPresent)(IDXGISwapChain* This, UINT SyncInterval, UINT Flags) = nullptr;
static HRESULT h_Present(IDXGISwapChain* This, UINT SyncInterval, UINT Flags)
{
	{
		const std::lock_guard<std::mutex> lock(sqRenderSnapshot_mutex);
		if (&sqRenderSnapshot.DrawData)
		{
			ImGui_ImplDX11_RenderDrawData(&sqRenderSnapshot.DrawData);
		}
	}

	// render our ImGui, todo: handle draw data on MainThread and only render it on RenderThread? I'm concerned about threading and fetching server/client data for ImGui rendering.
	ImGuiDisplay::GetInstance().Render();

	return o_pPresent(This, SyncInterval, Flags);
}

// this does various things, including initing the swapChain
static __int64 (*o_pSub_15470)(HWND hWnd, __int64 a2) = nullptr;
static __int64 h_sub_15470(HWND hWnd, __int64 a2)
{
	// swapChain is inited after here, so now we can hook the present func
	auto ret = o_pSub_15470(hWnd, a2);

	// create present hook
	auto* present = (*swapChain)->lpVtbl->Present;
	MH_CreateHook(present, h_Present, (void**)&o_pPresent);
	MH_EnableHook(present);

	return ret;
}

static bool IsKeyMsg(UINT uMsg)
{
	return uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST;
}

static bool IsMouseMsg(UINT uMsg)
{
	if (uMsg == WM_INPUT)
		return true;
	return uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandlerEx(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, ImGuiIO& io);
static int (*o_pGameWndProc)(void* game, const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) = nullptr;
static int h_GameWndProc(void* game, const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//spdlog::warn("MESSAGE: {}", uMsg);

	auto& instance = ImGuiDisplay::GetInstance();

	// debug imgui, takes all mouse and keyboard input while not in overlay-mode
	if (ImGuiRenderThreadContext != nullptr && GetImGuiMode() == 1)
	{
		auto res = ImGui_ImplWin32_WndProcHandlerEx(hWnd, uMsg, wParam, lParam, ImGuiRenderThreadContext->IO);

		if (IsMouseMsg(uMsg) || IsKeyMsg(uMsg))
			return res;
	}

	// squirrel imgui, only takes input when it needs to.
	if (ImGuiSquirrelThreadContext != nullptr)
	{
		auto res = ImGui_ImplWin32_WndProcHandlerEx(hWnd, uMsg, wParam, lParam, ImGuiSquirrelThreadContext->IO);

		if (IsMouseMsg(uMsg) && ImGuiSquirrelThreadContext->IO.WantCaptureMouse)
			return res;
		if (IsKeyMsg(uMsg) && ImGuiSquirrelThreadContext->IO.WantCaptureKeyboard)
			return res;
	}

	//spdlog::warn("GOT THROUGH");

	return o_pGameWndProc(game, hWnd, uMsg, wParam, lParam);
}

static void (*o_pSetCursor)(ISurface* surface, unsigned int cursor) = nullptr;
static void h_setCursor(ISurface* surface, unsigned int cursor)
{
	// keep track of if the game wants a cursor
	engineCursorVisible = (cursor != dc_user && cursor != dc_none && cursor != dc_blank);

	o_pSetCursor(surface, cursor);
}

static void (*o_pLockCursor)(ISurface* surface) = nullptr;
static void h_lockCursor(ISurface* surface)
{
	// only allow locking the cursor if ImGui doesn't want it
	if (GetImGuiMode() == 1)
		return;

	o_pLockCursor(surface);
}

ON_DLL_LOAD("materialsystem_dx11.dll", ImGuiMaterialSystem, (CModule module))
{
	device = module.Offset(0x14E8DD0).RCast<ID3D11Device**>();
	deviceContext = module.Offset(0x14E8DD8).RCast<ID3D11DeviceContext**>();
	swapChain = module.Offset(0x14EE258).RCast<IDXGISwapChain**>();

	o_pSub_15470 = module.Offset(0x15470).RCast<decltype(o_pSub_15470)>();
	HookAttach(&(PVOID&)o_pSub_15470, h_sub_15470);
}

ON_DLL_LOAD_RELIESON("inputsystem.dll", ImGuiInput, ConVar, (CModule module))
{
	o_pGameWndProc = module.Offset(0x8B80).RCast<decltype(o_pGameWndProc)>();
	HookAttach(&(PVOID&)o_pGameWndProc, h_GameWndProc);

	Cvar_imgui_mode = new ConVar("imgui_mode", "0", FCVAR_GAMEDLL | FCVAR_DONTRECORD, "Change ImGui debug display mode: 0 - none, 1 - full, 2 - overlay");
}

ON_DLL_LOAD("vguimatsurface.dll", ImGuiVGui, (CModule module))
{
	VGUI_Surface031 = Sys_GetFactoryPtr("vguimatsurface.dll", "VGUI_Surface031").RCast<ISurface*>();

	auto* setCursor = VGUI_Surface031->m_vtable->SetCursor;
	MH_CreateHook(setCursor, h_setCursor, (void**)&o_pSetCursor);
	MH_EnableHook(setCursor);

	auto* lockCursor = VGUI_Surface031->m_vtable->LockCursor;
	MH_CreateHook(lockCursor, h_lockCursor, (void**)&o_pLockCursor);
	MH_EnableHook(lockCursor);
}
