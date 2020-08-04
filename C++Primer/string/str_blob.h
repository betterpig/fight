#ifndef STRBLOB_H
#define STRBLOB_H
#include <vector>
#include <string>
#include <memory>
#include "str_blob_ptr.h"

using namespace std;

class StrBlobPtr;

class StrBlob
{
friend class StrBlobPtr;
public:
    typedef vector<string>::size_type size_type;
    StrBlob();//默认构造函数
    StrBlob(initializer_list<string> il);//初始化列表作形参的构造函数
    size_type size() const {return data->size();}
    bool empty() const {return data->empty();}
    string& front() const;
    string& back() const;
    const string& cfront() const;//函数返回类型为常应用，不可修改
    const string& cback() const;
    void push_back(const string& t) {data->push_back(t);}
    void pop_back();
    StrBlobPtr begin() {return StrBlobPtr(*this);}//返回保存有指向vector<string>弱指针的对象
    StrBlobPtr end() 
    {
        auto ret=StrBlobPtr(*this,data->size());
        return ret;
    }

private:
    shared_ptr<vector<string>> data;//共享指针指向vector<string>
    void check(size_type i,const string &msg) const;
};

StrBlob::StrBlob():data(make_shared<vector<string>> ()) { }
StrBlob::StrBlob(initializer_list<string> il):data(make_shared<vector<string>> (il)) { }

void StrBlob::check(size_type i,const string& msg) const
{
    if(i>=data->size())
        throw out_of_range(msg);
}

string& StrBlob::front() const
{
    check(0,"front on empty StrBlob");
    return data->front();
}

string& StrBlob::back() const
{
    check(0,"back on empty StrBlob");
    return data->back();
}

const string& StrBlob::cfront() const
{
    return front();
}

const string& StrBlob::cback() const
{
    return back();
}

void StrBlob::pop_back()
{
    check(0,"pop_back on empty StrBlob");
    data->pop_back();
}


#endif