#include <iostream>
#include <string>
#include "SalesData.h"

using namespace std;

int main()
{
    SalesData s1;
    SalesData s2("123");
    SalesData s3("123",2,10);
    SalesData s4(cin);
    print(cout,s1)<<endl;
    print(cout,s2)<<endl;
    print(cout,s3)<<endl;
    print(cout,s4)<<endl;
    SalesData total;
    if(read(cin,total))
    {
        SalesData trans;
        while(read(cin,trans))
        {
            if(total.isbn()==trans.isbn())
                total.combine(trans);
            else
            {
                print(cout,total)<<endl;
                total=trans;
            }
        }
        print(cout,total)<<endl;
    }
    else
        cerr<<"no data?!"<<endl;
}