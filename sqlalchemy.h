
#include <iostream>
#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>
#include <tuple>
using namespace std;

#define _COLUMN(x) #x, x
#define _DEFINE(T, args...) T() {Field<T>::Define(args);}

enum class MType {
    UNKNOW      = 0,
    SMALLINT,
    INTEGER,
    BIGINT,
    FLOAT,
    DOUBLE,
    STRING,
};

struct MParam {
    void  * buffer;
    MType   type;
};

template<typename T>
class Field {
public:
    Field() {}
    Field(const T & value) : val(value) {}
    ~Field() {}
    
    Field & Sum() {
        ostringstream   os;
        os << "sum(" << name << ")";
        val = os.str();
        return *this;
    }
    Field & operator =(const T & value) {
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
    template<typename Y>
    static void Define(const char * name, Field<Y> & field) {
        field.name = name;
    }
    template<typename Y, typename... Args>
    static  void Define(const char * name, Field<Y> & field, Args&&... args) {
        field.name = name;
        Define(args...);                                                                                                
    }
    
    T           val;
    string      name;

private:
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
    Session() : m_status(SqlStatus::IniStatus) {}
    ~Session() {}

    void Clear() {
        m_params.clear();
        m_sql.str("");
    }
    string String() {
        return m_sql.str();
    }
    Session & Table(const string & table) {
        m_table = table;
        return *this;
    }
    Session & Limit(uint32_t cnt) {
        m_sql << " limit " << cnt;
        return *this;
    }
    template<typename T>
    Session & Orderby(const Field<T> & field, bool desc = false) {
        m_sql << " order by " << field.name;
        if (desc) {
            m_sql << " desc";
        }
        return *this;
    }
    template<typename T>
    Session & Groupby(const Field<T> & field) {
        m_sql << " group by " << field.name;
        return *this;
    }
    Session & Filter(const string & cond) {
        m_sql << " where " << cond;        
        return *this;
    }
    template<typename... Args>
    Session & FilterAnd(Args &&...  args) {
        m_sql << " where ";
        Expr(" and ", args...);
        return *this;
    }
    template<typename... Args>
    Session & FilterOr(Args &&...  args) {
        m_sql << " where ";
        Expr(" and ", args...);
        return *this;
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
        m_sql << "insert into " << m_table << "(";
        Insert(args...);
        for (int i = 0; i < (int)m_params.size(); i++) {
            if (i == 0) {
                m_sql << " value(?";
            } else {
                m_sql << " ,?";
            }
        }
        m_sql << ")";
        return *this;
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
            if (param.type == MType::STRING) {
                cout << (char *)param.buffer ;
            } else if (param.type == MType::SMALLINT) {
                cout << *(int16_t *)param.buffer;
            } else if (param.type == MType::INTEGER) {
                cout << *(int32_t *)param.buffer;
            } else if (param.type == MType::BIGINT) {
                cout << *(int64_t *)param.buffer;
            } else if (param.type == MType::FLOAT) {
                cout << *(float *)param.buffer;
            } else if (param.type == MType::DOUBLE) {
                cout << *(double *)param.buffer;
            }
        }
        cout << ")" << endl;;
    }

private:
    template<typename T>
    inline void Expr(const string & link, const tuple<string, T*>& expr) {
        m_sql << get<0>(expr);
        MParam      param;
        param.buffer = (void *)get<1>(expr);
        param.type = GetMtype(get<1>(expr));
        m_params.push_back(param);
    }
    template<typename T, typename... Args>
    inline void Expr(const string & link, const tuple<string, T*>& expr, Args &&... args) {
        m_sql << get<0>(expr) << link;
        MParam      param;
        param.buffer = (void *)get<1>(expr);
        param.type = GetMtype(get<1>(expr));
        m_params.push_back(param);
        Expr(link, args...);
    }
    template<typename T>
    inline void Insert(Field<T>& field) {
        Expr("", field.Value(field.val, ")", ""));
    }
    template<typename T, typename... Args>
    inline void Insert(Field<T>& field, Args &&... args) {
        Expr("", field.Value(field.val, ",", ""));
        Insert(args...);
    }
    template<typename T>
    inline void Upt(Field<T>& field) {
        Expr("", field.Value(field.val, " = ", "?"));
    }
    template<typename T, typename... Args>
    inline void Upt(Field<T>& field, Args &&... args) {
        Expr("", field.Value(field.val, " = ", "?, "));
        Upt(args...);
    }
    template<typename T>
    inline void Select(const Field<T> & field) {
        m_sql << field.name;
    }
    template<typename T, typename... Args>
    inline void Select(const Field<T> & field, Args &&... args) {    
        m_sql << field.name << ",";
        Select(args...);
    }    
    template<typename T>
    inline MType GetMtype(T *) {
        if (is_same<int16_t, typename decay<T>::type>::value) {
            return MType::SMALLINT;
        } else if (is_same<int32_t, typename decay<T>::type>::value) {
            return MType::INTEGER;
        } else if (is_same<int64_t, typename decay<T>::type>::value) {
            return MType::BIGINT;
        } else if (is_same<float, typename decay<T>::type>::value) {
            return MType::FLOAT;
        } else if (is_same<double, typename decay<T>::type>::value) {
            return MType::DOUBLE;
        } else if (is_same<char, typename decay<T>::type>::value) {
            return MType::STRING;
        } else {
            return MType::UNKNOW;
        }
    }
    string              m_table;
    ostringstream       m_sql;
    SqlStatus           m_status;
    vector<MParam>      m_params;
};
