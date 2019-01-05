#ifndef _MY_MYSQL_H_
#define _MY_MYSQL_H_

#include <mysql.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

namespace herry
{
namespace mysql
{

template <typename To>
To lexical_cast(const char *const from)
{
    To to;

    std::stringstream str;
    str << from;
    str >> std::ws >> to;

    return std::move(to);
}

template <>
inline std::string lexical_cast<std::string>(const char *const from)
{
    return from;
}

class MySql;

template <typename Tuple, std::size_t N>
class FindTupleHelper
{
public:
    static void find(MYSQL_ROW item, Tuple &t)
    {
        FindTupleHelper<Tuple, N - 1>::find(item, t);
        std::get<N - 1>(t) = lexical_cast<typename std::remove_reference<decltype(std::get<N - 1>(t))>::type>(item[N - 1]);
    }
};

template <typename Tuple>
class FindTupleHelper<Tuple, 1>
{
public:
    static void find(MYSQL_ROW item, Tuple &t)
    {
        std::get<0>(t) = lexical_cast<typename std::remove_reference<decltype(std::get<0>(t))>::type>(item[0]);
    }
};

class MySql
{
public:
    MySql() : mMysql(mysql_init(NULL)) {}

    void Connect(const std::string &host, const std::string &user, const std::string &passwd, const std::string &db, unsigned int port)
    {
        if (mysql_real_connect(mMysql, host.c_str(), user.c_str(), passwd.c_str(), db.c_str(), port, NULL, 0) == NULL) {
            throw std::runtime_error("mysql_real_connect failed!");
        }
    }
    void SetCharacterSet(const std::string &csname)
    {
        mysql_set_character_set(mMysql, csname.c_str());
    }
    void IDUOperation(const std::string &sql_handle)
    {
        IDUOperation(sql_handle.c_str());
    }
    void IDUOperation(const char *sql_handle);
    template <typename T, typename... Tuple>
    bool FindTuple(const std::string &sql_handle, T &t, Tuple &... tuple)
    {
        return FindTuple(sql_handle.c_str(), t, tuple...);
    }
    template <typename T, typename... Tuple>
    bool FindTuple(const char *sql_handle, T &t, Tuple &... tuple);
    template <typename... Args>
    bool FindTuple(const char *sql_handle, std::tuple<Args...> &t);
    template <typename Container>
    bool FindTuples(const char *sql_handle, Container &container);
    template <template <typename, typename...> class Container, typename... Args>
    bool FindTuples(const char *sql_handle, Container<std::tuple<Args...>> &container);

private:
    template <typename T, typename... Args>
    void readItem(MYSQL_ROW item, T &t, Args &... rest);
    template <typename T>
    void readItem(MYSQL_ROW item, T &t);

    MYSQL *mMysql;
};

template <typename... Args>
bool MySql::FindTuple(const char *sql_handle, std::tuple<Args...> &t)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    bool ret = false;

    mysql_query(mMysql, sql_handle);
    result = mysql_store_result(mMysql);
    row = mysql_fetch_row(result);
    if (row) {
        constexpr size_t amount = sizeof...(Args);
        FindTupleHelper<decltype(t), amount>::find(row, t);
        ret = true;
    }
    mysql_free_result(result);

    return ret;
}

template <typename T, typename... Tuple>
bool MySql::FindTuple(const char *sql_handle, T &t, Tuple &... tuple)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    bool ret = false;

    mysql_query(mMysql, sql_handle);
    result = mysql_store_result(mMysql);
    row = mysql_fetch_row(result);
    if (row) {
        readItem(row, t, tuple...);
        ret = true;
    }
    mysql_free_result(result);

    return ret;
}

template <typename Container>
bool MySql::FindTuples(const char *sql_handle, Container &container)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    bool ret = false;

    mysql_query(mMysql, sql_handle);
    result = mysql_store_result(mMysql);
    while ((row = mysql_fetch_row(result))) {
        typename Container::value_type t;
        t = lexical_cast<decltype(t)>(*row);
        container.push_back(std::move(t));
    }
    mysql_free_result(result);

    return ret;
}

template <template <typename, typename...> class Container, typename... Args>
bool MySql::FindTuples(const char *sql_handle, Container<std::tuple<Args...>> &container)
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    bool ret = false;

    mysql_query(mMysql, sql_handle);
    result = mysql_store_result(mMysql);
    while ((row = mysql_fetch_row(result))) {
        std::tuple<Args...> t;
        constexpr size_t amount = sizeof...(Args);
        FindTupleHelper<decltype(t), amount>::find(row, t);
        container.push_back(std::move(t));
    }
    mysql_free_result(result);

    return ret;
}

template <typename T, typename... Args>
void MySql::readItem(MYSQL_ROW item, T &t, Args &... rest)
{
    t = lexical_cast<T>(*item);
    readItem(++item, rest...);
}

template <typename T>
void MySql::readItem(MYSQL_ROW item, T &t)
{
    t = lexical_cast<T>(*item);
}

} // namespace mysql
} // namespace herry

#endif