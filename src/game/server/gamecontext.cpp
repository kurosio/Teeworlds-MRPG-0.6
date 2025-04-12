/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "gamecontext.h"

#include <engine/storage.h>
#include <engine/map.h>

#include "worldmodes/dungeon.h"
#include "worldmodes/default.h"
#include "worldmodes/tutorial.h"

#include "entity_manager.h"
#include "core/command_processor.h"
#include "core/tools/path_finder.h"
#include "core/entities/items/drop_items.h"

#include "core/components/accounts/account_manager.h"
#include "core/components/Bots/BotManager.h"
#include "core/components/quests/quest_manager.h"
#include "core/components/skills/skill_manager.h"

#include "core/components/achievements/achievement_listener.h"
#include "core/components/Inventory/inventory_listener.h"
#include "core/components/Eidolons/EidolonInfoData.h"
#include "core/components/worlds/world_data.h"
#include "core/tools/vote_optional.h"
#include "core/tools/vote_wrapper.h"

CGS::CGS()
{
	m_pEntityManager = new CEntityManager(this);
	m_pMmoController = new CMmoController(this);

	m_MultiplierExp = 0;
	m_pStorage = nullptr;
	m_pServer = nullptr;
	m_pController = nullptr;
	m_pCommandProcessor = nullptr;
	m_pPathFinder = nullptr;
	mem_zero(m_apPlayers, sizeof(m_apPlayers));
	mem_zero(m_aBroadcastStates, sizeof(m_aBroadcastStates));
}

CGS::~CGS()
{
	m_Events.Clear();
	for(auto& pPlayer : m_apPlayers)
	{
		delete pPlayer;
		pPlayer = nullptr;
	}

	delete m_pController;
	delete m_pMmoController;
	delete m_pCommandProcessor;
	delete m_pPathFinder;
	delete m_pEntityManager;
}

CCharacter* CGS::GetPlayerChar(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return nullptr;

	return m_apPlayers[ClientID]->GetCharacter();
}

CPlayer* CGS::GetPlayer(int ClientID, bool CheckAuth, bool CheckCharacter) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return nullptr;

	CPlayer* pPlayer = m_apPlayers[ClientID];
	if(!pPlayer || (CheckAuth && !pPlayer->IsAuthed()) || (CheckCharacter && !pPlayer->GetCharacter()))
		return nullptr;

	return pPlayer;
}

CPlayer* CGS::GetPlayerByUserID(int AccountID) const
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		int WorldID = Server()->GetClientWorldID(i);
		CGS* pGS = (CGS*)Instance::GameServer(WorldID);
		if(!pGS)
			continue;

		CPlayer* pPlayer = pGS->GetPlayer(i, true);
		if(pPlayer && pPlayer->Account()->GetID() == AccountID)
			return pPlayer;
	}
	return nullptr;
}

CItemDescription* CGS::GetItemInfo(ItemIdentifier ItemID) const
{
	dbg_assert(CItemDescription::Data().find(ItemID) != CItemDescription::Data().end(), "invalid referring to the CItemDescription");
	return &CItemDescription::Data()[ItemID];
}

CQuestDescription* CGS::GetQuestInfo(QuestIdentifier QuestID) const
{
	return CQuestDescription::Data().find(QuestID) != CQuestDescription::Data().end() ? CQuestDescription::Data()[QuestID] : nullptr;
}

CAttributeDescription* CGS::GetAttributeInfo(AttributeIdentifier ID) const
{
	dbg_assert(CAttributeDescription::Data().find(ID) != CAttributeDescription::Data().end(), "invalid referring to the CAttributeDescription");
	return CAttributeDescription::Data()[ID].get();
}

CQuestsBoard* CGS::GetQuestBoard(int ID) const
{
	dbg_assert(CQuestsBoard::Data().find(ID) != CQuestsBoard::Data().end(), "invalid referring to the CQuestsBoard");
	return CQuestsBoard::Data()[ID];
}

CWorldData* CGS::GetWorldData(int ID) const
{
	int WorldID = ID == -1 ? GetWorldID() : ID;
	const auto& p = std::find_if(CWorldData::Data().begin(), CWorldData::Data().end(), [WorldID](const WorldDataPtr& p){return WorldID == p->GetID(); });
	return p != CWorldData::Data().end() ? (*p).get() : nullptr;
}

CEidolonInfoData* CGS::GetEidolonByItemID(ItemIdentifier ItemID) const
{
	const auto& p = std::find_if(CEidolonInfoData::Data().begin(), CEidolonInfoData::Data().end(), [ItemID](CEidolonInfoData& p){ return p.GetItemID() == ItemID; });
	return p != CEidolonInfoData::Data().end() ? &(*p) : nullptr;
}

