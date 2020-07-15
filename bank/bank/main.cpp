#include "account.h"
#include <iostream>
#include "array.h"
using namespace std;

int main() {
	Date date(2008, 11, 1);	//起始日期
	//建立几个账户

	Array<Account *> accounts;

	cout << "(a)add account (d)deposit (w)withdraw (s)show (c)change day (n)next month (e)exit" << endl;
	char cmd;
	do {
		//显示日期和总金额
		date.show();
		cout << "\tTotal: " << Account::getTotal() << "\tcommand> ";

		char type;
		int index, day;
		double amount, credit, rate, fee;
		string id, desc;
		Account* account;

		cin >> cmd;
		switch (cmd) {
		case 'a':	//增加账户
			cin >> type >> id;
			if (type == 's') {
				cin >> rate;
				account = new SavingsAccount(date, id, rate);
			}
			else {
				cin >> credit >> rate >> fee;
				account = new CreditAccount(date, id, credit, rate, fee);
			}
			accounts.push_back(account);
			break;
		case 'd':	//存入现金
			cin >> index >> amount;
			getline(cin, desc);
			accounts[index]->deposit(date, amount, desc);
			break;
		case 'w':	//取出现金
			cin >> index >> amount;
			getline(cin, desc);
			accounts[index]->withdraw(date, amount, desc);
			break;
		case 's':	//查询各账户信息
			for (int i = 0; i < accounts.getSize(); i++) {
				cout << "[" << i << "] ";
				accounts[i]->show();
				cout << endl;
			}
			break;
		case 'c':	//改变日期
			cin >> day;
			if (day < date.getDay())
				cout << "You cannot specify a previous day";
			else if (day > date.getMaxDay())
				cout << "Invalid day";
			else
				date = Date(date.getYear(), date.getMonth(), day);
			break;
		case 'n':	//进入下个月
			if (date.getMonth() == 12)
				date = Date(date.getYear() + 1, 1, 1);
			else
				date = Date(date.getYear(), date.getMonth() + 1, 1);
			for (int i = 0; i < accounts.getSize(); i++)
				accounts[i]->settle(date);
			break;
		}
	} while (cmd != 'e');
	return 0;
}