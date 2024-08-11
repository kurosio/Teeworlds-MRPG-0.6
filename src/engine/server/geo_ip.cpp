
#include <teeother/tools/geolite2pp/GeoLite2PP_error_category.hpp>

#include "geo_ip.h"

void CGeoIP::init(const std::string& db_path)
{
    if(!m_ptrInstance)
        m_ptrInstance = new CGeoIP();
    if(!m_ptrInstance->m_pDB)
    {
        try
        {
            m_ptrInstance->m_pDB = new GeoLite2PP::DB(db_path);
        }
        catch(const std::system_error& e)
        {
            dbg_msg("geolite2pp", "System error: %s", e.what());
        }
    }
}

std::string CGeoIP::getData(const std::string& field, const std::string& ip_address)
{
    try
    {
        GeoLite2PP::MStr result = m_ptrInstance->m_pDB->get_all_fields(ip_address);
        if(const auto it = result.find(field); it != result.end())
            return it->second;

    }
    catch(const std::invalid_argument& e)
    {
        dbg_msg("geolite2pp", "Invalid IP address: %s", e.what());
    }
    catch(const std::system_error& e)
    {
        dbg_msg("geolite2pp", "System error: %s", e.what());
    }
    catch(const std::length_error& e)
    {
        dbg_msg("geolite2pp", "Error: %s", e.what());
    }

    return "\0";
}

void CGeoIP::close()
{
    if(m_ptrInstance->m_pDB)
    {
        delete m_ptrInstance->m_pDB;
        m_ptrInstance->m_pDB = nullptr;
    }
}