void CGS::CreateBirthdayEffect(vec2 Pos, int64_t Mask)
{
	CNetEvent_Birthday* pEvent = m_Events.Create<CNetEvent_Birthday>(Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGS::CreateFinishEffect(vec2 Pos, int64_t Mask)
{
	CNetEvent_Finish* pEvent = m_Events.Create<CNetEvent_Finish>(Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGS::CreateDamage(vec2 Pos, int FromCID, int Amount, bool CritDamage, float Angle, int64_t Mask)
{
	float a = 3 * pi / 2 + Angle;
	float s = a - pi / 3;
	float e = a + pi / 3;
	for(int i = 0; i < Amount; i++)
	{
		float f = mix(s, e, (i + 1) / (float)(Amount + 2));
		if(auto* pEvent = m_Events.Create<CNetEvent_DamageInd>(Mask))
		{
			pEvent->m_X = (int)Pos.x;
			pEvent->m_Y = (int)Pos.y;
			pEvent->m_Angle = (int)(f * 256.0f);
		}
	}

	if(CritDamage)
	{
		if(CPlayer* pPlayer = GetPlayer(FromCID, true, true); pPlayer && pPlayer->GetItem(itShowCriticalDamage)->IsEquipped())
			Chat(FromCID, ":: Crit damage: {}p.", Amount);
	}
}

void CGS::CreateHammerHit(vec2 Pos, int64_t Mask)
{
	if(auto* pEvent = m_Events.Create<CNetEvent_HammerHit>(Mask))
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGS::CreateRandomRadiusExplosion(int ExplosionCount, float Radius, vec2 Pos, int Owner, int Weapon, int MaxDamage)
{
	for(int i = 0; i < ExplosionCount; ++i)
	{
		const float theta = random_float(0.0f, 2.0f * pi);
		const float Distance = sqrt(random_float()) * Radius;
		const vec2 ExplosionPos = Pos + vec2(Distance * cos(theta), Distance * sin(theta));
		CreateExplosion(ExplosionPos, Owner, Weapon, MaxDamage);
	}
}

void CGS::CreateCyrcleExplosion(int ExplosionCount, float Radius, vec2 Pos, int Owner, int Weapon, int MaxDamage)
{
	const float AngleStep = 2.0f * pi / (float)ExplosionCount;

	for(int i = 0; i < ExplosionCount; ++i)
	{
		const float Angle = i * AngleStep;
		const vec2 ExplosionPos = Pos + vec2(Radius * cos(Angle), Radius * sin(Angle));
		CreateExplosion(ExplosionPos, Owner, Weapon, MaxDamage);
	}
}

void CGS::CreateExplosion(vec2 Pos, int Owner, int Weapon, int MaxDamage)
{
	// create the explosion event
	if(auto* pEvent = m_Events.Create<CNetEvent_Explosion>())
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	// define constants
	constexpr float Radius = 135.0f;
	constexpr float InnerRadius = 48.0f;

	// find entities within explosion radius
	CCharacter* apEnts[MAX_CLIENTS];
	const int Num = m_World.FindEntities(Pos, Radius, reinterpret_cast<CEntity**>(apEnts), MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	for(int i = 0; i < Num; ++i)
	{
		CCharacter* pChar = apEnts[i];
		vec2 Diff = pChar->GetPos() - Pos;
		const float Length = length(Diff);
		vec2 ForceDir = normalize(Diff);

		// calculate damage factor
		const float Factor = 1.0f - clamp((Length - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
		const int Damage = static_cast<int>(Factor * MaxDamage);
		float Strength = 0.0f;

		// calculate explosion strength
		if(Damage > 0)
		{
			Strength = 0.5f;
			if(Owner != -1 && m_apPlayers[Owner])
			{
				Strength = m_apPlayers[Owner]->m_NextTuningParams.m_ExplosionStrength;
			}
		}

		// Apply damage and force
		pChar->TakeDamage(ForceDir * (Strength * Length), Damage, Owner, Weapon);
	}
}

void CGS::CreatePlayerSpawn(vec2 Pos, int64_t Mask)
{
	if(auto* pEvent = m_Events.Create<CNetEvent_Spawn>(Mask))
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGS::CreateDeath(vec2 Pos, int ClientID, int64_t Mask)
{
	if(auto* pEvent = m_Events.Create<CNetEvent_Death>(Mask))
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientId = ClientID;
	}
}

void CGS::CreateSound(vec2 Pos, int Sound, int64_t Mask)
{
	if(Sound >= NUM_SOUNDS)
	{
		if(auto* pEvent = m_Events.Create<CNetEvent_MapSoundWorld>(Mask))
		{
			pEvent->m_X = (int)Pos.x;
			pEvent->m_Y = (int)Pos.y;
			pEvent->m_SoundId = Sound - NUM_SOUNDS;
		}
	}
	else if(Sound >= 0)
	{
		if(auto* pEvent = m_Events.Create<CNetEvent_SoundWorld>(Mask))
		{
			pEvent->m_X = (int)Pos.x;
			pEvent->m_Y = (int)Pos.y;
			pEvent->m_SoundId = maximum(0, Sound);
		}
	}
}

void CGS::CreatePlayerSound(int ClientID, int Sound)
{
	if(Sound >= NUM_SOUNDS)
	{
		CNetMsg_Sv_MapSoundGlobal Msg;
		Msg.m_SoundId = Sound - NUM_SOUNDS;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID, -1, m_WorldID);
	}
	else if(Sound >= 0)
	{
		if(auto* pEvent = m_Events.Create<CNetEvent_SoundWorld>(CmaskOne(ClientID)); pEvent && GetPlayer(ClientID))
		{
			pEvent->m_X = (int)m_apPlayers[ClientID]->m_ViewPos.x;
			pEvent->m_Y = (int)m_apPlayers[ClientID]->m_ViewPos.y;
			pEvent->m_SoundId = maximum(0, Sound);
		}
	}
}

void CGS::SnapLaser(int SnappingClient, int ID, const vec2& To, const vec2& From, int StartTick, int LaserType, int Subtype, int Owner, int Flags) const
{
	if(GetClientVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser* pObj = Server()->SnapNewItem<CNetObj_DDNetLaser>(ID);
		if(!pObj)
			return;

		pObj->m_ToX = (int)To.x;
		pObj->m_ToY = (int)To.y;
		pObj->m_FromX = (int)From.x;
		pObj->m_FromY = (int)From.y;
		pObj->m_StartTick = StartTick;
		pObj->m_Owner = Owner;
		pObj->m_Type = LaserType;
		pObj->m_Subtype = Subtype;
		pObj->m_SwitchNumber = 0;
		pObj->m_Flags = Flags;
	}
	else
	{
		CNetObj_Laser* pObj = Server()->SnapNewItem<CNetObj_Laser>(ID);
		if(!pObj)
			return;

		pObj->m_X = (int)To.x;
		pObj->m_Y = (int)To.y;
		pObj->m_FromX = (int)From.x;
		pObj->m_FromY = (int)From.y;
		pObj->m_StartTick = StartTick;
	}
}

void CGS::SnapPickup(int SnappingClient, int ID, const vec2& Pos, int Type, int SubType) const
{
	if(GetClientVersion(SnappingClient) >= VERSION_DDNET_ENTITY_NETOBJS)
	{
		CNetObj_DDNetPickup* pPickup = Server()->SnapNewItem<CNetObj_DDNetPickup>(ID);
		if(!pPickup)
			return;

		pPickup->m_X = (int)Pos.x;
		pPickup->m_Y = (int)Pos.y;
		pPickup->m_Type = Type;
		pPickup->m_Subtype = SubType;
		pPickup->m_SwitchNumber = 0;
	}
	else
	{
		CNetObj_Pickup* pPickup = Server()->SnapNewItem<CNetObj_Pickup>(ID);
		if(!pPickup)
			return;

		pPickup->m_X = (int)Pos.x;
		pPickup->m_Y = (int)Pos.y;
		pPickup->m_Type = Type;
		if(GetClientVersion(SnappingClient) < VERSION_DDNET_WEAPON_SHIELDS)
		{
			if(Type >= POWERUP_ARMOR_SHOTGUN && Type <= POWERUP_ARMOR_LASER)
				pPickup->m_Type = POWERUP_ARMOR;
		}
		pPickup->m_Subtype = SubType;
	}
}

void CGS::SnapProjectile(int SnappingClient, int ID, const vec2& Pos, const vec2& Vel, int StartTick, int Type, int Owner, int Flags) const
{
	const int SnappingClientVersion = GetClientVersion(SnappingClient);

	if(SnappingClientVersion >= VERSION_DDNET_ENTITY_NETOBJS)
	{
		CNetObj_DDNetProjectile* pProj = Server()->SnapNewItem<CNetObj_DDNetProjectile>(ID);
		if(!pProj)
			return;

		pProj->m_X = round_to_int(Pos.x * 100.0f);
		pProj->m_Y = round_to_int(Pos.y * 100.0f);
		pProj->m_VelX = round_to_int(Vel.x);
		pProj->m_VelY = round_to_int(Vel.y);
		pProj->m_Type = Type;
		pProj->m_StartTick = StartTick;
		pProj->m_Owner = Owner;
		pProj->m_Flags = Flags;
		pProj->m_SwitchNumber = 0;
		pProj->m_TuneZone = 0;
	}
	else if(SnappingClientVersion >= VERSION_DDNET_MSG_LEGACY)
	{
		CNetObj_DDRaceProjectile* pProj = Server()->SnapNewItem<CNetObj_DDRaceProjectile>(ID);
		if(!pProj)
			return;

		float Angle = -std::atan2(Vel.x, Vel.y);
		pProj->m_X = (int)(Pos.x * 100.0f);
		pProj->m_Y = (int)(Pos.y * 100.0f);
		pProj->m_Angle = (int)(Angle * 1000000.0f);
		pProj->m_Data = Flags;
		pProj->m_StartTick = StartTick;
		pProj->m_Type = Type;
	}
	else
	{
		CNetObj_Projectile* pProj = Server()->SnapNewItem<CNetObj_Projectile>(ID);
		if(!pProj)
			return;

		pProj->m_X = (int)Pos.x;
		pProj->m_Y = (int)Pos.y;
		pProj->m_VelX = (int)(Vel.x * 100.0f);
		pProj->m_VelY = (int)(Vel.y * 100.0f);
		pProj->m_StartTick = StartTick;
		pProj->m_Type = Type;
	}
}

void CGS::SendChatTarget(int ClientID, const char* pText) const
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_pMessage = pText;
	Msg.m_ClientId = -1;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

/* #########################################################################
	CHAT FUNCTIONS
######################################################################### */
void CGS::SendChat(int ChatterClientID, int Mode, const char* pText, int64_t Mask)
{
	if(ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
		Console()->PrintF(IConsole::OUTPUT_LEVEL_ADDINFO, Mode == CHAT_TEAM ? "teamchat" : "chat",
			"%d:%d:%s: %s", ChatterClientID, Mode, Server()->ClientName(ChatterClientID), pText);
	else
		Console()->PrintF(IConsole::OUTPUT_LEVEL_ADDINFO, Mode == CHAT_TEAM ? "teamchat" : "chat",
			"*** %s", pText);

	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_ClientId = ChatterClientID;
	Msg.m_pMessage = pText;

	if(Mode == CHAT_ALL)
	{
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1, Mask);
	}
	else if(Mode == CHAT_TEAM)
	{
		CPlayer* pChatterPlayer = GetPlayer(ChatterClientID, true);
		if(!pChatterPlayer || !pChatterPlayer->Account()->HasGuild())
		{
			Chat(ChatterClientID, "This chat is for guilds and team members.");
			return;
		}

		// send chat to guild team
		Msg.m_Team = 1;
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			CPlayer* pSearchPlayer = GetPlayer(i, true);
			if(pSearchPlayer && pChatterPlayer->Account()->IsClientSameGuild(i))
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i, Mask);
		}

		// pack one for the recording only
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1, Mask);
	}
}

void CGS::SendChatRadius(int ChatterClientID, float Radius, const char* pText)
{
	int64_t MaskRadius = 0;

	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = GetPlayer(i);
		if(!m_apPlayers[i])
			continue;

		if(distance(pPlayer->m_ViewPos, m_apPlayers[ChatterClientID]->m_ViewPos) < Radius)
			MaskRadius |= CmaskOne(i);
	}
	SendChat(ChatterClientID, CHAT_ALL, pText, MaskRadius);
}

/* #########################################################################
	BROADCAST FUNCTIONS
######################################################################### */
void CGS::AddBroadcast(int ClientID, const char* pText, BroadcastPriority Priority, int LifeSpan)
{
	if(ClientID < 0 || ClientID >= MAX_PLAYERS)
		return;

	if(LifeSpan > 0)
	{
		if(m_aBroadcastStates[ClientID].m_TimedPriority > Priority)
			return;

		str_copy(m_aBroadcastStates[ClientID].m_TimedMessage, pText, sizeof(m_aBroadcastStates[ClientID].m_TimedMessage));
		m_aBroadcastStates[ClientID].m_LifeSpanTick = LifeSpan;
		m_aBroadcastStates[ClientID].m_TimedPriority = Priority;
	}
	else
	{
		if(m_aBroadcastStates[ClientID].m_NextPriority > Priority)
			return;

		str_copy(m_aBroadcastStates[ClientID].m_NextMessage, pText, sizeof(m_aBroadcastStates[ClientID].m_NextMessage));
		m_aBroadcastStates[ClientID].m_NextPriority = Priority;
	}
}

// the tick of the broadcast and his life
void CGS::BroadcastTick(int ClientID)
{
	// Check if the ClientID is valid
	if(ClientID < 0 || ClientID >= MAX_PLAYERS)
		return;

	// Check if the player exists and is in the same world
	if(m_apPlayers[ClientID] && IsPlayerInWorld(ClientID))
	{
		// Get the broadcast state for the given client ID
		CBroadcastState& Broadcast = m_aBroadcastStates[ClientID];

		// Check if the broadcast has a lifespan and the timed priority is greater than the next priority
		if(Broadcast.m_LifeSpanTick > 0 && Broadcast.m_TimedPriority > Broadcast.m_NextPriority)
		{
			// Copy the timed message to the next message buffer
			str_copy(Broadcast.m_NextMessage, Broadcast.m_TimedMessage, sizeof(Broadcast.m_NextMessage));
		}

		//Send broadcast only if the message is different, or to fight auto-fading
		if(Broadcast.m_Updated || str_comp(Broadcast.m_PrevMessage, Broadcast.m_NextMessage) != 0 || Broadcast.m_NoChangeTick < Server()->Tick())
		{
			// Check if the timed priority of the broadcast is less than MainInformation
			if(Broadcast.m_TimedPriority < BroadcastPriority::MainInformation)
			{
				// Format the broadcast message with basic player stats and append pAppend
				const char* pAppend = m_apPlayers[ClientID]->m_PlayerFlags & PLAYERFLAG_CHATTING ? "\0" : Broadcast.m_NextMessage;
				m_apPlayers[ClientID]->FormatBroadcastBasicStats(Broadcast.m_aCompleteMsg, sizeof(Broadcast.m_aCompleteMsg), pAppend);
			}
			else
			{
				// Copy aBufAppend into the complete message buffer
				str_copy(Broadcast.m_aCompleteMsg, Broadcast.m_NextMessage, sizeof(Broadcast.m_aCompleteMsg));
			}

			// Create a broadcast net message and send
			CNetMsg_Sv_Broadcast Msg;
			Msg.m_pMessage = Broadcast.m_aCompleteMsg;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

			// Reset the complete message buffer and no change tick
			str_copy(Broadcast.m_PrevMessage, Broadcast.m_NextMessage, sizeof(Broadcast.m_PrevMessage));
			Broadcast.m_aCompleteMsg[0] = '\0';
			Broadcast.m_Updated = false;
			Broadcast.m_NoChangeTick = Server()->Tick() + (Server()->TickSpeed() * 3);
		}

		// Update broadcast state
		if(Broadcast.m_LifeSpanTick > 0)
		{
			// Check if the lifespan tick is greater than 0
			Broadcast.m_LifeSpanTick--;
		}

		if(Broadcast.m_LifeSpanTick <= 0)
		{
			// Check if the lifespan tick is less than or equal to 0
			Broadcast.m_TimedMessage[0] = 0;
			Broadcast.m_TimedPriority = BroadcastPriority::Lower;
		}

		Broadcast.m_NextMessage[0] = 0;
		Broadcast.m_NextPriority = BroadcastPriority::Lower;
	}
	else
	{
		// Full reset
		m_aBroadcastStates[ClientID].m_LifeSpanTick = 0;
		m_aBroadcastStates[ClientID].m_NextPriority = BroadcastPriority::Lower;
		m_aBroadcastStates[ClientID].m_TimedPriority = BroadcastPriority::Lower;
		m_aBroadcastStates[ClientID].m_PrevMessage[0] = 0;
		m_aBroadcastStates[ClientID].m_NextMessage[0] = 0;
		m_aBroadcastStates[ClientID].m_TimedMessage[0] = 0;
		m_aBroadcastStates[ClientID].m_Updated = false;
	}
}

void CGS::MarkUpdatedBroadcast(int ClientID)
{
	if(ClientID >= 0 && ClientID < MAX_PLAYERS)
		m_aBroadcastStates[ClientID].m_Updated = true;
}

/* #########################################################################
	PACKET MESSAGE FUNCTIONS
######################################################################### */
void CGS::SendEmoticon(int ClientID, int Emoticon)
{
	CNetMsg_Sv_Emoticon Msg;
	Msg.m_ClientId = ClientID;
	Msg.m_Emoticon = Emoticon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1, m_WorldID);
}

void CGS::SendWeaponPickup(int ClientID, int Weapon)
{
	CNetMsg_Sv_WeaponPickup Msg;
	Msg.m_Weapon = Weapon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGS::SendTuningParams(int ClientID)
{
	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int* pParams = (int*)&m_Tuning;
	for(unsigned i = 0; i < sizeof(m_Tuning) / sizeof(int); i++)
		Msg.AddInt(pParams[i]);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

/* #########################################################################
	ENGINE GAMECONTEXT
######################################################################### */
void CGS::HandleNicknameChange(CPlayer* pPlayer, const char* pNewNickname) const
{
	if(!pPlayer)
		return;

	const int ClientID = pPlayer->GetCID();
	if(!pPlayer->IsAuthed())
	{
		Server()->SetClientName(ClientID, pNewNickname);
		return;
	}

	if(str_comp(Server()->ClientName(ClientID), pNewNickname) != 0 && !(pPlayer->m_PlayerFlags & PLAYERFLAG_IN_MENU))
	{
		std::string NewName = pNewNickname;
		auto fnCallback = [NewName](CPlayer* pPlayer, bool Accepted)
		{
			CGS* pGS = pPlayer->GS();
			const int ClientID = pPlayer->GetCID();
			if(Accepted)
			{
				if(pGS->Core()->AccountManager()->ChangeNickname(NewName, ClientID))
					pGS->Chat(ClientID, "Your nickname has been successfully updated.");
				else
					pGS->Chat(ClientID, "This nickname is already in use.");
			}
			else
				pGS->Chat(ClientID, "You need to set a '{}' nickname. This window will keep appearing until you confirm the nickname change.", Instance::Server()->ClientName(ClientID));
		};
		auto closeCondition = [NewName](CPlayer* pPlayer)
		{
			return str_comp(NewName.c_str(), pPlayer->GS()->Server()->ClientName(pPlayer->GetCID())) == 0;
		};
		const auto pOption = CVoteOptional::Create(ClientID, 10, "Nick {}?", NewName.c_str());
		pOption->RegisterCallback(fnCallback);
		pOption->RegisterCloseCondition(closeCondition);
	}
}

void CGS::OnInit(int WorldID)
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorageEngine>();

	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);
	m_WorldID = WorldID;

	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	// initialize listeners
	g_AchievementListener.Initialize();
	g_InventoryListener.Initialize();

	// initialize controller
	m_Collision.Init(Kernel(), WorldID);
	m_pMmoController->OnInit(m_pServer, m_pConsole, m_pStorage);
	InitWorld();

	// initialize entities
	m_Collision.InitEntities([this](int index, vec2 position, int flags)
	{
		m_pController->OnEntity(index, position, flags);
	});

	m_Collision.InitSwitchEntities([this](int type, vec2 position, int flags, int number)
	{
		m_pController->OnEntitySwitch(type, position, flags, number);
	});
	m_pController->CanSpawn(SPAWN_HUMAN_PRISON, &m_JailPosition);

	// initialize
	m_pCommandProcessor = new CCommandProcessor(this);
	m_pPathFinder = new CPathFinder(&m_Collision);
}

void CGS::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	m_pMmoController->OnConsoleInit(m_pConsole);
}

void CGS::OnDaytypeChange(int NewDaytype)
{
	// send day info
	const char* pWorldname = Server()->GetWorldName(m_WorldID);
	switch(NewDaytype)
	{
		case NIGHT_TYPE:
			UpdateExpMultiplier();
			ChatWorld(m_WorldID, "", "Nighttime adventure in the '{}' zone has been boosted by {}%!", pWorldname, m_MultiplierExp);
			break;
		case MORNING_TYPE:
			ChatWorld(m_WorldID, "", "The exp multiplier in the '{}' zone is 100%.", pWorldname);
			ResetExpMultiplier();
			break;
		default: break;
	}
}

void CGS::OnTick()
{
	m_World.m_Core.m_Tuning = m_Tuning;
	m_World.Tick();
	m_pController->Tick();

	// player updates on tick
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!Server()->ClientIngame(i) || !m_apPlayers[i] || m_apPlayers[i]->GetCurrentWorldID() != m_WorldID)
			continue;

		if(m_apPlayers[i]->IsMarkedForDestroy())
		{
			DestroyPlayer(i);
			continue;
		}

		m_apPlayers[i]->Tick();
		m_apPlayers[i]->PostTick();
		if(i < MAX_PLAYERS)
		{
			BroadcastTick(i);
		}
	}

	Core()->OnTick();
	UpdateCollisionZones();
}

