
#include <iostream>
#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>
#include <tuple>
#include "defer.h"
#include "engine.h"
using namespace std;

#define _FIELD(x) #x, x
#define _DEFINE(T, args...) \
    T() {\
        Column<T>::Define(args);\
    }
#define _CONSTRUCT(T, N, args...) \
    void Construct(MYSQL_BIND *bind, integral_constant<int, N>) {\
        uint32_t location = 0; \
        Column<T>::Construct(bind, location, args);\
    }
#define _AND(args...) Session::And(args)
#define _OR(args...) Session::Or(args)

template<typename T, int LENGTH = 0>
class Column {
public:
    Column() {}
    Column(const T & value) : val(value) {}
    Column(const Column<T, LENGTH>& other) : val(other.val), name(other.name) {}
    ~Column() {}
    
    Column & Sum() {
        ostringstream   os;
        os << "sum(" << name << ")";
        val = os.str();
        return *this;
    }
    Column & operator=(const T & value) {
        val = value;
        return *this;
    }
    template<typename Y>
    auto operator ==(Y && value) {
        return Operexpr(" = ", "?", value);
    }
    template<typename Y>
    auto operator !=(Y && value) {
        return Operexpr(" <> ", "?", value);
    }
    template<typename Y>
    auto operator >(Y && value) {
        return Operexpr(" > ", "?", value);
    }
    template<typename Y>
    auto operator >=(Y && value) {
        return Operexpr(" >= ", "?", value);
    }
    template<typename Y>
    auto operator <(Y && value) {
        return Operexpr(" < ", "?", value);
    }
    template<typename Y>
    auto operator <=(Y && value) {
        return Operexpr(" <= ", "?", value);
    }
    auto Value(T & value, const string & op, const string & r) const {
        return Operexpr(op, r, value);
    }
    template<typename Y, int L>
    static void Define(const char * name, Column<Y, L> & field) {
        field.name = name;
    }
    template<typename Y, int L, typename... Args>
    static void Define(const char * name, Column<Y, L> & field, Args&&... args) {
        field.name = name;
        Define(args...);                                                                                                
    }
    template<typename Y, int L>
    static void Construct(MYSQL_BIND *bind, uint32_t & location, Column<Y, L> & field) {
        Bind(bind[location++], field);
    }
    template<typename Y, int L, typename... Args>
    static void Construct(MYSQL_BIND * bind, uint32_t & location, Column<Y, L> & field, Args&&... args) {
        Bind(bind[location++], field);
        Construct(bind, location, args...);
    }
    inline enum_field_types MyType() {
        if (is_same<int16_t, typename decay<T>::type>::value) {
            return MYSQL_TYPE_SHORT;
        } else if (is_same<int32_t, typename decay<T>::type>::value) {
            return MYSQL_TYPE_LONG;
        } else if (is_same<int64_t, typename decay<T>::type>::value) {
            return MYSQL_TYPE_LONGLONG;
        } else if (is_same<float, typename decay<T>::type>::value) {
            return MYSQL_TYPE_FLOAT;
        } else if (is_same<double, typename decay<T>::type>::value) {
            return MYSQL_TYPE_DOUBLE;
        } else if (is_same<char, typename decay<T>::type>::value) {
            return MYSQL_TYPE_STRING;
        }        
        return MYSQL_TYPE_STRING;
    }
    
