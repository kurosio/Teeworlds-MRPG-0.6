#ifndef GAME_SERVER_COMPONENT_BASE_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_BASE_HOUSE_DATA_H

class CGS;
class CPlayer;

class IHouse
{
public:
    virtual ~IHouse() = default;
    virtual CGS* GS() const = 0;
    virtual CPlayer* GetPlayer() const { return nullptr; }
    virtual int GetID() const = 0;
    virtual vec2 GetPos() const = 0;
    virtual const char* GetTableName() const = 0;
};

#endif