void CGS::OnTickGlobal()
{
	// update player time period
	if(Server()->Tick() % (Server()->TickSpeed() * g_Config.m_SvPlayerPeriodCheckInterval) == 0)
	{
		for(int i = 0; i < MAX_PLAYERS; ++i)
		{
			if(CPlayer* pPlayer = GetPlayer(i, true))
				Core()->OnHandlePlayerTimePeriod(pPlayer);
		}
	}

	// send chat messages with interval
	if(Server()->Tick() % (Server()->TickSpeed() * g_Config.m_SvChatMessageInterval) == 0)
	{
		static const std::deque<std::string> vMessages = {
			"[INFO] We recommend that you use the function in F1 console \"ui_close_window_after_changing_setting 1\", this will allow the voting menu not to close after clicking to vote.",
			"[INFO] If you can't see the dialogs with NPCs, check in F1 console \"cl_motd_time\" so that the value is set.",
			"[INFO] Information and data can be found in the call voting menu.",
			"[INFO] The mod supports translation, you can find it in \"Call vote -> Settings -> Settings language\".",
			"[INFO] Don't know what to do? For example, try to find ways to improve your attributes, of which there are more than 25."
		};

		Chat(-1, vMessages[rand() % vMessages.size()].c_str());
	}

	// send top messages with interval
	if(Server()->Tick() % (Server()->TickSpeed() * g_Config.m_SvChatTopMessageInterval) == 0)
	{
		const auto RandomType = (ToplistType)(rand() % (int)ToplistType::NUM_TOPLIST_TYPES);
		auto vResult = Core()->GetTopList(RandomType, 2);
		if(vResult.size() < 2)
			return;

		if(RandomType == ToplistType::PlayerExpert)
		{
			Chat(-1, "-- {}", "Top Specialists in the Realm");
			for(auto& [Iter, Top] : vResult)
			{
				auto* pAttribute = GetAttributeInfo((AttributeIdentifier)Top.Data["ID"].to_int());
				auto* pNickname = Server()->GetAccountNickname(Top.Data["AccountID"].to_int());
				auto Value = Top.Data["Value"].to_int();
				Chat(-1, "{}: '{-} - {}({})'.", Top.Name, pNickname, Value, pAttribute->GetName());
			}

			return;
		}

		auto& Leader = vResult[1];
		auto& Second = vResult[2];

		if(RandomType == ToplistType::PlayerRating)
		{
			Chat(-1, "Rating leader: '{} ({})'! In second place is '{} ({})' — the competition is heating up!",
				Leader.Name, Leader.Data["Rating"].to_int(), Second.Name, Second.Data["Rating"].to_int());
		}
		else if(RandomType == ToplistType::PlayerWealthy)
		{
			Chat(-1, "Wealthiest player: '{}({$})' is richest! '{}({$})' close behind!",
				Leader.Name, Leader.Data["Bank"], Second.Name, Second.Data["Bank"]);
		}
		else if(RandomType == ToplistType::GuildLeveling)
		{
			Chat(-1, "The most experienced guild: '{} (Level {})'! Close behind: '{} (Level {})'.",
				Leader.Name, Leader.Data["Level"], Second.Name, Second.Data["Level"]);
		}
		else if(RandomType == ToplistType::GuildWealthy)
		{
			Chat(-1, "The richest guild: '{}({$})'! Second place: '{}({$})' is catching up!",
				Leader.Name, Leader.Data["Bank"], Second.Name, Second.Data["Bank"]);
		}
	}
}

