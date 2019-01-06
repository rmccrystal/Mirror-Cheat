#include "RageBot.h"
#include "RenderManager.h"
#include "Resolver.h"
#include "Autowall.h"
#include "edge.h"
#include <iostream>
#include <time.h>
#include "UTIL Functions.h"
#include "xostr.h"
#include <chrono>
#include "Hooks.h"
#include "global_count.h"
#include "laggycompensation.h"
#include "MD5.cpp"
#include "newbacktrack.h"
#include "new_backtrackhelper.h"
//float bigboi::current_yaw;
float current_real;
float current_desync;
Vector LastAngleAA2;
static bool dir = false;
static bool back = false;
static bool up = false;
static bool jitter = false;

static bool backup = false;
static bool default_aa = true;
static bool panic = false;
float hitchance_custom;
#define TICK_INTERVAL			(Interfaces::Globals->interval_per_tick)
#define TIME_TO_TICKS( dt )		( (int)( 0.5f + (float)(dt) / TICK_INTERVAL ) )
CRageBot * ragebot = new CRageBot;
void CRageBot::Init()
{
	IsAimStepping = false;
	IsLocked = false;
	TargetID = -1;
}
void CRageBot::Draw()
{
}
void NormalizeNum(Vector &vIn, Vector &vOut)
{
	float flLen = vIn.Length();
	if (flLen == 0) {
		vOut.Init(0, 0, 1);
		return;
	}
	flLen = 1 / flLen;
	vOut.Init(vIn.x * flLen, vIn.y * flLen, vIn.z * flLen);
}
float fov_ent(Vector ViewOffSet, Vector View, IClientEntity* entity, int hitbox)
{
	const float MaxDegrees = 180.0f;
	Vector Angles = View, Origin = ViewOffSet;
	Vector Delta(0, 0, 0), Forward(0, 0, 0);
	Vector AimPos = GetHitboxPosition(entity, hitbox);
	AngleVectors(Angles, &Forward);
	VectorSubtract(AimPos, Origin, Delta);
	NormalizeNum(Delta, Delta);
	float DotProduct = Forward.Dot(Delta);
	return (acos(DotProduct) * (MaxDegrees / PI));
}

int closest()
{
	int index = -1;
	float lowest_fov = 180.f; // maybe??

	IClientEntity* local_player = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	if (!local_player)
		return -1;

	if (!local_player->IsAlive())
		return -1;

	Vector local_position = local_player->GetAbsOrigin() + local_player->GetViewOffset();
	Vector angles;
	Interfaces::Engine->GetViewAngles(angles);
	for (int i = 1; i <= Interfaces::Globals->maxClients; i++)
	{
		IClientEntity *entity = Interfaces::EntList->GetClientEntity(i);

		if (!entity || entity->GetHealth() <= 0 || entity->team() == local_player->team() || entity->is_dormant() || entity == local_player)
			continue;

		float fov = fov_ent(local_position, angles, entity, 0);
		if (fov < lowest_fov)
		{
			lowest_fov = fov;
			index = i;
		}

	}
	return index;

}
float curtime_fixed(CUserCmd* ucmd) {
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

inline float RandomFloat(float min, float max)
{
	static auto fn = (decltype(&RandomFloat))(GetProcAddress(GetModuleHandle("vstdlib.dll"), "RandomFloat"));
	return fn(min, max);
}

int GetFPS()
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

static bool wasMoving = true;
static bool preBreak = false;
static bool shouldBreak = false;
static bool brokeThisTick = false;
static bool fake_walk = false;
static int chocked = 0;
static bool gaymode = false;
static bool doubleflick = false;
static bool has_broken = false;
bool is_broken;

void CRageBot::anti_lby(CUserCmd* cmd, bool& bSendPacket)
{
	if (Options::Menu.MiscTab.antilby.GetIndex() < 1)
		return;

	auto local_player = (IClientEntity*)Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	if (!local_player)
		return;

	auto local_weapon = local_player->GetActiveWeaponHandle();

	if (!local_weapon)
		return;

	float b = rand() % 4;

	static float oldCurtime = Interfaces::Globals->curtime;

	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());

	if (GameUtils::IsBomb(pWeapon) || GameUtils::IsGrenade(pWeapon))
		return;

	if (bSendPacket)
	{
		brokeThisTick = false;
		chocked = Interfaces::m_pClientState->chokedcommands;

		if (local_player->GetVelocity().Length2D() >= 0.1f && (local_player->GetFlags() & FL_ONGROUND))
		{
			if (GetAsyncKeyState(VK_SHIFT))
			{
				wasMoving = false;
				oldCurtime = Interfaces::Globals->curtime;
				if (Interfaces::Globals->curtime - oldCurtime >= 1.1f)
				{
					lbyupdate = true;
					shouldBreak = true;
					NextPredictedLBYUpdate = Interfaces::Globals->curtime;
				}
			}
			else
			{
				oldCurtime = Interfaces::Globals->curtime;
				wasMoving = true;
				has_broken = false;
			}
		}

		else
		{
			if (wasMoving &&Interfaces::Globals->curtime - oldCurtime > 0.22f)
			{
				wasMoving = false;
				lbyupdate = true;
				shouldBreak = true;
				NextPredictedLBYUpdate = Interfaces::Globals->curtime;
			}

			else if (Interfaces::Globals->curtime - oldCurtime > 1.1f)
			{
				shouldBreak = true;
				NextPredictedLBYUpdate = Interfaces::Globals->curtime;
			}

			else if (Interfaces::Globals->curtime - oldCurtime > 1.1f - TICKS_TO_TIME(chocked) - TICKS_TO_TIME(2))
			{
				lbyupdate = true;
				preBreak = true;
			}

			else if (Interfaces::Globals->curtime - oldCurtime > 1.f - TICKS_TO_TIME(chocked + 12))
				doubleflick = true;

			else
			{
				lbyupdate = false;
			}
		}
	}
	else if (shouldBreak )
	{
		static int choked = 0;

		if (Options::Menu.MiscTab.pitch_up.GetState() && !(hackManager.pLocal()->GetFlags() & FL_DUCKING))
		{
			oldCurtime = Interfaces::Globals->curtime;
			cmd->viewangles.x = -70.f + RandomFloat(-15, 35);
			shouldBreak = false;
		}

		switch (Options::Menu.MiscTab.antilby.GetIndex())
		{
		case 1:
		{
			brokeThisTick = true;
			oldCurtime = Interfaces::Globals->curtime;
			cmd->viewangles.y = cmd->viewangles.y + Options::Menu.MiscTab.BreakLBYDelta.GetValue();
			shouldBreak = false;
		}
		break;

		case 2:
		{
			brokeThisTick = true;
			oldCurtime = Interfaces::Globals->curtime;
			cmd->viewangles.y = cmd->viewangles.y + Options::Menu.MiscTab.BreakLBYDelta.GetValue();
			shouldBreak = false;
		}
		break;

		case 3:
		{	float addAngle = GetFPS() >= (TIME_TO_TICKS(1.f) * 0.5f) ? (2.9 * max(choked, GlobalBREAK::prevChoked) + 100) : 144.9f;

			if (is_broken)
			{
				brokeThisTick = true;
				oldCurtime = Interfaces::Globals->curtime;
				cmd->viewangles.y = cmd->viewangles.y + 90 + addAngle;
				shouldBreak = false;
			}
			else
			{			
				brokeThisTick = true;
				oldCurtime = Interfaces::Globals->curtime;
				cmd->viewangles.y = cmd->viewangles.y - 90 - addAngle;
				shouldBreak = false;
			}
		}
		break;
		}
	}
	
	else if (preBreak && Options::Menu.MiscTab.antilby.GetIndex() == 2)
	{
		brokeThisTick = true;
		float addAngle = GetFPS() >= (TIME_TO_TICKS(1.f) * 0.5f) ? (2.3789 * max(chocked, GlobalBREAK::prevChoked) + 90) : 144.9f;
		cmd->viewangles.y = cmd->viewangles.y + Options::Menu.MiscTab.BreakLBYDelta2.GetValue();
		preBreak = false;
	}

	else if (preBreak && Options::Menu.MiscTab.antilby.GetIndex() == 3 && is_broken)
	{	
			brokeThisTick = true;
			float addAngle = GetFPS() >= (TIME_TO_TICKS(1.f) * 0.5f) ? (2.3789 * max(chocked, GlobalBREAK::prevChoked) + 58) : 180;
			cmd->viewangles.y = cmd->viewangles.y - 40;
			preBreak = false;
		
	}

	else if (preBreak && Options::Menu.MiscTab.antilby.GetIndex() == 3 && !is_broken)
	{
		brokeThisTick = true;
		float addAngle = GetFPS() >= (TIME_TO_TICKS(1.f) * 0.5f) ? (2.3789 * max(chocked, GlobalBREAK::prevChoked) - 58) : 180;
		cmd->viewangles.y = cmd->viewangles.y + 40;
		preBreak = false;

	}
}
int BreakLagCompensation_low()
{

	float speed = hackManager.pLocal()->GetVelocity().Length2D();
	if (speed > 0.f)
	{
		auto distance_per_tick = speed * Interfaces::Globals->interval_per_tick;
		int choked_ticks = std::ceilf(65.f / distance_per_tick);
		return std::min<int>(choked_ticks, 2);
	}
}

int BreakLagCompensation()
{

	float speed = hackManager.pLocal()->GetVelocity().Length2D();
	if (speed > 0.f)
	{
		auto distance_per_tick = speed * Interfaces::Globals->interval_per_tick;
		int choked_ticks = std::ceilf(65.f / distance_per_tick);
		return std::min<int>(choked_ticks, 14);
	}
}

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

