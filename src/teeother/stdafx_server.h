#ifndef SERVER_STDAFX_H
#define SERVER_STDAFX_H

//TODO: add precompiled headers

// core
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>

// custom something that is subject to less changes is introduced
#include <base/system.h>
#include <teeother/components/localization.h>

#endif //SERVER_STDAFX_H