void CGS::OnSnap(int ClientID)
{
	// check valid player
	CPlayer* pPlayer = m_apPlayers[ClientID];
	if(!pPlayer || pPlayer->GetCurrentWorldID() != GetWorldID())
		return;

	// snap all objects
	m_pController->Snap();
	for(const auto& pIterPlayer : m_apPlayers)
	{
		if(pIterPlayer)
			pIterPlayer->Snap(ClientID);
	}
	m_World.Snap(ClientID);
	m_Events.Snap(ClientID);
}

void CGS::OnPostSnap()
{
	m_World.PostSnap();
	m_Events.Clear();
}

void CGS::OnMessage(int MsgID, CUnpacker* pUnpacker, int ClientID)
{
	// If the unpacking failed, print a debug message and return
	void* pRawMsg = m_NetObjHandler.SecureUnpackMsg(MsgID, pUnpacker);
	if(!pRawMsg)
	{
		if(g_Config.m_Debug)
		{
			Console()->PrintF(IConsole::OUTPUT_LEVEL_DEBUG, "server",
				"dropped weird message '%s' (%d), failed on '%s'", m_NetObjHandler.GetMsgName(MsgID), MsgID, m_NetObjHandler.FailedMsgOn());
		}

		return;
	}

	// If the player object does not exist (i.e., it is a null pointer), return without doing anything
	CPlayer* pPlayer = m_apPlayers[ClientID];
	if(!pPlayer)
		return;

	if(Server()->ClientIngame(ClientID))
	{
		if(MsgID == NETMSGTYPE_CL_SAY)
		{
			if(pPlayer->m_aPlayerTick[LastChat] > Server()->Tick())
				return;

			// initialize variables
			const auto pMsg = (CNetMsg_Cl_Say*)pRawMsg;
			pPlayer->m_aPlayerTick[LastChat] = Server()->Tick() + Server()->TickSpeed();
			if(!str_utf8_check(pMsg->m_pMessage))
				return;

			// is apply field edit
			if(pPlayer->m_pMotdMenu && pPlayer->m_pMotdMenu->ApplyFieldEdit(pMsg->m_pMessage))
				return;

			// check message
			const auto firstChar = pMsg->m_pMessage[0];
			if(firstChar == '/')
				CommandProcessor()->ProcessClientChatCommand(ClientID, pMsg->m_pMessage);
			else if(firstChar == '#')
				ChatWorld(pPlayer->GetCurrentWorldID(), "Nearby:", "'{}' performed an act '{}'.", Server()->ClientName(ClientID), pMsg->m_pMessage);
			else
				SendChat(ClientID, pMsg->m_Team ? CHAT_TEAM : CHAT_ALL, pMsg->m_pMessage);

			// set last message
			str_copy(pPlayer->m_aLastMsg, pMsg->m_pMessage, sizeof(pPlayer->m_aLastMsg));
			return;
		}

		if(MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			if(pPlayer->m_aPlayerTick[LastVote] > Server()->Tick())
				return;

			// initialize variables
			const auto pMsg = (CNetMsg_Cl_CallVote*)pRawMsg;
			pPlayer->m_aPlayerTick[LastVote] = Server()->Tick() + (Server()->TickSpeed() / 2);

			// check is option type
			if(str_comp_nocase(pMsg->m_pType, "option") == 0)
			{
				pPlayer->m_VotesData.ApplyVoteUpdaterData();
				if(CVoteOption* pActionVote = VoteWrapper::GetOptionVoteByAction(ClientID, pMsg->m_pValue))
				{
					const int ReasonNumber = clamp(str_toint(pMsg->m_pReason), 1, 100000);
					if(pActionVote->m_Callback.m_Impl)
						pActionVote->m_Callback.m_Impl(pPlayer, ReasonNumber, pMsg->m_pReason, pActionVote->m_Callback.m_pData);
					else
						OnClientVoteCommand(ClientID, pActionVote->m_aCommand, pActionVote->m_Extra1, pActionVote->m_Extra2, ReasonNumber, pMsg->m_pReason);
				}
			}
			return;
		}

		if(MsgID == NETMSGTYPE_CL_VOTE)
		{
			// update event key
			const auto pMsg = (CNetMsg_Cl_Vote*)pRawMsg;
			if(pMsg->m_Vote == 1)
				Server()->Input()->AppendEventKeyClick(ClientID, KEY_EVENT_VOTE_YES);
			else if(pMsg->m_Vote == 0)
				Server()->Input()->AppendEventKeyClick(ClientID, KEY_EVENT_VOTE_NO);

			// parse vote option result
			pPlayer->ParseVoteOptionResult(pMsg->m_Vote);
			return;
		}

		if(MsgID == NETMSGTYPE_CL_SETTEAM)
		{
			if(pPlayer->m_aPlayerTick[LastChangeTeam] > Server()->Tick())
				return;

			pPlayer->m_aPlayerTick[LastChangeTeam] = Server()->Tick() + Server()->TickSpeed();
			const char* pMsgText = !pPlayer->IsAuthed() ? "Use /register <name> <pass>\nOr /login <name> <pass>." : "Team change is not allowed.";
			Broadcast(pPlayer->GetCID(), BroadcastPriority::MainInformation, 100, pMsgText);
			return;
		}

		if(MsgID == NETMSGTYPE_CL_SETSPECTATORMODE)
		{
			const auto pMsg = (CNetMsg_Cl_SetSpectatorMode*)pRawMsg;
			int SpectatorID = clamp(pMsg->m_SpectatorId, (int)SPEC_FOLLOW, MAX_CLIENTS - 1);
			if(SpectatorID >= 0 && !Server()->ReverseTranslate(SpectatorID, ClientID))
				return;

			auto& LastSetSpecSpectatorMode = pPlayer->m_aPlayerTick[LastSetSpectatorMode];
			if(LastSetSpecSpectatorMode && LastSetSpecSpectatorMode + Server()->TickSpeed() / 4 > Server()->Tick())
				return;

			LastSetSpecSpectatorMode = Server()->Tick();
			if(SpectatorID >= 0 && (!m_apPlayers[SpectatorID] || m_apPlayers[SpectatorID]->GetTeam() == TEAM_SPECTATORS))
			{
				Chat(ClientID, "Invalid spectator id used");
				return;
			}

			pPlayer->m_SpectatorID = SpectatorID;
			return;
		}

		if(MsgID == NETMSGTYPE_CL_CHANGEINFO)
		{
			if(pPlayer->m_aPlayerTick[LastChangeInfo] > Server()->Tick())
				return;

			auto pMsg = (CNetMsg_Cl_ChangeInfo*)pRawMsg;
			pPlayer->m_aPlayerTick[LastChangeInfo] = Server()->Tick() + (Server()->TickSpeed() * g_Config.m_SvInfoChangeDelay);
			if(!str_utf8_check(pMsg->m_pClan) || !str_utf8_check(pMsg->m_pSkin))
				return;

			HandleNicknameChange(pPlayer, pMsg->m_pName);
			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);

			// set player info
			str_copy(pPlayer->Account()->m_TeeInfos.m_aSkinName, pMsg->m_pSkin, sizeof(pPlayer->Account()->m_TeeInfos.m_aSkinName));
			pPlayer->Account()->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
			pPlayer->Account()->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
			pPlayer->Account()->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;

			// expire server info
			Server()->ExpireServerInfo();
			return;
		}

		if(MsgID == NETMSGTYPE_CL_EMOTICON)
		{
			if(pPlayer->m_aPlayerTick[LastEmote] > Server()->Tick())
				return;

			// initialize variables
			const auto pMsg = (CNetMsg_Cl_Emoticon*)pRawMsg;
			pPlayer->m_aPlayerTick[LastEmote] = Server()->Tick() + (Server()->TickSpeed() / 2);

			// send emoticon and use skills by emoticon
			SendEmoticon(ClientID, pMsg->m_Emoticon);
			Core()->SkillManager()->UseSkillsByEmoticion(pPlayer, pMsg->m_Emoticon);
			return;
		}

		if(MsgID == NETMSGTYPE_CL_KILL)
		{
			if(pPlayer->m_aPlayerTick[LastSelfKill] > Server()->Tick())
				return;

			pPlayer->m_aPlayerTick[LastSelfKill] = Server()->Tick() + (Server()->TickSpeed() / 2);

			// close motd menu by vote optional
			if(pPlayer->m_pMotdMenu)
			{
				auto fnCallback = [](CPlayer* pPlayer, bool Accepted)
				{
					if(Accepted)
						pPlayer->CloseMotdMenu();
				};
				auto closeCondition = [](CPlayer* pPlayer)
				{
					return !pPlayer->m_pMotdMenu;
				};

				const auto pOption = CVoteOptional::Create(ClientID, 10, "Close motd menu?");
				pOption->RegisterCallback(fnCallback);
				pOption->RegisterCloseCondition(closeCondition);
				return;
			}

			// event key self kill
			Server()->Input()->AppendEventKeyClick(ClientID, KEY_EVENT_SELF_KILL);
			return;
		}

		if(MsgID == NETMSGTYPE_CL_ISDDNETLEGACY)
		{
			// Get client information from the server
			IServer::CClientInfo Info;
			Server()->GetClientInfo(ClientID, &Info);

			// Check if client has already provided DDNet version
			if(Info.m_GotDDNetVersion)
				return;

			// Get DDNet version from the unpacker
			int DDNetVersion = pUnpacker->GetInt();
			if(pUnpacker->Error() || DDNetVersion < 0)
				DDNetVersion = VERSION_DDRACE;

			// Set the DDNet version for the client on the server side
			Server()->SetClientDDNetVersion(ClientID, DDNetVersion);
			return;
		}

		if(MsgID == NETMSGTYPE_CL_SHOWOTHERSLEGACY)
		{
			dbg_msg("msg", "msg show others legacy cid '%d'", ClientID);
			return;
		}

		if(MsgID == NETMSGTYPE_CL_SHOWOTHERS)
		{
			dbg_msg("msg", "msg show others cid '%d'", ClientID);
			return;
		}

		if(MsgID == NETMSGTYPE_CL_SHOWDISTANCE)
		{
			dbg_msg("msg", "msg show distance cid '%d'", ClientID);
			return;
		}

		// custom
		if(MsgID == NETMSGTYPE_CL_ISMRPGSERVER)
		{
			// check protocol version
			if(const auto pMsg = (CNetMsg_Cl_IsMRPGServer*)pRawMsg; pMsg->m_Version != MRPG_PROTOCOL_VERSION)
			{
				Server()->Kick(ClientID, "Update client use updater or download in discord.");
				return;
			}

			// update protocol version and send good check
			CNetMsg_Sv_AfterIsMRPGServer GoodCheck;
			Server()->SetStateClientMRPG(ClientID, true);
			Server()->SendPackMsg(&GoodCheck, MSGFLAG_VITAL | MSGFLAG_FLUSH | MSGFLAG_NORECORD, ClientID);
		}
	}
	else
	{
		if(MsgID == NETMSGTYPE_CL_STARTINFO)
		{
			if(pPlayer->m_aPlayerTick[LastChangeInfo] != 0)
				return;

			pPlayer->m_aPlayerTick[LastChangeInfo] = Server()->Tick();

			// is authed
			if(pPlayer->IsAuthed())
			{
				CNetMsg_Sv_ReadyToEnter m;
				Server()->SendPackMsg(&m, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);
				pPlayer->m_VotesData.ClearVotes();
				return;
			}

			// set client info
			const auto pMsg = (CNetMsg_Cl_StartInfo*)pRawMsg;
			if(!str_utf8_check(pMsg->m_pName))
			{
				Server()->Kick(ClientID, "name is not valid utf8");
				return;
			}
			if(!str_utf8_check(pMsg->m_pClan))
			{
				Server()->Kick(ClientID, "clan is not valid utf8");
				return;
			}
			if(!str_utf8_check(pMsg->m_pSkin))
			{
				Server()->Kick(ClientID, "skin is not valid utf8");
				return;
			}
			Server()->SetClientName(ClientID, pMsg->m_pName);
			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);

			// set player info
			str_copy(pPlayer->Account()->m_TeeInfos.m_aSkinName, pMsg->m_pSkin, sizeof(pPlayer->Account()->m_TeeInfos.m_aSkinName));
			pPlayer->Account()->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
			pPlayer->Account()->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
			pPlayer->Account()->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;

			// send ready to enter
			CNetMsg_Sv_ReadyToEnter m;
			Server()->SendPackMsg(&m, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);
			pPlayer->m_VotesData.ClearVotes();

			// expire server info
			Server()->ExpireServerInfo();
		}
	}
}

