#ifndef GAME_SERVER_CORE_COMPONENTS_TUNES_TUNE_ZONE_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_TUNES_TUNE_ZONE_MANAGER_H

#include <map>
#include <optional>
#include <string>

struct CSoundData
{
    std::vector<char> m_vData;
};

class CTuningParams;
class IStorageEngine;

enum class ETuneZone
{
    DEFAULT = 0,
    SLOW,
    WALKING,

    NUM_TUNE_ZONES
};

class CTuneZoneManager
{
    char m_aDeleteTempfile[128];
    std::map<std::string, CSoundData> m_Sounds;
    std::map<ETuneZone, CTuningParams> m_Zones;

public:
    static CTuneZoneManager& GetInstance();

    int GetZoneID(ETuneZone Zone) const;
    const CTuningParams* GetParams(ETuneZone Zone) const;
    const CTuningParams* GetParams(int ZoneID) const;

    std::optional<std::string> BakeZonesIntoMap(const char* pMapName, IStorageEngine* pStorage);
	void DeleteTempfile(IStorageEngine* pStorage);

    void LoadSoundsFromDirectory(const char* pDirectory, IStorageEngine* pStorage);
    void RegisterSound(const std::string& sName, const void* pData, size_t DataSize);

private:
    CTuneZoneManager();
    ~CTuneZoneManager() = default;

    CTuneZoneManager(const CTuneZoneManager&) = delete;
    CTuneZoneManager& operator=(const CTuneZoneManager&) = delete;

};

#endif // GAME_SERVER_CORE_COMPONENTS_TUNES_TUNE_ZONE_MANAGER_H
