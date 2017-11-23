#include <vector>
#include <algorithm>
#include "sqlalchemy.h"
#include <iostream>

struct Students {
    Column<int64_t>      id;
    Column<int64_t>      userid;
    Column<int16_t>      type;
    Column<string, 64>   name;
    Column<int32_t>      age;
    Column<string, 32>   nick;

    _DEFINE(Students, _FIELD(id), _FIELD(userid), _FIELD(type), _FIELD(name), _FIELD(age), _FIELD(nick))
    _CONSTRUCT(Students, 0, id, userid, type, name, age, nick);
    _CONSTRUCT(Students, 1, userid, name);
    void Print() {
        cout << id.val << "," << userid.val << "," << type.val << "," << name.val << "," << age.val << "," << nick.val << endl;
    }
};

int main() {
    const EngHost   host("127.0.0.1", "root", "", "test", 3306);
    Engine      *eng = new Engine(host, 10);
    if (eng == nullptr) {
        return -1;
    }
    Session    se;
    se.Bind(eng);
    Students    student;
    int ret = se.Table("Students").Add(student.userid = 10024, student.type = 1, student.name = "qinghua", student.age = 38).Execute();
    cout << "ret=" << ret << endl;

    uint64_t id = se.GetInsertId();
    ret = se.Update(student.type = 2, student.nick = "haha").Filter(student.id == id).Execute();
    cout << "upt=" << ret << endl;
    
    vector<Students>  v;
    se.Table("Students").Query(student.id, student.userid, student.type, student.name, student.age, student.nick);
    se.GetAll(v);
    for_each(v.begin(), v.end(), [](Students s) {
        s.Print();
    });
    v.clear();
    se.Query(student.userid, student.name).Filter(student.id > 2, _OR(student.type == 1, student.type == 2));
    se.GetAll<Students, 1>(v);
    for_each(v.begin(), v.end(), [](Students s) {
        s.Print();
    });
    
    return 0;
}