void CGS::OnClientConnected(int ClientID)
{
	if(!m_apPlayers[ClientID])
	{
		const int AllocMemoryCell = ClientID + m_WorldID * MAX_CLIENTS;
		m_apPlayers[ClientID] = new(AllocMemoryCell) CPlayer(this, ClientID);
	}

	Server()->SendMotd(ClientID, g_Config.m_SvMotd);
	m_aBroadcastStates[ClientID] = {};
}

void CGS::OnClientEnter(int ClientID)
{
	CPlayer* pPlayer = m_apPlayers[ClientID];
	if(!pPlayer || pPlayer->IsBot())
		return;

	m_pController->OnPlayerConnect(pPlayer);
	m_pCommandProcessor->SendClientCommandsInfo(this, ClientID);

	if(!pPlayer->IsAuthed())
	{
		Chat(-1, "'{}' entered and joined the {}", Server()->ClientName(ClientID), g_Config.m_SvGamemodeName);
		CMmoController::AsyncClientEnterMsgInfo(Server()->ClientName(ClientID), ClientID);
		return;
	}

	Chat(ClientID, "Welcome to '{}'! Zone multiplier exp is at '{}%'.", Server()->GetWorldName(m_WorldID), m_MultiplierExp);
	Core()->AccountManager()->LoadAccount(pPlayer, false);
	Core()->SaveAccount(m_apPlayers[ClientID], SAVE_POSITION);
}

