#include "Resolver.h"
#include "Ragebot.h"
#include "Hooks.h"
#include "RenderManager.h"
#include "edge.h"
#include "LagCompensation2.h"
#include "laggycompensation.h"
#include "global_count.h"
#include "edge.h"
#include "Autowall.h"
#ifdef NDEBUG
#define XorStr( s ) ( XorCompileTime::XorString< sizeof( s ) - 1, __COUNTER__ >( s, std::make_index_sequence< sizeof( s ) - 1>() ).decrypt() )
#else
#define XorStr( s ) ( s )
#endif

resolver_setup * resolver = new resolver_setup();

namespace global_count
{
	int hits[65] = { 0.f };
	int shots_fired[65] = { 0.f };
	int missed_shots[64] = { 0.f };
	bool didhit[64] = { 0.f };
	bool on_fire;

	int missed;
	int hit;
}

void calculate_angle(Vector src, Vector dst, Vector &angles)
{
	Vector delta = src - dst;
	vec_t hyp = delta.Length2D();
	angles.y = (atan(delta.y / delta.x) * 57.295779513082f);
	angles.x = (atan(delta.z / hyp) * 57.295779513082f);
	angles.x = (atan(delta.z / hyp) * 57.295779513082f);
	angles[2] = 0.0f;
	if (delta.x >= 0.0) angles.y += 180.0f;
}
void NormalizeNumX(Vector &vIn, Vector &vOut)
{
	float flLen = vIn.Length();
	if (flLen == 0) {
		vOut.Init(0, 0, 1);
		return;
	}
	flLen = 1 / flLen;
	vOut.Init(vIn.x * flLen, vIn.y * flLen, vIn.z * flLen);
}

float fov_entX(Vector ViewOffSet, Vector View, IClientEntity* entity, int hitbox)
{
	const float MaxDegrees = 180.0f;
	Vector Angles = View, Origin = ViewOffSet;
	Vector Delta(0, 0, 0), Forward(0, 0, 0);
	Vector AimPos = GetHitboxPosition(entity, hitbox);
	AngleVectors(Angles, &Forward);
	VectorSubtract(AimPos, Origin, Delta);
	NormalizeNumX(Delta, Delta);
	float DotProduct = Forward.Dot(Delta);
	return (acos(DotProduct) * (MaxDegrees / PI));
}

int closestX()
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

		if (!entity || entity->GetHealth() <= 0 || entity->team() == local_player->team() || entity->IsDormant() || entity == local_player)
			continue;

		float fov = fov_entX(local_position, angles, entity, 0);
		if (fov < lowest_fov)
		{
			lowest_fov = fov;
			index = i;
		}

	}
	return index;

}
#define MASK_SHOT_BRUSHONLY			(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_DEBRIS)

float apply_freestanding(IClientEntity *enemy)
{
	IClientEntity* local_player = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	if (!(hackManager.pLocal()->GetHealth() > 0))
		return 0.0f;
	bool no_active = true;
	float bestrotation = 0.f;
	float highestthickness = 0.f;
	static float hold = 0.f;
	Vector besthead;

	auto leyepos = enemy->GetOrigin_likeajew() + enemy->GetViewOffset();
	auto headpos = GetHitboxPosition(enemy, 0);
	auto origin = enemy->GetOrigin_likeajew();

	int index = closestX();

	if (index == -1)
		return 0.0f;

	auto checkWallThickness = [&](IClientEntity* pPlayer, Vector newhead) -> float
	{

		Vector endpos1, endpos2;

		Vector eyepos = local_player->GetOrigin_likeajew() + local_player->GetViewOffset();
		Ray_t ray;
		CTraceFilterSkipTwoEntities filter(local_player, enemy);
		trace_t trace1, trace2;

		ray.Init(newhead, eyepos);
		Interfaces::Trace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace1);

		if (trace1.DidHit())
		{
			endpos1 = trace1.endpos;
			float add = newhead.Dist(eyepos) - leyepos.Dist(eyepos) + 75.f;
			return endpos1.Dist(eyepos) + add / 2; // endpos2
		}

		else
		{
			if (index == ragebot->get_target_crosshair())
			{
				ray.Init(eyepos, newhead);
				Interfaces::Trace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace2);

				if (trace2.DidHit())
				{
					endpos1 = trace1.endpos;
					float add = newhead.Dist(eyepos) - leyepos.Dist(eyepos) - 75;
					return endpos1.Dist(eyepos) + add / 2; // endpos2
				}
				else
				{
					endpos1 = trace1.endpos;
					float add = newhead.Dist(eyepos) - leyepos.Dist(eyepos);
					return endpos1.Dist(eyepos) + add / 2; // endpos2
				}
			}
			if (index != ragebot->get_target_crosshair())
			{
				endpos1 = trace1.endpos;
				float add = newhead.Dist(eyepos) - leyepos.Dist(eyepos) - 75.f;
				return endpos1.Dist(eyepos) + add / 2; // endpos2
			}
		}
		/*
		Do note that the above method will only work, for occasional body aim against other mirror
		users since this just uses MASK_SHOT, but we use, for the AA, MASK_OPAQUE_AND_NPCS.
		*/
	};

	float radius = Vector(headpos - origin).Length2D();

	for (float besthead = 0; besthead < 7; besthead += 0.1)
	{
		Vector newhead(radius * cos(besthead) + leyepos.x, radius * sin(besthead) + leyepos.y, leyepos.z);
		float totalthickness = 0.f;
		no_active = false;
		totalthickness += checkWallThickness(enemy, newhead);
		if (totalthickness > highestthickness)
		{
			highestthickness = totalthickness;

			bestrotation = besthead;
		}
	}
	return RAD2DEG(bestrotation);

}