static bool whatever = false;
void CRageBot::Fakelag(CUserCmd *pCmd, bool &bSendPacket)
{
	IClientEntity* pLocal = (IClientEntity*)Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(pLocal->GetActiveWeaponHandle());
	float yes = 100;


	if (!Interfaces::Engine->IsConnected() || !Interfaces::Engine->IsInGame())
		return;

	if ((pCmd->buttons & IN_USE) || pLocal->GetMoveType() == MOVETYPE_LADDER)
		return;

	if (GameUtils::IsGrenade(pWeapon))
		return;

	if (GameUtils::IsRevolver(pWeapon))
	{
		whatever = !whatever;
		if (whatever)
			bSendPacket = true;
		else
			bSendPacket = false;
	}

	auto anime = &pLocal->GetAnimOverlays()[1];
	static auto choked = 0;
	static int tick; tick++;
	static int factor = 7;
	const auto max_fake_lag3 = Options::Menu.MiscTab.FakelagStand.GetValue();
	const auto max_fake_lag = Options::Menu.MiscTab.FakelagBreakLC.GetState() ? BreakLagCompensation() : Options::Menu.MiscTab.FakelagMove.GetValue();
	const auto max_fake_lag2 = Options::Menu.MiscTab.FakelagBreakLC.GetState() ? BreakLagCompensation() : Options::Menu.MiscTab.Fakelagjump.GetValue();
	float flVelocity = pLocal->GetVelocity().Length2D() * Interfaces::Globals->interval_per_tick;
	if ((pCmd->buttons & IN_ATTACK && Options::Menu.MiscTab.fl_spike_shoot.GetState()) 
		|| (fabs(pLocal->GetVelocity().z) <= 5.0f  && Options::Menu.MiscTab.fl_spike_jump.GetState())) {		

		if (choked < 15)
		{
			choked++;
			bSendPacket = false;
		}
		else
		{
			choked = 0;
			bSendPacket = true;
		}
	}

	else if (pLocal->GetVelocity().Length2D() > 0.01 && pLocal->GetVelocity().Length2D() < 120.f
		&& Options::Menu.MiscTab.fl_spike_onpeek.GetState() && (pLocal->GetFlags() & FL_ONGROUND) && (!(fabs(pLocal->GetVelocity().z) <= 1.0f)))
	{
		bSendPacket = !(tick % factor);

		if (bSendPacket)
		{
			factor = clamp(static_cast<int>(std::ceil(69.f / flVelocity)), 1, 15);
			factor = clamp(static_cast<int>(std::ceil(64.f / hackManager.pLocal()->GetVelocity().Length2D())), 1, 15);
		}
	}
	else
	{
		if (choked < max_fake_lag && pLocal->GetVelocity().Length2D() >= 1 && (pLocal->GetFlags() & FL_ONGROUND))
		{
			choked++;
			bSendPacket = false;
		}
		else if (choked < max_fake_lag2 && pLocal->GetVelocity().Length2D() >= 1 && !(pLocal->GetFlags() & FL_ONGROUND))
		{
			choked++;
			bSendPacket = false;
		}
		else if (choked < max_fake_lag3 && pLocal->GetVelocity().Length2D() < 1)
		{
			choked++;
			bSendPacket = false;
		}
		else
		{
			choked = 0;
			bSendPacket = true;
		}
	}
}

int CRageBot::get_target_crosshair()
{
	int target = -1;
	float minFoV = Options::Menu.RageBotTab.AimbotFov.GetValue();
	IClientEntity * pLocal = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	Vector ViewOffset = pLocal->GetOrigin() + pLocal->GetViewOffset();
	Vector View; Interfaces::Engine->GetViewAngles(View);
	for (int i = 0; i < Interfaces::EntList->GetMaxEntities(); i++)
	{
		IClientEntity *pEntity = Interfaces::EntList->GetClientEntity(i);
		if (TargetMeetsRequirements(pEntity))
		{
			int NewHitBox = HitScan(pEntity);
			if (NewHitBox >= 0)
			{
				float fov = FovToPlayer(ViewOffset, View, pEntity, 0);
				if (fov < minFoV)
				{
					minFoV = fov;
					target = i;
				}
			}
		}
	}
	return target;
}
bool CRageBot::fakelag_conditions(CUserCmd * cmd)
{
	IClientEntity* local = (IClientEntity*)Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	C_BaseCombatWeapon* weapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(local->GetActiveWeaponHandle());

	IClientEntity* ent = Interfaces::EntList->GetClientEntity(GetTargetCrosshair());
	float min_damage = Options::Menu.RageBotTab.AccuracyMinimumDamage.GetValue();
	float fl_speed = local->GetVelocity().Length2D();
	float fl_vel = local->GetVelocity().Length2D() * Interfaces::Globals->interval_per_tick;
	float dmg = 0.f;
	if (closest() != -1 && CanHit_2(target_point, &dmg) && fl_speed > 1 && (local->GetFlags() & FL_ONGROUND) && !(cmd->buttons & IN_ATTACK))
	{
		return true;
	} // if we can see the head or pelvis or can wallbang them.

	if (cmd->buttons & IN_ATTACK)
	{
		return true;
	}

	if (fabs(local->GetVelocity().z) <= 5.0f && !(local->GetFlags() & FL_ONGROUND))
	{
		return true;
	}

	return false;
}

void CRageBot::fakelag_auto(CUserCmd *pCmd, bool &sendpacket)
{
	IClientEntity* local = (IClientEntity*)Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(local->GetActiveWeaponHandle());

	static auto choked = 0;

	if (!Interfaces::Engine->IsConnected() || !Interfaces::Engine->IsInGame())
		return;

	if ((pCmd->buttons & IN_USE) || local->GetMoveType() == MOVETYPE_LADDER)
		return;

	static int tick; tick++;
	static int factor = 7;

	float fl_speed = local->GetVelocity().Length2D();
	float fl_vel = local->GetVelocity().Length2D() * Interfaces::Globals->interval_per_tick;

	const auto break_fl = BreakLagCompensation_low();
	const auto break_fl_2 = BreakLagCompensation();
	if (GameUtils::IsGrenade(pWeapon) || GameUtils::IsRevolver(pWeapon))
	{
		if (choked < 1)
		{
			choked++;
			sendpacket = false;
		}
		else
		{
			choked = 0;
			sendpacket = true;
		}
	}
	else
	{
		if (Interfaces::m_pClientState->chokedcommands > 0 && fakelag_conditions(pCmd) && Interfaces::m_pClientState->chokedcommands < 15)
		{
			sendpacket = true;
		}
		else if (!(local->GetFlags() & FL_ONGROUND))
		{
			if (choked < break_fl_2)
			{
				choked++;
				sendpacket = false;
			}
			else
			{
				choked = 0;
				sendpacket = true;
			}
		}
		else
		{

			if (fl_speed > 0.1f)
			{
				if (choked < break_fl_2)
				{
					choked++;
					sendpacket = false;
				}
				else
				{
					choked = 0;
					sendpacket = true;
				}
			}
			else
			{
				if (choked < 2)
				{
					choked++;
					sendpacket = false;
				}
				else
				{
					choked = 0;
					sendpacket = true;
				}
			}

		}
	}
}

void reen(Vector& vec)
{
	for (int i = 0; i < 3; ++i)
	{
		while (vec[i] > 180.f)
			vec[i] -= 360.f;

		while (vec[i] < -180.f)
			vec[i] += 360.f;
	}

	vec[2] = 0.f;
}

void reec(Vector& vecAngles)
{
	if (vecAngles[0] > 89.f)
		vecAngles[0] = 89.f;
	if (vecAngles[0] < -89.f)
		vecAngles[0] = -89.f;
	if (vecAngles[1] > 180.f)
		vecAngles[1] = 180.f;
	if (vecAngles[1] < -180.f)
		vecAngles[1] = -180.f;

	vecAngles[2] = 0.f;
}
void RandomSeed(UINT seed)
{
	typedef void(*RandomSeed_t)(UINT);
	static RandomSeed_t m_RandomSeed = (RandomSeed_t)GetProcAddress(GetModuleHandle("vstdlib.dll"), "RandomSeed");
	m_RandomSeed(seed);
	return;
}

void CRageBot::auto_revolver(CUserCmd* m_pcmd)
{
	auto m_local = hackManager.pLocal();
	auto m_weapon = m_local->GetWeapon2();

	if (m_weapon && !shot_this_tick) 
	{
		if (*m_weapon->GetItemDefinitionIndex() == WEAPON_REVOLVER) 
		{
			m_pcmd->buttons |= IN_ATTACK;
			
			float flPostponeFireReady = m_weapon->GetFireReadyTime() ;
			if (flPostponeFireReady > 0 && flPostponeFireReady - 1 < Interfaces::Globals->curtime) 
			{
				m_pcmd->buttons &= ~IN_ATTACK;
			}
		}
	}

}


