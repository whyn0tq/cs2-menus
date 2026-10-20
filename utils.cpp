#include <stdio.h>
#include "utils.h"
#include "menus_internal.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"
#include <serversideclient.h>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <chrono>
#include <fstream>

Menus g_Menus;
PLUGIN_EXPOSE(Menus, g_Menus);

Menus::Menus() :
	m_DispatchConCommand(&ICvar::DispatchConCommand, this, &Menus::OnDispatchConCommand, &Menus::OnDispatchConCommandPost),
	m_GameFrame(&IServerGameDLL::GameFrame, this, nullptr, &Menus::GameFrame),
	m_GameFramePost(&IServerGameDLL::GameFrame, this, nullptr, &Menus::OnGameFramePost),
	m_GameServerSteamAPIActivated(&IServerGameDLL::GameServerSteamAPIActivated, this, &Menus::OnGameServerSteamAPIActivated, &Menus::OnGameServerSteamAPIActivatedPost),
	m_PreWorldUpdate(&IServerGameDLL::PreWorldUpdate, this, &Menus::OnPreWorldUpdate, &Menus::OnPreWorldUpdatePost),
	m_ServerHibernationUpdate(&IServerGameDLL::ServerHibernationUpdate, this, &Menus::OnServerHibernationUpdate, &Menus::OnServerHibernationUpdatePost),
	m_GameServerSteamAPIDeactivated(&IServerGameDLL::GameServerSteamAPIDeactivated, this, &Menus::OnGameServerSteamAPIDeactivated, &Menus::OnGameServerSteamAPIDeactivatedPost),
	m_HostNameChanged(&IServerGameDLL::OnHostNameChanged, this, &Menus::OnHostNameChanged, &Menus::OnHostNameChangedPost),
	m_PreFatalShutdown(&IServerGameDLL::PreFatalShutdown, this, &Menus::OnPreFatalShutdown, &Menus::OnPreFatalShutdownPost),
	m_UpdateWhenNotInGame(&IServerGameDLL::UpdateWhenNotInGame, this, &Menus::OnUpdateWhenNotInGame, &Menus::OnUpdateWhenNotInGamePost),
	m_ServerConVarChanged(&IServerGameDLL::ServerConVarChanged, this, &Menus::OnServerConVarChanged, &Menus::OnServerConVarChangedPost),
	m_PostEventAbstract(&IGameEventSystem::PostEventAbstract, this, &Menus::OnPostEventAbstract, &Menus::OnPostEventAbstractPost),
	m_SetClientListening(&IVEngineServer2::SetClientListening, this, &Menus::OnSetClientListening, &Menus::OnSetClientListeningPost),
	m_StartupServer(&INetworkServerService::StartupServer, this, nullptr, &Menus::StartupServer),
	m_CheckTransmit(&ISource2GameEntities::CheckTransmit, this, nullptr, &Menus::OnCheckTransmit),
	m_ClientCommand(&IServerGameClients::ClientCommand, this, &Menus::ClientCommand, nullptr),
	m_ClientDisconnect(&IServerGameClients::ClientDisconnect, this, nullptr, &Menus::OnClientDisconnect),
	m_ClientPutInServer(&IServerGameClients::ClientPutInServer, this, nullptr, &Menus::OnClientPutInServer),
	m_OnClientConnected(&IServerGameClients::OnClientConnected, this, &Menus::OnClientConnected, nullptr),
	m_ClientConnect(&IServerGameClients::ClientConnect, this, &Menus::OnClientConnect, nullptr),
	m_ClientSvcUserMessage(&IServerGameClients::ClientSvcUserMessage, this, &Menus::OnClientSvcUserMessage, nullptr),
	m_ClientActive(&IServerGameClients::ClientActive, this, nullptr, &Menus::OnClientActive),
	m_ClientFullyConnect(&IServerGameClients::ClientFullyConnect, this, nullptr, &Menus::OnClientFullyConnect),
	m_ClientSettingsChanged(&IServerGameClients::ClientSettingsChanged, this, &Menus::OnClientSettingsChanged, nullptr),
	m_ProcessUsercmds(&IServerGameClients::ProcessUsercmds, this, &Menus::OnProcessUsercmds, nullptr),
	m_ClientVoice(&IServerGameClients::ClientVoice, this, &Menus::OnClientVoice, nullptr),
	m_ClientCommandKeyValues(&IServerGameClients::ClientCommandKeyValues, this, &Menus::OnClientCommandKeyValues, nullptr),
	m_FireEvent(&IGameEventManager2::FireEvent, this, &Menus::OnFireEvent, &Menus::OnFireEventPost),
	m_OnTakeDamageAlive(static_cast<std::uint32_t>(-1), this, &Menus::Hook_OnTakeDamage_Alive, nullptr),
	m_ServerSideClientConnect(static_cast<std::uint32_t>(KHook::GetVtableIndex(&CServerSideClientBase::Connect)), this, nullptr, &Menus::OnServerSideClientClientConnect),
	m_PerformDisconnection(static_cast<std::uint32_t>(KHook::GetVtableIndex(&CServerSideClientBase::PerformDisconnection)), this, &Menus::OnCServerSideClientlientPerformDisconnection, nullptr),
	m_ProcessTick(static_cast<std::uint32_t>(KHook::GetVtableIndex(&CServerSideClientBase::ProcessTick)), this, &Menus::OnProcessTick, nullptr),
	m_ProcessStringCmd(static_cast<std::uint32_t>(KHook::GetVtableIndex(&CServerSideClientBase::ProcessStringCmd)), this, &Menus::OnProcessStringCmd, nullptr),
	m_BuildGameSessionManifest(&IGameSystem::OnBuildGameSessionManifest, this, nullptr, &Menus::OnBuildGameSessionManifest)
{
}

CGlobalVars* gpGlobals = nullptr;
IVEngineServer2* engine = nullptr;
CCSGameRules* g_pGameRules = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CPhysicsQuery* g_pGameTraceManager = nullptr;
IGameEventSystem* g_gameEventSystem = nullptr;
IGameEventManager2* gameeventmanager = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
INetworkGameServer* g_pNetworkGameServer = nullptr;

float g_flUniversalTime;
float g_flLastTickedTime;
bool g_bHasTicked;

int g_iCommitSuicide = 0;
int g_iRemoveWeapons = 0;
int g_iChangeTeam = 0;
int g_iCollisionRulesChanged = 0;
int g_iTeleport = 0;
int g_iRespawn = 0;
int g_iDropWeapon = 0;

KeyValues* g_hKVData;

int g_iMenuType[64];
int g_iMenuItem[64];
std::chrono::milliseconds g_iMenuLastButtonInput[64];
MenuPlayer g_MenuPlayer[64];
std::string g_TextMenuPlayer[64];

std::map<std::string, std::string> g_vecPhrases;

std::map<std::string, std::map<std::string, int>> g_Offsets;
std::map<std::string, std::map<std::string, int>> g_ChainOffsets;

MenusApi* g_pMenusApi = nullptr;
IMenusApi* g_pMenusCore = nullptr;

UtilsApi* g_pUtilsApi = nullptr;
IUtilsApi* g_pUtilsCore = nullptr;

PlayersApi* g_pPlayersApi = nullptr;
IPlayersApi* g_pPlayersCore = nullptr;

LayoutApi* g_pLayoutApi = nullptr;
ILayoutApi* g_pLayoutCore = nullptr;

ICookiesApi* g_pCookies = nullptr;

char szLanguage[16];
int g_iMenuTypeDefault;
int g_iMenuTime;
int g_iDelayAuthFailKick;
bool g_bMenuAddon;
const char* g_szMenuURL;
bool g_bMenuFlashFix;
bool g_bAccessUserChangeType;
bool g_bStopingUser;
bool g_bPanoramaMenu;
int g_iTimeoutMenu;
int g_iSoundType;
std::string g_szServerID;
bool g_bAllowDisableNotify;
bool g_bNotifyDisabledDefault;
bool g_bNotifyDisabled[64];

std::string g_szSettingsCommand;

std::map<std::string, std::string> g_mapSounds;
std::unordered_map<std::string, CHandle<CCSCustomHudLayout>> g_mapHudLayouts[64];
std::unordered_map<std::string, CHandle<CCSCustomHudLayout>> g_mapGlobalHudLayouts;

std::unordered_map<const Menu*, std::string> g_mapMenuDesc;
std::string g_szMenuDesc[64];

std::unordered_map<const Menu*, std::vector<ItemExtra>> g_mapItemExtra;
std::vector<ItemExtra> g_vItemExtra[64];

ItemExtra& UTIL_PushItemExtra(Menu& hMenu)
{
	auto& ex = g_mapItemExtra[&hMenu];
	ex.resize(hMenu.hItems.size());
	ex.back() = ItemExtra{};
	return ex.back();
}

ItemExtra* UTIL_ResolveItemExtra(const ItemRef& ref)
{
	if (!ref.pMenu || ref.iIndex < 0) return nullptr;
	auto it = g_mapItemExtra.find(ref.pMenu);
	if (it == g_mapItemExtra.end()) return nullptr;
	if (ref.iIndex >= (int)it->second.size()) return nullptr;
	return &it->second[ref.iIndex];
}

std::vector<std::string> g_vCommandEater;
std::map<int, std::vector<int>> g_mapTransmitState;

void* g_pServerSideClientVTable = nullptr;
void* g_pCCSPlayerPawnVTable = nullptr;
void* g_pEntityDebugGameSystemVTable = nullptr;

std::vector<std::string> g_mapPrecache;

std::map<int, std::map<std::string, CommandCallback>> ConsoleCommands;
std::map<int, std::map<std::string, CommandCallback>> ChatCommands;
Player* m_Players[64];