float apply_freestanding_low_performance(IClientEntity *enemy, float lowerbody, float brute_value)
{
	IClientEntity* local_player = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	if (!(hackManager.pLocal()->GetHealth() > 0))
		return 0.0f;
	bool no_active = true;
	float bestrotation = 0.f;
	float highestthickness = 0.f;
	static float hold = 0.f;
	Vector besthead;

	auto leyepos = enemy->GetOrigin_likeajew() + enemy->GetViewOffset();
	auto headpos = GetHitboxPosition(enemy, 0);
	auto origin = enemy->GetOrigin_likeajew();

	int index = closestX();

	if (index == -1)
		return 0.0f;

	auto checkWallThickness = [&](IClientEntity* pPlayer, Vector newhead) -> float
	{

		Vector endpos1, endpos2;

		Vector eyepos = local_player->GetOrigin_likeajew() + local_player->GetViewOffset();
		Ray_t ray;
		CTraceFilterSkipTwoEntities filter(local_player, enemy);
		trace_t trace1, trace2;

		ray.Init(newhead, eyepos);
		Interfaces::Trace->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace1);

		if (index == ragebot->get_target_crosshair())
		{
			if (trace1.DidHit())
			{
				endpos1 = trace1.endpos;
				float add = newhead.Dist(eyepos) - leyepos.Dist(eyepos) + 75.f;
				return endpos1.Dist(eyepos) + add / 2; // endpos2
			}

			else
			{
				endpos1 = trace1.endpos;
				float add = newhead.Dist(eyepos) - leyepos.Dist(eyepos) - 75.f;
				return endpos1.Dist(eyepos) + add / 2; // endpos2
			}
		}

		else
		{
			return lowerbody - brute_value;
		}

	};

	float radius = Vector(headpos - origin).Length2D();

	for (float besthead = 0; besthead < 7; besthead += 0.1)
	{
		Vector newhead(radius * cos(besthead) + leyepos.x, radius * sin(besthead) + leyepos.y, leyepos.z);
		float totalthickness = 0.f;
		no_active = false;
		totalthickness += checkWallThickness(enemy, newhead);
		if (totalthickness > highestthickness)
		{
			highestthickness = totalthickness;

			bestrotation = besthead;
		}
	}
	return RAD2DEG(bestrotation);

}

inline float RandomFloat(float min, float max)
{
	static auto fn = (decltype(&RandomFloat))(GetProcAddress(GetModuleHandle("vstdlib.dll"), "RandomFloat"));
	return fn(min, max);
}
void resolver_setup::preso(IClientEntity * pEntity)
{
	switch (Options::Menu.RageBotTab.preso.GetIndex())
	{
	case 1:
	{
		pEntity->GetEyeAnglesXY()->x = 89;
		resolver->resolved_pitch = 89.f;
	}
	break;
	case 2:
	{
		pEntity->GetEyeAnglesXY()->x = -89;
		resolver->resolved_pitch = -89.f;
	}
	break;
	case 3:
	{
		pEntity->GetEyeAnglesXY()->x = 0;
		resolver->resolved_pitch = 0.f;
	}
	break;
	case 4:
	{
		float last_simtime[64] = { 0.f };
		float stored_pitch_1[64] = { 0.f };
		float fixed_pitch[64] = { 0.f };

		bool has_been_set[64] = { false };

		const auto local = hackManager.pLocal();
		if (!local)
			return;

		for (auto i = 0; i < Interfaces::Engine->GetMaxClients(); ++i)
		{
			CMBacktracking* backtrack;
			const auto player = const_cast <IClientEntity*>(Interfaces::EntList->GetClientEntity(i));

			if (!player || local == player || player->team() == local->team() || player->IsImmune() || player->IsDormant())
			{
				stored_pitch_1[i] = { FLT_MAX };
				fixed_pitch[i] = { FLT_MAX };
				has_been_set[i] = { false };
				continue;
			}

			const auto eye = player->GetEyeAnglesXY();
			const auto sim = player->GetSimulationTime();

			auto pitch = 0.f;

			if (stored_pitch_1[i] == FLT_MAX || !has_been_set[i])
			{
				stored_pitch_1[i] = eye->x;
				has_been_set[i] = true;
			}
			
			if (stored_pitch_1[i] - eye->x < 30 && stored_pitch_1[i] - eye->x > -30)
			{
				pitch = eye->x;
			}
			else
			{
				pitch = stored_pitch_1[i];
			}

			resolver->resolved_pitch = pitch;

			player->SetAngle2(Vector(pitch, resolver->resolved_yaw, 0));
			player->SetAbsAngles(Vector(pitch, resolver->resolved_yaw, 0));
		}
	}
	break;

	}

}