bool IsAbleToShoot(IClientEntity* pLocal)
{
	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(pLocal->GetActiveWeaponHandle());
	if (!pLocal)return false;
	if (!pWeapon)return false;
	float flServerTime = pLocal->GetTickBase() * Interfaces::Globals->interval_per_tick;
	return (!(pWeapon->GetNextPrimaryAttack() > flServerTime));
}
float hitchance()
{
	float hitchance = 101;
	auto m_local = hackManager.pLocal();
	auto pWeapon = m_local->GetWeapon2();
	if (pWeapon && !warmup)
	{
		if (Options::Menu.RageBotTab.AccuracyHitchance.GetValue() > 0 || hitchance_custom > 0)
		{
			float inaccuracy = pWeapon->GetInaccuracy();
			if (inaccuracy == 0) inaccuracy = 0.0000001;
			inaccuracy = 1 / inaccuracy;
			hitchance = inaccuracy;
		}
		return hitchance;
	}
}
bool CRageBot::CanOpenFire()
{
	IClientEntity* pLocalEntity = (IClientEntity*)Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	if (!pLocalEntity)
		return false;
	C_BaseCombatWeapon* entwep = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(pLocalEntity->GetActiveWeaponHandle());
	float flServerTime = (float)pLocalEntity->GetTickBase() * Interfaces::Globals->interval_per_tick;
	float flNextPrimaryAttack = entwep->GetNextPrimaryAttack();
	std::cout << flServerTime << " " << flNextPrimaryAttack << std::endl;
	return !(flNextPrimaryAttack > flServerTime);
}
float GetLerpTimeX()
{
	int ud_rate = Interfaces::CVar->FindVar("cl_updaterate")->GetFloat();
	ConVar *min_ud_rate = Interfaces::CVar->FindVar("sv_minupdaterate");
	ConVar *max_ud_rate = Interfaces::CVar->FindVar("sv_maxupdaterate");
	if (min_ud_rate && max_ud_rate)
		ud_rate = max_ud_rate->GetFloat();
	float ratio = Interfaces::CVar->FindVar("cl_interp_ratio")->GetFloat();
	if (ratio == 0)
		ratio = 1.0f;
	float lerp = Interfaces::CVar->FindVar("cl_interp")->GetFloat();
	ConVar *c_min_ratio = Interfaces::CVar->FindVar("sv_client_min_interp_ratio");
	ConVar *c_max_ratio = Interfaces::CVar->FindVar("sv_client_max_interp_ratio");
	if (c_min_ratio && c_max_ratio && c_min_ratio->GetFloat() != 1)
		ratio = clamp(ratio, c_min_ratio->GetFloat(), c_max_ratio->GetFloat());
	return max(lerp, (ratio / ud_rate));
}
float InterpFix()
{
	static ConVar* cvar_cl_interp = Interfaces::CVar->FindVar("cl_interp");
	static ConVar* cvar_cl_updaterate = Interfaces::CVar->FindVar("cl_updaterate");
	static ConVar* cvar_sv_maxupdaterate = Interfaces::CVar->FindVar("sv_maxupdaterate");
	static ConVar* cvar_sv_minupdaterate = Interfaces::CVar->FindVar("sv_minupdaterate");
	static ConVar* cvar_cl_interp_ratio = Interfaces::CVar->FindVar("cl_interp_ratio");
	IClientEntity* pLocal = hackManager.pLocal();
	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());
	float cl_interp = cvar_cl_interp->GetFloat();
	int cl_updaterate = cvar_cl_updaterate->GetInt();
	int sv_maxupdaterate = cvar_sv_maxupdaterate->GetInt();
	int sv_minupdaterate = cvar_sv_minupdaterate->GetInt();
	int cl_interp_ratio = cvar_cl_interp_ratio->GetInt();
	if (sv_maxupdaterate <= cl_updaterate)
		cl_updaterate = sv_maxupdaterate;
	if (sv_minupdaterate > cl_updaterate)
		cl_updaterate = sv_minupdaterate;
	float new_interp = (float)cl_interp_ratio / (float)cl_updaterate;
	if (new_interp > cl_interp)
		cl_interp = new_interp;
	return max(cl_interp, cl_interp_ratio / cl_updaterate);
}
void CRageBot::Move(CUserCmd *pCmd, bool &bSendPacket)
{
	IClientEntity* pLocalEntity = (IClientEntity*)Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	if (!pLocalEntity || !Interfaces::Engine->IsConnected() || !Interfaces::Engine->IsInGame() || pLocalEntity->GetHealth() <= 0.f)
		return;

	if (Options::Menu.MiscTab.auto_fakelag.GetState())
		fakelag_auto(pCmd, bSendPacket);
	
	if (!Options::Menu.MiscTab.auto_fakelag.GetState())
		Fakelag(pCmd, bSendPacket);
	
	if (Options::Menu.MiscTab.AntiAimEnable.GetState())
	{
		static int ChokedPackets = 1;
		C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());
		if (!pWeapon)
			return;
		if (ChokedPackets < 1 && pLocalEntity->IsAlive() /*&& pCmd->buttons & IN_ATTACK*/ && !(pWeapon->IsKnife() || pWeapon->IsC4()))
		{
			bSendPacket = false;
		}
		
		else
		{
			if (pLocalEntity->IsAlive())
			{
				DoAntiAim(pCmd, bSendPacket);
			}
			ChokedPackets = 1;
		}
	
	}
	float simtime = 0;
	float current_aim_simulationtime = simtime;
	if (!Options::Menu.RageBotTab.lag_pred.GetState())
	{
		pCmd->tick_count = TIME_TO_TICKS(GetLerpTimeX());
	}
	if (Options::Menu.RageBotTab.lag_pred.GetState())
	{
		pCmd->tick_count = TIME_TO_TICKS(current_aim_simulationtime) + TIME_TO_TICKS(GetLerpTimeX());	
	}
	if (Options::Menu.RageBotTab.AimbotEnable.GetState())
	{
		DoAimbot(pCmd, bSendPacket);
		DoNoRecoil(pCmd);
		auto_revolver(pCmd);
	}
	
	if (Options::Menu.RageBotTab.AimbotAimStep.GetState())
	{
		Vector AddAngs = pCmd->viewangles - LastAngle;
		if (AddAngs.Length2D() > 25.f)
		{
			Normalize(AddAngs, AddAngs);
			AddAngs *= 25;
			pCmd->viewangles = LastAngle + AddAngs;
			GameUtils::NormaliseViewAngle(pCmd->viewangles);
		}
	}
	LastAngle = pCmd->viewangles;
}
Vector BestPoint(IClientEntity *targetPlayer, Vector &final)
{
	IClientEntity* pLocal = hackManager.pLocal();
	trace_t tr;
	Ray_t ray;
	CTraceFilter filter;
	filter.pSkip = targetPlayer;
	ray.Init(final + Vector(0, 0, Options::Menu.RageBotTab.pointscaleval.GetValue() / 10), final);
	Interfaces::Trace->TraceRay(ray, MASK_SHOT, &filter, &tr);
	final = tr.endpos;
	return final;
}
inline float FastSqrt(float x)
{
	unsigned int i = *(unsigned int*)&x;
	i += 127 << 23;
	i >>= 1;
	return *(float*)&i;
}
#define square( x ) ( x * x )
void ClampMovement(CUserCmd* pCommand, float fMaxSpeed)
{
	if (fMaxSpeed <= 0.f)
		return;
	float fSpeed = (float)(FastSqrt(square(pCommand->forwardmove) + square(pCommand->sidemove) + square(pCommand->upmove)));
	if (fSpeed <= 0.f)
		return;
	if (pCommand->buttons & IN_DUCK)
		fMaxSpeed *= 2.94117647f;
	if (fSpeed <= fMaxSpeed)
		return;
	float fRatio = fMaxSpeed / fSpeed;
	pCommand->forwardmove *= fRatio;
	pCommand->sidemove *= fRatio;
	pCommand->upmove *= fRatio;
}

