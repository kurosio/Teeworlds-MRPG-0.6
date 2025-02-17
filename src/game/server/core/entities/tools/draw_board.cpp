#include "draw_board.h"

#include <game/server/gamecontext.h>
#include "laser_orbite.h"
#include <game/server/entities/pickup.h>

#include <game/server/core/components/Inventory/InventoryManager.h>

IServer* CBrush::Server() const { return m_pBoard->Server(); }
CGS* CBrush::GS() const { return m_pBoard->GS(); }

[[nodiscard]] static CEntity* CreateEntityBrushItem(CGameWorld* pWorld, int ItemID, vec2 Pos)
{
	switch(ItemID)
	{
		case itPickupHealth: return new CPickup(pWorld, POWERUP_HEALTH, 0, Pos);
		case itPickupMana: return new CPickup(pWorld, POWERUP_ARMOR, 0, Pos);
		case itPickupShotgun: return new CPickup(pWorld, POWERUP_WEAPON, WEAPON_SHOTGUN, Pos);
		case itPickupGrenade: return new CPickup(pWorld, POWERUP_WEAPON, WEAPON_GRENADE, Pos);
		case itPickupLaser: return new CPickup(pWorld, POWERUP_WEAPON, WEAPON_LASER, Pos);
		default: return nullptr;
	}
}

CBrush::CBrush(CPlayer* pPlayer, CEntityDrawboard* pBoard, DrawboardEvent* pToolEvent)
{
	m_pPlayer = pPlayer;
	m_pBoard = pBoard;
	m_pToolEvent = pToolEvent;
	m_pEntity = nullptr;

	CBrush::InitBrushCollection();
	CBrush::UpdateEntity();
}

CBrush::~CBrush()
{
	if(m_pEntity)
	{
		m_pEntity->Reset();
		m_pEntity->GameWorld()->DestroyEntity(m_pEntity);
	}

	m_pBoard = nullptr;
	m_pPlayer = nullptr;
}

void CBrush::InitBrushCollection()
{
	m_vBrushItemsCollection = GS()->Core()->InventoryManager()->GetItemsCollection(ItemGroup::Decoration, std::nullopt);
	m_BrushItem = m_vBrushItemsCollection.begin();
}

bool CBrush::OnUpdate()
{
	if(!m_pPlayer || !m_pPlayer->GetCharacter() || !m_pBoard)
		return false;

	if(!UpdatePosition())
	{
		GS()->Chat(m_pPlayer->GetCID(), "You've gone outside the drawing area!");
		return false;
	}

	if(!ProccessEvent(DrawboardToolEvent::OnUpdate, nullptr))
		return false;

	SendBroadcast();
	return HandleInput();
}

bool CBrush::HandleInput()
{
	const int& ClientID = m_pPlayer->GetCID();
	Server()->Input()->BlockInputGroup(ClientID, BLOCK_INPUT_FREEZE_HAMMER | BLOCK_INPUT_FIRE | BLOCK_INPUT_HOOK);

	if(Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_FIRE))
		Draw();
	else if(Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_HOOK))
		Erase();
	else if(Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_NEXT_WEAPON))
		NextItem();
	else if(Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_PREV_WEAPON))
		PrevItem();
	else if(Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_SCOREBOARD))
		SwitchMode();
	else if(Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_MENU))
		return false;
	return true;
}

void CBrush::SendBroadcast() const
{
	// initialize variables
	std::string strAvailable{};
	std::string strSpaces(140, ' ');
	CItemDescription* pItem = GS()->GetItemInfo(*m_BrushItem);

	// check flags
	if(m_pBoard->m_Flags & DRAWBOARDFLAG_PLAYER_ITEMS)
	{
		CPlayerItem* pPlayerItem = m_pPlayer->GetItem(*m_BrushItem);
		strAvailable = fmt_localize(m_pPlayer->GetCID(), "has {}", pPlayerItem->GetValue());
	}
	else
	{
		strAvailable = fmt_localize(m_pPlayer->GetCID(), "unlimited");
	}

	// send broadcast
	GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::MainInformation, 50, "Drawing with: {} | {}"
		"\n{}"
		"\n\n- \"Fire\" add item"
		"\n- \"Hook\" erase item"
		"\n- \"Menu\" end drawing"
		"\n- \"Prev, Next\" - switch item"
		"\n{}",
		pItem->GetName(), strAvailable.c_str(), pItem->GetDescription(), strSpaces.c_str());
}

