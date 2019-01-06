#pragma once
#include "Interfaces.h"
struct FireBulletData
{
	FireBulletData(const Vector &eye_pos)
		: src(eye_pos)
	{
	}

	Vector           src;
	trace_t          enter_trace;
	Vector           direction;
	CTraceFilter    filter;
	float           trace_length;
	float           trace_length_remaining;
	float           current_damage;
	int             penetrate_count;
};

class autowall_2
{
public:

	void TraceLine(Vector & absStart, Vector & absEnd, unsigned int mask, IClientEntity * ignore, CGameTrace * ptr);

	void ClipTraceToPlayers(const Vector& absStart, const Vector absEnd, unsigned int mask, ITraceFilter* filter, CGameTrace* tr);

	void GetBulletTypeParameters(float & maxRange, float & maxDistance, char * bulletType, bool sv_penetration_type);

	bool BreakableEntity(IClientEntity * entity);

	void ScaleDamage(CGameTrace & enterTrace, CSWeaponInfo * weaponData, float & currentDamage);

	bool trace_to_exit(CGameTrace & enterTrace, CGameTrace & exitTrace, Vector startPosition, Vector direction);

	bool HandleBulletPenetration(CSWeaponInfo * weaponData, CGameTrace & enterTrace, Vector & eyePosition, Vector direction, int & possibleHitsRemaining, float & currentDamage, float penetrationPower, bool sv_penetration_type, float ff_damage_reduction_bullets, float ff_damage_bullet_penetration);

	bool FireBullet(C_BaseCombatWeapon * pWeapon, Vector & direction, float & currentDamage);

	float CanHit(Vector & point);

	bool PenetrateWall(IClientEntity * pBaseEntity, Vector & vecPoint);

	float trace_awall(float damage);
	bool handle_penetration;

	float damage_reduction;

};
extern autowall_2 * new_autowall;


class backup_autowall
{
public:

	

};
extern backup_autowall * backup_awall;

bool CanHit_2(const Vector &point, float *damage_given);
bool trace_autowallable_2(float& dmg);
bool SimulateFireBullet_2(IClientEntity *local, C_BaseCombatWeapon *weapon, FireBulletData &data);