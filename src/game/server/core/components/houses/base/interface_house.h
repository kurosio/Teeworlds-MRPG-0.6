#ifndef GAME_SERVER_COMPONENT_HOUSES_BASE_INTEFACE_HOUSE_H
#define GAME_SERVER_COMPONENT_HOUSES_BASE_INTEFACE_HOUSE_H

class CGS;
class CPlayer;

class IHouse
{
public:
    enum class Type
    {
        Guild,
        Player
    };

    virtual ~IHouse() = default;
    virtual CGS* GS() const = 0;
    virtual CPlayer* GetPlayer() const { return nullptr; }
    virtual int GetID() const = 0;
    virtual vec2 GetPos() const = 0;
    virtual const char* GetTableName() const = 0;
    virtual Type GetHouseType() const = 0;
};

#endif