void CRageBot::DoAimbot(CUserCmd *pCmd, bool &bSendPacket)
{

	IClientEntity* pLocal = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	bool FindNewTarget = true;
	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(pLocal->GetActiveWeaponHandle());
	if (!pWeapon)
		return;

	if (pWeapon->GetAmmoInClip() == 0 || pWeapon->IsKnife() || pWeapon->IsC4() || pWeapon->IsGrenade())
		return;

	if (pLocal->GetFlags() & FL_FROZEN)
		return;

	if (warmup)
		return;

	if (IsLocked && TargetID > -0 && HitBox >= 0)
	{
		pTarget = Interfaces::EntList->GetClientEntity(TargetID);
		if (pTarget && TargetMeetsRequirements(pTarget))
		{
			HitBox = HitScan(pTarget);

			if (HitBox >= 0)
			{
				Vector ViewOffset = pLocal->GetOrigin() + pLocal->GetViewOffset();
				Vector View; Interfaces::Engine->GetViewAngles(View);
				float FoV = FovToPlayer(ViewOffset, View, pTarget, HitBox);
				if (FoV < Options::Menu.RageBotTab.AimbotFov.GetValue())
					FindNewTarget = false;
			}
		}
	}
	if (FindNewTarget)
	{
		Globals::Shots = 0;
		TargetID = 0;
		pTarget = nullptr;
		HitBox = -1;
		TargetID = GetTargetCrosshair();
		if (TargetID >= 0)
		{
			pTarget = Interfaces::EntList->GetClientEntity(TargetID);
		}
	}
	if (TargetID >= 0 && pTarget)
	{
		if (CanOpenFire() && (pCmd->buttons & IN_ATTACK))
		{
			auto m_local = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
			auto m_weapon = m_local->GetWeapon2();

			if (m_weapon)
			{
				if (!m_weapon->IsGrenade() && !m_weapon->IsKnife() && !m_weapon->IsC4() && m_weapon->GetAmmoInClip() > 0)
				{
					global_count::shots_fired[pTarget->GetIndex()] += 1;
				}
			}
		}

		float old_sim = 0.f ;
		float current_sim = 0.f ;
		current_sim = pTarget->GetSimulationTime();
		bool can_shoot;

		if (Options::Menu.RageBotTab.delay_shot.GetState() && old_sim != current_sim)
		{
			can_shoot = true;
			old_sim = current_sim;
		}
		else
			can_shoot = false;

		float quickspeed = Options::Menu.ColorsTab.quickstop_speed.GetValue();

		HitBox = HitScan(pTarget);

		if (!CanOpenFire())
			return;

		if (Options::Menu.RageBotTab.QuickStop.GetState() && pLocal->GetFlags() & FL_ONGROUND)
			ClampMovement(pCmd, quickspeed);

		Vector AimPoint = GetHitboxPosition(pTarget, HitBox);
		target_point = AimPoint;
		shot_this_tick = false;
		float hitchanceval = Options::Menu.RageBotTab.AccuracyHitchance.GetValue();
		GlobalBREAK::hitchance = hitchanceval;
		pTarget->GetPredicted(AimPoint);

		bool IsAtPeakOfJump = fabs(pLocal->GetVelocity().z) <= 5.0f;
		global_count::missed_shots[pTarget->GetIndex()] = global_count::shots_fired[pTarget->GetIndex()] - global_count::hits[pTarget->GetIndex()];
		static int missedshots[65];
		missedshots[pTarget->GetIndex()] = global_count::missed_shots[pTarget->GetIndex()];

		if (Options::Menu.RageBotTab.pointscaleyes.GetState())
			AimPoint = BestPoint(pTarget, AimPoint);

		if (pWeapon->IsC4())
			return;

		float dmg = 0.f;
		if (GameUtils::IsScopedWeapon(pWeapon) && !pWeapon->IsScoped() && Options::Menu.RageBotTab.AccuracyAutoScope.GetState())
		{
			pCmd->buttons |= IN_ATTACK2;
		}
		else if ((hitchance()) >= Options::Menu.RageBotTab.AccuracyHitchance.GetValue()* 1.5 && CanHit_2(AimPoint, &dmg)/*new_autowall->PenetrateWall(pTarget, AimPoint)*/ || (GameUtils::IsScout(pWeapon) && !(pLocal->GetFlags() & FL_ONGROUND) && IsAtPeakOfJump))
		{
			if (AimAtPoint(pLocal, AimPoint, pCmd, bSendPacket))
			{
				if (Options::Menu.RageBotTab.AimbotAutoFire.GetState() && !(pCmd->buttons & IN_ATTACK))
				{
					if (Options::Menu.RageBotTab.lag_pred.GetState())
					{
						CBacktracking::Get().ShotBackTrackAimbotStart(pTarget);
						CBacktracking::Get().RestoreTemporaryRecord(pTarget);
						CBacktracking::Get().ShotBackTrackedTick(pTarget);
					}

					if (Options::Menu.RageBotTab.delay_shot.GetState() && can_shoot)
					{
						pCmd->buttons |= IN_ATTACK;
						shot_this_tick = true;
					}
					
					if (!Options::Menu.RageBotTab.delay_shot.GetState())
					{
						pCmd->buttons |= IN_ATTACK;
						shot_this_tick = true;
					}

					if (!(pCmd->buttons |= IN_ATTACK))
					{
						shot_this_tick = false;
					}
				}
				else if (pCmd->buttons & IN_ATTACK || pCmd->buttons & IN_ATTACK2) return;

				if (Options::Menu.RageBotTab.AimbotAutoFire.GetState() && !(pCmd->buttons & IN_ATTACK))
				{
					if (GameUtils::IsZeus(pWeapon))
					{
						if ((pTarget->GetOrigin() - pLocal->GetOrigin()).Length() <= 115.f && pTarget->GetHealth() < 70)
							pCmd->buttons |= IN_ATTACK;
						else if ((pTarget->GetOrigin() - pLocal->GetOrigin()).Length() <= 100.f && pTarget->GetHealth() >= 70)
							pCmd->buttons |= IN_ATTACK;
					}
					else {
						pCmd->buttons |= IN_ATTACK;
						shot_this_tick = true;
					}
				}
				else if (pCmd->buttons & IN_ATTACK || pCmd->buttons & IN_ATTACK2)
					return;
			}
		}

		if (IsAbleToShoot(pLocal) && pCmd->buttons & IN_ATTACK) {
			Globals::Shots += 1;
			global_count::shots_fired[pTarget->GetIndex()] += 1;
		}

		if (Options::Menu.RageBotTab.AimbotAutoFire.GetState() && !(pCmd->buttons & IN_ATTACK) && GameUtils::IsZeus(pWeapon) && CanHit_2(AimPoint, &dmg))
		{
			if ((pTarget->GetOrigin() - pLocal->GetOrigin()).Length() <= 115.f && pTarget->GetHealth() < 70)
				pCmd->buttons |= IN_ATTACK;
			else if ((pTarget->GetOrigin() - pLocal->GetOrigin()).Length() <= 100.f && pTarget->GetHealth() >= 70)
				pCmd->buttons |= IN_ATTACK;
		}

		if (pWeapon != nullptr)
		{
			if (GameUtils::IsPistol(pWeapon) && *pWeapon->m_AttributeManager()->m_Item()->ItemDefinitionIndex() != 64 && !GameUtils::IsGrenade(pWeapon) && !GameUtils::IsBomb(pWeapon))
			{
				static bool WasFiring = false;
				if (hitchance() >= Options::Menu.RageBotTab.AccuracyHitchance.GetValue()* 1.5 || (*pWeapon->m_AttributeManager()->m_Item()->ItemDefinitionIndex() == 31 && CanHit_2(AimPoint, &dmg)))
				{
					if (Options::Menu.RageBotTab.lag_pred.GetState())
					{
						CBacktracking::Get().ShotBackTrackAimbotStart(pTarget);
						CBacktracking::Get().RestoreTemporaryRecord(pTarget);
						CBacktracking::Get().ShotBackTrackedTick(pTarget);
					}
					if (pCmd->buttons & IN_ATTACK)
					{
						if (WasFiring)
						{
							pCmd->buttons &= ~IN_ATTACK;
						}
					}
					WasFiring = pCmd->buttons & IN_ATTACK ? true : false;
				}
			}
		}
	}
}

bool CRageBot::TargetMeetsRequirements(IClientEntity* pEntity)
{
	auto local = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	if (pEntity && !pEntity->is_dormant() && pEntity->IsAlive() && pEntity->GetIndex() != local->GetIndex())
	{

		ClientClass *pClientClass = pEntity->GetClientClass();
		player_info_t pinfo;
		if (pClientClass->m_ClassID == (int)CSGOClassID::CCSPlayer && Interfaces::Engine->GetPlayerInfo(pEntity->GetIndex(), &pinfo))
		{
			// Team Check
			if (pEntity->team() != local->team())
			{
				// Spawn Check
				if (!pEntity->has_gungame_immunity())
				{
					return true;
				}
			}
		}
	}
	return false;
}

