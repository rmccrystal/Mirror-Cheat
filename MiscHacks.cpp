#include "MiscHacks.h"
#include "Interfaces.h"
#include "RenderManager.h"
#include "IClientMode.h"
#include <chrono>
#include <algorithm>
#include <time.h>
#include "Hooks.h"
#include "edge.h"
#include "Autowall.h"
CMiscHacks* g_Misc = new CMiscHacks;
template<class T, class U>
inline T clamp(T in, U low, U high)
{
	if (in <= low)
		return low;
	else if (in >= high)
		return high;
	else
		return in;
}
inline float bitsToFloat(unsigned long i)
{
	return *reinterpret_cast<float*>(&i);
}
inline float FloatNegate(float f)
{
	return bitsToFloat(FloatBits(f) ^ 0x80000000);
}
Vector AutoStrafeView;
void CMiscHacks::Init()
{
}
void CMiscHacks::Draw()
{
	if (!Interfaces::Engine->IsConnected() || !Interfaces::Engine->IsInGame())
		return;

}
void RotateMovement(CUserCmd * pCmd, float rotation)
{
	rotation = DEG2RAD(rotation);
	float cosr, sinr;
	cosr = cos(rotation);
	sinr = sin(rotation);
	float forwardmove, sidemove;
	forwardmove = (cosr * pCmd->forwardmove) - (sinr * pCmd->sidemove);
	sidemove = (sinr * pCmd->forwardmove) - (cosr * pCmd->sidemove);
	pCmd->forwardmove = forwardmove;
	pCmd->sidemove = sidemove;
}
void CMiscHacks::c_strafe(CUserCmd* m_pcmd)
{
	IClientEntity *m_local = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	Vector View(m_pcmd->viewangles);
	float niglet = 0;

	if (m_local->GetFlags() & (int)FL_ONGROUND)
		return;

	m_pcmd->forwardmove = 450.f;
	int random = rand() % 100;
	int random2 = rand() % 1000;
	static bool dir;
	static float current_y = View.y;
	if (m_local->GetVelocity().Length() > 50.f)
	{
		niglet += 0.00007;
		current_y += 3 - niglet;
	}
	else
	{
		niglet = 0;
	}
	View.y = current_y;
	if (random == random2)
		View.y += random;

//	char bufferxf[64];
//	sprintf_s(bufferxf, "Circlestrafer running c_yaw: %1f", current_y);

	RotateMovement(m_pcmd, current_y);
}
template<class T>
static T* FindHudElement(const char* name)
{
	static auto pThis = *reinterpret_cast<DWORD**>(Utilities::Memory::FindPatternV2("client_panorama.dll", "B9 ? ? ? ? E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 8D 58") + 1);

	static auto find_hud_element = reinterpret_cast<DWORD(__thiscall*)(void*, const char*)>(Utilities::Memory::FindPatternV2("client_panorama.dll", "55 8B EC 53 8B 5D 08 56 57 8B F9 33 F6 39"));
	return (T*)find_hud_element(pThis, name);
}
void preservefeed(IGameEvent* Event) {
	if (hackManager.pLocal()->IsAlive())
	{
		static DWORD* _death_notice = FindHudElement<DWORD>("CCSGO_HudDeathNotice");
		static void(__thiscall *_clear_notices)(DWORD) = (void(__thiscall*)(DWORD))Utilities::Memory::FindPatternV2("client_panorama.dll", "55 8B EC 83 EC 0C 53 56 8B 71 58");

		if (round_change)
		{

			_death_notice = FindHudElement<DWORD>("CCSGO_HudDeathNotice");
			if (_death_notice - 20)
				_clear_notices(((DWORD)_death_notice - 20));
			round_change = false;
		}

		if (_death_notice)
			*(float*)((DWORD)_death_notice + 0x50) = Options::Menu.VisualsTab.killfeed.GetState() ? 100 : 1;
	}
}