    T           val;
    string      name;

private:
    template<typename Y, int L>
    static typename enable_if<is_pointer<Y>::value>::type Bind(MYSQL_BIND & bind, Column<Y, L> & field) {
        bind.buffer_type = field.MyType();
        bind.buffer = (void *)field.val;
    }
    template<typename Y, int L>
    static typename enable_if<is_arithmetic<Y>::value>::type Bind(MYSQL_BIND & bind, Column<Y, L> & field) {
        bind.buffer_type = field.MyType();
        bind.buffer = (void *)&field.val;
    }
    template<int L>
    static void Bind(MYSQL_BIND & bind, Column<string, L> & field) {
        bind.buffer_type = MYSQL_TYPE_STRING;
        field.val.clear();
        field.val.resize(L);
        bind.buffer = (void *)field.val.c_str();
        bind.buffer_length = L;
    }
    template<typename Y>
    inline typename enable_if<is_pointer<Y>::value, tuple<string, const char*>>::type
    Operexpr(const string & op, const string & r, Y value) const {
        return make_tuple(name + op + r, value);
    }    
    template<typename Y>
    inline typename enable_if<is_arithmetic<Y>::value, tuple<string, const Y*>>::type
    Operexpr(const string & op, const string & r, Y & value) const {
        return make_tuple(name + op + r, &value);
    }
    inline tuple<string, const char*> Operexpr(const string & op, const string & r, string & value) const {
        return make_tuple(name + op + r, value.c_str());
    }
};

class Session {
public:
    Session() {}
    ~Session() {}

    void Clear() {
        m_params.clear();
        m_sql.str("");
    }
    string String() {
        return m_sql.str();
    }
    void Bind(Engine * engine) {
        m_engine = engine;
    }
    int Execute() {
        int    ret          = -1;
        function<int(MyHdl&)>  func = [&](MyHdl & hdl) {
            if (mysql_stmt_execute(hdl.stmt) != 0) {
                return -1;
            }
            m_insertId = mysql_stmt_insert_id(hdl.stmt);
            return (int)mysql_stmt_affected_rows(hdl.stmt);
        };
        DoSql(ret, func, false);
        return ret;
    }
    uint64_t GetInsertId() {
        return m_insertId;
    }
    template<typename T, int N = 0>
    int GetAll(vector<T> & result) {
        int     ret         = -1;
        function<int(MyHdl&)>  func = [&result](MyHdl & hdl) { 
            MYSQL_RES * res = mysql_stmt_result_metadata(hdl.stmt);
            if (res == nullptr) {
                return -1;
            }
            unsigned int count = mysql_num_fields(res);
            mysql_free_result(res);

            if (count == 0 || mysql_stmt_execute(hdl.stmt) != 0
                || mysql_stmt_store_result(hdl.stmt) != 0) {
                return -1;
            }
            MYSQL_BIND *bind = (MYSQL_BIND *)calloc(count, sizeof(MYSQL_BIND));
            if (bind == nullptr) {
                return -1;
            }
            Defer  guard([&bind]{
                delete bind;
            });
            T    data;
            data.Construct(bind, integral_constant<int, N>());
            if (mysql_stmt_bind_result(hdl.stmt, bind) != 0) {
                return -1;
            }
            while (true) {
                int fetch = mysql_stmt_fetch(hdl.stmt);
                if (fetch == 1) {
                    return -1;
                } else if (fetch == MYSQL_NO_DATA) {
                    break;
                } else {
                    result.push_back(data);
                    data.Construct(bind, integral_constant<int, N>());
                }
            }
            return 0;
        };
        DoSql(ret, func, false);
        return ret;
    }
    Session & Table(const string & table) {
        m_table = table;
        return *this;
    }
    Session & Limit(uint32_t cnt) {
        m_sql << " limit " << cnt;
        return *this;
    }
    template<typename T, int L>
    Session & Orderby(const Column<T, L> & field, bool desc = false) {
        m_sql << " order by " << field.name;
        if (desc) {
            m_sql << " desc";
        }
        return *this;
    }
    template<typename T, int L>
    Session & Groupby(const Column<T, L> & field) {
        m_sql << " group by " << field.name;
        return *this;
    }
    template<typename... Args>
    Session & Filter(Args &&...  args) {
        m_sql << " where ";
        Expr(" and ", args...);
        return *this;
    }
    template<typename... Args>
    static tuple<string, vector<MYSQL_BIND>> And(Args &&...  args) {
        return Session::Link("(", vector<MYSQL_BIND>{}, " and ", args...);
    }
    template<typename... Args>
    static tuple<string, vector<MYSQL_BIND>> Or(Args &&...  args) {
        return Session::Link("(", vector<MYSQL_BIND>{}, " or ", args...);
    }
    template<typename... Args>
    Session & Query(Args &&... args) {
        Clear();
        m_sql << "select ";
        Select(args...);
        m_sql << " from " << m_table;
        return *this;
    }
    template<typename... Args>
    Session & Add(Args &&... args) {
        Clear();
        Record("insert", args...)
        return *this;
    }
    template<typename... Args>
    Session & Replace(Args &&... args) {
        Clear();
        Record("replace", args...)
        return *this;
    }
    template<typename... Args>
    Session & operator()(Args &&... args) {
        
    }
    template<typename... Args>
    Session & Update(Args &&... args) {
        Clear();
        m_sql << "update " << m_table << " set ";
        Upt(args...);
        return *this;
    }
    