player_info_t GetInfo2(int Index) {
	player_info_t Info;
	Interfaces::Engine->GetPlayerInfo(Index, &Info);
	return Info;
}

static int GetSequenceActivity(IClientEntity* pEntity, int sequence)
{
	const model_t* pModel = pEntity->GetModel();
	if (!pModel)
		return 0;

	auto hdr = Interfaces::ModelInfo->GetStudiomodel(pEntity->GetModel());

	if (!hdr)
		return -1;

	static auto get_sequence_activity = reinterpret_cast<int(__fastcall*)(void*, studiohdr_t*, int)>(Utilities::Memory::FindPatternV2("client_panorama.dll", "55 8B EC 53 8B 5D 08 56 8B F1 83"));

	return get_sequence_activity(pEntity, hdr, sequence);
}

float NormalizeFloatToAngle(float input)
{
	for (auto i = 0; i < 3; i++) {
		while (input < -180.0f) input += 360.0f;
		while (input > 180.0f) input -= 360.0f;
	}
	return input;
}

float override_yaw(IClientEntity* player, IClientEntity* local) {
	Vector eye_pos, pos_enemy;
	CalcAngle(player->GetEyePosition(), local->GetEyePosition(), eye_pos);

	if (Render::TransformScreen(player->GetOrigin(), pos_enemy))
	{
		if (GUI.GetMouse().x < pos_enemy.x)
			return (eye_pos.y - 90);
		else if (GUI.GetMouse().x > pos_enemy.x)
			return (eye_pos.y + 90);
	}

}

#define M_PI 3.14159265358979323846
void VectorAnglesBrute(const Vector& forward, Vector &angles)
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

Vector calc_angle_trash(Vector src, Vector dst)
{
	Vector ret;
	VectorAnglesBrute(dst - src, ret);
	return ret;
}

bool playerStoppedMoving(IClientEntity* pEntity)
{
	for (int w = 0; w < 13; w++)
	{
		AnimationLayer currentLayer = pEntity->GetAnimOverlays()[1];
		const int activity = pEntity->GetSequenceActivity(currentLayer.m_nSequence);
		float flcycle = currentLayer.m_flCycle, flprevcycle = currentLayer.m_flPrevCycle, flweight = currentLayer.m_flWeight, flweightdatarate = currentLayer.m_flWeightDeltaRate;
		uint32_t norder = currentLayer.m_nOrder;
		if (activity == ACT_CSGO_IDLE_ADJUST_STOPPEDMOVING)
			return true;
	}
	return false;
}


int IClientEntity::GetSequenceActivity(int sequence)
{
	auto hdr = Interfaces::ModelInfo->GetStudiomodel(this->GetModel());

	if (!hdr)
		return -1;

	static auto getSequenceActivity = (DWORD)(Utilities::Memory::FindPatternV2("client_panorama.dll", "55 8B EC 83 7D 08 FF 56 8B F1 74"));
	static auto GetSequenceActivity = reinterpret_cast<int(__fastcall*)(void*, studiohdr_t*, int)>(getSequenceActivity);

	return GetSequenceActivity(this, hdr, sequence);
}

bool balance979(IClientEntity* player, AnimationLayer *layer)
{
	for (int i = 0; i < 15; i++)
	{
		const int activity = player->GetSequenceActivity(layer[i].m_nSequence);
		if (activity == 979)
		{
			return true;
		}
	}
	return false;
}