void CGS::OnClientDrop(int ClientID, const char* pReason)
{
	if(!m_apPlayers[ClientID] || m_apPlayers[ClientID]->IsBot())
		return;

	// update clients on drop
	m_pController->OnPlayerDisconnect(m_apPlayers[ClientID]);

	if((Server()->ClientIngame(ClientID) || Server()->IsClientChangingWorld(ClientID)) && IsPlayerInWorld(ClientID))
	{
		Chat(-1, "'{}' has left the {}", Server()->ClientName(ClientID), g_Config.m_SvGamemodeName);
		Console()->PrintF(IConsole::OUTPUT_LEVEL_STANDARD, "game", "leave player='%d:%s'", ClientID, Server()->ClientName(ClientID));
		Core()->SaveAccount(m_apPlayers[ClientID], SAVE_POSITION);
	}

	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = nullptr;
}

void CGS::OnClientDirectInput(int ClientID, void* pInput)
{
	m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput*)pInput);

	int Flags = ((CNetObj_PlayerInput*)pInput)->m_PlayerFlags;
	if((Flags & 256) || (Flags & 512))
	{
		Server()->Kick(ClientID, "please update your client or use DDNet client");
	}
}

void CGS::OnClientPredictedInput(int ClientID, void* pInput)
{
	m_apPlayers[ClientID]->OnPredictedInput((CNetObj_PlayerInput*)pInput);
}

