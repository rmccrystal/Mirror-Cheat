#pragma once
#include "Hacks.h"
struct AntiaimData_t
{
	AntiaimData_t(const float& dist, const bool& inair, int player)
	{
		this->flDist = dist;
		this->bInAir = inair;
		this->iPlayer = player;
	}
	float flDist;
	bool bInAir;
	int	iPlayer;
};
class CRageBot : public CHack
{
public:

	void Init();
	void Draw();

	float NextPredictedLBYUpdate = 0.f;
	bool lbyupdate;
	int HitScan(IClientEntity* pEntity);
	void Fakelag(CUserCmd * pCmd, bool & bSendPacket);
	bool fakelag_conditions(CUserCmd * cmd);
	
	void fakelag_auto(CUserCmd * pCmd, bool & bSendPacket);
	int get_target_crosshair();
	void auto_revolver(CUserCmd * m_pcmd);
	void DoNoSpread(CUserCmd * pCmd);
	bool CanOpenFire();
	void DoAimbot(CUserCmd *pCmd, bool &bSendPacket);
	void DoNoRecoil(CUserCmd *pCmd);
	void Move(CUserCmd *pCmd, bool &bSendPacket);
	CUserCmd* cmd;
	bool shot_this_tick;
	Vector target_point;

private:
	std::vector<AntiaimData_t> Entities;
	int GetTargetCrosshair();
	void anti_lby(CUserCmd* cmd, bool& bSendPacket);
	int GetTargetDistance();

	bool TargetMeetsRequirements(IClientEntity* pEntity);
	void aimAtPlayer(CUserCmd * pCmd);

	bool AimAtPoint(IClientEntity * pLocal, Vector point, CUserCmd * pCmd, bool & bSendPacket);
	void freestanding_spin(CUserCmd * pCmd, bool & bSendPacket);
	void freestanding_jitter(CUserCmd * pCmd, bool & bSendPacket);
	float FovToPlayer(Vector ViewOffSet, Vector View, IClientEntity* pEntity, int HitBox);
	void Base_AntiAim(CUserCmd * pCmd, IClientEntity * pLocal);
	void Desync_AntiAim(CUserCmd * pCmd, IClientEntity * pLocal);
	void DoPitch(CUserCmd * pCmd);

	void DoPitch_2(CUserCmd * pCmd);

	void AtTargets(Vector & viewangles);

	void DoAntiAim(CUserCmd *pCmd, bool&bSendPacket);
	void desync_yaw(float yaw, CUserCmd * pCmd);
	
	bool IsAimStepping;
	Vector LastAimstepAngle;
	Vector LastAngle;
	IClientEntity * pTarget;
	bool IsLocked;
	int TargetID;
	int HitBox;
	Vector AimPoint;

};
extern CRageBot * ragebot;