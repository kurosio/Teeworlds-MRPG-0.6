#ifndef GAME_SERVER_CORE_COMPONENTS_TUNES_TUNE_ZONE_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_TUNES_TUNE_ZONE_MANAGER_H

#include <map>
#include <optional>
#include <string>
#include <vector>

class CTuningParams;
class IStorageEngine;

enum class ETuneZone
{
	DEFAULT = 0,
	SLOW,
	WALKING,

	NUM_TUNE_ZONES
};

struct CSoundData
{
	std::vector<char> m_vData;
};

class CTuneZoneManager
{
public:
	static CTuneZoneManager& GetInstance();

	int GetZoneID(ETuneZone Zone) const;
	const CTuningParams* GetParams(ETuneZone Zone) const;
	const CTuningParams* GetParams(int ZoneID) const;

	void LoadSoundsFromDirectory(const char* pDirectory, IStorageEngine* pStorage);
	void RegisterSound(const std::string& sName, const void* pData, size_t DataSize);

	std::optional<std::string> BakePreparedMap(const char* pMapName, IStorageEngine* pStorage);

	std::map<std::string, CSoundData> m_Sounds;
	std::map<ETuneZone, CTuningParams> m_Zones;

private:
	CTuneZoneManager();
	~CTuneZoneManager() = default;

	CTuneZoneManager(const CTuneZoneManager&) = delete;
	CTuneZoneManager& operator=(const CTuneZoneManager&) = delete;
};

#endif // GAME_SERVER_CORE_COMPONENTS_TUNES_TUNE_ZONE_MANAGER_H