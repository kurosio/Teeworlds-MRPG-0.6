#ifndef ENGINE_SERVER_GEOIPHANDLER_H
#define ENGINE_SERVER_GEOIPHANDLER_H

#include <teeother/tools/geolite2pp/GeoLite2PP.hpp>

class CGeoIP
{
	inline static CGeoIP* m_ptrInstance {};

public:
	static void init(const std::string& db_path);
	static void free()
	{
		delete m_ptrInstance;
		m_ptrInstance = nullptr;
	}
	static void close();

	/*
	  "continent"
	  "registered_country"
	  "country"
	  "city"
	  "subdivision"
	  "subdivision_iso_code"
	  "country_iso_code"
	  "accuracy_radius"
	  "latitude"
	  "longitude"
	  "time_zone"
	  "postal_code"
	*/
	static std::string getData(const std::string& field, const std::string& ip_address);

private:
    GeoLite2PP::DB *m_pDB{};
};

#endif