float CRageBot::FovToPlayer(Vector ViewOffSet, Vector View, IClientEntity* pEntity, int aHitBox)
{

	CONST FLOAT MaxDegrees = 180.0f;

	Vector Angles = View;

	Vector Origin = ViewOffSet;

	Vector Delta(0, 0, 0);

	Vector Forward(0, 0, 0);

	AngleVectors(Angles, &Forward);

	Vector AimPos = GetHitboxPosition(pEntity, aHitBox);

	VectorSubtract(AimPos, Origin, Delta);

	Normalize(Delta, Delta);

	FLOAT DotProduct = Forward.Dot(Delta);
	// Time to calculate the field of view
	return (acos(DotProduct) * (MaxDegrees / PI));
}
float FovToPoint(Vector ViewOffSet, Vector View, Vector Point)
{
	Vector Origin = ViewOffSet;
	Vector Delta(0, 0, 0);
	Vector Forward(0, 0, 0);
	Vector AimPos = Point;

	AngleVectors(View, &Forward);

	Delta = AimPos - Origin;

	Normalize(Delta, Delta);

	FLOAT DotProduct = Forward.Dot(Delta);

	return (acos(DotProduct) * (180.f / PI));
}
int CRageBot::GetTargetCrosshair()
{
	int target = -1;
	float minFoV = Options::Menu.RageBotTab.AimbotFov.GetValue();
	IClientEntity * pLocal = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	Vector ViewOffset = pLocal->GetOrigin() + pLocal->GetViewOffset();
	Vector View; Interfaces::Engine->GetViewAngles(View);
	for (int i = 0; i < Interfaces::EntList->GetMaxEntities(); i++)
	{
		IClientEntity *pEntity = Interfaces::EntList->GetClientEntity(i);
		if (TargetMeetsRequirements(pEntity))
		{
			int NewHitBox = HitScan(pEntity);
			if (NewHitBox >= 0)
			{
				float fov = FovToPlayer(ViewOffset, View, pEntity, 0);
				if (fov < minFoV)
				{
					minFoV = fov;
					target = i;
				}
			}
		}
	}
	return target;
}
int CRageBot::GetTargetDistance()
{
	int target = -1;
	int minDist = 99999;
	IClientEntity* pLocal = hackManager.pLocal();
	Vector ViewOffset = pLocal->GetOrigin() + pLocal->GetViewOffset();
	Vector View; Interfaces::Engine->GetViewAngles(View);
	for (int i = 0; i < Interfaces::EntList->GetMaxEntities(); i++)
	{
		IClientEntity *pEntity = Interfaces::EntList->GetClientEntity(i);
		if (TargetMeetsRequirements(pEntity))
		{
			int NewHitBox = HitScan(pEntity);
			if (NewHitBox >= 0)
			{
				Vector Difference = pLocal->GetOrigin() - pEntity->GetOrigin();
				int Distance = Difference.Length();
				float fov = FovToPlayer(ViewOffset, View, pEntity, 0);
				if (Distance < minDist && fov < Options::Menu.RageBotTab.AimbotFov.GetValue())
				{
					minDist = Distance;
					target = i;
				}
			}
		}
	}
	return target;
}
float GetFov(const QAngle& viewAngle, const QAngle& aimAngle)
{
	Vector ang, aim;
	AngleVectors(viewAngle, &aim);
	AngleVectors(aimAngle, &ang);
	return RAD2DEG(acos(aim.Dot(ang) / aim.LengthSqr()));
}
double inline __declspec (naked) __fastcall FASTSQRT(double n)
{
	_asm fld qword ptr[esp + 4]
		_asm fsqrt
	_asm ret 8
}
float VectorDistance(Vector v1, Vector v2)
{
	return FASTSQRT(pow(v1.x - v2.x, 2) + pow(v1.y - v2.y, 2) + pow(v1.z - v2.z, 2));
}
int CRageBot::HitScan(IClientEntity* pEntity)
{
	IClientEntity* pLocal = hackManager.pLocal();
	std::vector<int> HitBoxesToScan;

	float health = Options::Menu.RageBotTab.BaimIfUnderXHealth.GetValue();

	int shots = global_count::shots_fired[pEntity->GetIndex()];

	if (shots > Options::Menu.RageBotTab.baim.GetValue() + 4)
		shots = 0;

	Vector enemy_head = GetHitboxPosition(pEntity, 0);
	Vector enemy_body = GetHitboxPosition(pEntity, 2);

	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());

	float dmg = 0.f;

	if (pWeapon != nullptr)
	{
		if (pEntity->GetHealth() < health)
		{
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_UPPER_ARM);
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_UPPER_ARM);
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_FOREARM);
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_FOREARM);
		}
		else if (Options::Menu.RageBotTab.AWPAtBody.GetState() && GameUtils::AWP(pWeapon))
		{
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
			HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
		}
		else
		{
			if (Options::Menu.RageBotTab.target_auto2.GetIndex() < 1 && GameUtils::AutoSniper(pWeapon))
			{
				switch (Options::Menu.RageBotTab.target_auto.GetIndex())
				{
				case 0:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
					break;
				case 1:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_NECK);
					break;
				case 2:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
					break;
				case 3:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
					break;
				case 4:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
					break;
				}

			}

			else if (Options::Menu.RageBotTab.target_scout2.GetIndex() < 1 && GameUtils::IsScout(pWeapon))
			{
				switch (Options::Menu.RageBotTab.target_scout.GetIndex())
				{
				case 0:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
					break;
				case 1:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_NECK);
					break;
				case 2:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
					break;
				case 3:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
					break;
				case 4:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
					break;
				}
			}

			else if (Options::Menu.RageBotTab.target_awp2.GetIndex() < 1 && GameUtils::AWP(pWeapon) && !Options::Menu.RageBotTab.preset_awp.GetState())
			{
				switch (Options::Menu.RageBotTab.target_awp.GetIndex())
				{
				case 0:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
					break;
				case 1:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_NECK);
					break;
				case 2:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
					break;
				case 3:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
					break;
				case 4:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
					break;
				}
			}

			else if (Options::Menu.RageBotTab.target_pistol2.GetIndex() < 1 && GameUtils::IsPistol(pWeapon) && !Options::Menu.RageBotTab.preset_pistol.GetState())
			{
				switch (Options::Menu.RageBotTab.target_pistol.GetIndex())
				{
				case 0:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
					break;
				case 1:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_NECK);
					break;
				case 2:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
					break;
				case 3:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
					break;
				case 4:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
					break;
				}
			}

			else if (Options::Menu.RageBotTab.target_otr2.GetIndex() < 1 && !Options::Menu.RageBotTab.preset_otr.GetState() && GameUtils::IsRifle(pWeapon) || GameUtils::IsShotgun(pWeapon) || GameUtils::IsMP(pWeapon) || GameUtils::IsMachinegun(pWeapon))
			{
				switch (Options::Menu.RageBotTab.target_otr.GetIndex())
				{
				case 0:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
					break;
				case 1:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_NECK);
					break;
				case 2:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
					break;
				case 3:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
					break;
				case 4:
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
					break;
				}
			}

			else
			{
				if (GameUtils::IsZeus(pWeapon))
				{
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
				}
				else if (GetAsyncKeyState(Options::Menu.RageBotTab.bigbaim.GetKey()) && !Options::Menu.m_bIsOpen)
				{
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_NECK);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
				}
				else if (Options::Menu.RageBotTab.headonly_if_vis.GetState() && CanHit_2(enemy_head, &dmg) && !GetAsyncKeyState(Options::Menu.RageBotTab.bigbaim.GetKey()) && !(Options::Menu.RageBotTab.baim_fakewalk.GetState() && resolver->enemy_fakewalk))
				{
					HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
				}
				else
				{
					if ( ( Options::Menu.RageBotTab.baim_fakewalk.GetState() && resolver->enemy_fakewalk )
						|| ( Options::Menu.RageBotTab.baim_fake.GetState() && resolver->enemy_fake )
						|| ( Options::Menu.RageBotTab.baim_inair.GetState() && resolver->enemy_inair ) )
					{
						HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
						HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_UPPER_ARM);
						HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_UPPER_ARM);
						HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
						HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
						HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
						HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
						HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
						HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
					}
					else
					{
						if (GameUtils::AutoSniper(pWeapon))
						{
							
								switch (Options::Menu.RageBotTab.target_auto2.GetIndex())
								{
								case 1:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									break;
								case 2:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_FOREARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_FOREARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									break;
								case 3:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_FOREARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_FOREARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_FOOT);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_FOOT);
									break;
								}
							
						}

						if (pWeapon->isSCOUT())
						{
						
								switch (Options::Menu.RageBotTab.target_scout2.GetIndex())
								{
								case 1:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									break;
								case 2:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									break;
								case 3:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_FOREARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_FOREARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_FOOT);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_FOOT);
									break;
								}
							
						}

						if (GameUtils::AWP(pWeapon))
						{
						
								switch (Options::Menu.RageBotTab.target_awp2.GetIndex())
								{
								case 1:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									break;
								case 2:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									break;
								case 3:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_FOREARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_FOREARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_FOOT);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_FOOT);
									break;
								}
							
						}

						if (GameUtils::IsPistol(pWeapon))
						{
							
								switch (Options::Menu.RageBotTab.target_pistol2.GetIndex())
								{
								case 1:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									break;
								case 2:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									break;
								case 3:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_FOREARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_FOREARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_FOOT);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_FOOT);
									break;
								}
							
						}

						if (GameUtils::IsRifle(pWeapon) || GameUtils::IsShotgun(pWeapon) || GameUtils::IsMP(pWeapon) || GameUtils::IsMachinegun(pWeapon))
						{
							
								switch (Options::Menu.RageBotTab.target_otr2.GetIndex())
								{
								case 1:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									break;
								case 2:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									break;
								case 3:
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_HEAD);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LOWER_CHEST);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_UPPER_ARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_FOREARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_FOREARM);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_CALF);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_FOOT);
									HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_FOOT);
									break;
								}
							
						}

						if (GameUtils::IsZeus(pWeapon))
						{
							HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_BELLY);
							HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_PELVIS);
							HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
							HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_UPPER_CHEST);
							HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_LEFT_THIGH);
							HitBoxesToScan.push_back((int)CSGOHitboxID::HITBOX_RIGHT_THIGH);
						}
					}
				}
			}
		}

		for (auto HitBoxID : HitBoxesToScan)
		{
			Vector Point = GetHitboxPosition(pEntity, HitBoxID);
			float dmg = 0.f;
	//		if (new_autowall->PenetrateWall(pEntity, Point))
			if (CanHit_2(Point, &dmg))
			{
				return HitBoxID;
			}
		}
		return -1;
	}
}

void CRageBot::DoNoRecoil(CUserCmd *pCmd)
{
	if (!Options::Menu.RageBotTab.AimbotEnable.GetState())
		return;
	IClientEntity* pLocal = hackManager.pLocal();
	if (pLocal)
	{
		Vector AimPunch = pLocal->localPlayerExclusive()->GetAimPunchAngle();
		if (AimPunch.Length2D() > 0 && AimPunch.Length2D() < 150)
		{
			pCmd->viewangles -= AimPunch * 2.00;
			GameUtils::NormaliseViewAngle(pCmd->viewangles);		
		}
	}
}
void CRageBot::aimAtPlayer(CUserCmd *pCmd)
{
	IClientEntity* pLocal = hackManager.pLocal();

	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());

	if (!pLocal || !pWeapon)
		return;

	Vector eye_position = pLocal->GetEyePosition();

	float best_dist = pWeapon->GetCSWpnData()->range;

	IClientEntity* target = nullptr;

	for (int i = 0; i <= Interfaces::Engine->GetMaxClients(); i++)
	{
		IClientEntity *pEntity = Interfaces::EntList->GetClientEntity(i);
		if (TargetMeetsRequirements(pEntity))
		{
			if (TargetID != -1)
				target = Interfaces::EntList->GetClientEntity(TargetID);
			else
				target = pEntity;

			Vector target_position = target->GetEyePosition();
			Vector CurPos = target->GetEyePosition() + target->GetAbsOrigin();

			float temp_dist = eye_position.DistTo(target_position);
			QAngle angle = QAngle(0, 0, 0);
			float lowest = 99999999.f;
			if (CurPos.DistToSqr(eye_position) < lowest)
			{
				lowest = CurPos.DistTo(eye_position);
				CalcAngle(eye_position, target_position, angle);
			}
		}
	}
}

