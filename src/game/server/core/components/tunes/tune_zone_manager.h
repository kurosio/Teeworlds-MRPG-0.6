#ifndef GAME_SERVER_CORE_COMPONENTS_TUNES_TUNE_ZONE_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_TUNES_TUNE_ZONE_MANAGER_H

#include <map>
#include <optional>
#include <string>

class CTuningParams;
class IStorageEngine;

enum class ETuneZone
{
    DEFAULT = 0,
    SLOW,

    NUM_TUNE_ZONES
};

class CTuneZoneManager
{
public:
    static CTuneZoneManager& GetInstance();

    int GetZoneID(ETuneZone Zone) const;
    const CTuningParams* GetParams(ETuneZone Zone) const;
    const CTuningParams* GetParams(int ZoneID) const;

    std::optional<std::string> BakeZonesIntoMap(const char* pMapName, IStorageEngine* pStorage);
	void DeleteTempfile(IStorageEngine* pStorage);

private:
    CTuneZoneManager();
    ~CTuneZoneManager() = default;

    CTuneZoneManager(const CTuneZoneManager&) = delete;
    CTuneZoneManager& operator=(const CTuneZoneManager&) = delete;

    std::map<ETuneZone, CTuningParams> m_Zones;

	char m_aDeleteTempfile[128];
};

#endif // GAME_SERVER_CORE_COMPONENTS_TUNES_TUNE_ZONE_MANAGER_H
