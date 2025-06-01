/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "bonus_manager.h"

#include <engine/shared/linereader.h>
#include <game/server/gamecontext.h>

inline static std::string getFileName(int AccountID)
{
	return fmt_default("server_data/account_bonuses/{}.txt", AccountID);
}

const char* BonusManager::GetStringBonusType(int bonusType) const
{
	switch(bonusType)
	{
		case BONUS_TYPE_EXPERIENCE: return "Experience Boost";
		case BONUS_TYPE_GOLD: return "Gold Boost";
		case BONUS_TYPE_HP: return "HP Boost";
		case BONUS_TYPE_MP: return "MP Boost";
		default: return "Unknown Bonus";
	}
}

void BonusManager::SendInfoAboutActiveBonuses() const
{
	const auto pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
	if(const auto activeBonusesCount = static_cast<int>(m_vTemporaryBonuses.size()); activeBonusesCount == 0)
		pGS->Chat(m_ClientID, "You have no active bonuses.");
	else
		pGS->Chat(m_ClientID, "You have '{} active bonus{}'.", activeBonusesCount, activeBonusesCount > 1 ? "es" : "");
}

void BonusManager::AddBonus(const TemporaryBonus& bonus)
{
	const auto pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);

	// check bonus
	bool bonusStacked = false;
	for(auto& existingBonus : m_vTemporaryBonuses)
	{
		if(existingBonus.Type == bonus.Type && existingBonus.Amount == bonus.Amount)
		{
			existingBonus.Duration += bonus.Duration;
			bonusStacked = true;

			// information
			const char* bonusType = GetStringBonusType(bonus.Type);
			const int addedDurationMinutes = bonus.Duration / 60;
			const int newTotalDurationMinutes = existingBonus.Duration / 60;
			pGS->Chat(m_ClientID, "'{} +{~.2}%' has been extended by '{} minutes'.", bonusType, bonus.Amount, addedDurationMinutes);
			pGS->Chat(m_ClientID, "New total duration: '{} minutes'.", newTotalDurationMinutes);
			break;
		}
	}

	// new bonus
	if(!bonusStacked)
	{
		m_vTemporaryBonuses.push_back(bonus);

		// information
		const char* bonusType = GetStringBonusType(bonus.Type);
		pGS->Chat(m_ClientID, "You have received: '{} +{~.2}%'", bonusType, bonus.Amount);
	}

	Save();
}

void BonusManager::Load()
{
	const auto* pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
	const auto* pPlayer = pGS->GetPlayer(m_ClientID);

	if(!pPlayer)
		return;

	auto* pStorage = pGS->Storage();
	const auto Filename = getFileName(pPlayer->Account()->GetID());
	CLineReader Reader;
	if(!Reader.OpenFile(pStorage->OpenFile(Filename.c_str(), IOFLAG_READ, IStorageEngine::TYPE_ABSOLUTE)))
		return;

	const time_t currentTime = time(nullptr);
	while(const char* pReadLine = Reader.Get())
	{
		TemporaryBonus bonus;
#if defined(__GNUC__) && __WORDSIZE == 64
		if(sscanf(pReadLine, "%d %f %ld %d", &bonus.Type, &bonus.Amount, &bonus.StartTime, &bonus.Duration) == 4)
#else
		if(sscanf(pReadLine, "%d %f %lld %d", &bonus.Type, &bonus.Amount, &bonus.StartTime, &bonus.Duration) == 4)
#endif
		{
			int elapsedTime = static_cast<int>(difftime(currentTime, bonus.StartTime));
			if(elapsedTime < bonus.Duration)
			{
				bonus.StartTime = currentTime - elapsedTime;
				m_vTemporaryBonuses.push_back(bonus);
			}
		}
	}
}

void BonusManager::Save() const
{
	const auto* pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
	const auto* pPlayer = pGS->GetPlayer(m_ClientID);

	if(!pPlayer)
		return;

	auto* pStorage = pGS->Storage();
	const auto Filename = getFileName(pPlayer->Account()->GetID());
	if(const auto File = pStorage->OpenFile(Filename.c_str(), IOFLAG_WRITE, IStorageEngine::TYPE_ABSOLUTE))
	{
		for(const auto& bonus : m_vTemporaryBonuses)
		{
			char buffer[128];
#if defined(__GNUC__) && __WORDSIZE == 64
			str_format(buffer, sizeof(buffer), "%d %.2f %ld %d\n", bonus.Type, bonus.Amount, bonus.StartTime, bonus.Duration);
#else
			str_format(buffer, sizeof(buffer), "%d %.2f %lld %d\n", bonus.Type, bonus.Amount, bonus.StartTime, bonus.Duration);
#endif
			io_write(File, buffer, str_length(buffer));
		}
		io_close(File);
	}
}

void BonusManager::PostTick()
{
	bool hasChanges = false;
	for(auto it = m_vTemporaryBonuses.begin(); it != m_vTemporaryBonuses.end();)
	{
		if(!it->IsActive())
		{
			CGS* pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
			pGS->Chat(m_ClientID, "Your '{}' of '{~.2}%' has expired.", GetStringBonusType(it->Type), it->Amount);
			it = m_vTemporaryBonuses.erase(it);
			hasChanges = true;
		}
		else
		{
			++it;
		}
	}

	if(hasChanges)
	{
		Save();
	}
}

float BonusManager::GetTotalBonusPercentage(int bonusType) const
{
	float totalPercentage = 0.0f;
	for(const auto& bonus : m_vTemporaryBonuses)
	{
		if(bonus.Type == bonusType)
		{
			totalPercentage += bonus.Amount;
		}
	}

	return totalPercentage;
}

std::string BonusManager::GetBonusActivitiesString() const
{
	std::string resultStr{};
	int bonusesInLine = 0;

	for(int bonusType = BONUS_TYPE_EXPERIENCE; bonusType <= END_BONUS_TYPE; ++bonusType)
	{
		if(const int bonusPercentage = (int)GetTotalBonusPercentage(bonusType); bonusPercentage > 0)
		{
			if(!resultStr.empty() && bonusesInLine >= 2)
			{
				resultStr += "\n";
				bonusesInLine = 0;
			}
			else if(!resultStr.empty())
			{
				resultStr += ", ";
			}

			std::string bonusName;
			switch(bonusType)
			{
				case BONUS_TYPE_EXPERIENCE: bonusName = "EXP"; break;
				case BONUS_TYPE_GOLD: bonusName = "Gold"; break;
				case BONUS_TYPE_HP: bonusName = "HP"; break;
				case BONUS_TYPE_MP: bonusName = "MP"; break;
				default: bonusName = "Unknown"; break;
			}

			resultStr += bonusName + " +" + std::to_string(bonusPercentage) + "%";
			++bonusesInLine;
		}
	}

	return resultStr;
}