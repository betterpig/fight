#include <iostream>
#include <string>
#include "SalesData.h"

using namespace std;

int main()
{
    SalesData data1,data2;
    double price=0;
    cin>>data1.bookno>>data1.units_sold>>price;
    data1.revenue=data1.units_sold*price;
    cin>>data2.bookno>>data2.units_sold>>price;
    data2.revenue=data2.units_sold*price;

    if(data1.bookno==data2.bookno)
    {
        unsigned total_count=data1.units_sold+data2.units_sold;
        double total_revenue=data1.revenue+data2.revenue;
        cout<<data1.bookno<<" "<<total_count<<" "<<total_revenue<<" ";
        if(total_count!=0)
            cout<<total_revenue/total_count<<endl;
        else
            cout<<"no sales"<<endl;
        return 0;
    }
    else
    {
        cerr<<"data must refer to the same ISBN"<<endl;
        return -1;
    }
}