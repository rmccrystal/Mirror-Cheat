#include "global_count.h"

void game_event::on_hurt(IGameEvent * Event)
{
	for (auto i = 0; i < Interfaces::Engine->GetMaxClients(); ++i)
	{
		const auto player = const_cast <IClientEntity*>(Interfaces::EntList->GetClientEntity(i));

		if (!Event)
			global_count::didhit[i] = false;

		if (!strcmp(Event->GetName(), "player_hurt"))
		{
			int deadfag = Event->GetInt("userid");
			int attackingfag = Event->GetInt("attacker");
			if (Interfaces::Engine->GetPlayerForUserID(attackingfag) == Interfaces::Engine->GetLocalPlayer())
			{
				global_count::hits[player->GetIndex()]++;
				global_count::shots_fired[player->GetIndex()]++;
			}
		}

		if (!strcmp(Event->GetName(), "round_prestart"))
		{
			global_count::hits[player->GetIndex()] = 0;
			global_count::shots_fired[player->GetIndex()] = 0;
			global_count::missed_shots[player->GetIndex()] = 0;
		}
	}
}
