#pragma once
#include"date.h"
#include<string>
#include"accumulator.h"

using namespace std;

class Account
{
private:
	std::string id;	//账号
	double balance;	//余额
	static double total;

protected:
	Account(const Date &date, const std::string &str);
	void record(const Date &date, double amount, const std::string &desc);
	void error(const std::string &msg) const;

public:
	std::string getId() const { return id; }
	double getBalance() const { return balance; }
	static double getTotal() { return total; }
	virtual void show() const;

	virtual void deposit(const Date &date, double amount, const std::string &desc)=0;
	virtual void withdraw(const Date &date, double amount, const std::string &desc) = 0;
	virtual void settle(const Date &date) = 0;
};


class SavingsAccount:public Account
{
private:
	Accumulator acc;
	double rate;	//年利率
	
public:
	SavingsAccount(const Date &date, const std::string &id, const double &rate);
	
	double getRate() const { return rate; }
	void deposit(const Date &date, double amount, const std::string &desc);
	void withdraw(const Date &date, double amount, const std::string &desc);
	void settle(const Date &date);
	void show() const override;
};

class CreditAccount:public Account
{
private:
	Accumulator acc;
	double credit;
	double rate;
	double fee;	

public:
	CreditAccount(const Date &date, const std::string &id,double credit, const double &rate,double fee);
	
	double getDebt() const {	//获得欠款额
		double balance = getBalance();
		return (balance < 0 ? balance : 0);
	}
	double getCredit() const { return credit; }
	double getRate() const { return rate; }
	double getFee() const { return fee; }
	double getAvailableCredit() const { return rate; }

	void deposit(const Date &date, double amount, const std::string &desc);
	void withdraw(const Date &date, double amount, const std::string &desc);
	void settle(const Date &date);
	void show() const override;

};

