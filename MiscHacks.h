#pragma once

#include "Hacks.h"

Vector GetAutostrafeView();

class CMiscHacks : public CHack
{
public:
	void Init();
	void Draw();
	void c_strafe(CUserCmd * m_pcmd);
	void Move(CUserCmd *pCmd, bool &bSendPacket);
	void FakeWalk0(CUserCmd * pCmd, bool & bSendPacket);

private:
	
	void AutoPistol(CUserCmd * pCmd);
	void strafe_2(CUserCmd * cmd);
	void PostProcces();
	void SlowMo(CUserCmd * pCmd, bool & bSendPacket);

	
	void AutoJump(CUserCmd *pCmd);
	void airduck(CUserCmd * pCmd);
	void RageStrafe(CUserCmd *pCmd);
	void zstrafe(CUserCmd * m_pcmd);


	int CircleFactor = 0;
	
}; extern CMiscHacks* g_Misc;