bool CRageBot::AimAtPoint(IClientEntity* pLocal, Vector point, CUserCmd *pCmd, bool &bSendPacket)
{
	bool ReturnValue = false;
	if (point.Length() == 0) return ReturnValue;
	Vector angles;
	Vector src = pLocal->GetOrigin() + pLocal->GetViewOffset();
	CalcAngle(src, point, angles);
	GameUtils::NormaliseViewAngle(angles);
	if (angles[0] != angles[0] || angles[1] != angles[1])
	{
		return ReturnValue;
	}
	IsLocked = true;
	Vector ViewOffset = pLocal->GetOrigin() + pLocal->GetViewOffset();
	if (!IsAimStepping)
		LastAimstepAngle = LastAngle;
	float fovLeft = FovToPlayer(ViewOffset, LastAimstepAngle, Interfaces::EntList->GetClientEntity(TargetID), 0);
	Vector AddAngs = angles - LastAimstepAngle;
	if (fovLeft > 37.0f && Options::Menu.MiscTab.OtherSafeMode.GetIndex() == 1)
	{
		if (!pLocal->IsMoving())
		{
			Normalize(AddAngs, AddAngs);
			AddAngs *= 37;
			LastAimstepAngle += AddAngs;
			GameUtils::NormaliseViewAngle(LastAimstepAngle);
			angles = LastAimstepAngle;
		}
		else
		{
			Normalize(AddAngs, AddAngs);
			AddAngs *= 39;
			LastAimstepAngle += AddAngs;
			GameUtils::NormaliseViewAngle(LastAimstepAngle);
			angles = LastAimstepAngle;
		}
	}

	else
	{
		ReturnValue = true;
	}

	if (Options::Menu.RageBotTab.AimbotSilentAim.GetState())
	{
		pCmd->viewangles = angles;
	}
	if (!Options::Menu.RageBotTab.AimbotSilentAim.GetState())
	{
		Interfaces::Engine->SetViewAngles(angles);
	}
	return ReturnValue;
}

void AngleVectors3(const QAngle &angles, Vector& forward)
{
	float	sp, sy, cp, cy;
	SinCos(DEG2RAD(angles[0]), &sp, &cp);
	SinCos(DEG2RAD(angles[1]), &sy, &cy);
	forward.x = cp * cy;
	forward.y = cp * sy;
	forward.z = -sp;
}
//--------------------------------------------------------------------------------
void AngleVectors3(const QAngle &angles, Vector& forward, Vector& right, Vector& up)
{
	float sr, sp, sy, cr, cp, cy;
	SinCos(DEG2RAD(angles[0]), &sp, &cp);
	SinCos(DEG2RAD(angles[1]), &sy, &cy);
	SinCos(DEG2RAD(angles[2]), &sr, &cr);
	forward.x = (cp * cy);
	forward.y = (cp * sy);
	forward.z = (-sp);
	right.x = (-1 * sr * sp * cy + -1 * cr * -sy);
	right.y = (-1 * sr * sp * sy + -1 * cr *  cy);
	right.z = (-1 * sr * cp);
	up.x = (cr * sp * cy + -sr * -sy);
	up.y = (cr * sp * sy + -sr * cy);
	up.z = (cr * cp);
}

#define RandomInt(min, max) (rand() % (max - min + 1) + min)
#define	MASK_ALL				(0xFFFFFFFF)
#define	MASK_SOLID				(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_MONSTER|CONTENTS_GRATE) 			/**< everything that is normally solid */
#define	MASK_PLAYERSOLID		(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER|CONTENTS_GRATE) 	/**< everything that blocks player movement */
#define	MASK_NPCSOLID			(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER|CONTENTS_GRATE) /**< blocks npc movement */
#define	MASK_WATER				(CONTENTS_WATER|CONTENTS_MOVEABLE|CONTENTS_SLIME) 							/**< water physics in these contents */
#define	MASK_OPAQUE				(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_OPAQUE) 							/**< everything that blocks line of sight for AI, lighting, etc */
#define MASK_OPAQUE_AND_NPCS	(MASK_OPAQUE|CONTENTS_MONSTER)										/**< everything that blocks line of sight for AI, lighting, etc, but with monsters added. */
#define	MASK_VISIBLE			(MASK_OPAQUE|CONTENTS_IGNORE_NODRAW_OPAQUE) 								/**< everything that blocks line of sight for players */
#define MASK_VISIBLE_AND_NPCS	(MASK_OPAQUE_AND_NPCS|CONTENTS_IGNORE_NODRAW_OPAQUE) 							/**< everything that blocks line of sight for players, but with monsters added. */
#define	MASK_SHOT				(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEBRIS|CONTENTS_HITBOX) 	/**< bullets see these as solid */
#define MASK_SHOT_HULL			(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEBRIS|CONTENTS_GRATE) 	/**< non-raycasted weapons see this as solid (includes grates) */
#define MASK_SHOT_PORTAL		(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW) 							/**< hits solids (not grates) and passes through everything else */
#define MASK_SHOT_BRUSHONLY			(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_DEBRIS) // non-raycasted weapons see this as solid (includes grates)
#define MASK_SOLID_BRUSHONLY	(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_GRATE) 					/**< everything normally solid, except monsters (world+brush only) */
#define MASK_PLAYERSOLID_BRUSHONLY	(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_PLAYERCLIP|CONTENTS_GRATE) 			/**< everything normally solid for player movement, except monsters (world+brush only) */
#define MASK_NPCSOLID_BRUSHONLY	(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_MONSTERCLIP|CONTENTS_GRATE) 			/**< everything normally solid for npc movement, except monsters (world+brush only) */
#define MASK_NPCWORLDSTATIC		(CONTENTS_SOLID|CONTENTS_WINDOW|CONTENTS_MONSTERCLIP|CONTENTS_GRATE) 					/**< just the world, used for route rebuilding */
#define MASK_SPLITAREAPORTAL	(CONTENTS_WATER|CONTENTS_SLIME) 		



float fov_player(Vector ViewOffSet, Vector View, IClientEntity* entity, int hitbox)
{
	const float MaxDegrees = 180.0f;
	Vector Angles = View, Origin = ViewOffSet;
	Vector Delta(0, 0, 0), Forward(0, 0, 0);
	Vector AimPos = GameUtils::get_hitbox_location(entity, hitbox);
	AngleVectors(Angles, &Forward);
	VectorSubtract(AimPos, Origin, Delta);
	NormalizeNum(Delta, Delta);
	float DotProduct = Forward.Dot(Delta);
	return (acos(DotProduct) * (MaxDegrees / PI));
}

int closest_to_crosshair()
{
	int index = -1;
	float lowest_fov = INT_MAX;
	IClientEntity* local_player = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	if (!local_player)
		return -1;

	if (local_player->IsAlive())
	{
		Vector local_position = local_player->GetAbsOriginlol() + local_player->GetViewOffset();
		Vector angles;
		Interfaces::Engine->GetViewAngles(angles);
		for (int i = 1; i <= Interfaces::Globals->maxClients; i++)
		{
			IClientEntity *entity = Interfaces::EntList->GetClientEntity(i);
			if (!entity || entity->GetHealth() <= 0 || entity->team() == local_player->team() || entity->is_dormant() || entity == local_player)
				continue;
			float fov = fov_player(local_position, angles, entity, 0);
			if (fov < lowest_fov)
			{
				lowest_fov = fov;
				index = i;
			}
		}
		return index;
	}
}

int RandomIntf(int min, int max)
{
	return rand() % max + min;
}

int GetFPSXOXO()
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



