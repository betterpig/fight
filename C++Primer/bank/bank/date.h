#pragma once

namespace 
{	//namespace使下面的定义只在当前文件中有效
	//存储平年中某个月1日之前有多少天，为便于getMaxDay函数的实现，该数组多出一项
	const int DAYS_BEFORE_MONTH[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
}

class Date
{
private:
	int year;
	int month;
	int day;
	int totalDays;

public:
	Date(int year, int month, int day);
	int getYear() const { return year; }
	int getMonth() const { return month; }
	int getDay() const { return day; }

	int getMaxDay() const;
	bool isLeapYear() const //判断当年是否为闰年
	{	
		return year % 4 == 0 && year % 100 != 0 || year % 400 == 0;
	}
	int operator - (const Date &lastDate) const
	{
		return totalDays - lastDate.totalDays;
	}
	void show() const;
};