struct SndOpEventGuid_t;
void (*UTIL_Remove)(CEntityInstance*) = nullptr;
int (*UTIL_TakeDamage)(CCSPlayer_DamageReactServices*, CTakeDamageInfo*) = nullptr;
bool (*UTIL_IsHearingClient)(void* serverClient, int index) = nullptr;
void (*UTIL_Say)(const CCommandContext& ctx, CCommand& args) = nullptr;
void (*UTIL_SetModel)(CBaseModelEntity*, const char* szModel) = nullptr;
void (*UTIL_DispatchSpawn)(CEntityInstance*, CEntityKeyValues*) = nullptr;
void (*UTIL_SayTeam)(const CCommandContext& ctx, CCommand& args) = nullptr;
void (*UTIL_SwitchTeam)(CCSPlayerController* pPlayer, int iTeam) = nullptr;
void (*UTIL_StopSoundEvent)(CBaseEntity *pEntity, const char *pszSound) = nullptr;
void (*UTIL_RespawnPlayer)(CBasePlayerController* pController, CCSPlayerPawn* pPawn, bool a3, bool a4, bool a5, bool a6) = nullptr;
IGameEventListener2* (*UTIL_GetLegacyGameEventListener)(CPlayerSlot slot) = nullptr;
CBaseEntity* (*UTIL_CreateEntity)(const char *pClassName, CEntityIndex iForceEdictIndex) = nullptr;
void (*UTIL_SetMoveType)(CBaseEntity *pThis, MoveType_t nMoveType, MoveCollide_t nMoveCollide) = nullptr;
SndOpEventGuid_t (*UTIL_EmitSoundFilter)(uint8_t unk1[32], IRecipientFilter& filter, CEntityIndex ent, const EmitSound_t& params);
void (*UTIL_AcceptInput)(CEntityInstance* pThis, const char* pInputName, CEntityInstance* pActivator, CEntityInstance* pCaller, variant_t& pValue) = nullptr;
bool (*UTIL_TraceShape)(CPhysicsQuery*, const Ray_t* ray, const Vector* start, const Vector* end, CTraceFilter* filter, trace_t* trace) = nullptr;
void (*UTIL_TerminateRound)(CGameRules* pGameRules, float delay, unsigned int reason, int64 teamid) = nullptr;
AcquireResult::Type (*UTIL_CanAcquire)(CPlayer_ItemServices* pItemServices, CEconItemView* pItemView, AcquireMethod::Type eMethod, uint* pLimit) = nullptr;

using namespace DynLibUtils;

funchook_t* m_SayHook;
funchook_t* m_SayTeamHook;
funchook_t* m_TakeDamageHook;
funchook_t* m_IsHearingClientHook;
funchook_t* m_CanAcquireHook;

std::vector<std::string> SplitStringBySpace(const std::string& input);
bool containsOnlyDigits(const std::string& str) {
	return str.find_first_not_of("0123456789") == std::string::npos;
}

void SayTeamHook(const CCommandContext& ctx, CCommand& args)
{
	bool bCallback = true;
	bCallback = g_pUtilsApi->SendChatListenerPreCallback(ctx.GetPlayerSlot().Get(), args.ArgS(), true);
	if(args[1][0])
	{
		if(g_pEntitySystem)
		{
			auto pController = CCSPlayerController::FromSlot(ctx.GetPlayerSlot().Get());
			if(bCallback && pController && pController->GetPawn() && pController->m_steamID() != 0 && g_MenuPlayer[ctx.GetPlayerSlot().Get()].bEnabled && containsOnlyDigits(std::string(args[1] + 1)))
				bCallback = false;
		}
	}
	bCallback = g_pUtilsApi->SendChatListenerPostCallback(ctx.GetPlayerSlot().Get(), args.ArgS(), bCallback, true);
	if(bCallback)
	{
		UTIL_SayTeam(ctx, args);
	}
}

void SayHook(const CCommandContext& ctx, CCommand& args)
{
	bool bCallback = true;
	bCallback = g_pUtilsApi->SendChatListenerPreCallback(ctx.GetPlayerSlot().Get(), args.ArgS(), false);
	if(args[1][0])
	{
		if(g_pEntitySystem)
		{
			auto pController = CCSPlayerController::FromSlot(ctx.GetPlayerSlot().Get());
			if(bCallback && pController && pController->GetPawn() && pController->m_steamID() != 0 && g_MenuPlayer[ctx.GetPlayerSlot().Get()].bEnabled && containsOnlyDigits(std::string(args[1] + 1)))
				bCallback = false;
		}
	}
	bCallback = g_pUtilsApi->SendChatListenerPostCallback(ctx.GetPlayerSlot().Get(), args.ArgS(), bCallback, false);
	if(bCallback)
	{
		UTIL_Say(ctx, args);
	}
}

int Hook_TakeDamage(CCSPlayer_DamageReactServices* pService, CTakeDamageInfo* info)
{
    if (!pService || !info) return UTIL_TakeDamage(pService, info);

    CCSPlayerPawn* pPawn = pService->GetPawn();
    if (!pPawn) return UTIL_TakeDamage(pService, info);

    auto pController = pPawn->m_hController();
    if (!pController) return UTIL_TakeDamage(pService, info);

    int iPlayerSlot = pController->GetEntityIndex().Get() - 1;
    if (iPlayerSlot < 0 || iPlayerSlot >= 64) return UTIL_TakeDamage(pService, info);

    if (!g_pUtilsApi->SendHookOnTakeDamagePre(iPlayerSlot, info)) return 1;
 	
	return UTIL_TakeDamage(pService, info);
}

std::string Colorizer(std::string str)
{
	for (size_t i = 0; i < std::size(colors_hex); i++)
	{
		size_t pos = 0;

		while ((pos = str.find(colors_text[i], pos)) != std::string::npos)
		{
			str.replace(pos, colors_text[i].length(), colors_hex[i]);
			pos += colors_hex[i].length();
		}
	}

	return str;
}

