#include "sqlalchemy.h"
#include <iostream>

struct UserTb {
    Field<int64_t>      id;
    Field<int64_t>      userid;
    Field<int16_t>      type;
    Field<string>       name;
    Field<int32_t>      age;

    _DEFINE(UserTb, _COLUMN(id), _COLUMN(userid), _COLUMN(type), _COLUMN(name), _COLUMN(age))
};

int main() {
    Session    se;
    UserTb     user;
    string      ss  = "123456";
    se.Table("my_table").Query(user.name, user.age).FilterAnd(user.id > 0, user.userid != 10024, user.type == 1, user.name == ss, user.name == "abc").Orderby(user.id).Limit(10);
    se.Print();
    se.Add(user.userid = 10, user.type = 1024, user.name = "aaa", user.name = ss, user.age = 20);
    se.Print();
    return 0;
}