bool resolver_setup::adjusting_stop(IClientEntity* player, AnimationLayer *layer)
{
	for (int i = 0; i < 15; i++)
	{
		for (int s = 0; s < 14; s++)
		{
			auto anim_layer = player->GetAnimOverlay(s);
			if (!anim_layer.m_pOwner)
				continue;
			const int activity = player->GetSequenceActivity(layer[i].m_nSequence);
			if (activity == 981 && anim_layer.m_flWeight == 1.f)
			{
				return true;
			}
		}
	}
	return false;
}

bool resolver_setup::high_delta(IClientEntity * player, AnimationLayer *layer)
{
	for (int s = 0; s < 14; s++)
	{
		AnimationLayer record[15];

		auto anim_layer = player->GetAnimOverlay(s);
		auto anime = &player->GetAnimOverlays()[1];

		if (!anim_layer.m_pOwner)
			continue;

		for (auto i = 0; i < Interfaces::Engine->GetMaxClients(); ++i)
		{
			const int activity = player->GetSequenceActivity(layer[i].m_nSequence);

			if ((anim_layer.m_flPrevCycle != anim_layer.m_flCycle || anim_layer.m_flWeight == 1.f) && activity == 979)
			{
				return true;
			}
		}
	}
	return false;
}

bool resolver_setup::low_delta(IClientEntity * player, AnimationLayer *layer)
{
	for (int s = 0; s < 14; s++)
	{
		auto anim_layer = player->GetAnimOverlay(s);
		auto anime = &player->GetAnimOverlays()[1];

		if (!anim_layer.m_pOwner)
			continue;

		for (auto i = 0; i < Interfaces::Engine->GetMaxClients(); ++i)
		{
			const int activity = player->GetSequenceActivity(layer[i].m_nSequence);

			if (anim_layer.m_flPrevCycle > 0.92f && anim_layer.m_flCycle > 0.92f && anim_layer.m_flWeight == 0.f)
			{
				return true;
			}
		}
	}
	return false;
}

bool resolver_setup::is_fakewalk(IClientEntity* player, float speed, AnimationLayer * layer)
{
	if (hackManager.pLocal()->GetHealth() <= 0)
		return false;

	if (player->IsDormant())
		return false;

	for (int s = 0; s < 14; s++)
	{
		const int activity = player->GetSequenceActivity(layer[s].m_nSequence);
		if (speed > 0.1f && player->GetAnimOverlay(12).m_flWeight != 0.f && activity == 979)
		{
			return true;
		}
	}
	return false;
}

bool predict_lby(IClientEntity* player, float oldlby[64], float lby, float speed)
{
	static bool nextflick[64];

	static float add_time[64];

	const auto sim = player->GetSimulationTime();

	if (!Options::Menu.RageBotTab.resolver_predictlby.GetState())
		return false;

	for (auto i = 0; i < Interfaces::Engine->GetMaxClients(); ++i)
	{
		if (oldlby[i] != lby && speed <= 0.1f)
		{
			add_time[i] = Interfaces::Globals->interval_per_tick + 1.1f;
		}

		if (speed >= 0.1f)
		{
			add_time[i] = 0.22f;
			nextflick[i] = sim + add_time[i];
		}

		if (sim >= nextflick[i] && speed <= 0.1f)
		{
			add_time[i] = 1.1f;
			nextflick[i] = sim + add_time[i];
			return true;
		}
	}
	return false;
}

