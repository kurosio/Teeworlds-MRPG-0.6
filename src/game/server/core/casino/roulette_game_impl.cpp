#include "roulette_game.h"

#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include "entities/roulette_arrow.h"

// red numbers on a standard european wheel (rest are black; 0 is green)
static bool IsRed(int n)
{
	switch (n)
	{
		case 15: case 4: case 2: case 17: case 6: case 13:
		case 11: case 8: case 10: case 24: case 33: case 20:
		case 31: case 22: case 29: case 28: case 35: case 26:
			return true;
		default:
			return false;
	}
}

ERouletteColor CRouletteGame::GetNumberColor(int Number)
{
	if (Number == 0)
		return ERouletteColor::GREEN;
	return IsRed(Number) ? ERouletteColor::RED : ERouletteColor::BLACK;
}

CRouletteGame::CRouletteGame(CGS* pGS, vec2 Pos, int LobbyDurationTicks, int ShowResultTicks)
	: CCasinoGame(pGS, LobbyDurationTicks, ShowResultTicks), m_pGS(pGS), m_Pos(Pos)
{
	m_pArrow = new CEntityRouletteArrow(&m_pGS->m_World, -1, m_Pos);
	EnterLobby();
}

CRouletteGame::~CRouletteGame()
{
	if (m_pArrow)
		m_pArrow->Destroy();
}

void CRouletteGame::EnterLobby()
{
	m_State = ERouletteState::LOBBY;
	m_ResultNumber = 0;
	m_ResultColor = ERouletteColor::GREEN;
	m_StateEndTick = 0;
	ClearPlayers();
}

void CRouletteGame::EnterSpin()
{
	const int Duration = m_pGS->Server()->TickSpeed() * 6;
	int finalNumber = clamp((int)floorf(random_float(37.0f)), 0, 36);

	m_State = ERouletteState::SPINNING;
	m_StateEndTick = m_pGS->Server()->Tick() + Duration;

	NotifyStart();

	if (!m_pArrow)
		return;

	m_pArrow->Spin(Duration, finalNumber, [this](int number)
	{
		m_ResultNumber = number;
		m_ResultColor = GetNumberColor(number);

		RouletteResult Result;
		Result.m_Number = m_ResultNumber;
		Result.m_Color = m_ResultColor;
		Result.m_vPlayers.reserve(GetPlayers().size());

		for (auto& bet : GetPlayers())
		{
			const auto kind = static_cast<ERouletteBetKind>(bet.m_Type);

			if (kind == ERouletteBetKind::COLOR)
			{
				const auto chosen = static_cast<ERouletteColor>(bet.m_Threshold);
				bet.m_Win = (chosen == m_ResultColor);
			}
			else if (kind == ERouletteBetKind::RANGE)
			{
				if (m_ResultNumber == 0)
					bet.m_Win = false;
				else
				{
					const auto range = static_cast<ERouletteRange>(bet.m_Threshold);
					switch (range)
					{
						case ERouletteRange::LOW:  bet.m_Win = (m_ResultNumber >= 1 && m_ResultNumber <= 12); break;
						case ERouletteRange::MID:  bet.m_Win = (m_ResultNumber >= 13 && m_ResultNumber <= 24); break;
						case ERouletteRange::HIGH: bet.m_Win = (m_ResultNumber >= 25 && m_ResultNumber <= 36); break;
					}
				}
			}
			else
			{
				bet.m_Win = (bet.m_Threshold == m_ResultNumber);
			}

			bet.m_Payout = bet.m_Win ? CalcPayout(bet.m_Bet, kind, bet.m_Threshold) : 0;
			if (bet.m_Win)
				SettleBet(bet);
			Result.m_vPlayers.push_back(bet);
		}

		if (m_pfnOnFinished)
			m_pfnOnFinished(Result);

		EnterShowResult();
	});
}

void CRouletteGame::EnterShowResult()
{
	m_State = ERouletteState::SHOW_RESULT;
	m_StateEndTick = m_pGS->Server()->Tick() + m_ShowResultTicks;
}