void CGS::OnUpdateClientServerInfo(nlohmann::json* pJson, int ClientID)
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	if(!pPlayer)
		return;

	const auto& TeeInfo = pPlayer->GetTeeInfo();
	nlohmann::json JsSkin;

	if(TeeInfo.m_UseCustomColor)
	{
		JsSkin =
		{
			{"name", TeeInfo.m_aSkinName},
			{"color_body", TeeInfo.m_ColorBody},
			{"color_feet", TeeInfo.m_ColorFeet}
		};
	}
	else
	{
		JsSkin = { {"name", TeeInfo.m_aSkinName} };
	}

	(*pJson)["skin"] = std::move(JsSkin);
	(*pJson)["afk"] = pPlayer->IsAfk();
	(*pJson)["team"] = pPlayer->GetTeam();
}

void CGS::OnClientPrepareChangeWorld(int ClientID)
{
	if(m_apPlayers[ClientID])
	{
		m_apPlayers[ClientID]->KillCharacter(WEAPON_WORLD);
		delete m_apPlayers[ClientID];
		m_apPlayers[ClientID] = nullptr;
	}

	const int AllocMemoryCell = ClientID + m_WorldID * MAX_CLIENTS;
	m_apPlayers[ClientID] = new(AllocMemoryCell) CPlayer(this, ClientID);
	Core()->QuestManager()->Update(m_apPlayers[ClientID]);
}

bool CGS::IsClientReady(int ClientID) const
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	return pPlayer && pPlayer->m_aPlayerTick[LastChangeInfo] > 0;
}

bool CGS::IsClientPlayer(int ClientID) const
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	return !pPlayer || pPlayer->GetTeam() != TEAM_SPECTATORS;
}

bool CGS::IsClientCharacterExist(int ClientID) const
{
	return GetPlayer(ClientID, false, true) != nullptr;
}

bool CGS::IsClientMRPG(int ClientID) const
{
	return Server()->GetStateClientMRPG(ClientID) || (ClientID >= MAX_PLAYERS && ClientID < MAX_CLIENTS);
}

void* CGS::GetLastInput(int ClientID) const
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	return pPlayer ? (void*)pPlayer->m_pLastInput : nullptr;
}

