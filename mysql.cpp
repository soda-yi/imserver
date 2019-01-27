#include "mysql.hpp"

#include <sstream>

using std::ostringstream;

namespace herry
{
namespace mysql
{

void MySql::IDUOperation(const char *sql_handle)
{
    ostringstream serr;

    if (mysql_query(mMysql, sql_handle)) {
        serr << "mysql_query failed(" << mysql_error(mMysql) << ")";
    }

    MYSQL_RES *result = mysql_store_result(mMysql);
    mysql_free_result(result);
}

} // namespace mysql
} // namespace herry