void resolver_setup::resolver_nospread(IClientEntity* pEntity, int CurrentTarget)
{
	static float movinglby[64] = { 0.f };
	static float oldlby[64] = { 0.f };
	static float delta[64] = { 0.f };

	static bool breaking[64] = { false };
	static bool lowdelta[64] = { false };
	static bool highdelta[64] = { false };

	IClientEntity* local = (IClientEntity*)Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

	if (!local)
		return;

	for (auto i = 0; i < Interfaces::Engine->GetMaxClients(); ++i)
	{
		const auto player = const_cast <IClientEntity*>(Interfaces::EntList->GetClientEntity(i));

		if (player == nullptr)
			continue;

		if (player == local)
			continue;

		if (player->team() == local->team())
			continue;

		if (!player->IsAlive())
			continue;

		if (player->IsDormant())
			continue;

		Vector local_position = local->m_VecORIGIN() + local->m_vecViewOffset();
		Vector velocityangle;

		VectorAnglesBrute(player->GetVelocity(), velocityangle);

		const auto lby = player->GetLowerBodyYaw();
		const auto eye = player->GetEyeAnglesXY();
		const auto sim = player->GetSimulationTime();
		const auto speed = player->GetVelocity().Length2D();

		static float movinglby[64] = { 0.f };

		if (oldlby[i] != lby && oldlby[i] == 0.f && speed < 1.f)
		{
			oldlby[i] = lby;
		}

		if (oldlby[i] != lby && oldlby[i] != 0.f && speed < 1.f)
		{
			breaking[i] = true;
			delta[i] = oldlby[i] - lby;
		}

		if (speed > 1.f)
		{
			movinglby[i] = lby;
		}

		float yaw[64]; //	bool no_freestand = !(Options::Menu.RageBotTab.apply_freestanding.GetState());
		if (Options::Menu.RageBotTab.resolver.GetIndex() > 2)
		{
			if (!(player->GetFlags() & FL_ONGROUND))
			{
				if (Options::Menu.RageBotTab.resolver_dofreestanding.GetState())
					yaw[i] = apply_freestanding(player); //if (!no_freestand && (player->GetIndex() == closestX()))
				else
					yaw[i] = velocityangle.y - 180.f;
			}
			else
			{
				if (speed < 1.f && breaking[i])
				{
					yaw[i] = lby - delta[i];
				}

				if (speed < 1.f && !breaking[i])
				{
					yaw[i] = lby;
				}

				if (speed >= 1.f && movinglby[i] == 0.0f)
				{
					yaw[i] = lby;
				}

				if (speed >= 1.f && movinglby[i] != 0.f)
				{
					yaw[i] = movinglby[i];
				}

			}
			player->GetEyeAnglesXY()->y = yaw[i];
		}
		else
		{
			if (!(player->GetFlags() & FL_ONGROUND))
			{
				switch (Globals::Shots % 15)
				{
				case 0: yaw[i] = calc_angle_trash(local->GetHealth() <= 0 ? local->m_VecORIGIN() : local_position, player->m_VecORIGIN()).y - 180.f;
					break;
				case 1: yaw[i] = velocityangle.y;
					break;
				case 2: yaw[i] = velocityangle.y - 180.f;
					break;
				case 3: yaw[i] = lby;
					break;
				case 4: yaw[i] = lby - 45.f;
					break;
				case 5: yaw[i] = lby + 90.f;
					break;
				case 6: yaw[i] = lby - 130.f;
					break;
				case 7: yaw[i] = lby - 180.f;
					break;
				case 8: yaw[i] = calc_angle_trash(local->GetHealth() <= 0 ? local->m_VecORIGIN() : local_position, player->m_VecORIGIN()).y - 160.f;
					break;
				case 9: yaw[i] = calc_angle_trash(local->GetHealth() <= 0 ? local->m_VecORIGIN() : local_position, player->m_VecORIGIN()).y + 160.f;
					break;
				case 10: yaw[i] = calc_angle_trash(local->GetHealth() <= 0 ? local->m_VecORIGIN() : local_position, player->m_VecORIGIN()).y - 180.f;
					break;
				case 11: yaw[i] = velocityangle.y - 180.f;
					break;
				case 12: yaw[i] = velocityangle.y;
					break;
				case 13: yaw[i] = velocityangle.y - 45.f;
					break;
				case 14: yaw[i] = velocityangle.y + 90.f;
					break;
				case 15: yaw[i] = player->GetEyeAnglesXY()->y - lby;
					break;
				}
			}
			else
			{
				if (speed < 1.f && breaking[i])
				{
					yaw[i] = lby - delta[i];
				}

				if (speed < 1.f && !breaking[i])
				{
					yaw[i] = lby;
				}

				if (speed >= 1.f && movinglby[i] != 0.f)
				{
					yaw[i] = movinglby[i];
				}

				if (speed >= 1.f && movinglby[i] == 0.f)
				{
					yaw[i] = lby - 60;
				}
			}
			player->GetEyeAnglesXY()->y = yaw[i];
		}

	}
}

void resolver_setup::resolver_freestand(IClientEntity* pEntity, int CurrentTarget)
{
	//	CUserCmd* cmdlist = *(CUserCmd**)((DWORD)Interfaces::pInput + 0xEC);
	//	CUserCmd* pCmd = cmdlist;

	bool no_freestand = !(Options::Menu.RageBotTab.resolver_dofreestanding.GetState());

	float movinglby[64];

	float yaw[64] = { 0.f };
	float delta[64] = { 0.f };
	float oldlby[64] = { 0.f };
	float last_sim[64] = { 0.f };

	static bool nofake[64];
	static bool didmove[64];
	static bool breaking[64];

	for (auto i = 0; i < Interfaces::Engine->GetMaxClients(); ++i)
	{
		const auto player = const_cast <IClientEntity*>(Interfaces::EntList->GetClientEntity(i));

		if (player == nullptr)
			continue;

		if (player == hackManager.pLocal())
			continue;

		if (player->team() == hackManager.pLocal()->team())
			continue;

		if (!player->IsAlive())
			continue;

		if (player->IsDormant())
			continue;

		const auto speed = player->GetVelocity().Length2D();
		const auto lby = player->GetLowerBodyYaw();
		const auto sim = player->GetSimulationTime();

		if (speed <= 0.1f)
		{

			if (oldlby[i] != lby)
			{
				oldlby[i] = lby;
			}
		}

		if (Options::Menu.RageBotTab.resolver_compensate_performance.GetState())
		{
			yaw[i] = apply_freestanding_low_performance(player, lby, oldlby[i]);
		}
		if (!Options::Menu.RageBotTab.resolver_compensate_performance.GetState())
		{
			yaw[i] = apply_freestanding(player);
		}


		player->GetEyeAnglesXY()->y = yaw[i];
	}

}

