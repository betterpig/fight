#ifndef SALESDATA_H
#define SALESDATA_H

#include <string>
using namespace std;

struct SalesData
{
    string bookno;
    unsigned units_sold;
    double revenue;

    SalesData():bookno(""),units_sold(0),revenue(0.0) {}//通过初始值列表显式的初始化数据成员
    SalesData(const string& s):bookno(s),units_sold(0),revenue(0.0) {}
    SalesData(const string& s,unsigned n,double p):bookno(s),units_sold(n),revenue(p*n) {}
    SalesData(istream&);

    string isbn() const {return bookno; }
    SalesData& combine(const SalesData&);
    double avg_price() const;
};

SalesData add(const SalesData& lhs,const SalesData& rhs)
{
    SalesData ret=lhs;//默认情况下，拷贝类的对象是拷贝的类的数据成员
    ret.combine(rhs);//调用已有的combine函数，而不是重新写一个加法的过程
    return ret;
}
ostream& print(ostream& os,const SalesData& data)
{
    os<<data.isbn()<<" "<<data.units_sold<<" "<<data.revenue<<" "<<data.avg_price();
    return os;
}
istream& read(istream& is,SalesData& data)//需要改变data对象，所以不能是常引用
{
    double price=0;
    is>>data.bookno>>data.units_sold>>price;
    data.revenue=price*data.units_sold;
    return is;
}

SalesData::SalesData(istream& is)//实际调用read函数为当前对象初始化
{//实际上在执行函数体之前，对象中的数据成员已经执行了类内初始化或默认初始化
    read(is,*this);
}

SalesData& SalesData::combine(const SalesData& rhs)
{
    units_sold+=rhs.units_sold;
    revenue+=rhs.revenue;
    return *this;
}

double SalesData::avg_price() const
{
    if(units_sold)
        return revenue/units_sold;
    else 
        return 0;
}



#endif