void CMiscHacks::Move(CUserCmd *pCmd, bool &bSendPacket)
{
	IClientEntity *pLocal = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	IGameEvent* Event;
	if (!hackManager.pLocal()->IsAlive())
		return;


	if (Options::Menu.VisualsTab.killfeed.GetState())
		preservefeed(Event);

	if (Options::Menu.RageBotTab.AimbotEnable.GetState())
		AutoPistol(pCmd);
	if (Options::Menu.MiscTab.OtherAutoJump.GetState() && Options::Menu.MiscTab.OtherSafeMode.GetIndex() != 2)
		AutoJump(pCmd);
	if (Options::Menu.MiscTab.airduck_type.GetIndex() != 0)
	{
		airduck(pCmd);
	}
	Interfaces::Engine->GetViewAngles(AutoStrafeView);
	if (Options::Menu.MiscTab.OtherAutoStrafe.GetState() && !GetAsyncKeyState(Options::Menu.MiscTab.CircleStrafeKey.GetKey()))
	{	
		strafe_2(pCmd);
	}
	if (GetAsyncKeyState(Options::Menu.MiscTab.Zstrafe.GetKey()))
	{
		zstrafe(pCmd);
	}
	if (GetAsyncKeyState(Options::Menu.MiscTab.CircleStrafeKey.GetKey()))
	{
		c_strafe(pCmd);
	}
	if (Interfaces::Engine->IsInGame() && Interfaces::Engine->IsConnected())
	{
		if (Options::Menu.MiscTab.OtherSafeMode.GetIndex() != 2)
		{
			SlowMo(pCmd, bSendPacket);
			FakeWalk0(pCmd, bSendPacket);
		}
	}
	if (Interfaces::Engine->IsInGame() && Interfaces::Engine->IsConnected())
	{
		if (Options::Menu.VisualsTab.DisablePostProcess.GetState())
			PostProcces();

		// ------- Oi thundercunt, this is needed for the weapon configs ------- //
	
		C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(pLocal->GetActiveWeaponHandle());
		if (pWeapon)
		{
			if (pWeapon != nullptr)
			{
				if (GameUtils::AutoSniper(pWeapon))
				{
					
					Options::Menu.RageBotTab.AccuracyHitchance.SetValue((float)Options::Menu.RageBotTab.hc_auto.GetValue());
					Options::Menu.RageBotTab.AccuracyMinimumDamage.SetValue((float)Options::Menu.RageBotTab.md_auto.GetValue());
										
				}

				if (GameUtils::IsPistol(pWeapon))
				{
					
					Options::Menu.RageBotTab.AccuracyHitchance.SetValue((float)Options::Menu.RageBotTab.hc_pistol.GetValue());
					Options::Menu.RageBotTab.AccuracyMinimumDamage.SetValue((float)Options::Menu.RageBotTab.md_pistol.GetValue());
					
				}

				if (GameUtils::IsScout(pWeapon))
				{
					
					Options::Menu.RageBotTab.AccuracyHitchance.SetValue((float)Options::Menu.RageBotTab.hc_scout.GetValue());
					Options::Menu.RageBotTab.AccuracyMinimumDamage.SetValue((float)Options::Menu.RageBotTab.md_scout.GetValue());
					
				}

				if (GameUtils::AWP(pWeapon))
				{
				
					Options::Menu.RageBotTab.AccuracyHitchance.SetValue((float)Options::Menu.RageBotTab.hc_awp.GetValue());
					Options::Menu.RageBotTab.AccuracyMinimumDamage.SetValue((float)Options::Menu.RageBotTab.md_awp.GetValue());
					
				}

				if (GameUtils::IsRifle(pWeapon) || GameUtils::IsShotgun(pWeapon) || GameUtils::IsMP(pWeapon) || GameUtils::IsMachinegun(pWeapon))
				{
				
					Options::Menu.RageBotTab.AccuracyHitchance.SetValue((float)Options::Menu.RageBotTab.hc_otr.GetValue());
					Options::Menu.RageBotTab.AccuracyMinimumDamage.SetValue((float)Options::Menu.RageBotTab.md_otr.GetValue());
					
				}

				if (GameUtils::IsZeus(pWeapon))
				{
					Options::Menu.RageBotTab.AccuracyHitchance.SetValue(5);
					Options::Menu.RageBotTab.AccuracyMinimumDamage.SetValue(25);
				}
			}
		}
	}
}
int GetFxPS()
{
	static int fps = 0;
	static int count = 0;
	using namespace std::chrono;
	auto now = high_resolution_clock::now();
	static auto last = high_resolution_clock::now();
	count++;
	if (duration_cast<milliseconds>(now - last).count() > 1000)
	{
		fps = count;
		count = 0;
		last = now;
	}
	return fps;
}
float curtime_fixedx(CUserCmd* ucmd) {
	auto local_player = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	static int g_tick = 0;
	static CUserCmd* g_pLastCmd = nullptr;
	if (!g_pLastCmd || g_pLastCmd->hasbeenpredicted) {
		g_tick = local_player->GetTickBase();
	}
	else {
		++g_tick;
	}
	g_pLastCmd = ucmd;
	float curtime = g_tick * Interfaces::Globals->interval_per_tick;
	return curtime;
}

