#include <iostream>
#include "str_blob.h"
#include "str_blob_ptr.h"
#include "str_vec.h"

using namespace std;

int main()
{
    StrBlob a1={"hi","bye","now"};
    StrBlobPtr p=a1;
    *(p+1)="you";
    cout<<*(p+1)<<endl;
    cout<<p->size()<<endl;
}