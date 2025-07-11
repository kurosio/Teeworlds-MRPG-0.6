#ifndef GAME_SERVER_MULTIPLIERS_H
#define GAME_SERVER_MULTIPLIERS_H

struct Multipliers
{
	enum Type
	{
		EXPERIENCE,
		GOLD,
	};

	int Experience { 100 };
	int Gold { 100 };

	void Apply(Type type, std::integral auto& value) const
	{
		switch(type)
		{
			default: value = translate_to_percent_rest(value, (float)Experience); break;
			case GOLD: value = translate_to_percent_rest(value, (float)Gold); break;
		}
	}
};

#endif