void CRageBot::freestanding_spin(CUserCmd* pCmd, bool &bSendPacket) {

	if (!Interfaces::Engine->IsConnected())
		return;

	IClientEntity* local_player = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	float range = Options::Menu.MiscTab.freerange.GetValue();
	static int Ticks = 0;
	if (Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::MOUSE_LEFT))
		return;

	bool no_active = true;
	float bestrotation = 0.f;
	float highestthickness = 0.f;
	static float hold = 0.f;
	Vector besthead;
	float opposite = 0.f;

	auto leyepos = hackManager.pLocal()->GetOrigin_likeajew() + hackManager.pLocal()->GetViewOffset();
	auto headpos = GetHitboxPosition(local_player, 0);
	auto origin = hackManager.pLocal()->GetOrigin_likeajew();

	auto checkWallThickness = [&](IClientEntity* pPlayer, Vector newhead) -> float
	{

		Vector endpos1, endpos2;

		Vector eyepos = pPlayer->GetOrigin_likeajew() + pPlayer->GetViewOffset();
		Ray_t ray;
		ray.Init(newhead, eyepos);
		CTraceFilterSkipTwoEntities filter(pPlayer, hackManager.pLocal());

		trace_t trace1, trace2;
		Interfaces::Trace->TraceRay(ray, MASK_SHOT | MASK_SHOT_BRUSHONLY, &filter, &trace1);

		if (trace1.DidHit())
			endpos1 = trace1.endpos;
		else
			return 0.f;

		ray.Init(eyepos, newhead);
		Interfaces::Trace->TraceRay(ray, MASK_SHOT | MASK_SHOT_BRUSHONLY, &filter, &trace2);

		if (trace2.DidHit())
			endpos2 = trace2.endpos;

		float add = newhead.Dist(eyepos) - leyepos.Dist(eyepos) + 3.f;
		return endpos1.Dist(endpos2) + add / 3;

	};

	int index = closest();
	static IClientEntity* entity;

	if (!local_player->IsAlive())
	{
		hold = 0.f;
	}

	if (index != -1)
		entity = Interfaces::EntList->GetClientEntity(index);

	if (!entity->isValidPlayer() || entity == nullptr)
	{
		pCmd->viewangles.y -= 180.f;
		return;
	}

	float radius = Vector(headpos - origin).Length2D();
	float server_time = hackManager.pLocal()->GetTickBase() * Interfaces::Globals->interval_per_tick;
	double speed = 250;
	double exe = fmod(static_cast<double>(server_time)*speed, range * 2);
	if (index == -1)
	{
		no_active = true;
	}
	else
	{
		for (float besthead = 0; besthead < 7; besthead += 0.1)
		{
			Vector newhead(radius * cos(besthead) + leyepos.x, radius * sin(besthead) + leyepos.y, leyepos.z);
			float totalthickness = 0.f;
			no_active = false;
			totalthickness += checkWallThickness(entity, newhead);
			if (totalthickness > highestthickness)
			{
				highestthickness = totalthickness;
				opposite = besthead - 180;
				bestrotation = besthead;
			}
		}
	}
	if (no_active)
	{
		pCmd->viewangles.y -= (180 - range) + static_cast<float>(exe);
		return;
	}
	else
	{
		if (bSendPacket)
		{		
			jitter = !jitter;
			if (jitter)
				pCmd->viewangles.y = RAD2DEG(bestrotation) - 50.f;	
			else
				pCmd->viewangles.y = RAD2DEG(bestrotation) + 50.f;
		}
		else
		{
			pCmd->viewangles.y = RAD2DEG(bestrotation - range) + static_cast<float>(exe);
		}
		
	}
}
void CRageBot::freestanding_jitter(CUserCmd* pCmd, bool &bSendPacket) {

	if (!Interfaces::Engine->IsConnected())
		return;

	IClientEntity* local_player = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	float range = Options::Menu.MiscTab.freerange.GetValue();
	static int Ticks = 0;
	if (Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::MOUSE_LEFT))
		return;

	bool no_active = true;
	float bestrotation = 0.f;
	float highestthickness = 0.f;
	static float hold = 0.f;
	Vector besthead;
	float opposite = 0.f;

	auto leyepos = hackManager.pLocal()->GetOrigin_likeajew() + hackManager.pLocal()->GetViewOffset();
	auto headpos = GetHitboxPosition(local_player, 0);
	auto origin = hackManager.pLocal()->GetOrigin_likeajew();

	auto checkWallThickness = [&](IClientEntity* pPlayer, Vector newhead) -> float
	{

		Vector endpos1, endpos2;

		Vector eyepos = pPlayer->GetOrigin_likeajew() + pPlayer->GetViewOffset();
		Ray_t ray;
		ray.Init(newhead, eyepos);
		CTraceFilterSkipTwoEntities filter(pPlayer, hackManager.pLocal());

		trace_t trace1, trace2;
		Interfaces::Trace->TraceRay(ray, MASK_SHOT | MASK_SHOT_BRUSHONLY | MASK_OPAQUE_AND_NPCS, &filter, &trace1);

		if (trace1.DidHit())
			endpos1 = trace1.endpos;
		else
			return 0.f;

		ray.Init(eyepos, newhead);
		Interfaces::Trace->TraceRay(ray, MASK_SHOT | MASK_SHOT_BRUSHONLY | MASK_OPAQUE_AND_NPCS, &filter, &trace2);

		if (trace2.DidHit())
			endpos2 = trace2.endpos;

		float add = newhead.Dist(eyepos) - leyepos.Dist(eyepos) + 3.f;
		return endpos1.Dist(endpos2) + add / 3;

	};

	int index = closest();
	static IClientEntity* entity;

	if (!local_player->IsAlive())
	{
		hold = 0.f;
	}

	if (index != -1)
		entity = Interfaces::EntList->GetClientEntity(index); // maybe?

	if (!entity->isValidPlayer() || entity == nullptr)
	{
		pCmd->viewangles.y -= 180.f;
		return;
	}

	float radius = Vector(headpos - origin).Length2D();

	if (index == -1)
	{
		no_active = true;
	}
	else
	{
		for (float besthead = 0; besthead < 7; besthead += 0.1)
		{
			Vector newhead(radius * cos(besthead) + leyepos.x, radius * sin(besthead) + leyepos.y, leyepos.z);
			float totalthickness = 0.f;
			no_active = false;
			totalthickness += checkWallThickness(entity, newhead);
			if (totalthickness > highestthickness)
			{
				hold = besthead;
				highestthickness = totalthickness;
				bestrotation = besthead;
			}
		}
	}
	if (no_active && hold != 0.f)
	{
		pCmd->viewangles.y = RAD2DEG(hold);
		return;
	}
	else if (no_active && hold == 0.f)
	{
		pCmd->viewangles.y -= 180;
		return;
	}
	else
	{
		if (bSendPacket)
		{
			jitter = !jitter;
			if (jitter)
			{
				pCmd->viewangles.y = RAD2DEG(bestrotation) - range;
			}
			else
			{
				pCmd->viewangles.y = RAD2DEG(bestrotation) + range;
			}
	
		}
		else
		{
			jitter = !jitter;
			if (jitter)
				pCmd->viewangles.y = RAD2DEG(bestrotation) + range;
			else
				pCmd->viewangles.y = RAD2DEG(bestrotation) - range;
		}

	}
}


#define M_PI 3.14159265358979323846
void VectorAnglesBrute2(const Vector& forward, Vector &angles)
{
	float tmp, yaw, pitch;
	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0) pitch = 270; else pitch = 90;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0) yaw += 360; tmp = sqrt(forward[0] * forward[0] + forward[1] * forward[1]); pitch = (atan2(-forward[2], tmp) * 180 / M_PI);
		if (pitch < 0) pitch += 360;
	} angles[0] = pitch; angles[1] = yaw; angles[2] = 0;
}
static bool jitter2 = false;
//--------------------------------------------------------------------------------
void CRageBot::Base_AntiAim(CUserCmd* pCmd, IClientEntity* pLocal)
{
	if (GetAsyncKeyState(Options::Menu.MiscTab.manualleft.GetKey())) // right
	{
		dir = true;
		back = false;
		up = false;
		bigboi::indicator = 1;
	}

	if (GetAsyncKeyState(Options::Menu.MiscTab.manualright.GetKey())) // left
	{
		dir = false;
		back = false;
		up = false;
		bigboi::indicator = 2;
	}

	if (GetAsyncKeyState(Options::Menu.MiscTab.manualback.GetKey()))
	{
		dir = false;
		back = true;
		up = false;
		bigboi::indicator = 3;
	}

	if (GetAsyncKeyState(Options::Menu.MiscTab.manualfront.GetKey()))
	{
		dir = false;
		back = false;
		up = true;
		bigboi::indicator = 4;
	}
	static QAngle angles;
	Vector oldAngle = pCmd->viewangles;
	float oldForward = pCmd->forwardmove;
	float oldSideMove = pCmd->sidemove;

//	if ((pCmd->buttons & IN_ATTACK))
//		return;

	IClientEntity* localp = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());
	float speed = localp->GetVelocity().Length2D();

	float lby1 = Options::Menu.MiscTab.lby1.GetValue();
	float randlbyr = Options::Menu.MiscTab.randlbyr.GetValue();
	float twitch_assist = rand() % 100;

	float standing = Options::Menu.MiscTab.standing_desync_offset.GetValue();
	float moving = Options::Menu.MiscTab.moving_desync_offset.GetValue();
	float air = Options::Menu.MiscTab.air_desync_offset.GetValue();

	if (speed <= 10 && (pLocal->GetFlags() & FL_ONGROUND))
	{
		switch (Options::Menu.MiscTab.AntiAimYaw.GetIndex())
		{

		case 1:
		{
			pCmd->viewangles.y -= 180;
		}
		break;

		case 2:
		{
			jitter = !jitter;
			if (jitter)
				pCmd->viewangles.y -= 150;
			else
				pCmd->viewangles.y += 150;

		}
		break;

		case 3:
		{
			if (dir && !back && !up)
				pCmd->viewangles.y -= 90.f ;
			else if (!dir && !back && !up)
				pCmd->viewangles.y += 90.f;
			else if (!dir && back && !up)
				pCmd->viewangles.y -= 180.f ;
			else if (!dir && !back && up)
				pCmd->viewangles.y;
		}
		break;

		case 4:
		{
			pCmd->viewangles.y -= hackManager.pLocal()->GetLowerBodyYaw() + lby1;
		}
		break;

		case 5:
		{
			pCmd->viewangles.y -= hackManager.pLocal()->GetLowerBodyYaw() + RandomFloat(randlbyr, -randlbyr);
		}
		break;

		case 6:
		{
			static float jewishvalue;

			if (jewishvalue < 180)
				jewishvalue = jewishvalue + 2;

			if (jewishvalue >= 180)
				jewishvalue = 0;

			float angle = sinf(jewishvalue) * 0.5 * 180;

			if (Interfaces::m_pClientState->chokedcommands == 1)
			{
				pCmd->viewangles.y -= 180 + angle;
			}
			else
			{
				pCmd->viewangles.y -= 180 - angle;
			}
		}
		break;

		case 7:
		{
			jitter = !jitter;
			if (jitter)
				pCmd->viewangles.y -= 180 + RandomFloat(15, -15);
		
		}
		break;

		}
	}

	if (speed > 10 && (pLocal->GetFlags() & FL_ONGROUND))
	{
		switch (Options::Menu.MiscTab.AntiAimYawrun.GetIndex())
		{

		case 1:
		{
			pCmd->viewangles.y -= 180;
		}
		break;

		case 2:
		{
			jitter = !jitter;
			if (jitter)
				pCmd->viewangles.y -= 150;
			else
				pCmd->viewangles.y += 150;

		}
		break;

		case 3:
		{
			if (dir && !back && !up)
				pCmd->viewangles.y -= 90.f;
			else if (!dir && !back && !up)
				pCmd->viewangles.y += 90.f;
			else if (!dir && back && !up)
				pCmd->viewangles.y -= 180.f;
			else if (!dir && !back && up)
				pCmd->viewangles.y;
		}
		break;

		case 4:
		{
			pCmd->viewangles.y -= hackManager.pLocal()->GetLowerBodyYaw() + lby1;
		}
		break;

		case 5:
		{
			pCmd->viewangles.y -= hackManager.pLocal()->GetLowerBodyYaw() + RandomFloat(randlbyr, -randlbyr);
		}
		break;

		case 6:
		{
			static float jewishvalue;

			if (jewishvalue < 180)
				jewishvalue = jewishvalue + 2;

			if (jewishvalue >= 180)
				jewishvalue = 0;

			float angle = sinf(jewishvalue) * 0.5 * 180;

			if (Interfaces::m_pClientState->chokedcommands == 1)
			{
				pCmd->viewangles.y -= 180 + angle;
			}
			else
			{
				pCmd->viewangles.y -= 180 - angle;
			}
		}
		break;

		case 7:
		{
			jitter = !jitter;
			if (jitter)
				pCmd->viewangles.y -= 180 + RandomFloat(15, -15);
		}
		break;
		}
	}

	if (!(pLocal->GetFlags() & FL_ONGROUND))
	{
		switch (Options::Menu.MiscTab.AntiAimYaw3.GetIndex())
		{

		case 1:
		{
			pCmd->viewangles.y -= 180 ;
		}
		break;

		case 2:
		{
			jitter = !jitter;
			if (jitter)
				pCmd->viewangles.y -= 150 ;
			else
				pCmd->viewangles.y += 150 ;

		}
		break;

		case 3:
		{
			if (dir && !back && !up)
				pCmd->viewangles.y -= 90.f ;
			else if (!dir && !back && !up)
				pCmd->viewangles.y += 90.f ;
			else if (!dir && back && !up)
				pCmd->viewangles.y -= 180.f ;
			else if (!dir && !back && up)
				pCmd->viewangles.y ;
		}
		break;

		case 4:
		{
			pCmd->viewangles.y -= hackManager.pLocal()->GetLowerBodyYaw() + lby1 ;
		}
		break;

		case 5:
		{
			pCmd->viewangles.y -= hackManager.pLocal()->GetLowerBodyYaw() + RandomFloat(randlbyr, -randlbyr) ;
		}
		break;

		case 6:
		{
			static float jewishvalue;

			if (jewishvalue < 180)
				jewishvalue = jewishvalue + 2;

			if (jewishvalue >= 180)
				jewishvalue = 0;

			float angle = sinf(jewishvalue) * 0.5 * 180;

			if (Interfaces::m_pClientState->chokedcommands == 1)
			{
				pCmd->viewangles.y -= 180 + angle;
			}
			else
			{
				pCmd->viewangles.y -= 180 - angle;
			}
		}
		break;

		case 7:
		{
			jitter = !jitter;
			if (jitter)
				pCmd->viewangles.y -= 180 + RandomFloat(15, -15);
		}
		break;
		}
	}

}