int CGS::GetClientVersion(int ClientID) const
{
	IServer::CClientInfo Info = { 0 };
	Server()->GetClientInfo(ClientID, &Info);
	return Info.m_DDNetVersion;
}

const char* CGS::Version() const { return GAME_VERSION; }
const char* CGS::NetVersion() const { return GAME_NETVERSION; }

void CGS::OnClearClientData(int ClientID)
{
	Core()->OnResetClientData(ClientID);
	VoteWrapper::Data()[ClientID].clear();

	// clear active snap bots for player
	for(auto& pActiveSnap : DataBotInfo::ms_aDataBot)
		pActiveSnap.second.m_aActiveByQuest[ClientID] = false;
}

void CGS::UpdateCollisionZones()
{
	// update tile text by interval
	const int TextZoneUpdateInterval = Server()->TickSpeed() * g_Config.m_SvIntervalTileTextUpdate;
	if(Server()->Tick() % TextZoneUpdateInterval == 0)
	{
		const auto& vTextZones = Collision()->GetTextZones();
		for(const auto& [Number, Detail] : vTextZones)
		{
			const auto& Text = Detail.Text;
			for(const auto& Pos : Detail.vPositions)
			{
				EntityManager()->Text(Pos, TextZoneUpdateInterval, Text.c_str());
			}
		}
	}
}

bool CGS::SendMenuMotd(CPlayer* pPlayer, int Menulist) const
{
	return pPlayer ? Core()->OnSendMenuMotd(pPlayer, Menulist) : false;
}

void CGS::UpdateExpMultiplier()
{
	if(IsWorldType(WorldType::Dungeon))
	{
		m_MultiplierExp = g_Config.m_SvRaidDungeonExpMultiplier;
		return;
	}

	m_MultiplierExp = (100 + maximum(20, rand() % 200));
}

void CGS::ResetExpMultiplier()
{
	m_MultiplierExp = 100;
}

void CGS::UpdateVotesIfForAll(int MenuList) const
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->m_VotesData.GetCurrentMenuID() == MenuList)
			m_apPlayers[i]->m_VotesData.UpdateVotes(MenuList);
	}
}

bool CGS::OnClientVoteCommand(int ClientID, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	CPlayer* pPlayer = GetPlayer(ClientID, false, true);
	if(!pPlayer)
	{
		Chat(ClientID, "Deploy it while still alive!");
		return true;
	}

	// parsing default vote commands
	if(pPlayer->m_VotesData.DefaultVoteCommands(pCmd, Extra1, Extra2, ReasonNumber, pReason))
		return true;

	// parsing everything else
	const auto csqlReason = sqlstr::CSqlString<64>(pReason);
	return Core()->OnPlayerVoteCommand(pPlayer, pCmd, Extra1, Extra2, ReasonNumber, csqlReason.cstr());
}

bool CGS::OnClientMotdCommand(int ClientID, const char* pCmd)
{
	auto* pPlayer = GetPlayer(ClientID, false, true);
	if(!pPlayer)
	{
		Chat(ClientID, "Deploy it while still alive!");
		return true;
	}

	if(PPSTR(pCmd, "BACKPAGE") == 0 || PPSTR(pCmd, "MENU") == 0)
		return true;

	const bool Result = Core()->OnPlayerMotdCommand(pPlayer, &pPlayer->m_MotdData, pCmd);
	if(Result)
	{
		CreatePlayerSound(ClientID, SOUND_PICKUP_ARMOR);
	}

	return Result;
}

bool CGS::DestroyPlayer(int ClientID)
{
	if(ClientID < 0 || ClientID > MAX_CLIENTS || !m_apPlayers[ClientID])
		return false;

	if(m_apPlayers[ClientID]->GetCharacter())
		m_apPlayers[ClientID]->KillCharacter(WEAPON_WORLD);
	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = nullptr;
	return true;
}

int CGS::CreateBot(short BotType, int BotID, int SubID)
{
	int BotClientID = MAX_PLAYERS;
	while(m_apPlayers[BotClientID])
	{
		BotClientID++;
		if(BotClientID >= MAX_CLIENTS)
			return -1;
	}

	Server()->InitClientBot(BotClientID);
	const int AllocMemoryCell = BotClientID + m_WorldID * MAX_CLIENTS;
	m_apPlayers[BotClientID] = new(AllocMemoryCell) CPlayerBot(this, BotClientID, BotID, SubID, BotType);
	return BotClientID;
}

bool CGS::TakeItemCharacter(int ClientID)
{
	CPlayer* pPlayer = GetPlayer(ClientID, true, true);
	if(!pPlayer)
		return false;

	std::vector<CEntityDropItem*> vDrops;
	for(CEntity* item : m_World.FindEntities(pPlayer->GetCharacter()->m_Core.m_Pos, 24, 64, CGameWorld::ENTTYPE_ITEM_DROP))
		vDrops.push_back((CEntityDropItem*)item);

	for(const auto& pDrop : vDrops)
	{
		if(pDrop && pDrop->TakeItem(ClientID))
			return true;
	}
	return false;
}

bool CGS::IsWorldType(WorldType Type) const
{
	return Server()->IsWorldType(m_WorldID, Type);
}

void CGS::InitWorld()
{
	const auto pWorldDetail = Server()->GetWorldDetail(m_WorldID);
	const auto worldType = pWorldDetail->GetType();

	// determine controller and world type
	std::string_view worldTypeStr;
	switch(worldType)
	{
		case WorldType::Dungeon:
			m_pController = new CGameControllerDungeon(this);
			worldTypeStr = "Dungeon";
			m_AllowedPVP = false;
			break;

		case WorldType::Tutorial:
			m_pController = new CGameControllerTutorial(this);
			worldTypeStr = "Tutorial";
			m_AllowedPVP = false;
			break;

		default:
			m_pController = new CGameControllerDefault(this);
			worldTypeStr = "Default";
			m_AllowedPVP = true;
			break;
	}

	// initialize controller and update game state
	UpdateExpMultiplier();
	m_pController->OnInit();

	// Log world initialization details
	const char* pStrStatePvp = m_AllowedPVP ? "yes" : "no";
	dbg_msg("world init", "%s(ID: %d) | %s | PvP: %s",
		Server()->GetWorldName(m_WorldID), m_WorldID,
		worldTypeStr.data(), pStrStatePvp);
}

bool CGS::IsPlayerInWorld(int ClientID, int WorldID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return false;

	int PlayerWorldID = m_apPlayers[ClientID]->GetCurrentWorldID();
	return PlayerWorldID == (WorldID == -1 ? m_WorldID : WorldID);
}

bool CGS::ArePlayersNearby(vec2 Pos, float Distance) const
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = m_apPlayers[i];
		if(pPlayer && IsPlayerInWorld(i) && distance(Pos, pPlayer->m_ViewPos) <= Distance)
			return true;
	}

	return false;
}

IGameServer* CreateGameServer() { return new CGS; }