void VectorAnglesXXX(Vector forward, Vector &angles)
{
	float tmp, yaw, pitch;

	if (forward[2] == 0 && forward[0] == 0)
	{
		yaw = 0;

		if (forward[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / PI);

		if (yaw < 0)
			yaw += 360;
		tmp = sqrt(forward[0] * forward[0] + forward[1] * forward[1]);
		pitch = (atan2(-forward[2], tmp) * 180 / PI);

		if (pitch < 0)
			pitch += 360;
	}

	if (pitch > 180)
		pitch -= 360;
	else if (pitch < -180)
		pitch += 360;

	if (yaw > 180)
		yaw -= 360;
	else if (yaw < -180)
		yaw += 360;

	if (pitch > 89)
		pitch = 89;
	else if (pitch < -89)
		pitch = -89;

	if (yaw > 180)
		yaw = 180;
	else if (yaw < -180)
		yaw = -180;

	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}
Vector CalcAngleFakewalk(Vector src, Vector dst)
{
	Vector ret;
	VectorAnglesXXX(dst - src, ret);
	return ret;
}

void rotate_movement(float yaw, CUserCmd* cmd)
{
	Vector viewangles;
	QAngle yamom;
	Interfaces::Engine->GetViewAngles(viewangles);
	float rotation = DEG2RAD(viewangles.y - yaw);
	float cos_rot = cos(rotation);
	float sin_rot = sin(rotation);
	float new_forwardmove = (cos_rot * cmd->forwardmove) - (sin_rot * cmd->sidemove);
	float new_sidemove = (sin_rot * cmd->forwardmove) + (cos_rot * cmd->sidemove);
	cmd->forwardmove = new_forwardmove;
	cmd->sidemove = new_sidemove;
}

float fakewalk_curtime(CUserCmd* ucmd)
{
	auto local_player = hackManager.pLocal();

	if (!local_player)
		return 0;

	int g_tick = 0;
	CUserCmd* g_pLastCmd = nullptr;
	if (!g_pLastCmd || g_pLastCmd->hasbeenpredicted)
	{
		g_tick = (float)local_player->GetTickBase();
	}
	else {
		++g_tick;
	}
	g_pLastCmd = ucmd;
	float curtime = g_tick * Interfaces::Globals->interval_per_tick;
	return curtime;
}
void CMiscHacks::FakeWalk0(CUserCmd* pCmd, bool &bSendPacket)
{
	IClientEntity* pLocal = hackManager.pLocal();
	int key1 = Options::Menu.MiscTab.fw.GetKey();
	if (key1 >0 && GetAsyncKeyState(key1) && !Options::Menu.m_bIsOpen)
	{
		globalsh.fakewalk = true;
		static int iChoked = -1;
		iChoked++;
		if (pCmd->forwardmove > 0)
		{
			pCmd->buttons |= IN_BACK;
			pCmd->buttons &= ~IN_FORWARD;
		}
		if (pCmd->forwardmove < 0)
		{
			pCmd->buttons |= IN_FORWARD;
			pCmd->buttons &= ~IN_BACK;
		}
		if (pCmd->sidemove < 0)
		{
			pCmd->buttons |= IN_MOVERIGHT;
			pCmd->buttons &= ~IN_MOVELEFT;
		}
		if (pCmd->sidemove > 0)
		{
			pCmd->buttons |= IN_MOVELEFT;
			pCmd->buttons &= ~IN_MOVERIGHT;
		}
		static int choked = 0;
		choked = choked > 14 ? 0 : choked + 1;

		float nani = Options::Menu.MiscTab.FakeWalkSpeed.GetValue() / 14;

		pCmd->forwardmove = choked < nani || choked > 14 ? 0 : pCmd->forwardmove;
		pCmd->sidemove = choked < nani || choked > 14 ? 0 : pCmd->sidemove; //100:6 are about 16,6, quick maths
		bSendPacket = choked < 1;

	}
}

static __declspec(naked) void __cdecl Invoke_NET_SetConVar(void* pfn, const char* cvar, const char* value)
{
	__asm
	{
		push    ebp
		mov     ebp, esp
		and     esp, 0FFFFFFF8h
		sub     esp, 44h
		push    ebx
		push    esi
		push    edi
		mov     edi, cvar
		mov     esi, value
		jmp     pfn
	}
}

void CMiscHacks::AutoPistol(CUserCmd* pCmd)
{
	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());
	static bool WasFiring = false;
	if (GameUtils::IsPistol)
	{
		if (pCmd->buttons & IN_ATTACK)
		{
			if (WasFiring)
			{
				pCmd->buttons &= ~IN_ATTACK;
			}
		}
		WasFiring = pCmd->buttons & IN_ATTACK ? true : false;
	}
	else
		return;
}
void CMiscHacks::SlowMo(CUserCmd *pCmd, bool &bSendPacket)
{
	IClientEntity* pLocal = hackManager.pLocal();
	if (GetAsyncKeyState(Options::Menu.MiscTab.FakeWalk.GetKey()) && !Options::Menu.m_bIsOpen)
	{
		static int iChoked = -1;
		iChoked++;
		static bool slowmo;
		slowmo = !slowmo;
		const auto lag = 90;
		INetChannel* gayes = (INetChannel*)Interfaces::m_pClientState->m_NetChannel;
		gayes->m_nOutSequenceNr += 1;
		gayes->m_nOutSequenceNrAck -= 1;
		Interfaces::m_pClientState->lastoutgoingcommand += 1;
		if (iChoked < lag)
		{
			bSendPacket = false;
			if (slowmo)
			{
				pCmd->tick_count = INT_MAX;
				pCmd->command_number += INT_MAX + pCmd->tick_count % 2 ? 1 : 0;
				pCmd->buttons |= pLocal->GetMoveType() == IN_FORWARD;
				pCmd->buttons |= pLocal->GetMoveType() == IN_LEFT;
				pCmd->buttons |= pLocal->GetMoveType() == IN_BACK;
				INetChannel* gayes = (INetChannel*)Interfaces::m_pClientState->m_NetChannel;
				gayes->m_nOutSequenceNr += 1;
				gayes->m_nOutSequenceNrAck -= 1;
				Interfaces::m_pClientState->lastoutgoingcommand += 1;
				pCmd->forwardmove = pCmd->sidemove = 0.f;
			}
			else
			{
				bSendPacket = true;
				iChoked = -1;
				Interfaces::Globals->frametime *= (pLocal->GetVelocity().Length2D()) / 1.2;
				pCmd->buttons |= pLocal->GetMoveType() == IN_FORWARD;
			}
		}
		else
		{
			if (!bSendPacket)
			{
				if (slowmo)
				{
					pCmd->tick_count = INT_MAX;
					pCmd->command_number += pCmd->tick_count % 2 ? 1 : 0;
					pCmd->buttons |= pLocal->GetMoveType() == IN_FORWARD;
					pCmd->buttons |= pLocal->GetMoveType() == IN_LEFT;
					pCmd->buttons |= pLocal->GetMoveType() == IN_BACK;
					pCmd->forwardmove = pCmd->sidemove = 0.f;
				}
			}
			else
			{
				if (slowmo)
				{
					bSendPacket = true;
					iChoked = -1;
					pCmd->tick_count = INT_MAX;
					Interfaces::Globals->frametime *= (pLocal->GetVelocity().Length2D()) / 1.25;
					pCmd->buttons |= pLocal->GetMoveType() == IN_FORWARD;
				}
			}
		}
	}
}
void CMiscHacks::AutoJump(CUserCmd *pCmd)
{
	auto g_LocalPlayer = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	auto userCMD = pCmd;
	if (Options::Menu.MiscTab.autojump_type.GetIndex() < 1)
	{	
		if (g_LocalPlayer->GetMoveType() == MOVETYPE_NOCLIP || g_LocalPlayer->GetMoveType() == MOVETYPE_LADDER) return;
		if (userCMD->buttons & IN_JUMP && !(g_LocalPlayer->GetFlags() & FL_ONGROUND)) 
		{
			userCMD->buttons &= ~IN_JUMP;
		}
	}
	if (Options::Menu.MiscTab.autojump_type.GetIndex() > 0)
	{
		if (g_LocalPlayer->GetMoveType() == MOVETYPE_NOCLIP || g_LocalPlayer->GetMoveType() == MOVETYPE_LADDER) 
			return;
		userCMD->buttons |= IN_JUMP;
	}
}
void CMiscHacks::airduck(CUserCmd *pCmd) // quack
{
	auto local = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	
	if (Options::Menu.MiscTab.airduck_type.GetIndex() == 1)
	{
		if (!(local->GetFlags() & FL_ONGROUND))
		{
			pCmd->buttons |= IN_DUCK;
		}
	}
	if (Options::Menu.MiscTab.airduck_type.GetIndex() == 2)
	{
		if (!(local->GetFlags() & FL_ONGROUND))
		{
			pCmd->buttons |= IN_DUCK;
			pCmd->buttons &= IN_DUCK;
			pCmd->buttons |= IN_DUCK;
			pCmd->buttons &= IN_DUCK;
		}
	}
}
template<class T, class U>
inline T clampangle(T in, U low, U high)
{
	if (in <= low)
		return low;
	else if (in >= high)
		return high;
	else
		return in;
}
void CMiscHacks::RageStrafe(CUserCmd *userCMD)
{
	auto g_LocalPlayer = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());;
	if (g_LocalPlayer->GetMoveType() == MOVETYPE_NOCLIP || g_LocalPlayer->GetMoveType() == MOVETYPE_LADDER || !g_LocalPlayer->IsAlive()) return;
	if (!Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::KEY_SPACE) ||
		Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::KEY_A) ||
		Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::KEY_D) ||
		Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::KEY_S) ||
		Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::KEY_W))
		return;
	if (!(g_LocalPlayer->GetFlags() & FL_ONGROUND)) {
		if (userCMD->mousedx > 1 || userCMD->mousedx < -1) {
			userCMD->sidemove = clamp(userCMD->mousedx < 0.f ? -400.f : 400.f, -400, 400);
		}
		else {
			if (g_LocalPlayer->GetVelocity().Length2D() == 0 || g_LocalPlayer->GetVelocity().Length2D() == NAN || g_LocalPlayer->GetVelocity().Length2D() == INFINITE)
			{
				userCMD->forwardmove = 400;
				return;
			}
			userCMD->forwardmove = clamp(5850.f / g_LocalPlayer->GetVelocity().Length2D(), -400, 400);
			if (userCMD->forwardmove < -400 || userCMD->forwardmove > 400)
				userCMD->forwardmove = 0;
			userCMD->sidemove = clamp((userCMD->command_number % 2) == 0 ? -400.f : 400.f, -400, 400);
			if (userCMD->sidemove < -400 || userCMD->sidemove > 400)
				userCMD->sidemove = 0;
		}
	}
}