bool CBrush::UpdatePosition()
{
	vec2 NewPos = m_pPlayer->GetCharacter()->GetMousePos();
	if(m_Flags & BRUSHFLAG_CELL_POSITION)
	{
		m_Position.x = (float)(round_to_int(NewPos.x) / 32 * 32) + 16.f;
		m_Position.y = (float)(round_to_int(NewPos.y) / 32 * 32) + 16.f;
	}
	else
	{
		m_Position = NewPos;
	}
	m_pEntity->SetPos(m_Position);
	return distance(m_Position, m_pBoard->GetPos()) <= m_pBoard->m_Radius;
}

void CBrush::UpdateEntity()
{
	if(m_pEntity)
	{
		m_pEntity->Reset();
		m_pEntity->GameWorld()->DestroyEntity(m_pEntity);
		m_pEntity = nullptr;
	}

	m_pEntity = CreateEntityBrushItem(&GS()->m_World, *m_BrushItem, m_Position);
}

void CBrush::Draw()
{
	if(GS()->Collision()->CheckPoint(m_pEntity->GetPos()))
	{
		GS()->Chat(m_pPlayer->GetCID(), "Can't set in collide tile");
		return;
	}

	auto* pPoint = new EntityPoint(m_pEntity, *m_BrushItem);
	if(!m_pBoard->Draw(this, pPoint))
	{
		delete pPoint;
		return;
	}

	m_pEntity = nullptr;
	UpdateEntity();
}

void CBrush::Erase()
{
	if(m_pBoard->Erase(this, m_Position))
	{
	}
}

void CBrush::SwitchMode()
{
	if(m_Flags & BRUSHFLAG_CELL_POSITION)
		m_Flags &= ~BRUSHFLAG_CELL_POSITION;
	else
		m_Flags |= BRUSHFLAG_CELL_POSITION;
}

void CBrush::NextItem()
{
	++m_BrushItem;
	if(m_BrushItem == m_vBrushItemsCollection.end())
		m_BrushItem = m_vBrushItemsCollection.begin();
	UpdateEntity();
}

void CBrush::PrevItem()
{
	if(m_BrushItem == m_vBrushItemsCollection.begin())
		m_BrushItem = m_vBrushItemsCollection.end();
	--m_BrushItem;
	UpdateEntity();
}

bool CBrush::ProccessEvent(DrawboardToolEvent Event, EntityPoint* pPoint) const
{
	if(m_pToolEvent && m_pToolEvent->m_Callback)
		return m_pToolEvent->m_Callback(Event, m_pPlayer, pPoint, m_pToolEvent->m_pData);
	return true;
}

CEntityDrawboard::CEntityDrawboard(CGameWorld* pGameWorld, vec2 Pos, float Radius)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DRAW_BOARD, Pos)
{
	m_Radius = Radius;
	GameWorld()->InsertEntity(this);
}

CEntityDrawboard::~CEntityDrawboard()
{
	for(const auto& pBrush : m_vBrushes)
		delete pBrush;
	for(const auto& pEntity : m_vEntities)
		delete pEntity;

	delete m_pOrbite;
	m_vBrushes.clear();
	m_vEntities.clear();
}

void CEntityDrawboard::RegisterEvent(DrawboardToolCallback Callback, void* pUser)
{
	m_ToolEvent.m_Callback = Callback;
	m_ToolEvent.m_pData = pUser;
}

bool CEntityDrawboard::StartDrawing(CPlayer* pPlayer)
{
	auto iterBrush = SearchBrush(pPlayer);
	if(iterBrush != m_vBrushes.end())
		return false;

	auto* pBrush = new CBrush(pPlayer, this, &m_ToolEvent);
	if(!pBrush->ProccessEvent(DrawboardToolEvent::OnStart, nullptr))
	{
		delete pBrush;
		return false;
	}

	if(!m_pOrbite)
		m_pOrbite = new CEntityLaserOrbite(GameWorld(), -1, nullptr, 15, LaserOrbiteType::InsideOrbite, 0.f, m_Radius, LASERTYPE_FREEZE, CmaskOne(pPlayer->GetCID()));
	else
		m_pOrbite->AddClientMask(pPlayer->GetCID());

	m_vBrushes.emplace(new CBrush(pPlayer, this, &m_ToolEvent));
	return true;
}