void CRouletteGame::Tick()
{
	switch (m_State)
	{
		case ERouletteState::LOBBY:
			// lobby state
			PurgeOutOfRange();
			if (GetPlayers().empty())
			{
				m_StateEndTick = 0;
				return;
			}

			if (m_StateEndTick == 0)
				m_StateEndTick = m_pGS->Server()->Tick() + m_LobbyDurationTicks;
			
			if (m_pGS->Server()->Tick() >= m_StateEndTick)
				EnterSpin();
			
			break;

		case ERouletteState::SPINNING:
			// spinning state
			break;

		case ERouletteState::SHOW_RESULT:
			// show result state
			if (m_pGS->Server()->Tick() >= m_StateEndTick)
				EnterLobby();
			break;
	}
}

bool CRouletteGame::Join(int ClientID, int Bet, ERouletteBetKind Kind, int Value)
{
	if (m_State != ERouletteState::LOBBY || Bet <= 0 || !IsValidBet(Kind, Value))
		return false;

	auto* pPlayer = m_pGS->GetPlayer(ClientID, true);
	if (!pPlayer || !pPlayer->GetCharacter())
		return false;

	if (IsPlayerJoined(ClientID))
		return false;

	return CCasinoGame::Join(ClientID, Bet, static_cast<int>(Kind), Value, pPlayer->m_CurrencyCasinoItemID);
}

bool CRouletteGame::Leave(int ClientID, int* pRefund)
{
	if (m_State != ERouletteState::LOBBY)
		return false;

	return CCasinoGame::Leave(ClientID, pRefund);
}

bool CRouletteGame::IsValidBet(ERouletteBetKind Kind, int Value)
{
	switch (Kind)
	{
		case ERouletteBetKind::COLOR:
			return (Value == (int)ERouletteColor::GREEN
				|| Value == (int)ERouletteColor::RED
				|| Value == (int)ERouletteColor::BLACK);
		case ERouletteBetKind::NUMBER:
			return (Value >= 0 && Value <= 36);
		case ERouletteBetKind::RANGE:
			return (Value == (int)ERouletteRange::LOW
				|| Value == (int)ERouletteRange::MID
				|| Value == (int)ERouletteRange::HIGH);
	}
	return false;
}

float CRouletteGame::GetWinProbability(ERouletteBetKind Kind, int Value)
{
	switch (Kind)
	{
	case ERouletteBetKind::COLOR:
		if (Value == (int)ERouletteColor::GREEN)
			return 1.0f / 37.0f;
		return 18.0f / 37.0f;
	case ERouletteBetKind::NUMBER:
		return 1.0f / 37.0f;
	case ERouletteBetKind::RANGE:
		return 12.0f / 37.0f;
	}
	return 0.0f;
}

float CRouletteGame::GetMultiplier(ERouletteBetKind Kind, int Value)
{
	const float p = GetWinProbability(Kind, Value);
	return p <= 0.0001f ? 0.0f : (1.0f / p) * 0.97f;
}

int CRouletteGame::CalcPayout(int Bet, ERouletteBetKind Kind, int Value)
{
	const float m = GetMultiplier(Kind, Value);
	if (m <= 0.0f)
		return 0;

	// calculate in 64-bit and clamp: a big bet times the multiplier can overflow int
	const auto Payout = (int64_t)std::ceil((double)Bet * (double)m);
	return (int)std::clamp<int64_t>(Payout, 0, std::numeric_limits<int>::max());
}

const char* CRouletteGame::BetKindName(ERouletteBetKind Kind)
{
	switch (Kind)
	{
		case ERouletteBetKind::COLOR:  return "COLOR";
		case ERouletteBetKind::NUMBER: return "NUMBER";
		case ERouletteBetKind::RANGE:  return "RANGE";
	}
	return "?";
}

const char* CRouletteGame::ColorName(ERouletteColor Color)
{
	switch (Color)
	{
		case ERouletteColor::GREEN: return "GREEN";
		case ERouletteColor::RED:   return "RED";
		case ERouletteColor::BLACK: return "BLACK";
	}

	return "?";
}

const char* CRouletteGame::RangeName(ERouletteRange Range)
{
	switch (Range)
	{
		case ERouletteRange::LOW:  return "1-12";
		case ERouletteRange::MID:  return "13-24";
		case ERouletteRange::HIGH: return "25-36";
	}
	return "?";
}