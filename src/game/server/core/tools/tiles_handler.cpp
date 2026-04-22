/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "tiles_handler.h"

#include <game/collision.h>
#include <game/server/entities/character.h>

namespace
{
	bool IsPosInActionZone(CCollision* pCollision, vec2 Pos, std::string_view ActionZoneName)
	{
		return pCollision->IsActiveActionZone(ActionZoneName, Pos);
	}
}

void CTileHandler::Handle(int Index)
{
	// initialize variables
	const int Indices[TILES_LAYER_NUM] =
	{
		m_pCollision->GetMainTileIndex(Index),
		m_pCollision->GetFrontTileIndex(Index),
		m_pCollision->GetExtraTileIndex(Index)
	};


	// handle tiles layers
	for(int i = 0; i < TILES_LAYER_NUM; ++i)
	{
		const int TileIndex = Indices[i];
		if(TileIndex >= TILE_AIR && TileIndex < MAX_TILES)
		{
			if(m_MarkedTiles[i] != TileIndex)
			{
				m_MarkEnter[i] = TileIndex;
				m_MarkExit[i] = m_MarkedTiles[i];
				m_MarkedTiles[i] = TileIndex;
			}
		}
	}

	// handle action zone
	m_aActionZonePrevPos[(int)EActionZonePosition::PLAYER] = m_aActionZonePos[(int)EActionZonePosition::PLAYER];
	m_aActionZonePrevPos[(int)EActionZonePosition::MOUSE] = m_aActionZonePos[(int)EActionZonePosition::MOUSE];
	m_aActionZonePos[(int)EActionZonePosition::PLAYER] = m_pCharacter->GetPos();
	m_aActionZonePos[(int)EActionZonePosition::MOUSE] = m_pCharacter->GetMousePos();
	if(!m_ActionZonePosInitialized)
	{
		m_aActionZonePrevPos[(int)EActionZonePosition::PLAYER] = m_aActionZonePos[(int)EActionZonePosition::PLAYER];
		m_aActionZonePrevPos[(int)EActionZonePosition::MOUSE] = m_aActionZonePos[(int)EActionZonePosition::MOUSE];
		m_ActionZonePosInitialized = true;
	}
}

bool CTileHandler::IsEnter(int TileIndex)
{
	bool Entered = false;
	for(int i = 0; i < TILES_LAYER_NUM; ++i)
	{
		if(TileIndex == m_MarkEnter[i])
		{
			m_MarkEnter[i] = TILE_AIR;
			Entered = true;
		}
	}
	return Entered;
}

bool CTileHandler::IsExit(int TileIndex)
{
	bool Exited = false;
	for(int i = 0; i < TILES_LAYER_NUM; ++i)
	{
		if(TileIndex == m_MarkExit[i])
		{
			m_MarkExit[i] = TILE_AIR;
			Exited = true;
		}
	}
	return Exited;
}

bool CTileHandler::IsEnterActionZone(std::string_view ActionZoneName, EActionZonePosition Position) const
{
	const int PositionIndex = (int)Position;
	const bool IsCurrent = IsPosInActionZone(m_pCollision, m_aActionZonePos[PositionIndex], ActionZoneName);
	const bool IsPrev = IsPosInActionZone(m_pCollision, m_aActionZonePrevPos[PositionIndex], ActionZoneName);
	return IsCurrent && !IsPrev;
}

bool CTileHandler::IsLeaveActionZone(std::string_view ActionZoneName, EActionZonePosition Position) const
{
	const int PositionIndex = (int)Position;
	const bool IsCurrent = IsPosInActionZone(m_pCollision, m_aActionZonePos[PositionIndex], ActionZoneName);
	const bool IsPrev = IsPosInActionZone(m_pCollision, m_aActionZonePrevPos[PositionIndex], ActionZoneName);
	return !IsCurrent && IsPrev;
}

bool CTileHandler::IsActiveActionZone(std::string_view ActionZoneName, EActionZonePosition Position) const
{
	return IsPosInActionZone(m_pCollision, m_aActionZonePos[(int)Position], ActionZoneName);
}