void CRageBot::Desync_AntiAim(CUserCmd* pCmd, IClientEntity* pLocal)
{

	IClientEntity* localp = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	float speed = localp->GetVelocity().Length2D();

	float standing = Options::Menu.MiscTab.standing_desync_offset.GetValue();
	float moving = Options::Menu.MiscTab.moving_desync_offset.GetValue();
	float air = Options::Menu.MiscTab.air_desync_offset.GetValue();

	float server_time = hackManager.pLocal()->GetTickBase() * Interfaces::Globals->interval_per_tick;
	float time = TIME_TO_TICKS(server_time);

	while (time >= server_time)
		time = 0.f;

	float idk = rand() % 100;

	if (speed <= 10 && (pLocal->GetFlags() & FL_ONGROUND))
	{
		jitter = !jitter;
		if (time >= server_time / 2)
		{
			if (idk < 70)
			{
				if (!jitter)
					pCmd->viewangles.y = lineRealAngle + standing;

			}
			else
			{
				if (!jitter)
					pCmd->viewangles.y = lineRealAngle - standing;
				
			}
		}
		else
		{
			if (idk < 70)
			{
				if (jitter)
					pCmd->viewangles.y = lineRealAngle - standing;
			}
			else
			{
				if (jitter)
					pCmd->viewangles.y = lineRealAngle + standing;
			
			}
		}

	}

	if (speed > 10 && (pLocal->GetFlags() & FL_ONGROUND))
	{

		jitter = !jitter;
		if (time >= server_time / 2)
		{
			if (jitter)
				pCmd->viewangles.y = lineRealAngle + moving;
		}
		else
		{
			if (jitter)
				pCmd->viewangles.y = lineRealAngle - moving;
		}

	}

	if (!(pLocal->GetFlags() & FL_ONGROUND))
	{
		jitter = !jitter;
		if (time >= server_time / 2)
		{
			if (jitter)
				pCmd->viewangles.y = lineRealAngle - air;
			else
				pCmd->viewangles.y = lineRealAngle + air;
		}
		else
		{
			if (jitter)
				pCmd->viewangles.y = lineRealAngle - air;
			else
				pCmd->viewangles.y = lineRealAngle + air;
		}

	}
}

void CRageBot::DoPitch(CUserCmd * pCmd)
{
	IClientEntity* pLocal = hackManager.pLocal();

	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());

	if ((pCmd->buttons & IN_ATTACK))
		return;

	if (Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::MOUSE_LEFT))
		return;

	int jit = rand() % 100;
	static int Ticks = 0;
	if (Options::Menu.MiscTab.OtherSafeMode.GetIndex() != 2)
	{
		switch (Options::Menu.MiscTab.AntiAimPitch.GetIndex())
		{
		case 0:
			break;
		case 1:
			pCmd->viewangles.x = 89;
			break;
		case 2:
			pCmd->viewangles.x = -89;
			break;
		case 3:
		{
			if (jitter)
				pCmd->viewangles.x = 89;
			else
				pCmd->viewangles.x = -89;
			jitter = !jitter;
		}
		break;
		case 4:
		{
			pCmd->viewangles.x = 0 + RandomFloat(-89.f, 89);
		}
		break;
		case 5:
		{
			pCmd->viewangles.x = 0.f;
		}
		case 6:
		{
			pCmd->viewangles.x = -179.f;
		}
		break;
		case 7:
		{
			pCmd->viewangles.x = 179.f;
		}
		break;
		case 8:
		{
			if (pLocal->GetFlags() & FL_ONGROUND && pLocal->GetVelocity().Length2D() < 0.1f)
			{
				pCmd->viewangles.z = -45.f; // if we do this in the air then fucking rip airstrafe
				pCmd->viewangles.x = 1080.f;
			}
			else
			{
				if (pLocal->GetVelocity().Length2D() > 250 && !(pLocal->GetFlags() & FL_ONGROUND) && !GetAsyncKeyState(Options::Menu.MiscTab.CircleStrafeKey.GetKey()))
				{
					if (jit < 90)
						pCmd->viewangles.x = 175.9995f;
					else
					{
						pCmd->viewangles.x = 270;

					}
				}
				else
				{
					if (jit < 90)
						pCmd->viewangles.x = 1080.f;
					else
					{
						pCmd->viewangles.x = 991.f;

					}
				}
			}

		}
		break;

		case 9:
		{
			pCmd->viewangles.x = -1080.f;
		}
		break;
		case 10:
		{
			pCmd->viewangles.x = Options::Menu.MiscTab.custom_pitch.GetValue();
		}
		break;
		}
	}
}

void CRageBot::DoPitch_2(CUserCmd * pCmd)
{
	IClientEntity* pLocal = hackManager.pLocal();

	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());

	if (GameUtils::IsGrenade(pWeapon))
		return;

	if ((pCmd->buttons & IN_ATTACK))
		return;

	if (Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::MOUSE_LEFT))
		return;

	if ((Options::Menu.MiscTab.OtherSafeMode.GetIndex() < 3 || Options::Menu.MiscTab.OtherSafeMode.GetIndex() > 2 && (pLocal->GetFlags() & FL_ONGROUND)))
		pCmd->viewangles.x = 89.f;
	if (Options::Menu.MiscTab.OtherSafeMode.GetIndex() > 2 && !(pLocal->GetFlags() & FL_ONGROUND))
		DoPitch(pCmd);
}

void CRageBot::DoAntiAim(CUserCmd *pCmd, bool &bSendPacket)
{
	IClientEntity* pLocal = hackManager.pLocal();
	C_BaseCombatWeapon* pWeapon = (C_BaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());
	Vector oldview = pCmd->viewangles;

	if (!Interfaces::Engine->IsInGame() || !Interfaces::Engine->IsConnected())
		return;

	if (pLocal->GetHealth() <= 0.f)
		return;

	if ((pCmd->buttons & IN_USE) || pLocal->GetMoveType() == MOVETYPE_LADDER)
		return;

	if (Interfaces::m_iInputSys->IsButtonDown(ButtonCode_t::MOUSE_LEFT))
		return;

	if (GameUtils::IsGrenade(pWeapon) || pWeapon->IsGrenade())
		return;

	if (pWeapon && (pWeapon->IsKnife() || pWeapon->IsC4()))
	{
		if (!CanOpenFire() || pCmd->buttons & IN_ATTACK2)
			return;
	}

	if (!Options::Menu.MiscTab.AntiAimEnable.GetState())
		return;

	if (Options::Menu.MiscTab.disable_on_dormant.GetState() && closest() == -1)
		return;

	if (pLocal->GetFlags() & FL_FROZEN)
		return;
	
	if (!Options::Menu.MiscTab.do_freestanding.GetState())
	{
		DoPitch(pCmd);

		if (!bSendPacket)		
			Base_AntiAim(pCmd, hackManager.pLocal());
		
		if (bSendPacket)
			Desync_AntiAim(pCmd, hackManager.pLocal());
	}

	if (Options::Menu.MiscTab.do_freestanding.GetState())
	{
		DoPitch_2(pCmd);	
		Options::Menu.MiscTab.freestandtype.GetState() ? freestanding_spin(pCmd, bSendPacket) : freestanding_jitter(pCmd, bSendPacket);				
	}

	if (Options::Menu.MiscTab.antilby.GetIndex() > 0 && (pLocal->GetFlags() & FL_ONGROUND))
		anti_lby(pCmd, bSendPacket);
}