void* Menus::OnMetamodQuery(const char* iface, int* ret)
{
	if (!strcmp(iface, MENUS_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pMenusCore;
	}
	if (!strcmp(iface, UTILS_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pUtilsCore;
	}
	if (!strcmp(iface, PLAYERS_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pPlayersCore;
	}
	if (!strcmp(iface, LAYOUT_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pLayoutCore;
	}

	*ret = META_IFACE_FAILED;
	return nullptr;
}

int CheckActionMenu(int iSlot, CCSPlayerController* pController, int iButton)
{
	if(!pController) return 0;
	if(iSlot < 0 || iSlot >= 64) return 0;
	auto& hMenuPlayer = g_MenuPlayer[iSlot];
	auto& hMenu = hMenuPlayer.hMenu;
	if(hMenuPlayer.bEnabled)
	{
		hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;
		if(iButton == 9 && hMenu.bExit)
		{
			hMenuPlayer.iList = 0;
			hMenuPlayer.bEnabled = false;
			if(g_iMenuType[iSlot] == 0)
			{
				for (size_t i = 0; i < 8; i++)
				{
					g_pUtilsCore->PrintToChat(iSlot, " \x08-\x01");
				}
			}
			if(hMenu.hFunc) hMenu.hFunc("exit", "exit", 9, iSlot);
			hMenuPlayer.hMenu.clear();
		}
		else if(iButton == 8)
		{
			int iItems = size(hMenu.hItems) / 5;
			if (size(hMenu.hItems) % 5 > 0) iItems++;
			if(iItems > hMenuPlayer.iList+1)
			{
				hMenuPlayer.iList++;
				g_pMenusCore->DisplayPlayerMenu(hMenu, iSlot, false, true);
				if(hMenu.hFunc) hMenu.hFunc("next", "next", 8, iSlot);
			}
		}
		else if(iButton == 7)
		{
			if(hMenuPlayer.iList != 0 || hMenuPlayer.hMenu.bBack)
			{
				if(hMenuPlayer.iList != 0)
				{
					hMenuPlayer.iList--;
					g_pMenusCore->DisplayPlayerMenu(hMenu, iSlot, false, true);
				}
				else if(hMenu.hFunc) hMenu.hFunc("back", "back", 7, iSlot);
			}
		}
		else
		{
			int iItems = size(hMenu.hItems);
			int iItem = hMenuPlayer.iList*5+iButton-1;
			if(iItems <= iItem) return 1;
			if(hMenu.hItems.size() <= iItem) return 1;
			if(hMenu.hItems[iItem].iType != 1) return 1;
			if(hMenu.hFunc) hMenu.hFunc(hMenu.hItems[iItem].sBack.c_str(), hMenu.hItems[iItem].sText.c_str(), iButton, iSlot);
		}
		return 1;
	}
	return 0;
}

bool FASTCALL IsHearingClient(void* serverClient, int index)
{
	return g_pUtilsApi->SendHookOnHearingClient(index)?UTIL_IsHearingClient(serverClient, index):false;
}

AcquireResult::Type CanAcquireHook(CPlayer_ItemServices* pItemServices, CEconItemView* pItemView, AcquireMethod::Type eMethod, uint* pLimit)
{
	if(!pItemServices || !pItemView)
		return UTIL_CanAcquire(pItemServices, pItemView, eMethod, pLimit);

	CCSPlayerPawn* pPawn = pItemServices->GetPawn();
	if(!pPawn)
		return UTIL_CanAcquire(pItemServices, pItemView, eMethod, pLimit);
	CCSPlayerController* pController = (CCSPlayerController*)pPawn->GetController();
	if(!pController)
		return UTIL_CanAcquire(pItemServices, pItemView, eMethod, pLimit);
	int iSlot = pController->GetPlayerSlot();
	if(iSlot < 0 || iSlot >= 64)
		return UTIL_CanAcquire(pItemServices, pItemView, eMethod, pLimit);

	bool bHandled = false;
	AcquireResult::Type result = g_pUtilsApi->SendCanAcquirePre(iSlot, pItemServices, pItemView, eMethod, bHandled);

	if(!bHandled)
		result = UTIL_CanAcquire(pItemServices, pItemView, eMethod, pLimit);

	g_pUtilsApi->SendCanAcquirePost(iSlot, pItemServices, pItemView, eMethod);
	return result;
}

void UtilsApi::SetTransmitState(int iEntityIndex, bool bState, std::vector<int> vecSlots)
{
	if(vecSlots.empty()) {
		for (int i = 0; i < 64; i++)
		{
			if(bState) g_mapTransmitState[iEntityIndex].erase(std::remove(g_mapTransmitState[iEntityIndex].begin(), g_mapTransmitState[iEntityIndex].end(), i), g_mapTransmitState[iEntityIndex].end());
			else g_mapTransmitState[iEntityIndex].push_back(i);
		}
	} else {
		for (int i = 0; i < vecSlots.size(); i++)
		{
			if(bState) g_mapTransmitState[iEntityIndex].erase(std::remove(g_mapTransmitState[iEntityIndex].begin(), g_mapTransmitState[iEntityIndex].end(), vecSlots[i]), g_mapTransmitState[iEntityIndex].end());
			else g_mapTransmitState[iEntityIndex].push_back(vecSlots[i]);
		}
	}
}


void Menus::AllPluginsLoaded() {
	char error[64];
	int ret;
	g_pCookies = (ICookiesApi *)g_SMAPI->MetaFactory(COOKIES_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pCookies = nullptr;
		g_hKVData = new KeyValues("Data");
		const char *pszPath = "addons/data/menus_data.ini";
		if (!g_hKVData->LoadFromFile(g_pFullFileSystem, pszPath)) {
			char szPath2[256];
			g_SMAPI->Format(szPath2, sizeof(szPath2), "%s/%s", g_SMAPI->GetBaseDir(), pszPath);
			std::fstream file;
			file.open(szPath2, std::fstream::out | std::fstream::trunc);
			file << "\"Data\"\n{\n\n}\n";
			file.close();
		}
		return;
	}
	g_pCookies->HookClientCookieLoaded(g_PLID, [](int iSlot) {
		const char* szMenuType = g_pCookies->GetCookie(iSlot, "Utils.MenuType");
		if (szMenuType && szMenuType[0]) g_iMenuType[iSlot] = UTIL_SanitizeMenuType(atoi(szMenuType));
		else g_iMenuType[iSlot] = UTIL_SanitizeMenuType(g_iMenuTypeDefault);

		const char* szNotifyDisabled = g_pCookies->GetCookie(iSlot, "Utils.NotifyDisabled");
		if (szNotifyDisabled && szNotifyDisabled[0]) g_bNotifyDisabled[iSlot] = atoi(szNotifyDisabled) != 0;
		else g_bNotifyDisabled[iSlot] = g_bNotifyDisabledDefault;
	});
}

int UTIL_SanitizeMenuType(int iType)
{
	if(!g_bPanoramaMenu && iType == 3)
		return (g_iMenuTypeDefault == 3) ? 0 : g_iMenuTypeDefault;
	return iType;
}

int GetClientCookieMenuType(int iSlot)
{
	if(g_pPlayersApi->IsFakeClient(iSlot)) return g_iMenuTypeDefault;
	uint64 m_steamID = g_pPlayersApi->GetSteamID64(iSlot);
	if(m_steamID == 0) return g_iMenuTypeDefault;
	char szSteamID[64];
	g_SMAPI->Format(szSteamID, sizeof(szSteamID), "%llu", m_steamID);
	KeyValues *hData = g_hKVData->FindKey(szSteamID, false);
	if(!hData) return g_iMenuTypeDefault;
	return UTIL_SanitizeMenuType(hData->GetInt("Utils.MenuType", g_iMenuTypeDefault));
}

bool GetClientCookieNotifyDisabled(int iSlot)
{
	if(g_pPlayersApi->IsFakeClient(iSlot)) return g_bNotifyDisabledDefault;
	uint64 m_steamID = g_pPlayersApi->GetSteamID64(iSlot);
	if(m_steamID == 0) return g_bNotifyDisabledDefault;
	char szSteamID[64];
	g_SMAPI->Format(szSteamID, sizeof(szSteamID), "%llu", m_steamID);
	KeyValues *hData = g_hKVData->FindKey(szSteamID, false);
	if(!hData) return g_bNotifyDisabledDefault;
	return hData->GetInt("Utils.NotifyDisabled", g_bNotifyDisabledDefault ? 1 : 0) != 0;
}

bool SetClientCookie(int iSlot, const char* sCookieName, const char* sData)
{
	if(g_pPlayersApi->IsFakeClient(iSlot)) return false;
	uint64 m_steamID = g_pPlayersApi->GetSteamID64(iSlot);
	if(m_steamID == 0) return false;
	char szSteamID[64];
	g_SMAPI->Format(szSteamID, sizeof(szSteamID), "%llu", m_steamID);
	KeyValues *hData = g_hKVData->FindKey(szSteamID, true);
	hData->SetString(sCookieName, sData);
	g_hKVData->SaveToFile(g_pFullFileSystem, "addons/data/menus_data.ini");
	return true;
}

void SettingMenu(int iSlot)
{
	Menu hMenu;
	hMenu.szTitle = g_vecPhrases["MenuMenusTitle"];
	g_pMenusCore->AddItemMenu(hMenu, "0", g_vecPhrases["MenuMenusItem0"].c_str(), g_iMenuType[iSlot] == 0 ? ITEM_DISABLED : ITEM_DEFAULT);
	g_pMenusCore->AddItemMenu(hMenu, "1", g_vecPhrases["MenuMenusItem1"].c_str(), g_iMenuType[iSlot] == 1 ? ITEM_DISABLED : ITEM_DEFAULT);
	g_pMenusCore->AddItemMenu(hMenu, "2", g_vecPhrases["MenuMenusItem2"].c_str(), g_iMenuType[iSlot] == 2 ? ITEM_DISABLED : ITEM_DEFAULT);

	if(g_bPanoramaMenu)
		g_pMenusCore->AddItemMenu(hMenu, "3", g_vecPhrases["MenuMenusItem3"].c_str(), g_iMenuType[iSlot] == 3 ? ITEM_DISABLED : ITEM_DEFAULT);
	g_pMenusCore->SetExitMenu(hMenu, true);
	g_pMenusCore->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot){
		if(iItem < 7) {
			int iType = std::atoi(szBack);
			iType = UTIL_SanitizeMenuType(iType);
			g_iMenuType[iSlot] = iType;
			if(g_pCookies) g_pCookies->SetCookie(iSlot, "Utils.MenuType", std::to_string(iType).c_str());
			else SetClientCookie(iSlot, "Utils.MenuType", std::to_string(iType).c_str());
			SettingMenu(iSlot);
		}
	});
	g_pMenusCore->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void UtilsApi::OpenSettingsMenu(int iSlot)
{
	Menu hMenu;
	hMenu.szTitle = g_vecPhrases["MenuSettingsTitle"];

	if (g_bAccessUserChangeType)
		g_pMenusCore->AddItemMenu(hMenu, "settings_menu_type", g_vecPhrases["MenuMenusTitle"].c_str(), ITEM_DEFAULT);

	if (g_bAllowDisableNotify) {
		g_pMenusCore->AddToggleMenu(hMenu, "settings_notify_toggle",
			g_vecPhrases["MenuNotifyToggle"].c_str(),
			g_bNotifyDisabled[iSlot],
			[](const char* szBack, bool bState, int iItem, int iSlot) {
				g_bNotifyDisabled[iSlot] = bState;
				if (g_pCookies) g_pCookies->SetCookie(iSlot, "Utils.NotifyDisabled", bState ? "1" : "0");
				else SetClientCookie(iSlot, "Utils.NotifyDisabled", bState ? "1" : "0");
			});
	}

	SendHookOnSettingsOpen(iSlot, hMenu);

	g_pMenusCore->SetExitMenu(hMenu, true);
	g_pMenusCore->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
		if (strcmp(szBack, "settings_menu_type") == 0) {
			SettingMenu(iSlot);
			return;
		}
		if (strcmp(szBack, "settings_notify_toggle") == 0) {
			g_bNotifyDisabled[iSlot] = !g_bNotifyDisabled[iSlot];
			if (g_pCookies) g_pCookies->SetCookie(iSlot, "Utils.NotifyDisabled", g_bNotifyDisabled[iSlot] ? "1" : "0");
			else SetClientCookie(iSlot, "Utils.NotifyDisabled", g_bNotifyDisabled[iSlot] ? "1" : "0");
			g_pUtilsApi->OpenSettingsMenu(iSlot);
			return;
		}
		g_pUtilsApi->SendHookOnSettingsItem(szBack, szFront, iItem, iSlot);
	});
	g_pMenusCore->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

KHook::Return<void> Menus::OnCheckTransmit(ISource2GameEntities*, CCheckTransmitInfo **pInfoInfoList, int nInfoCount, CBitVec<16384> &unionTransmitEdicts, CBitVec<16384> &unionTransmitEdicts2, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntityIndices)
{
	g_pUtilsApi->SendEntityCheckTransmit(pInfoInfoList, nInfoCount, unionTransmitEdicts, unionTransmitEdicts2, pNetworkables, pEntityIndicies, nEntityIndices, false);
	if (!g_pEntitySystem) return { KHook::Action::Ignore };

	for (int i = 0; i < nInfoCount; i++)
	{
		auto &pInfo = pInfoInfoList[i];
		int iPlayerSlot = reinterpret_cast<CCheckTransmitInfoHack*>(pInfo)->m_nPlayerSlot;

		CCSPlayerController* pSelfController = CCSPlayerController::FromSlot(iPlayerSlot);
		if (!pSelfController) continue;

		if(g_mapTransmitState.size() > 0)
		{
			for (auto& [iEntityIndex, vecSlots] : g_mapTransmitState)
			{
				if(vecSlots.size() > 0)
				{
					if(std::find(vecSlots.begin(), vecSlots.end(), iPlayerSlot) != vecSlots.end())
					{
						pInfo->m_pTransmitEntity->Clear(iEntityIndex);
					}
				}
			}
		}
	}
	return { KHook::Action::Ignore };
}

static std::string FindHudLayout(int iSlot, CEntityInstance* pLayout)
{
	if (!pLayout)
		return "";

	for (auto& [name, hndl] : g_mapHudLayouts[iSlot])
		if (hndl.Get() == pLayout)
			return name;

	for (auto& [name, hndl] : g_mapGlobalHudLayouts)
		if (hndl.Get() == pLayout)
			return name;

	return "";
}

KHook::Return<void> Menus::OnClientSvcUserMessage(IServerGameClients*, CPlayerSlot slot, int um_type, uint32 size, const void *buf )
{
	g_pPlayersApi->ClientSvcUserMessage(slot.Get(), um_type, size, buf);
	if (um_type != CS_UM_CustomHudClicked)
		return { KHook::Action::Ignore };

	CCSUsrMsg_CustomHudClicked msg;
	if (!msg.ParseFromArray(buf, size))
		return { KHook::Action::Ignore };

	const int iSlot = slot.Get();
	const std::string sButtonId = msg.button_id();
	const char* szButton = sButtonId.c_str();
	CEntityInstance* pCustomLayout = CEntityHandle::FromPackedInt(msg.custom_hud_layout()).Get();
	std::string sLayoutId = FindHudLayout(iSlot, pCustomLayout);
	if (sLayoutId.empty())
		return { KHook::Action::Ignore };
	UTIL_HandleLayoutClick(iSlot, sLayoutId.c_str(), szButton);
	g_pLayoutApi->SendCustomHudClickedCallback(iSlot, sLayoutId.c_str(), szButton);
	return { KHook::Action::Ignore };
}

void Menus::OnEntityCreated(CEntityInstance* pEntity)
{
	g_pUtilsApi->SendEntityCreated(pEntity);
}

void Menus::OnEntitySpawned(CEntityInstance* pEntity)
{
	g_pUtilsApi->SendEntitySpawned(pEntity);
}

void Menus::OnEntityDeleted(CEntityInstance* pEntity)
{
	g_pUtilsApi->SendEntityDeleted(pEntity);
}

void Menus::OnEntityParentChanged(CEntityInstance* pEntity, CEntityInstance* pNewParent)
{
	g_pUtilsApi->SendEntityParentChanged(pEntity, pNewParent);
}

bool Menus::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	g_SMAPI->AddListener( this, this );

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_gameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameEntities, ISource2GameEntities, SOURCE2GAMEENTITIES_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);

	m_DispatchConCommand.Add(g_pCVar);
	m_GameFrame.Add(g_pSource2Server);
	m_GameFramePost.Add(g_pSource2Server);
	m_GameServerSteamAPIActivated.Add(g_pSource2Server);
	m_PreWorldUpdate.Add(g_pSource2Server);
	m_ServerHibernationUpdate.Add(g_pSource2Server);
	m_GameServerSteamAPIDeactivated.Add(g_pSource2Server);
	m_HostNameChanged.Add(g_pSource2Server);
	m_PreFatalShutdown.Add(g_pSource2Server);
	m_UpdateWhenNotInGame.Add(g_pSource2Server);
	m_ServerConVarChanged.Add(g_pSource2Server);
	m_PostEventAbstract.Add(g_gameEventSystem);
	m_SetClientListening.Add(engine);
	m_ClientCommand.Add(g_pSource2GameClients);
	m_StartupServer.Add(g_pNetworkServerService);
	m_ClientDisconnect.Add(g_pSource2GameClients);
	m_ClientPutInServer.Add(g_pSource2GameClients);
	m_OnClientConnected.Add(g_pSource2GameClients);
	m_ClientConnect.Add(g_pSource2GameClients);
	m_CheckTransmit.Add(g_pSource2GameEntities);
	m_ClientSvcUserMessage.Add(g_pSource2GameClients);
	m_ClientActive.Add(g_pSource2GameClients);
	m_ClientFullyConnect.Add(g_pSource2GameClients);
	m_ClientSettingsChanged.Add(g_pSource2GameClients);
	m_ProcessUsercmds.Add(g_pSource2GameClients);
	m_ClientVoice.Add(g_pSource2GameClients);
	m_ClientCommandKeyValues.Add(g_pSource2GameClients);

	
	ConVar_Register(FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_GAMEDLL);

	if (late)
	{
		g_pGameEntitySystem = GameEntitySystem();
		g_pEntitySystem = g_pGameEntitySystem;
		gpGlobals = engine->GetServerGlobals();
	}

	//0 - в чат
	//1 - также как в чат но в центр
	//2 - выбор через WASD 
	//3 - panorama menu

	g_pMenusApi = new MenusApi();
	g_pMenusCore = g_pMenusApi;

	g_pUtilsApi = new UtilsApi();
	g_pUtilsCore = g_pUtilsApi;

	g_pPlayersApi = new PlayersApi();
	g_pPlayersCore = g_pPlayersApi;

	g_pLayoutApi = new LayoutApi();
	g_pLayoutCore = g_pLayoutApi;

	{
		KeyValues* g_kvCore = new KeyValues("Core");
		const char *pszPath = "addons/configs/core.cfg";

		if (!g_kvCore->LoadFromFile(g_pFullFileSystem, pszPath))
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to load %s\n", g_PLAPI->GetLogTag(), pszPath);
			return false;
		}

		g_SMAPI->Format(szLanguage, sizeof(szLanguage), "%s", g_kvCore->GetString("ServerLang", "en"));
		g_iMenuTypeDefault = g_kvCore->GetInt("MenuType", 0);
		g_iMenuTime = g_kvCore->GetInt("MenuTime", 60);
		g_bMenuAddon = g_kvCore->GetBool("MenuAddon", false);
		g_szMenuURL = g_kvCore->GetString("MenuURL");
		g_bMenuFlashFix = g_kvCore->GetBool("MenuFlashFix", false);
		g_iDelayAuthFailKick = g_kvCore->GetInt("delay_auth_fail_kick", 30);
		g_bAccessUserChangeType = g_kvCore->GetBool("AccessUserChangeType", true);
		g_bStopingUser = g_kvCore->GetBool("StopingUser", false);
		g_bPanoramaMenu = g_kvCore->GetBool("PanoramaMenu", true);
		g_iTimeoutMenu = g_kvCore->GetInt("TimeoutInputMenu", 160);
		g_iSoundType = g_kvCore->GetInt("sound_type", 0);
		g_szServerID = g_kvCore->GetString("server_id", "0");
		g_szSettingsCommand = g_kvCore->GetString("SettingsCommand", "!settings");
		g_bAllowDisableNotify = g_kvCore->GetBool("AllowDisableNotify", true);
		g_bNotifyDisabledDefault = g_kvCore->GetBool("NotifyDisabledDefault", false);

		g_mapSounds.clear();
		const char* szBackSound = g_kvCore->GetString("sound_back", "");
		if(szBackSound && szBackSound[0]) g_mapSounds["back"] = szBackSound;
		const char* szNextSound = g_kvCore->GetString("sound_next", "");
		if(szNextSound && szNextSound[0]) g_mapSounds["next"] = szNextSound;
		const char* szSelectSound = g_kvCore->GetString("sound_select", "");
		if(szSelectSound && szSelectSound[0]) g_mapSounds["select"] = szSelectSound;
		const char* szExitSound = g_kvCore->GetString("sound_exit", "");
		if(szExitSound && szExitSound[0]) g_mapSounds["exit"] = szExitSound;
		const char* szMoveSound = g_kvCore->GetString("sound_move", "");
		if(szMoveSound && szMoveSound[0]) g_mapSounds["move"] = szMoveSound;

		const char* szCommandEater = g_kvCore->GetString("commands_eater");
		if(szCommandEater && szCommandEater[0])
		{
			std::string sCommandEater(szCommandEater);
			std::istringstream ss(sCommandEater);
			std::string token;
			while(std::getline(ss, token, ','))
			{
				g_vCommandEater.push_back(token);
			}
		}

		if(g_bAccessUserChangeType)
		{
			const char* g_szMenuChangeCommand = g_kvCore->GetString("MenuChangeCommand");
			g_pUtilsApi->RegCommand(g_PLID, {}, {g_szMenuChangeCommand}, [](int iSlot, const char* szContent){
				if(g_pPlayersApi->IsFakeClient(iSlot)) return false;
				SettingMenu(iSlot);
				return false;
			});
		}

		g_pUtilsApi->RegCommand(g_PLID, {"mm_settings"}, {g_szSettingsCommand}, [](int iSlot, const char* szContent){
			if(g_pPlayersApi->IsFakeClient(iSlot)) return false;
			g_pUtilsApi->OpenSettingsMenu(iSlot);
			return false;
		});

		// g_pUtilsApi->RegCommand(g_PLID, {"notifytest"}, {"notifytest"}, [](int iSlot, const char* szContent){
		// 	if(g_pPlayersApi->IsFakeClient(iSlot)) return true;

		// 	auto tokens = SplitStringBySpace(szContent ? szContent : "");
		// 	int iType = (tokens.size() > 1) ? atoi(tokens[1].c_str()) : 0;
		// 	int iPos  = (tokens.size() > 2) ? atoi(tokens[2].c_str()) : 0;
		// 	float flDur = (tokens.size() > 3) ? (float)atof(tokens[3].c_str()) : 5.0f;
		// 	bool bAll = (tokens.size() > 4) ? (atoi(tokens[4].c_str()) != 0) : false;

		// 	static const char* const s_szTitles[] = { "Успех", "Внимание", "Ошибка" };
		// 	int iT = (iType < 0 || iType > 2) ? 0 : iType;
		// 	char szText[128];
		// 	g_SMAPI->Format(szText, sizeof(szText), "Тест тоста: type=%d pos=%d dur=%.1f", iType, iPos, flDur);
		// 	char szChat[160];
		// 	g_SMAPI->Format(szChat, sizeof(szChat), " \x04[Notify]\x01 %s: %s", s_szTitles[iT], szText);

		// 	if(bAll)
		// 		g_pUtilsApi->ShowNotifyAll(iType, s_szTitles[iT], szText, flDur, iPos, szChat);
		// 	else
		// 		g_pUtilsApi->ShowNotify(iSlot, iType, s_szTitles[iT], szText, flDur, iPos, szChat);
		// 	return true;
		// });
	}

	g_pUtilsApi->LoadTranslations("menus.phrases");

	{
		KeyValues::AutoDelete g_kvPrecacher("Precacher");
		const char* pszPrecachePath = "addons/configs/precacher.ini";
		if (g_kvPrecacher->LoadFromFile(g_pFullFileSystem, pszPrecachePath))
		{
			FOR_EACH_VALUE(g_kvPrecacher, pValue)
			{
				const char* szResource = pValue->GetString(nullptr, nullptr);
				if (szResource && szResource[0])
					g_pUtilsApi->AddPrecache(szResource);
			}
		}
	}

	KeyValues::AutoDelete g_kvSigs("Gamedata");
	const char *pszPath = "addons/configs/signatures.ini";

	if (!g_kvSigs->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		g_pUtilsApi->ErrorLog("[%s] Failed to load %s\n", g_PLAPI->GetLogTag(), pszPath);
		return false;
	}
	CModule libserver(g_pSource2Server);
	const char* pszSay = g_kvSigs->GetString("UTIL_Say");
	if(pszSay && pszSay[0]) {
		UTIL_SayTeam = libserver.FindPattern(pszSay).RCast< decltype(UTIL_SayTeam) >();
		if (!UTIL_SayTeam)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_Say", g_PLAPI->GetLogTag());
		}
		else
		{
			m_SayTeamHook = funchook_create();
			funchook_prepare(m_SayTeamHook, (void**)&UTIL_SayTeam, (void*)SayTeamHook);
			funchook_install(m_SayTeamHook, 0);
		}

		UTIL_Say = libserver.FindPattern(pszSay).RCast< decltype(UTIL_Say) >();
		if (!UTIL_Say)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_Say", g_PLAPI->GetLogTag());
		}
		else
		{
			m_SayHook = funchook_create();
			funchook_prepare(m_SayHook, (void**)&UTIL_Say, (void*)SayHook);
			funchook_install(m_SayHook, 0);
		}
	}

	const char* pszTakeDamage = g_kvSigs->GetString("OnTakeDamagePre");
	if(pszTakeDamage && pszTakeDamage[0]) {
		UTIL_TakeDamage = libserver.FindPattern(pszTakeDamage).RCast< decltype(UTIL_TakeDamage) >();
		if (!UTIL_TakeDamage)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_TakeDamage", g_PLAPI->GetLogTag());
		}
		else
		{
			m_TakeDamageHook = funchook_create();
			funchook_prepare(m_TakeDamageHook, (void**)&UTIL_TakeDamage, (void*)Hook_TakeDamage);
			funchook_install(m_TakeDamageHook, 0);
		}
	}

	CModule libengine(engine);
	const char* pszIsHearingClient = g_kvSigs->GetString("IsHearingClient");
	if(pszIsHearingClient && pszIsHearingClient[0]) {
		UTIL_IsHearingClient = libengine.FindPattern(g_kvSigs->GetString("IsHearingClient")).RCast< decltype(UTIL_IsHearingClient) >();
		if (!UTIL_IsHearingClient)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get IsHearingClient", g_PLAPI->GetLogTag());
		}
		else
		{
			m_IsHearingClientHook = funchook_create();
			funchook_prepare(m_IsHearingClientHook, (void**)&UTIL_IsHearingClient, (void*)IsHearingClient);
			funchook_install(m_IsHearingClientHook, 0);
		}
	}

	const char* pszSetModel = g_kvSigs->GetString("CBaseModelEntity_SetModel");
	if(pszSetModel && pszSetModel[0]) {
		UTIL_SetModel = libserver.FindPattern(pszSetModel).RCast< decltype(UTIL_SetModel) >();
		if (!UTIL_SetModel)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get CBaseModelEntity_SetModel", g_PLAPI->GetLogTag());
		}
	}

	const char* pszRespawnPlayer = g_kvSigs->GetString("UTIL_RespawnPlayer");
	if(pszRespawnPlayer && pszRespawnPlayer[0]) {
		UTIL_RespawnPlayer = libserver.FindPattern(pszRespawnPlayer).RCast< decltype(UTIL_RespawnPlayer) >();
		if (!UTIL_RespawnPlayer)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_RespawnPlayer", g_PLAPI->GetLogTag());
		}
	}

	const char* pszAcceptInput = g_kvSigs->GetString("UTIL_AcceptInput");
	if(pszAcceptInput && pszAcceptInput[0]) {
		UTIL_AcceptInput = libserver.FindPattern(pszAcceptInput).RCast< decltype(UTIL_AcceptInput) >();
		if (!UTIL_AcceptInput)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_AcceptInput", g_PLAPI->GetLogTag());
		}
	}
	
	const char* pszRemove = g_kvSigs->GetString("UTIL_Remove");
	if(pszRemove && pszRemove[0]) {
		UTIL_Remove = libserver.FindPattern(pszRemove).RCast< decltype(UTIL_Remove) >();
		if (!UTIL_Remove)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_Remove", g_PLAPI->GetLogTag());
		}
	}

	const char* pszDispatchSpawn = g_kvSigs->GetString("UTIL_DispatchSpawn");
	if(pszDispatchSpawn && pszDispatchSpawn[0]) {
		UTIL_DispatchSpawn = libserver.FindPattern(pszDispatchSpawn).RCast< decltype(UTIL_DispatchSpawn) >();
		if (!UTIL_DispatchSpawn)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_DispatchSpawn", g_PLAPI->GetLogTag());
		}
	}

	const char* pszCreateEntity = g_kvSigs->GetString("UTIL_CreateEntity");
	if(pszCreateEntity && pszCreateEntity[0]) {
		UTIL_CreateEntity = libserver.FindPattern(pszCreateEntity).RCast< decltype(UTIL_CreateEntity) >();
		if (!UTIL_CreateEntity)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_CreateEntity", g_PLAPI->GetLogTag());
		}
	}

	const char* pszGetLegacyGameEventListener = g_kvSigs->GetString("GetLegacyGameEventListener");
	if(pszGetLegacyGameEventListener && pszGetLegacyGameEventListener[0]) {
		UTIL_GetLegacyGameEventListener = libserver.FindPattern(pszGetLegacyGameEventListener).RCast< decltype(UTIL_GetLegacyGameEventListener) >();
		if (!UTIL_GetLegacyGameEventListener)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get GetLegacyGameEventListener", g_PLAPI->GetLogTag());
		}
	}
	
	const char* pszSwitchTeam = g_kvSigs->GetString("SwitchTeam");
	if(pszSwitchTeam && pszSwitchTeam[0]) {
		UTIL_SwitchTeam = libserver.FindPattern(pszSwitchTeam).RCast< decltype(UTIL_SwitchTeam) >();
		if (!UTIL_SwitchTeam)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get SwitchTeam", g_PLAPI->GetLogTag());
		}
	}

	const char* pszSetMoveType = g_kvSigs->GetString("SetMoveType");
	if(pszSetMoveType && pszSetMoveType[0]) {
		UTIL_SetMoveType = libserver.FindPattern(pszSetMoveType).RCast< decltype(UTIL_SetMoveType) >();
		if (!UTIL_SetMoveType)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get SetMoveType", g_PLAPI->GetLogTag());
		}
	}
	
	const char* pszEmitSoundFilter = g_kvSigs->GetString("EmitSoundFilter");
	if(pszEmitSoundFilter && pszEmitSoundFilter[0]) {
		UTIL_EmitSoundFilter = libserver.FindPattern(pszEmitSoundFilter).RCast< decltype(UTIL_EmitSoundFilter) >();
		if (!UTIL_EmitSoundFilter)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_EmitSoundFilter", g_PLAPI->GetLogTag());
		}
	}

	const char* pszStopSoundEvent = g_kvSigs->GetString("StopSoundEvent");
	if(pszStopSoundEvent && pszStopSoundEvent[0]) {
		UTIL_StopSoundEvent = libserver.FindPattern(pszStopSoundEvent).RCast< decltype(UTIL_StopSoundEvent) >();
		if (!UTIL_StopSoundEvent)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get StopSoundEvent", g_PLAPI->GetLogTag());
		}
	}

	g_iCommitSuicide = g_kvSigs->GetInt("CommitSuicide", 0);
	g_iChangeTeam = g_kvSigs->GetInt("ChangeTeam", 0);
	g_iCollisionRulesChanged = g_kvSigs->GetInt("CollisionRulesChanged", 0);
	g_iTeleport = g_kvSigs->GetInt("Teleport", 0);
	g_iRespawn = g_kvSigs->GetInt("Respawn", 0);
	g_iDropWeapon = g_kvSigs->GetInt("DropWeapon", 0);
	g_iRemoveWeapons = g_kvSigs->GetInt("RemoveWeapons", 0);
	g_pCCSPlayerPawnVTable = libserver.GetVirtualTableByName("CCSPlayerPawn");
	if (!g_pCCSPlayerPawnVTable)
	{
		g_pUtilsApi->ErrorLog("[%s] Failed to find CCSPlayerPawn vtable", g_PLAPI->GetLogTag());
	}
	else
	{
		m_OnTakeDamageAlive.Configure(g_kvSigs->GetInt("OnTakeDamage_Alive"));
		m_OnTakeDamageAlive.AddGlobal(reinterpret_cast<CCSPlayerPawn*>(&g_pCCSPlayerPawnVTable));
	}


	g_pServerSideClientVTable = libengine.GetVirtualTableByName("CServerSideClient");
	if (g_pServerSideClientVTable)
	{
		m_ServerSideClientConnect.AddGlobal(reinterpret_cast<CServerSideClient*>(&g_pServerSideClientVTable));
		m_PerformDisconnection.AddGlobal(reinterpret_cast<CServerSideClient*>(&g_pServerSideClientVTable));
		m_ProcessTick.AddGlobal(reinterpret_cast<CServerSideClient*>(&g_pServerSideClientVTable));
		m_ProcessStringCmd.AddGlobal(reinterpret_cast<CServerSideClient*>(&g_pServerSideClientVTable));
	}

	g_pEntityDebugGameSystemVTable = libserver.GetVirtualTableByName("CEntityDebugGameSystem");
	if (g_pEntityDebugGameSystemVTable)
		m_BuildGameSessionManifest.AddGlobal(reinterpret_cast<IGameSystem*>(&g_pEntityDebugGameSystemVTable));

	const char* pszGameEventManager = g_kvSigs->GetString("GetGameEventManager");
	if(pszGameEventManager && pszGameEventManager[0]) {
		auto gameEventManagerFn = libserver.FindPattern(pszGameEventManager);
		if( !gameEventManagerFn ) {
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get GetGameEventManager", g_PLAPI->GetLogTag());
		}
		else
		{
			gameeventmanager = gameEventManagerFn.ResolveRelativeAddress(0x3, 0x7).GetValue<IGameEventManager2*>();
			m_FireEvent.Add(gameeventmanager);
		}
	}

	const char* pszTraceShape = g_kvSigs->GetString("UTIL_TraceShape");
	if(pszTraceShape && pszTraceShape[0]) {
		UTIL_TraceShape = libserver.FindPattern(pszTraceShape).RCast< decltype(UTIL_TraceShape) >();
		if (!UTIL_TraceShape)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get UTIL_TraceShape", g_PLAPI->GetLogTag());
		}
	}

	const char* pszTerminateRound = g_kvSigs->GetString("TerminateRound");
	if(pszTerminateRound && pszTerminateRound[0]) {
		UTIL_TerminateRound = libserver.FindPattern(pszTerminateRound).RCast< decltype(UTIL_TerminateRound) >();
		if (!UTIL_TerminateRound)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get TerminateRound", g_PLAPI->GetLogTag());
		}
	}

	const char* pszGameTraceManager = g_kvSigs->GetString("GetGameTraceManager");
	if(pszGameTraceManager && pszGameTraceManager[0]) {
		auto gameTraceManagerFn = libserver.FindPattern(pszGameTraceManager);
		if( !gameTraceManagerFn ) g_pUtilsApi->ErrorLog("[%s] Failed to find function to get GetGameTraceManager", g_PLAPI->GetLogTag());
		else g_pGameTraceManager = *gameTraceManagerFn.ResolveRelativeAddress(3, 7).RCast<CPhysicsQuery**>();
	}

	const char* pszCanAcquire = g_kvSigs->GetString("CanAcquire");
	if(pszCanAcquire && pszCanAcquire[0]) {
		UTIL_CanAcquire = libserver.FindPattern(pszCanAcquire).RCast< decltype(UTIL_CanAcquire) >();
		if (!UTIL_CanAcquire)
		{
			g_pUtilsApi->ErrorLog("[%s] Failed to find function to get CanAcquire", g_PLAPI->GetLogTag());
		}
		else
		{
			m_CanAcquireHook = funchook_create();
			funchook_prepare(m_CanAcquireHook, (void**)&UTIL_CanAcquire, (void*)CanAcquireHook);
			funchook_install(m_CanAcquireHook, 0);
		}
	}

	new CTimer(1.0f, []()
	{
		for(int i = 0; i < 64; i++)
		{
			if (!m_Players[i] || m_Players[i]->IsFakeClient() || m_Players[i]->IsAuthenticated())
				continue;
			
			if(engine->IsClientFullyAuthenticated(CPlayerSlot(i)))
			{
				m_Players[i]->SetAuthenticated(true);
				m_Players[i]->SetSteamId(m_Players[i]->GetUnauthenticatedSteamId());
				g_pPlayersApi->SendClientAuthCallback(i, m_Players[i]->GetUnauthenticatedSteamId64());
			}
		}
		return 1.0f;
	});

	g_pUtilsApi->RegCommand(g_PLID, {"mm_1"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 1)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_2"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 2)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_3"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 3)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_4"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 4)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_5"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 5)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_6"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 6)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_7"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 7)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_8"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 8)) return true;
		return false;
	});
	g_pUtilsApi->RegCommand(g_PLID, {"mm_9"}, {}, [](int iSlot, const char* szContent){
		if(g_iMenuType[iSlot] < 2) if(CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 9)) return true;
		return false;
	});

	return true;
}

