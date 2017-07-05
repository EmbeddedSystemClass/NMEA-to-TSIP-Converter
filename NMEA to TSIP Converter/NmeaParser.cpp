/*
 * NmeaParser.cpp
 *
 * Created: 02.07.2017 20:51:05
 *  Author: Kelvin
 */ 
#include "stdafx.h"

 class NmeaParser
 {
	 public:
	 void tsipPush(u8 x)
	 {
		 
	 }
	 //void (*tsipPush)(u8);
	 //NmeaParser(void (*tsipPushi)(u8))
	 //{
		 //tsipPush = tsipPushi;
	 //}
	 protected:
 #define MSG_ENCODE(_a,_b) ((_a)|(u16(_b)<<8))
 #define DDF_MULTI(_x) (M_PI /180 /60 /DecimalDivisor[(_x)])
 
const u8 DLE = 0x10;
const u8 ETX = 0x03;
float Zero = 0; 
const bool Error = true;
const bool Ok = false;
const u16 DecimalDivisor[5] = {1,10,100,1000,10000};
const float DecimalDivisorF[5] = {DDF_MULTI(0),DDF_MULTI(1),DDF_MULTI(2),DDF_MULTI(3),DDF_MULTI(4)};
const u8 MaxDecimalDivisor = 4;

bool globalError; //���� ������ ������ NMEA
u8 byteCount; //������ ���� ������ 
u8 comaPoint; //����� ���� ������
u8 charPoint; //����� ������� � ������� ���� ������
u8 checkSum; //����������� �����
u8 data; //�������������� ���� NMEA

u8 Hex2Int() //������� ASCII � �����
{
	if (data >= '0' && data <= '9')
	return data - '0';
	if (data >= 'A' && data <= 'F')
	return data - 'A' + 10;
	if (data >= 'a' && data <= 'f')
	return data - 'a' + 10;
	globalError = Error;
	return 0;
}

u8 updateFlag; //����� ���������� ������

enum 
{
	UpdateTime = 1,
	UpdateDate = 2,
	UpdateLatitude = 4,
	UpdateLongitude = 8,
	UpdateAltitude = 16,
	UpdateSpeed = 32,
	UpdateCourse = 64,
	UpdateDateTime = UpdateTime | UpdateDate,
	UpdatePosition = UpdateLatitude | UpdateLongitude | UpdateAltitude,
	UpdateVelocity = UpdateSpeed | UpdateCourse
};

void GetFixedDigits(s32 &param, u8 &divisor, u8 first6, u8 size) //������ ����� � ������������� ����� ������
{
	if(charPoint == size) return;
	if(charPoint == 0)
	{
		param = 0;
		divisor = 0;
	}
	if(charPoint < size || divisor < MaxDecimalDivisor)
	{
		param *= (charPoint < size && (charPoint == first6 || charPoint == first6+2))?6:10;
		param += Hex2Int();
		if(charPoint > size) ++divisor;
	}
}

void GetFloat(float &param)
{
	static bool isSign;
	static bool isDot;
	static u8 divisor;
	if(charPoint == 0)
	{
		isSign = false;
		isDot = false;
		param = 0;
		divisor = 0;
	}
	switch(data)
	{
		case ' ':
		case '+': break;
		case '-': isSign = true; break;
		case '.': isDot = true; break;
		default:
		if(isDot)
		{
			if(divisor < MaxDecimalDivisor)
			{
				++divisor;
				float temp = Hex2Int();
				temp /= DecimalDivisor[divisor];
				if(isSign)
				{
					param -= temp;
				}
				else
				{
					param += temp;
				}
			}
		}
		else
		{
			param *= 10;
			if(isSign)
			{
				param -= Hex2Int();
			}
			else
			{
				param += Hex2Int();
			}
		}
		break;
	}
}

struct
{
	u32 seconds; //������ � ������� ���
	u8 centiSeconds; //����� ������
	u8 day; 
	u8 month;
	u8 year;
	float gpsTimeOfWeek; //������ � ������� GPS ������
	u16 gpsWeekNumber; //����������� ����� ������� GPS ������
	float gpsUtcOffset; //�������� ������� GPS UTC ������
	const u8 gpsUtcOffsetConst = 18; //��-��, �� ������ ����������
	u32 gpsTimeFixSec; //���� ������ � UTC �������� � ��������
	float timeOfFix; //�����, ����� ��� ������ ����� ���������/��������.
	const u32 secInDay = u32(24)*60*60; //������ � ������
	
	void TimeOfFixCalc()
	{
		timeOfFix = gpsTimeFixSec + seconds + float(centiSeconds)/100;
		if(timeOfFix >= 7 * secInDay)
		{
			timeOfFix -= 7 * secInDay;
		}
	}
	
	void GetTime()
	{
		switch(charPoint)
		{
			case 7:
			centiSeconds = Hex2Int()*10;
			break;
			case 8:
			centiSeconds += Hex2Int();
			break;
			case 5:
			updateFlag |= UpdateTime;
			default:
			GetFixedDigits(seconds, centiSeconds, 2, 6);
			break;
		}
	}

	void GetDate()
	{
		switch(charPoint)
		{
			case 0:
			month = 0;
			year = 0;
			day = Hex2Int();
			break;
			case 1:
			day *= 10;
			day += Hex2Int();
			break;
			case 2:
			month = Hex2Int();
			break;
			case 3:
			month *= 10;
			month += Hex2Int();
			break;
			case 4:
			year = Hex2Int();
			break;
			case 5:
			year *= 10;
			year += Hex2Int();
			
			DateTimeCalc();
			updateFlag |= UpdateDate;
			break;
		}
	}
	
	void DateTimeCalc() //���������� ������� ������ � ������ ������ GPS
	{
		u16 m = month;
		u16 y = year;
		if (m > 2) { m -= 3; }
		else	   { m += 12 - 3; --y;}
		if(y >= 80)
		{
			globalError = Error;
			return;
		}
		//https://ru.stackoverflow.com/questions/455831/%D0%92%D1%8B%D1%87%D0%B8%D1%81%D0%BB%D0%B8%D1%82%D1%8C-%D0%BA%D0%BE%D0%BB%D0%B8%D1%87%D0%B5%D1%81%D1%82%D0%B2%D0%BE-%D0%B4%D0%BD%D0%B5%D0%B9-%D0%B1%D0%B5%D0%B7-%D0%BC%D0%B0%D1%81%D1%81%D0%B8%D0%B2%D0%B0-%D0%B8-%D1%80%D0%B5%D0%BA%D1%83%D1%80%D1%81%D0%B8%D0%B8
		//���������� ���� � January 6, 1980
		u16 gpsTime = day + (153 * m + 2) / 5 + 365 * y + (y >> 2) + 7359;//- y / 100 + y / 400 - 723126;
		gpsWeekNumber = gpsTime / 7;
		gpsTimeFixSec = (gpsTime % 7) * secInDay + gpsUtcOffsetConst;
		gpsTimeOfWeek = gpsTimeFixSec + seconds + float(centiSeconds)/100;
		gpsUtcOffset = gpsUtcOffsetConst;
		if(gpsTimeOfWeek >= 7 * secInDay)
		{
			gpsTimeOfWeek -= 7 * secInDay;
			gpsWeekNumber += 1;
		}
	}
} nmeaDateTime;

struct
{
	s32 latitudeMinutes;
	u8 latitudeDivisor;
	s32 longitudeMinutes;
	u8 longitudeDivisor;
	float latitudeRadians;
	float longitudeRadians;
	float mslAltitudeMeters;
	float haeAltitudeMeters;
	float mslAboveHae;
	
	void GetLatitude()
	{
		GetFixedDigits(latitudeMinutes, latitudeDivisor, 2, 4);
		if(charPoint == 3) updateFlag |= UpdateLatitude;
	}

	void GetLongitude()
	{
		GetFixedDigits(longitudeMinutes,longitudeDivisor, 3, 5);
		if(charPoint == 4) updateFlag |= UpdateLongitude;
	}
	
	void PositionCalc() 
	{
		latitudeRadians = latitudeMinutes * DecimalDivisorF[latitudeDivisor];
		longitudeRadians = longitudeMinutes * DecimalDivisorF[longitudeDivisor];
		haeAltitudeMeters = mslAltitudeMeters + mslAboveHae;
	}
} nmeaPosition;

struct
{
	float speedKnots;
	float courseDegrees;
	float eastVelocityMps;
	float northVelocityMps;
	float upVelocityMps;
	
	const float knots2m = 0.514;
	void VelocityCalc()
	{
		float fi = courseDegrees * M_PI / 180;
		eastVelocityMps = speedKnots * sin(fi) * knots2m;
		northVelocityMps = speedKnots * cos(fi) * knots2m;
		upVelocityMps = 0;
	}
} nmeaVelocity;

struct
{
	float pDop;
	float hDop;
	float vDop;
	float tDop;
} nmeaPrecision;


void floatPush(float &arg)
{
	u8 *f2byte = ((u8*) &arg) +3;
	for(u8 i = 0; i < 4; ++i, --f2byte)
	{
		if(*f2byte == DLE) tsipPush(DLE);
		tsipPush(*f2byte);
	}
}
void int16Push(u16 &arg) 
{
	u8 *i2byte = ((u8*) &arg) +1;
	for(u8 i = 0; i < 2; ++i, --i2byte)
	{
		if(*i2byte == DLE) tsipPush(DLE);
		tsipPush(*i2byte);
	}
}

 public:
 
bool Parse(u8 c)
{
	data = c;
	static union
	{
		u16 msg16;
		u8 msg8[2];
	} msgType;
	
	globalError = Ok;
	checkSum ^= data;
	
	switch(++byteCount)
	{
	case 1: if(data!='$') byteCount=0; checkSum = 0; break;
	case 2: if(data!='G') byteCount=0; updateFlag = 0; break;
	case 3: if(data!='P' && data!='N') byteCount=0; break; //�GP� - GPS, �GN� - �������+GPS
	case 4: if(data!='R' && data!='G') byteCount=0; break;
	case 5: msgType.msg8[0] = data; break;
	case 6: msgType.msg8[1] = data; 
	comaPoint = 0;
	charPoint = 0;
	break;
	case 7: 
	if(data != '*')
	{ 
		byteCount = 6;
		if(data == ',')
		{
			++comaPoint;
			charPoint = 0;
			break;
		}
		switch(msgType.msg16)
		{
			case MSG_ENCODE('M','C'): //RMC - ��������������� ����������� ����� GPS ������
			switch(comaPoint)
			{
				case 1: nmeaDateTime.GetTime(); break; //UTC �����
				case 2: break; //������: �A� � ������ ����������, �V� � ������������.
				case 3: nmeaPosition.GetLatitude(); break; //������
				case 4: if(data == 'S') nmeaPosition.latitudeMinutes *= -1; break; //�N� ��� �������� ��� �S� ��� ����� ������
				case 5: nmeaPosition.GetLongitude(); break; //�������
				case 6: if(data == 'W') nmeaPosition.longitudeMinutes *= -1; break; //�E� ��� ��������� ��� �W� ��� �������� �������
				case 7: GetFloat(nmeaVelocity.speedKnots); updateFlag |= UpdateSpeed; break; //�������� ������������ ����� � �����
				case 8: GetFloat(nmeaVelocity.courseDegrees); updateFlag |= UpdateCourse; break; // ������� ���� (����������� ��������) � �������� �� ������� �� ������
				case 9: nmeaDateTime.GetDate(); break; //����
				case 10: break; //��������� ��������� � ��������
				case 11: break;//��� ��������� ���������� ����� �E� � �������, �W� � ���������
				case 12: break;//��������� ������: �A� � ����������, �D� � ����������������, �E� � �������������, �N� � ������������� ������
			}
			
			break;
			case MSG_ENCODE('G','A'): //GGA - ������ � ��������� ����������� ��������������
			switch(comaPoint)
			{
				case 1: nmeaDateTime.GetTime(); break; //UTC �����
				case 2:	nmeaPosition.GetLatitude(); break; //������
				case 3:	if(data == 'S') nmeaPosition.latitudeMinutes *= -1; break; //�N� ��� �������� ��� �S� ��� ����� ������
				case 4:	nmeaPosition.GetLongitude(); break; //�������
				case 5:	if(data == 'W') nmeaPosition.longitudeMinutes *= -1; break; //�E� ��� ��������� ��� �W� ��� �������� �������
				case 6:	 break; //�������� �������� �������: 0 = No GPS, 1 = GPS, 2 = DGPS
				case 7:	 break; //���������� ������������ ���������
				case 8:	 break; //�������������� ��������
				case 9:	GetFloat(nmeaPosition.mslAltitudeMeters); updateFlag |= UpdateAltitude; break; //������ ��� ������� ����
				case 10: break; //M - �����
				case 11: GetFloat(nmeaPosition.mslAboveHae); break; //������ ������ ���� ��� ����������� WGS 84
				case 12: break; //M - �����
				case 13: break; //�����, ��������� � ������� ��������� ��������� DGPS ��������
				case 14: break; //����������������� ����� ������� ������� DGPS
			}
			break;
			case MSG_ENCODE('S','A'):
			break;
			case MSG_ENCODE('S','V'):
			break;
		}
		++charPoint;
	}
	break;
	case 8:
	checkSum ^= Hex2Int() << 4;
	break;
	case 9:
	checkSum ^= Hex2Int() ^ '*';
	if(checkSum) //������ ����������� �����
	{
		byteCount = 0;
	}
	break;
	default: //�������� TSIP
	if((updateFlag & UpdateDateTime) == UpdateDateTime) //0x41 GPS �����
	{
		tsipPush(DLE);
		tsipPush(0x41);
		floatPush(nmeaDateTime.gpsTimeOfWeek);
		int16Push(nmeaDateTime.gpsWeekNumber);
		floatPush(nmeaDateTime.gpsUtcOffset);
		tsipPush(DLE);
		tsipPush(ETX);
	}
	if((updateFlag & UpdatePosition) == UpdatePosition) //0x4A �������
	{
		nmeaPosition.PositionCalc();
		nmeaDateTime.TimeOfFixCalc();
		tsipPush(DLE);
		tsipPush(0x4A);
		floatPush(nmeaPosition.latitudeRadians);
		floatPush(nmeaPosition.longitudeRadians);
		floatPush(nmeaPosition.haeAltitudeMeters);
		
		floatPush(Zero);
		floatPush(nmeaDateTime.timeOfFix);
		tsipPush(DLE);
		tsipPush(ETX);
	}
	if((updateFlag & UpdateVelocity) == UpdateVelocity) //0x56 ��������
	{
		nmeaVelocity.VelocityCalc();
		nmeaDateTime.TimeOfFixCalc();
		tsipPush(DLE);
		tsipPush(0x56);
		floatPush(nmeaVelocity.eastVelocityMps);
		floatPush(nmeaVelocity.northVelocityMps);
		floatPush(nmeaVelocity.upVelocityMps);
		
		floatPush(Zero);
		floatPush(nmeaDateTime.timeOfFix);
		tsipPush(DLE);
		tsipPush(ETX);
	}
	break;
	}
	if(globalError == Error) byteCount = 0;
	return globalError;
	}
	
	
 };