void CEntityDrawboard::EndDrawing(CPlayer* pPlayer)
{
	auto iterBrush = SearchBrush(pPlayer);
	if(iterBrush == m_vBrushes.end())
		return;

	if((*iterBrush)->ProccessEvent(DrawboardToolEvent::OnEnd, nullptr))
	{
		delete* iterBrush;
		m_vBrushes.erase(iterBrush);
	}
}

bool CEntityDrawboard::Draw(CBrush* pBrush, EntityPoint* pPoint)
{
	if(!pBrush || !pPoint || !pPoint->m_pEntity)
		return false;

	if(m_Flags & DRAWBOARDFLAG_PLAYER_ITEMS)
	{
		CPlayerItem* pPlayerItem = pBrush->m_pPlayer->GetItem(pPoint->m_ItemID);
		if(!pPlayerItem->HasItem())
		{
			GS()->Chat(pBrush->m_pPlayer->GetCID(), "You don't have '{}'.", pPlayerItem->Info()->GetName());
			return false;
		}

		if(pBrush->ProccessEvent(DrawboardToolEvent::OnPointAdd, pPoint))
		{
			m_vEntities.push_back(pPoint);
			pPlayerItem->Remove(1);
			return true;
		}

		return false;
	}

	if(pBrush->ProccessEvent(DrawboardToolEvent::OnPointAdd, pPoint))
	{
		m_vEntities.push_back(pPoint);
		return true;
	}

	return false;
}

bool CEntityDrawboard::Erase(CBrush* pBrush, vec2 Pos)
{
	auto iterEntity = SearchPoint(Pos);
	if(!pBrush || iterEntity == m_vEntities.end())
		return false;

	if(pBrush->ProccessEvent(DrawboardToolEvent::OnPointErase, *iterEntity))
	{
		if(m_Flags & DRAWBOARDFLAG_PLAYER_ITEMS)
		{
			CPlayerItem* pPlayerItem = pBrush->m_pPlayer->GetItem((*iterEntity)->m_ItemID);
			pPlayerItem->Add(1);
		}

		(*iterEntity)->m_pEntity->Reset();
		(*iterEntity)->m_pEntity->MarkForDestroy();
		m_vEntities.erase(iterEntity);
		return true;
	}

	return false;
}

ska::unordered_set<CBrush*>::iterator CEntityDrawboard::SearchBrush(CPlayer* pPlayer)
{
	return std::find_if(m_vBrushes.begin(), m_vBrushes.end(), [pPlayer](const CBrush* pBrush) { return pBrush->m_pPlayer == pPlayer; });
}

std::vector<EntityPoint*>::iterator CEntityDrawboard::SearchPoint(vec2 Pos)
{
	return std::find_if(m_vEntities.begin(), m_vEntities.end(), [Pos](const EntityPoint* pPoint) { return distance(pPoint->m_pEntity->GetPos(), Pos) < 32.0f; });
}

void CEntityDrawboard::Tick()
{
	if(m_vBrushes.empty())
	{
		if(m_pOrbite)
		{
			delete m_pOrbite;
			m_pOrbite = nullptr;
		}
		return;
	}

	for(auto iterBrush = m_vBrushes.begin(); iterBrush != m_vBrushes.end();)
	{
		if(!(*iterBrush)->OnUpdate())
		{
			delete *iterBrush;
			iterBrush = m_vBrushes.erase(iterBrush);
		}
		else
		{
			++iterBrush;
		}
	}
}

void CEntityDrawboard::Snap(int SnappingClient)
{
}

void CEntityDrawboard::AddPoint(vec2 Pos, int ItemID)
{
	if(auto* pEntity = CreateEntityBrushItem(&GS()->m_World, ItemID, Pos))
		m_vEntities.push_back(new EntityPoint(pEntity, ItemID));
}