#define nn(nMin, nMax) (rand() % (nMax - nMin + 1) + nMin);
bool bHasGroundSurface(IClientEntity* pLocalBaseEntity, const Vector& vPosition)
{
	trace_t pTrace;
	Vector vMins, vMaxs; pLocalBaseEntity->GetRenderBounds(vMins, vMaxs);

	UTIL_TraceLine(vPosition, { vPosition.x, vPosition.y, vPosition.z - 32.f }, MASK_PLAYERSOLID_BRUSHONLY, pLocalBaseEntity, 0, &pTrace);

	return pTrace.fraction != 1.f;
}
void CMiscHacks::zstrafe(CUserCmd* m_pcmd)
{
	auto m_local = hackManager.pLocal();

	if (!(m_local->GetMoveType() == MOVETYPE_LADDER && !m_local->GetMoveType() == MOVETYPE_NOCLIP))
	{
		Vector vPosition = m_local->GetAbsOriginlol();
		vPosition += m_local->GetVelocity() * (Interfaces::Globals->interval_per_tick * 16);

		if (!bHasGroundSurface(m_local, vPosition))
			m_pcmd->buttons &= ~IN_JUMP;

		m_pcmd->sidemove = nn(-150, 150);
	}
}
void CMiscHacks::strafe_2(CUserCmd * cmd)
{
	auto local = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	if (local->GetMoveType() == MOVETYPE_NOCLIP || local->GetMoveType() == MOVETYPE_LADDER || local->is_ghost() || !local || !local->IsAlive())
		return;

	if (Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::KEY_A) || Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::KEY_D) || Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::KEY_S) || Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::KEY_W) || Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::KEY_LSHIFT))
		return;

	if (!(local->GetFlags() & FL_ONGROUND)) {
		if (cmd->mousedx > 1 || cmd->mousedx < -1) {
			cmd->sidemove = clamp(cmd->mousedx < 0.f ? -450.0f : 450.0f, -450.0f, 450.0f);
		}
		else {
			cmd->forwardmove = 10000.f / local->GetVelocity().Length();
			cmd->sidemove = (cmd->command_number % 2) == 0 ? -450.0f : 450.0f;
			if (cmd->forwardmove > 450.0f)
				cmd->forwardmove = 450.0f;
		}
	}
}

Vector GetAutostrafeView()
{
	return AutoStrafeView;
}

void CMiscHacks::PostProcces()
{
	ConVar* Meme = Interfaces::CVar->FindVar("mat_postprocess_enable");
	SpoofedConvar* meme_spoofed = new SpoofedConvar(Meme);
	meme_spoofed->SetString("mat_postprocess_enable 0");
}
