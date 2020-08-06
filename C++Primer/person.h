#include <string>
#include <iostream>
using namespace std;

struct Person
{
    string name;
    string address;

    string get_name() const {return name;}
    string get_address() const {return address;}
};

ostream& print(ostream& os,const Person& item)
{
    os<<item.name<<" "<<item.address<<" ";
    return os;
}

istream& read(istream& is,Person& item)
{
    is>>item.name>>item.address;
    return is;
}