    void Print() {
        cout << "SQL: " << m_sql.str() << endl;
        cout << "VALUE: (";
        bool   first = true;
        for (auto param : m_params) {
            if (!first) {
                cout << ",";
            }            
            first = false;
            if (param.buffer_type == MYSQL_TYPE_STRING) {
                cout << (char *)param.buffer ;
            } else if (param.buffer_type  == MYSQL_TYPE_SHORT) {
                cout << *(int16_t *)param.buffer;
            } else if (param.buffer_type  == MYSQL_TYPE_LONG) {
                cout << *(int32_t *)param.buffer;
            } else if (param.buffer_type  == MYSQL_TYPE_LONGLONG) {
                cout << *(int64_t *)param.buffer;
            } else if (param.buffer_type  == MYSQL_TYPE_FLOAT) {
                cout << *(float *)param.buffer;
            } else if (param.buffer_type  == MYSQL_TYPE_DOUBLE) {
                cout << *(double *)param.buffer;
            }
        }
        cout << ")" << endl;;
    }
    template<typename T>
    static inline enum_field_types GetMtype(T *) {
        if (is_same<int16_t, typename decay<T>::type>::value
            || is_same<uint16_t, typename decay<T>::type>::value) {
            return MYSQL_TYPE_SHORT;
        } else if (is_same<int32_t, typename decay<T>::type>::value
            || is_same<uint32_t, typename decay<T>::type>::value) {
            return MYSQL_TYPE_LONG;
        } else if (is_same<int64_t, typename decay<T>::type>::value
            || is_same<uint64_t, typename decay<T>::type>::value) {
            return MYSQL_TYPE_LONGLONG;
        } else if (is_same<float, typename decay<T>::type>::value) {
            return MYSQL_TYPE_FLOAT;
        } else if (is_same<double, typename decay<T>::type>::value) {
            return MYSQL_TYPE_DOUBLE;
        } else if (is_same<char, typename decay<T>::type>::value) {
            return MYSQL_TYPE_STRING;
        } else if (is_same<signed char, typename decay<T>::type>::value
            || is_same<unsigned char, typename decay<T>::type>::value) {
            return MYSQL_TYPE_TINY;
        }
        return MYSQL_TYPE_STRING;
    }

private:
    template<typename T>
    inline void Expr(const string & link, const tuple<string, T>& expr) {
        m_sql << get<0>(expr);
        m_params = Session::Append(m_params, get<1>(expr));
    }
    template<typename T, typename... Args>
    inline void Expr(const string & link, const tuple<string, T>& expr, Args &&... args) {
        m_sql << get<0>(expr) << link;
        m_params = Session::Append(m_params, get<1>(expr));
        Expr(link, args...);
    }
    template<typename... Args>
    inline void Record(const string & op, Args &&... args) {
        m_sql << op << " into " << m_table << "(";
        Insert(args...);
        for (int i = 0; i < (int)m_params.size(); i++) {
            if (i == 0) {
                m_sql << " value(?";
            } else {
                m_sql << " ,?";
            }
        }
        m_sql << ")";
    }
    template<typename T, int L>
    inline void Insert(Column<T, L>& field) {
        Expr("", field.Value(field.val, ")", ""));
    }
    template<typename T, int L, typename... Args>
    inline void Insert(Column<T, L>& field, Args &&... args) {
        Expr("", field.Value(field.val, ",", ""));
        Insert(args...);
    }
    template<typename T, int L>
    inline void Upt(Column<T, L>& field) {
        Expr("", field.Value(field.val, " = ", "?"));
    }
    template<typename T, int L, typename... Args>
    inline void Upt(Column<T, L>& field, Args &&... args) {
        Expr("", field.Value(field.val, " = ", "?, "));
        Upt(args...);
    }
    template<typename T, int L>
    inline void Select(const Column<T, L> & field) {
        m_sql << field.name;
    }
    template<typename T, int L, typename... Args>
    inline void Select(const Column<T, L> & field, Args &&... args) {    
        m_sql << field.name << ",";
        Select(args...);
    }
    template<typename T, typename F>
    inline void DoSql(T & ret, F sqlcmd, bool dismiss) {
        if (m_engine == nullptr) {
            return;
        }
        MyHdl    hdl = m_engine->GetHdl();
        if (hdl.conn == nullptr || hdl.stmt == nullptr) {
            return;
        }
        Defer   guard([&]{
            m_engine->RetHdl(hdl);
        });
        if (mysql_stmt_prepare(hdl.stmt, m_sql.str().c_str(), m_sql.str().size()) != 0) {
            return;
        }
        if (!m_params.empty() && mysql_stmt_bind_param(hdl.stmt, &m_params[0]) != 0) {
            return;
        }
        if (sqlcmd) {
            ret = sqlcmd(hdl);
        }
        if (dismiss) {
            guard.Dismiss();
        }
    }
    template<typename T>
    static inline typename enable_if<is_pointer<T>::value, vector<MYSQL_BIND>>::type
    Append(vector<MYSQL_BIND> params, T buffer) {
        MYSQL_BIND      param;
        memset(&param, 0, sizeof(param));
        param.buffer = (void *)buffer;
        param.buffer_type = Session::GetMtype(buffer);
        param.buffer_length = Session::GetMSize(buffer);
        params.emplace_back(param);
        return params;
    }
    static inline auto Append(vector<MYSQL_BIND> params, const vector<MYSQL_BIND> & param) {
        for (auto v : param) {
             params.emplace_back(v);
        }
        return params;
    }
    template<typename T>
    static inline tuple<string, vector<MYSQL_BIND>> Link(const string & str, const vector<MYSQL_BIND>& params, 
            const string & op, const tuple<string, T>& expr) {
        ostringstream   osStr;
        osStr << str << get<0>(expr) << ")";
        auto  newParams = Session::Append(params, get<1>(expr));
        return make_tuple(osStr.str(), newParams);
    }
    template<typename T, typename... Args>
    static inline tuple<string, vector<MYSQL_BIND>> Link(const string & str, const vector<MYSQL_BIND>& params, 
            const string & op, const tuple<string, T>& expr, Args &&... args) {
        ostringstream   osStr;
        osStr << str << get<0>(expr) << op;
        auto  newParams = Session::Append(params, get<1>(expr));
        return Link(osStr.str(), newParams, op, args...);
    }    
    template<typename T>    
    static inline typename enable_if<is_same<char, typename decay<T>::type>::value, size_t>::type GetMSize(T * value) {
        return strlen(value);
    }
    template<typename T>    
    static inline typename enable_if<!is_same<char, typename decay<T>::type>::value, size_t>::type GetMSize(T * value) {
        return sizeof(T);
    }
    string              m_table;
    ostringstream       m_sql;
    vector<MYSQL_BIND>  m_params;
    Engine              *m_engine;
    uint64_t            m_insertId;
};