int total_missed[64];
int total_hit[64];
IGameEvent* event = nullptr;
extra s_extra;
void angle_correction::missed_due_to_desync(IGameEvent* event) {

	if (event == nullptr)
		return;
	int user = event->GetInt("userid");
	int attacker = event->GetInt("attacker");
	bool player_hurt[64], hit_entity[64];

	if (Interfaces::Engine->GetPlayerForUserID(user) != Interfaces::Engine->GetLocalPlayer()
		&& Interfaces::Engine->GetPlayerForUserID(attacker) == Interfaces::Engine->GetLocalPlayer()) {
		player_hurt[Interfaces::Engine->GetPlayerForUserID(user)] = true;
	}

	if (Interfaces::Engine->GetPlayerForUserID(user) != Interfaces::Engine->GetLocalPlayer())
	{
		Vector bullet_impact_location = Vector(event->GetFloat("x"), event->GetFloat("y"), event->GetFloat("z"));
		if (Globals::aim_point != bullet_impact_location) return;
		hit_entity[Interfaces::Engine->GetPlayerForUserID(user)] = true;
	}
	if (!player_hurt[Interfaces::Engine->GetPlayerForUserID(user)] && hit_entity[Interfaces::Engine->GetPlayerForUserID(user)]) {
		s_extra.current_flag[Interfaces::Engine->GetPlayerForUserID(user)] = correction_flags::DESYNC;
		++total_missed[Interfaces::Engine->GetPlayerForUserID(user)];
	}
	if (player_hurt[Interfaces::Engine->GetPlayerForUserID(user)] && hit_entity[Interfaces::Engine->GetPlayerForUserID(user)]) {
		++total_hit[Interfaces::Engine->GetPlayerForUserID(user)];
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

float lerp_time()
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
void angle_correction::ac_smart(IClientEntity* entity)
{
	missed_due_to_desync(event);

	if (is_slow_walking(entity))
	{
		s_extra.current_flag[entity->GetIndex()] = correction_flags::SLOW_WALK;
		resolver->enemy_fakewalk = true;
	}
	else
		resolver->enemy_fakewalk = false;

	float old_yaw[64] = { NULL };
	float current_yaw[64] = { NULL };
	float resolved_yaw[64] = { 0.f };

	float b1gdelta[64] = { 0.f };

	if (entity->GetEyeAnglesXY()->y != current_yaw[entity->GetIndex()])
	{
		b1gdelta[entity->GetIndex()] = entity->GetEyeAnglesXY()->y - old_yaw[entity->GetIndex()];

		if (current_yaw[entity->GetIndex()] == NULL)
		{
			old_yaw[entity->GetIndex()] = current_yaw[entity->GetIndex()];
			current_yaw[entity->GetIndex()] = entity->GetEyeAnglesXY()->y;
		}

		else
		{
			if (current_yaw[entity->GetIndex()] - entity->GetEyeAnglesXY()->y >= 116.f || current_yaw[entity->GetIndex()] - entity->GetEyeAnglesXY()->y <= 116.f)
			{
				current_yaw[entity->GetIndex()] = entity->GetEyeAnglesXY()->y;
				old_yaw[entity->GetIndex()] = current_yaw[entity->GetIndex()];
			}
			else
			{
				current_yaw[entity->GetIndex()] = entity->GetEyeAnglesXY()->y;
			}
		}
	}

	if (total_missed[entity->GetIndex()] > 4)
	{
		resolver->enemy_fake = false;
		total_missed[entity->GetIndex()] = 0;
	}

	if (backtracking->IsTickValid(TIME_TO_TICKS(entity->GetSimulationTime())))
	{
		GlobalBREAK::userCMD->tick_count = TIME_TO_TICKS(entity->GetSimulationTime() + lerp_time());
	}

	if (entity->GetVelocity().Length2D() <= 1.f)
	{
		if (b1gdelta[entity->GetIndex()] >= -58.f && b1gdelta[entity->GetIndex()] <= 58.f) 
		{
			/* 58 being the max desync ammount. rn we are compensating
			for someone jittering between this range in a manner
			akin to backjitter or similar. */
			switch (Globals::missedshots % 4)
			{
			case 0 | 1:
			{
				resolver->enemy_fake = true;
				resolved_yaw[entity->GetIndex()] = entity->GetEyeAnglesXY()->y - b1gdelta[entity->GetIndex()];
			}
			break;

			case 2 | 3:
			{
				resolver->enemy_fake = false;
				resolved_yaw[entity->GetIndex()] = entity->GetEyeAnglesXY()->y + b1gdelta[entity->GetIndex()];
			}
			break;

			case 4:
			{
				resolver->enemy_fake = true;
				resolved_yaw[entity->GetIndex()] = entity->GetEyeAnglesXY()->y;
			}
			break;

			}
		}

		else
		{
			switch (Globals::missedshots % 3)
			{
				/* if they are using a variant of aimware's
				combo of "Switch" with a "Jitter" desync */
				case 0:
				{
					resolver->enemy_fake = false;
					resolved_yaw[entity->GetIndex()] = entity->GetEyeAnglesXY()->y - 58;
				}
				break;

				case 1:
				{
					resolver->enemy_fake = false;
					resolved_yaw[entity->GetIndex()] = entity->GetEyeAnglesXY()->y + 58;
				}
				break;

				case 2:
				{
					resolver->enemy_fake = true;
					resolved_yaw[entity->GetIndex()] = entity->GetEyeAnglesXY()->y - b1gdelta[entity->GetIndex()];
				}
				break;

				case 3:
				{
					resolver->enemy_fake = true;
					resolved_yaw[entity->GetIndex()] = entity->GetEyeAnglesXY()->y - 29;
				}
				break;

			}
		}
	}
	else // if they are moving
	{
		resolved_yaw[entity->GetIndex()] = entity->GetLowerBodyYaw(); 
	}
	resolver->resolved_yaw = resolved_yaw[entity->GetIndex()];
	entity->GetEyeAnglesXY()->y = resolved_yaw[entity->GetIndex()];

}

bool angle_correction::is_slow_walking(IClientEntity* entity) {
	float velocity_2D[64], old_velocity_2D[64];

	if (entity->GetVelocity().Length2D() != velocity_2D[entity->GetIndex()] && entity->GetVelocity().Length2D() != NULL) {
		old_velocity_2D[entity->GetIndex()] = velocity_2D[entity->GetIndex()];
		velocity_2D[entity->GetIndex()] = entity->GetVelocity().Length2D();
	}

	if (velocity_2D[entity->GetIndex()] > 0.1) {
		int tick_counter[64];

		if (velocity_2D[entity->GetIndex()] == old_velocity_2D[entity->GetIndex()])
			++tick_counter[entity->GetIndex()];
		else
			tick_counter[entity->GetIndex()] = 0;

		while (tick_counter[entity->GetIndex()] > (1 / Interfaces::Globals->interval_per_tick) * fabsf(0.1f))// should give use 100ms in ticks if their speed stays the same for that long they are definetely up to something..
			return true;

	}
	return false;
}


void resolver_setup::resolver_release(IClientEntity* pEntity, int CurrentTarget)
{

	bool can_freestand = Options::Menu.RageBotTab.resolver_dofreestanding.GetState();

	bool in_air[64] = { false };
	bool change[64] = { false };
	bool fakewalk[64] = { false };
	bool wasmoving[64] = { false };
	bool wasfakewalk[64] = { false };
	bool lowdelta[64] = { false };
	bool highdelta[64] = { false };
	bool didsync[64] = { false };
	bool spin[64] = { false };
	bool moving[64] = { false };
	bool hadslowlby[64] = { false };
	bool was_inair[64] = { false };
	bool prebreak[64] = { false };
	bool lowdelta_2[64] = { false };
	bool balance[64] = { false };
	bool nofake[64] = { false };

	float delta[64] = { 0.f };
	float oldlby[64] = { 0.f };
	float last_sim[64] = { 0.f };
	float movinglby[64] = { 0.f };
	float deltaDiff[64] = { 0.f };
	float move_delta[64] = { 0.f };
	float slowlby[64] = { 0.f };
	float fakewalk_change[64] = { 0.f };
	float last_forced_shot[64] = { 0.0f };
	float stand_lby[64] = { 0.0f };

	static float add_time[64];

	static bool updated_once[64];
	static bool lbybreak[64];
	static bool nextflick[64];
	static bool lbybacktrack[64];

	bool stage1 = false;		// stages needed cause we are iterating all layers, otherwise won't work :)
	bool stage2 = false;
	bool stage3 = false;

	if (!(hackManager.pLocal()->GetHealth() > 0))
		return;

	for (auto i = 0; i < Interfaces::Engine->GetMaxClients(); ++i)
	{
		const auto player = const_cast <IClientEntity*>(Interfaces::EntList->GetClientEntity(i));

		if (player == nullptr)
			continue;

		if (player == hackManager.pLocal())
			continue;

		if (player->team() == hackManager.pLocal()->team())
			continue;

		if (!player->IsAlive())
			continue;

		//		if (player->is_ghost())
		//			continue;

		if (player->IsDormant())
		{
			change[i] = false;
			continue;
		}

		auto update = false;

		const auto lby = player->GetLowerBodyYaw();
		const auto eye = player->GetEyeAnglesXY();
		const auto sim = player->GetSimulationTime();
		const auto speed = player->GetVelocity().Length2D();

		if (sim - last_sim[i] >= 1)
		{
			if (sim - last_sim[i] == 1)
			{
				nofake[i] = true;
			}
			last_sim[i] = sim;
		}

		int missed[65];
		int fired[65];
		fired[i] = global_count::shots_fired[i];

		missed[i] = global_count::shots_fired[i] - global_count::hits[i];
		while (missed[i] > 3) missed[i] -= 3;
		while (missed[i] < 0) missed[i] += 3;

		while (fired[i] > 3) fired[i] -= 3;
		while (fired[i] < 0) fired[i] += 2;


		if (oldlby[i] != lby && speed < 0.1f)
		{
			oldlby[i] = lby;
			update = true;
			change[i] = true;

			delta[i] = fabsf(lby - oldlby[i]);
		}
		if (wasmoving[i])
		{
			if (movinglby[i] - lby > 35.f || movinglby[i] - lby < -35.f)
			{
				move_delta[i] = movinglby[i] - lby;
			}
			else
			{
				move_delta[i] = FLT_MAX;
			}
		}
		if (speed > 0.1f && speed < 90.f)
		{
			enemy_fakewalk = false;
			hadslowlby[i] = true;
			moving[i] = true;
			slowlby[i] = lby;
		}
		// ------------------- Resolve ------------------- //

		float yaw[64] = { 0.f };

		if (in_air)
		{
			yaw[i] = lby;
		}
		else
		{
			if (speed > 1.f)
			{				
				yaw[i] = lby;			
			}
			else
			{
				if (!nofake[i])
				{
					if (predict_lby(player, oldlby, lby, speed) || update) // :elephant: 
					{
						yaw[i] = lby;
					}
					else
					{

						if (wasmoving[i])
						{
							if (move_delta[i] != FLT_MAX)
							{
								yaw[i] = lby - move_delta[i];
							}
							else
							{
								//		if (delta[i] >= 60.f || delta[i] <= 60.f)
								//		{
								//			yaw[i] = lby - delta[i];
								//		}
								//		else
								yaw[i] = movinglby[i];
							}
						}
						else
						{
							if (change[i])
							{
								if (can_freestand)
								{
									yaw[i] = apply_freestanding(player);
								}
								else
									yaw[i] = lby - delta[i];
							}
							else
							{

								if (can_freestand)
								{
									yaw[i] = (Options::Menu.RageBotTab.resolver_compensate_performance.GetState() ? apply_freestanding_low_performance(player, lby, oldlby[i]) : apply_freestanding(player));
								}
								else
								{
									yaw[i] = lby;
								}

							}
						}
					}
				}
			}
		}

		player->GetEyeAnglesXY()->y = yaw[i];
	}

}

void resolver_setup::FSN(IClientEntity* pEntity, ClientFrameStage_t stage)
{
	if (stage == ClientFrameStage_t::FRAME_NET_UPDATE_POSTDATAUPDATE_START)
	{
		for (int i = 1; i < Interfaces::Engine->GetMaxClients(); i++)
		{
			pEntity = (IClientEntity*)Interfaces::EntList->GetClientEntity(i);

			if (!pEntity || pEntity == hackManager.pLocal() || pEntity->is_dormant() || hackManager.pLocal()->IsAlive() || pEntity->team() == hackManager.pLocal()->team())
				continue;

			angle_correction ac;

			if (Options::Menu.RageBotTab.preso.GetIndex() > 0)
			{
				resolver_setup::preso(pEntity);
			}
			else
				resolver->resolved_pitch = pEntity->GetEyeAnglesXY()->x;

			pEntity->GetEyeAnglesXY()->z = 0.f;
		
			if (Options::Menu.RageBotTab.resolver.GetIndex() == 1) 
				resolver_setup::resolver_release(pEntity, stage);		
			// sorry, i couldn't bring myself to remove this. I loved it too much back when i was the only one using it.
		
			if (Options::Menu.RageBotTab.resolver.GetIndex() > 1)
				ac.ac_smart(pEntity);				
		}
	}
}


