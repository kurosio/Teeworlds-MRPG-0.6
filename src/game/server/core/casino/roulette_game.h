#ifndef GAME_SERVER_CORE_CASINO_ROULETTE_GAME_H
#define GAME_SERVER_CORE_CASINO_ROULETTE_GAME_H

#include "casino_game.h"

enum class ERouletteBetKind { COLOR = 0, NUMBER = 1, RANGE = 2 };
enum class ERouletteColor { GREEN = 0, RED = 1, BLACK = 2 };
enum class ERouletteRange { LOW = 0, MID = 1, HIGH = 2 };
enum class ERouletteState { LOBBY, SPINNING, SHOW_RESULT };

class CGS;
class CEntityRouletteArrow;

class CRouletteGame : public CCasinoGame
{
public:
	using SlotBet = CCasinoGame::SlotBet;

	struct RouletteResult
	{
		int m_Number{};
		ERouletteColor m_Color{};
		std::vector<SlotBet> m_vPlayers{};
	};

	using FOnFinished = std::function<void(const RouletteResult&)>;

private:
	vec2 m_Pos{};
	CEntityRouletteArrow* m_pArrow{};

	ERouletteState m_State = ERouletteState::LOBBY;
	int m_ResultNumber{};
	ERouletteColor m_ResultColor{};
	FOnFinished m_pfnOnFinished;

	void EnterLobby();
	void EnterSpin();
	void EnterShowResult();

public:
	CRouletteGame(CGS* pGS, vec2 Pos, int LobbyDurationTicks = 15, int ShowResultTicks = 4);
	~CRouletteGame() override;

	void Tick() override;
	vec2 GetPos() const override { return m_Pos; }
	bool Leave(int ClientID, int* pRefund = nullptr) override;

	bool Join(int ClientID, int Bet, ERouletteBetKind Kind, int Value);
	void SetOnFinished(FOnFinished pfCallback) { m_pfnOnFinished = std::move(pfCallback); }

	ERouletteState GetState() const { return m_State; }

	// helpers
	static const char* RangeName(ERouletteRange R);
	static bool IsValidBet(ERouletteBetKind Kind, int Value);
	static float GetWinProbability(ERouletteBetKind Kind, int Value);
	static float GetMultiplier(ERouletteBetKind Kind, int Value);
	static int CalcPayout(int Bet, ERouletteBetKind Kind, int Value);
	static ERouletteColor GetNumberColor(int Number);
	static const char* BetKindName(ERouletteBetKind K);
	static const char* ColorName(ERouletteColor C);
};

#endif // GAME_SERVER_CORE_CASINO_ROULETTE_GAME_H