void Menus::OnPluginUnload(PluginId id) {
	g_pUtilsApi->ClearAllHooks(id);
	g_pPlayersApi->ClearAllHooks(id);
	g_pLayoutApi->ClearAllHooks(id);
}

KHook::Return<void> Menus::OnBuildGameSessionManifest(IGameSystem*, const EventBuildGameSessionManifest_t* pMsg)
{
    IEntityResourceManifest* pResourceManifest = pMsg->m_pResourceManifest;
    for (auto& it : g_mapPrecache)
    {
        pResourceManifest->AddResource(it.c_str());
    }
	return { KHook::Action::Ignore };
}

KHook::Return<bool> Menus::Hook_OnTakeDamage_Alive(CCSPlayerPawn* pPawn, CTakeDamageInfoContainer *pInfoContainer)
{
	if(!pPawn) return { KHook::Action::Ignore, true };
	CBasePlayerController* pPlayerController = pPawn->m_hController();
    if (pPlayerController)
	{
    	int iPlayerSlot = pPlayerController->GetEntityIndex().Get() - 1;
		g_pUtilsApi->SendHookOnTakeDamage(iPlayerSlot, pInfoContainer);
	}
	return { KHook::Action::Ignore, true };
}

KHook::Return<void> Menus::OnServerSideClientClientConnect(CServerSideClient* pClient, int socket, const char* pszName, CPlayerUserId nUserID, INetChannel* pNetChannel, uint8 nConnectionTypeFlags, uint32 uChallengeNumber)
{
	if(!pClient) return { KHook::Action::Ignore };
	int iSlot = pClient->GetPlayerSlot().Get();
	if(iSlot >= 0 && iSlot < 64)
		g_pPlayersApi->OnClientSessionStart(iSlot);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnCServerSideClientlientPerformDisconnection(CServerSideClient* pClient, ENetworkDisconnectionReason reason)
{
	if(!pClient) return { KHook::Action::Ignore };
	int iSlot = pClient->GetPlayerSlot().Get();
	if(iSlot >= 0 && iSlot < 64)
		g_pPlayersApi->OnClientSessionEnd(iSlot);
	return { KHook::Action::Ignore };
}

KHook::Return<bool> Menus::OnProcessTick(CServerSideClient* pClient, const CNETMsg_Tick_t& msg)
{
	if(!pClient) return { KHook::Action::Ignore, true };
	int iSlot = pClient->GetPlayerSlot().Get();
	if(iSlot >= 0 && iSlot < 64 && !g_pPlayersApi->ProcessTick(iSlot, msg))
		return { KHook::Action::Supersede, false };
	return { KHook::Action::Ignore, true };
}

KHook::Return<bool> Menus::OnProcessStringCmd(CServerSideClient* pClient, const CNETMsg_StringCmd_t& msg)
{
	if(!pClient) return { KHook::Action::Ignore, true };
	int iSlot = pClient->GetPlayerSlot().Get();
	if(iSlot >= 0 && iSlot < 64 && !g_pPlayersApi->ProcessStringCmd(iSlot, msg))
		return { KHook::Action::Supersede, false };
	return { KHook::Action::Ignore, true };
}

bool Menus::Unload(char *error, size_t maxlen)
{
	m_DispatchConCommand.Remove(g_pCVar);
	m_GameFrame.Remove(g_pSource2Server);
	m_GameFramePost.Remove(g_pSource2Server);
	m_GameServerSteamAPIActivated.Remove(g_pSource2Server);
	m_PreWorldUpdate.Remove(g_pSource2Server);
	m_ServerHibernationUpdate.Remove(g_pSource2Server);
	m_GameServerSteamAPIDeactivated.Remove(g_pSource2Server);
	m_HostNameChanged.Remove(g_pSource2Server);
	m_PreFatalShutdown.Remove(g_pSource2Server);
	m_UpdateWhenNotInGame.Remove(g_pSource2Server);
	m_ServerConVarChanged.Remove(g_pSource2Server);
	m_PostEventAbstract.Remove(g_gameEventSystem);
	m_SetClientListening.Remove(engine);
	m_ClientCommand.Remove(g_pSource2GameClients);
	m_StartupServer.Remove(g_pNetworkServerService);
	m_ClientDisconnect.Remove(g_pSource2GameClients);
	m_ClientPutInServer.Remove(g_pSource2GameClients);
	m_OnClientConnected.Remove(g_pSource2GameClients);
	m_ClientConnect.Remove(g_pSource2GameClients);
	m_CheckTransmit.Remove(g_pSource2GameEntities);
	m_ClientSvcUserMessage.Remove(g_pSource2GameClients);
	m_ClientActive.Remove(g_pSource2GameClients);
	m_ClientFullyConnect.Remove(g_pSource2GameClients);
	m_ClientSettingsChanged.Remove(g_pSource2GameClients);
	m_ProcessUsercmds.Remove(g_pSource2GameClients);
	m_ClientVoice.Remove(g_pSource2GameClients);
	m_ClientCommandKeyValues.Remove(g_pSource2GameClients);
	if(gameeventmanager) m_FireEvent.Remove(gameeventmanager);
	if(g_pCCSPlayerPawnVTable) m_OnTakeDamageAlive.RemoveGlobal(reinterpret_cast<CCSPlayerPawn*>(&g_pCCSPlayerPawnVTable));
	if(g_pServerSideClientVTable)
	{
		m_ServerSideClientConnect.RemoveGlobal(reinterpret_cast<CServerSideClient*>(&g_pServerSideClientVTable));
		m_PerformDisconnection.RemoveGlobal(reinterpret_cast<CServerSideClient*>(&g_pServerSideClientVTable));
		m_ProcessTick.RemoveGlobal(reinterpret_cast<CServerSideClient*>(&g_pServerSideClientVTable));
		m_ProcessStringCmd.RemoveGlobal(reinterpret_cast<CServerSideClient*>(&g_pServerSideClientVTable));
	}
	if(g_pEntityDebugGameSystemVTable) m_BuildGameSessionManifest.RemoveGlobal(reinterpret_cast<IGameSystem*>(&g_pEntityDebugGameSystemVTable));
	if(m_SayHook) funchook_destroy(m_SayHook);
	if(m_SayTeamHook) funchook_destroy(m_SayTeamHook);
	if(m_TakeDamageHook) funchook_destroy(m_TakeDamageHook);
	if(m_CanAcquireHook) funchook_destroy(m_CanAcquireHook);

	ConVar_Unregister();
	
	return true;
}

KHook::Return<void> Menus::OnGameServerSteamAPIActivated(IServerGameDLL*)
{
	m_CallbackValidateAuthTicketResponse.Register(this, &Menus::OnValidateAuthTicketHook);
	g_pUtilsApi->SendServerSteamAPIActivated();
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnPreWorldUpdate(IServerGameDLL*, bool simulating)
{
	g_pUtilsApi->SendServerPreWorldUpdate(simulating);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnServerHibernationUpdate(IServerGameDLL*, bool bHibernating)
{
	g_pUtilsApi->SendServerHibernationUpdate(bHibernating);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnGameServerSteamAPIDeactivated(IServerGameDLL*)
{
	g_pUtilsApi->SendServerSteamAPIDeactivated();
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnHostNameChanged(IServerGameDLL*, const char *pHostname)
{
	g_pUtilsApi->SendServerHostNameChanged(pHostname);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnPreFatalShutdown(const IServerGameDLL*)
{
	g_pUtilsApi->SendServerPreFatalShutdown();
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnUpdateWhenNotInGame(IServerGameDLL*, float flFrameTime)
{
	g_pUtilsApi->SendServerUpdateWhenNotInGame(flFrameTime);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnServerConVarChanged(IServerGameDLL*, const char *pVarName, const char *pValue)
{
	g_pUtilsApi->SendServerConVarChanged(pVarName, pValue);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnPostEventAbstract(IGameEventSystem*, CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients, INetworkMessageInternal *pEvent, const CNetMessage *pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	g_pUtilsApi->SendServerPostEvent(nClientCount, clients, pEvent, pData);
	return { KHook::Action::Ignore };
}

KHook::Return<bool> Menus::OnSetClientListening(IVEngineServer2* pEngine, CPlayerSlot iReceiver, CPlayerSlot iSender, bool bListen)
{
	bool bResult = g_pUtilsApi->SendServerSetClientListening(iReceiver.Get(), iSender.Get(), bListen);
	if(bResult != bListen)
		return KHook::Recall(&IVEngineServer2::SetClientListening, KHook::Return<bool>{ KHook::Action::Ignore, bResult }, pEngine, iReceiver, iSender, bResult);
	return { KHook::Action::Ignore, bResult };
}

KHook::Return<void> Menus::OnGameFramePost(IServerGameDLL*, bool simulating, bool bFirstTick, bool bLastTick)
{
	g_pUtilsApi->SendServerGameFramePost(simulating, bFirstTick, bLastTick);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnPreWorldUpdatePost(IServerGameDLL*, bool simulating)
{
	g_pUtilsApi->SendServerPreWorldUpdatePost(simulating);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnServerHibernationUpdatePost(IServerGameDLL*, bool bHibernating)
{
	g_pUtilsApi->SendServerHibernationUpdatePost(bHibernating);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnGameServerSteamAPIActivatedPost(IServerGameDLL*)
{
	g_pUtilsApi->SendServerSteamAPIActivatedPost();
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnGameServerSteamAPIDeactivatedPost(IServerGameDLL*)
{
	g_pUtilsApi->SendServerSteamAPIDeactivatedPost();
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnHostNameChangedPost(IServerGameDLL*, const char *pHostname)
{
	g_pUtilsApi->SendServerHostNameChangedPost(pHostname);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnPreFatalShutdownPost(const IServerGameDLL*)
{
	g_pUtilsApi->SendServerPreFatalShutdownPost();
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnUpdateWhenNotInGamePost(IServerGameDLL*, float flFrameTime)
{
	g_pUtilsApi->SendServerUpdateWhenNotInGamePost(flFrameTime);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnServerConVarChangedPost(IServerGameDLL*, const char *pVarName, const char *pValue)
{
	g_pUtilsApi->SendServerConVarChangedPost(pVarName, pValue);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnPostEventAbstractPost(IGameEventSystem*, CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients, INetworkMessageInternal *pEvent, const CNetMessage *pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	g_pUtilsApi->SendServerPostEventPost(nClientCount, clients, pEvent, pData);
	return { KHook::Action::Ignore };
}

KHook::Return<bool> Menus::OnSetClientListeningPost(IVEngineServer2*, CPlayerSlot iReceiver, CPlayerSlot iSender, bool bListen)
{
	g_pUtilsApi->SendServerSetClientListeningPost(iReceiver.Get(), iSender.Get(), bListen);
	return { KHook::Action::Ignore, bListen };
}

KHook::Return<void> Menus::OnClientConnected(IServerGameClients*, CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, const char* pszAddress, bool bFakePlayer)
{
	if(bFakePlayer)
		m_Players[slot.Get()] = new Player(slot.Get(), true);
	g_pPlayersApi->OnClientConnected(slot.Get());
	return { KHook::Action::Ignore };
}

KHook::Return<bool> Menus::OnClientConnect(IServerGameClients*, CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason )
{
	Player *pPlayer = new Player(slot.Get());
	pPlayer->SetUnauthenticatedSteamId(new CSteamID(xuid));

	std::string ip(pszNetworkID);

	for (size_t i = 0; i < ip.length(); i++)
	{
		if (ip[i] == ':')
		{
			ip = ip.substr(0, i);
			break;
		}
	}
	pPlayer->SetIpAddress(ip);
	pPlayer->SetConnected();
	m_Players[slot.Get()] = pPlayer;
	UTIL_ResetPlayerSlot(slot.Get());
	if(!g_pPlayersApi->ClientConnect(slot.Get()))
		return { KHook::Action::Supersede, false };
	return { KHook::Action::Ignore, true };
}

KHook::Return<void> Menus::OnClientPutInServer(IServerGameClients*, CPlayerSlot slot, char const *pszName, int type, uint64 xuid )
{
	m_Players[slot.Get()]->SetInGame(true);

	int iSlot = slot.Get();
	if(iSlot >= 0 && iSlot < 64 && !g_pPlayersApi->IsFakeClient(iSlot))
		UTIL_EnsureLayout(iSlot);
	g_pPlayersApi->ClientPutInServer(iSlot);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnClientActive(IServerGameClients*, CPlayerSlot slot, bool bLoadGame, char const *pszName, uint64 xuid )
{
	g_pPlayersApi->ClientActive(slot.Get());
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnClientFullyConnect(IServerGameClients*, CPlayerSlot slot )
{
	g_pPlayersApi->ClientFullyConnect(slot.Get());
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnClientSettingsChanged(IServerGameClients*, CPlayerSlot slot )
{
	g_pPlayersApi->ClientSettingsChanged(slot.Get());
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnProcessUsercmds(IServerGameClients*, CPlayerSlot slot, const CNetMessagePB<CCLCMsg_Move> &msg, bool paused )
{
	g_pPlayersApi->ProcessUsercmds(slot.Get(), static_cast<const CCLCMsg_Move_t&>(msg), paused);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnClientVoice(IServerGameClients*, CPlayerSlot slot )
{
	g_pPlayersApi->ClientVoice(slot.Get());
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnClientCommandKeyValues(IServerGameClients*, CPlayerSlot slot, KeyValues *pKeyValues )
{
	g_pPlayersApi->ClientCommandKeyValues(slot.Get(), pKeyValues);
	return { KHook::Action::Ignore };
}

void Menus::OnValidateAuthTicketHook(ValidateAuthTicketResponse_t *pResponse)
{
	uint64 iSteamId = pResponse->m_SteamID.ConvertToUint64();
	for (int i = 0; i < 64; i++)
	{
		if (!m_Players[i] || m_Players[i]->IsFakeClient() || !(m_Players[i]->GetUnauthenticatedSteamId64() == iSteamId))
			continue;
		switch (pResponse->m_eAuthSessionResponse)
		{
			case k_EAuthSessionResponseOK:
			{
				if(m_Players[i]->IsAuthenticated())
					return;
				m_Players[i]->SetAuthenticated(true);
				m_Players[i]->SetSteamId(m_Players[i]->GetUnauthenticatedSteamId());
				g_pPlayersApi->SendClientAuthCallback(i, iSteamId);

				if(!g_pCookies) {
					g_iMenuType[i] = GetClientCookieMenuType(i);
					g_bNotifyDisabled[i] = GetClientCookieNotifyDisabled(i);
				}
				return;
			}

			case k_EAuthSessionResponseAuthTicketInvalid:
			case k_EAuthSessionResponseAuthTicketInvalidAlreadyUsed:
			{
				if (!g_iDelayAuthFailKick)
					return;

				g_pUtilsApi->PrintToChat(i, g_vecPhrases["AuthTicketInvalid"].c_str());
				[[fallthrough]];
			}

			default:
			{
				if (!g_iDelayAuthFailKick)
					return;

				g_pUtilsApi->PrintToChat(i, g_vecPhrases["AuthFailed"].c_str(), g_iDelayAuthFailKick);

				new CTimer(g_iDelayAuthFailKick, [i]()
				{
					if(!m_Players[i] || m_Players[i]->IsFakeClient() || m_Players[i]->IsAuthenticated()) return -1.f;
					engine->DisconnectClient(i, NETWORK_DISCONNECT_KICKED_NOSTEAMLOGIN);
					return -1.f;
				});
			}
		}
	}
}

void UtilsApi::LoadTranslations(const char* FileName)
{
	KeyValues::AutoDelete g_kvPhrases("Phrases");
	char pszPath[256];
	g_SMAPI->Format(pszPath, sizeof(pszPath), "addons/translations/%s.txt", FileName);

	if (!g_kvPhrases->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return;
	}

	for (KeyValues *pKey = g_kvPhrases->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey())
		g_vecPhrases[std::string(pKey->GetName())] = std::string(pKey->GetString(szLanguage));
}

KHook::Return<bool> Menus::OnFireEvent(IGameEventManager2* pManager, IGameEvent* pEvent, bool bDontBroadcast)
{
    if (!pEvent) {
        return { KHook::Action::Ignore, false };
    }

    const char* szName = pEvent->GetName();
	g_pUtilsApi->SendHookEventCallback(szName, pEvent, bDontBroadcast);
	g_pUtilsApi->SendServerFireEvent(pEvent, bDontBroadcast);

	m_EventCopies.push(gameeventmanager->DuplicateEvent(pEvent));

	EventInfo info{ bDontBroadcast };
	EventHookResult result = g_pUtilsApi->SendEventHookPre(szName, pEvent, &info);

	if(result >= EventHookResult::Handled) {
		gameeventmanager->FreeEvent(pEvent);
		return { KHook::Action::Supersede, false };
	}

	if(info.bDontBroadcast != bDontBroadcast) {
		return KHook::Recall(&IGameEventManager2::FireEvent, KHook::Return<bool>{ KHook::Action::Ignore, true }, pManager, pEvent, info.bDontBroadcast);
	}

	return { KHook::Action::Ignore, true };
}

KHook::Return<bool> Menus::OnFireEventPost(IGameEventManager2*, IGameEvent* pEvent, bool bDontBroadcast)
{
	if(m_EventCopies.empty()) {
		return { KHook::Action::Ignore, true };
	}
	IGameEvent* pCopy = m_EventCopies.top();
	m_EventCopies.pop();
	if(pCopy) {
		g_pUtilsApi->SendServerFireEventPost(pCopy, bDontBroadcast);
		g_pUtilsApi->SendEventHookPost(pCopy->GetName(), pCopy, bDontBroadcast);
		gameeventmanager->FreeEvent(pCopy);
	}
	return { KHook::Action::Ignore, true };
}

KHook::Return<void> Menus::GameFrame(IServerGameDLL*, bool simulating, bool bFirstTick, bool bLastTick)
{
	g_pUtilsApi->SendServerGameFrame(simulating, bFirstTick, bLastTick);
	if(!g_pGameRules)
	{
		CCSGameRulesProxy* pGameRulesProxy = static_cast<CCSGameRulesProxy*>(UTIL_FindEntityByClassname("cs_gamerules"));
		if (pGameRulesProxy)
		{
			g_pGameRules = pGameRulesProxy->m_pGameRules();
			if(g_pGameRules) g_pUtilsApi->SendHookGameRules();
		}
	} else if(g_bMenuFlashFix) {
		if(!g_pGameRules->m_bWarmupPeriod() && !g_pGameRules->m_bGameRestart()) g_pGameRules->m_bGameRestart() = g_pGameRules->m_flRestartRoundTime().GetTime() < gpGlobals->curtime;
		else g_pGameRules->m_bGameRestart() = false;
	}
	g_pUtilsApi->NextFrame();

	if (simulating && g_bHasTicked)
	{
		g_flUniversalTime += gpGlobals->curtime - g_flLastTickedTime;
	}

	g_flLastTickedTime = gpGlobals->curtime;
	g_bHasTicked = true;

	for (int i = g_timers.Count() - 1; i >= 0; i--)
	{
		auto timer = g_timers[i];

		if (timer->m_flLastExecute == -1)
			timer->m_flLastExecute = g_flUniversalTime;

		if (timer->m_flLastExecute + timer->m_flInterval <= g_flUniversalTime)
		{
			if (!timer->Execute())
			{
				delete timer;
				g_timers.Remove(i);
			}
			else
			{
				timer->m_flLastExecute = g_flUniversalTime;
			}
		}
	}
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::ClientCommand(IServerGameClients*, CPlayerSlot slot, const CCommand &args)
{
	g_pPlayersApi->ClientCommand(slot.Get(), args);
	bool bFound = g_pUtilsApi->FindAndSendCommandCallback(args.Arg(0), slot.Get(), args.ArgS(), true);
	if(bFound) return { KHook::Action::Supersede };
	return { KHook::Action::Ignore };
}

std::string StripQuotes(const std::string& str) {
	if (str.size() >= 2 && str.front() == '"' && str.back() == '"')
		return str.substr(1, str.size() - 2);
	return str;
}

std::string TrimTrailingQuote(const std::string& str) {
	if (!str.empty() && str.back() == '"')
		return str.substr(0, str.size() - 1);
	return str;
}

std::vector<std::string> SplitStringBySpace(const std::string& input) {
	std::istringstream iss(input);
	std::vector<std::string> tokens;
	std::string token;
	while (iss >> token) {
		token = TrimTrailingQuote(token);
		tokens.push_back(token);
	}
	return tokens;
}

KHook::Return<void> Menus::OnDispatchConCommand(ICvar*, ConCommandRef cmdHandle, const CCommandContext& ctx, const CCommand& args)
{
	if (!g_pEntitySystem)
		return { KHook::Action::Ignore };

	auto iCommandPlayerSlot = ctx.GetPlayerSlot();
	if(!g_pUtilsApi->SendServerDispatchConCommand(iCommandPlayerSlot.Get(), args))
		return { KHook::Action::Supersede };
	bool bSay = !V_strcmp(args.Arg(0), "say");
	bool bTeamSay = !V_strcmp(args.Arg(0), "say_team");
	int iSlot = iCommandPlayerSlot.Get();
	if (iSlot != -1 && (bSay || bTeamSay))
	{
		auto pController = CCSPlayerController::FromSlot(iSlot);
		bool bCommand = *args[1] == '!' || *args[1] == '/';
		if (pController && bCommand)
		{
			std::string message = args.ArgS() + 2;
			auto tokens = SplitStringBySpace(message);
			if (!tokens.empty())
			{
				if (containsOnlyDigits(tokens[0]))
				{
					if (g_iMenuType[iSlot] < 2)
					{
						int iButton = atoi(tokens[0].c_str());
						if (CheckActionMenu(iSlot, pController, iButton))
							return { KHook::Action::Supersede };
					}
				}
			}
		}

		if (std::string(args[1]).size() > 1)
		{
			const char* pszArgS = args.ArgS();
			const char* pszMessage = (pszArgS[0] == '"') ? pszArgS + 1 : pszArgS;
			auto tokens = SplitStringBySpace(pszMessage);
			if (!tokens.empty())
			{
				const char* arg0 = tokens[0].c_str();
				bool bFound = g_pUtilsApi->FindAndSendCommandCallback(arg0, iSlot, pszMessage, false);
				if (bFound) return { KHook::Action::Supersede };
				else if (g_vCommandEater.size() > 0 && g_pUtilsApi->FindCommand(arg0))
				{
					for (auto& command : g_vCommandEater)
					{
						if (arg0[0] == command[0])
						{
							return { KHook::Action::Supersede };
						}
					}
				}
			}
		}
	} else {
		bool bFound = g_pUtilsApi->FindAndSendCommandCallback(args.Arg(0), iSlot, args.ArgS(), true);
		if (bFound) return { KHook::Action::Supersede };
	}
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnDispatchConCommandPost(ICvar*, ConCommandRef cmdHandle, const CCommandContext& ctx, const CCommand& args)
{
	g_pUtilsApi->SendServerDispatchConCommandPost(ctx.GetPlayerSlot().Get(), args);
	return { KHook::Action::Ignore };
}

CGlobalVars* getGlobalVars()
{
    INetworkGameServer* server = g_pNetworkServerService->GetIGameServer();
    if (!server) return nullptr;
    return g_pNetworkServerService->GetIGameServer()->GetGlobals();
}

KHook::Return<void> Menus::StartupServer(INetworkServerService*, const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*)
{
	for(int i = 0; i < 64; i++)
	{
		g_pMenusCore->ClosePlayerMenu(i);
		for (auto& [panelId, layout] : g_mapHudLayouts[i])
		{
			if(layout) g_mapTransmitState[layout->entindex()].clear();
		}
		g_mapHudLayouts[i].clear();
	}
	for (auto& [name, layout] : g_mapGlobalHudLayouts)
	{
		if(layout) g_mapTransmitState[layout->entindex()].clear();
	}
	g_mapGlobalHudLayouts.clear();
	g_Offsets.clear();
	g_ChainOffsets.clear();
	g_pGameRules = nullptr;
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pGameEntitySystem;
	gpGlobals = getGlobalVars();
	if(g_bHasTicked) {
		g_pUtilsApi->SendHookMapEnd();
		g_pUtilsApi->SendServerMapEnd();
	} else {
		char szMapName[256];
		g_SMAPI->Format(szMapName, sizeof(szMapName), "%s", gpGlobals->mapname);
		g_pUtilsApi->SendHookMapStart(szMapName);
		g_pUtilsApi->SendServerMapStart(szMapName);
	}
	g_bHasTicked = false;
	g_pUtilsApi->SendHookStartup();
	static bool s_bServerStarted = false;
	if(!s_bServerStarted)
	{
		s_bServerStarted = true;
		g_pUtilsApi->SendServerStartup();
	}
	g_pGameEntitySystem->AddListenerEntity(this);
	return { KHook::Action::Ignore };
}

KHook::Return<void> Menus::OnClientDisconnect(IServerGameClients*, CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID )
{
	int iSlot = slot.Get();
	g_pPlayersApi->ClientDisconnect(iSlot);
	g_pMenusCore->ClosePlayerMenu(iSlot);
	if(iSlot >= 0 && iSlot < 64)
		UTIL_DestroyLayout(iSlot);
	if (iSlot < 0 || iSlot >= 64 || !m_Players[iSlot]) return { KHook::Action::Ignore };
	delete m_Players[iSlot];
	m_Players[iSlot] = nullptr;

	UTIL_ResetPlayerSlot(iSlot);
	return { KHook::Action::Ignore };
}

const char* UtilsApi::GetVersion()
{
	return g_PLAPI->GetVersion();
}

const char* UtilsApi::GetServerID()
{
	return g_szServerID.c_str();
}

///////////////////////////////////////
const char* Menus::GetLicense()
{
	return "GPL";
}

const char* Menus::GetVersion()
{
	return "1.9.1";
}

const char* Menus::GetDate()
{
	return __DATE__;
}

const char *Menus::GetLogTag()
{
	return "GameUtils";
}

const char* Menus::GetAuthor()
{
	return "Pisex, whyn0tq";
}

const char* Menus::GetDescription()
{
	return "Game Utils";
}

const char* Menus::GetName()
{
	return "Game Utils";
}

const char* Menus::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
