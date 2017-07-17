/*
* NmeaParser.cpp
*
* Created: 02.07.2017 20:51:05
*  Author: Kelvin
*/

#ifndef NMEA_PARSER_H_
#define NMEA_PARSER_H_

#include "stdafx.h"

enum class ReturnCode
{
	Ok = 0,
	Error,
	CheckSumError
};


class NmeaParser
{
	public:
	void (*tsipPushRaw)(u8);
	NmeaParser(void (*tsipPushRawA)(u8)):tsipPushRaw(tsipPushRawA)
	{
		nmeaHealth.HealthClear();
	}
	
	private:
	#define MSG_ENCODE(_a,_b) ((_a)|(u16(_b)<<8))
	
	const u8 DLE = 0x10;
	const u8 ETX = 0x03;
	float Zero = 0;
	const u16 DecimalDivisor[5] = {1,10,100,1000,10000};
	const u8 MaxDecimalDivisor = 4;

	ReturnCode parseError; //���� ������ ������ NMEA
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
		parseError = ReturnCode::Error;
		return 0;
	}

	u16 updateFlag; //����� ���������� ������

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
		UpdateVelocity = UpdateSpeed | UpdateCourse,
		UpdateDimension = 128,
		UpdatePrn  = 256,
		UpdatePDop = 512,
		UpdateHDop = 1024,
		UpdateVDop = 2048,
		UpdatePrecision = UpdateDimension | UpdatePDop | UpdateHDop | UpdateVDop,
		UpdateQuality = 4096
	};

	struct
	{
		s32 seconds; //������ � ������� ���
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

		bool DateTimeCalc() //���������� ������� ������ � ������ ������ GPS
		{
			u16 m = month;
			u16 y = year;
			if (m > 2) { m -= 3; }
			else	   { m += 12 - 3; --y;}
			if(y >= 80)
			{
				return true;
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
			return false;
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
		float clockBiasMeters;
		
		static float RDCalc(const u16 divisor)
		{
			return M_PI /180 /60 / divisor;
		}
		const float RadiansDivisor[5] = {RDCalc(1),RDCalc(10),RDCalc(100),RDCalc(1000),RDCalc(10000)};
		void PositionCalc()
		{
			latitudeRadians = latitudeMinutes * RadiansDivisor[latitudeDivisor];
			longitudeRadians = longitudeMinutes * RadiansDivisor[longitudeDivisor];
			haeAltitudeMeters = mslAltitudeMeters + mslAboveHae;
			clockBiasMeters = -16000.0; //!
		}
	} nmeaPosition;

	struct
	{
		float speedKnots;
		float courseDegrees;
		float eastVelocityMps;
		float northVelocityMps;
		float upVelocityMps;
		float clockBiasRateMps;
		
		const float knots2m = 0.514;
		void VelocityCalc()
		{
			float fi = courseDegrees * M_PI / 180;
			float speedMps = speedKnots * knots2m;
			eastVelocityMps = speedMps * sin(fi);
			northVelocityMps = speedMps * cos(fi);
			upVelocityMps = 0;
			clockBiasRateMps = 156.0; //!
		}
	} nmeaVelocity;

	struct
	{
		u8 dimension;
		u8 numberSv;
		u8 svprn[12];// = {0x07, 0x1E, 0x01, 0x0D, 0x0B, 0x1C, 0x0F,};
		float pDop;
		float hDop;
		float vDop;
		float tDop;
		
		void PrecisionCalc()
		{
			dimension &= 0x0F;
			dimension |= numberSv<<4;
			tDop = 1;
		}
	} nmeaPrecision;

	struct 
	{
		u8 qualityIndicator; // 0 = No GPS, 1 = GPS, 2 = DGPS
		u8 nSatellitesUse;
		bool isTimeValid;
		u8 statusCode;
		
		void HealthClear()
		{
			qualityIndicator = 0;
			nSatellitesUse = 0;
			isTimeValid = 0;
		}
		
		void HealthCalc()
		{
			//if(!isTimeValid)
			//{
				//statusCode = 0x01; //Don't have GPS time yet
			//}
			//else 
			if(nSatellitesUse < 4)
			{
				statusCode = 0x08 + nSatellitesUse;
			}
			else statusCode = 0x00;
		}
		
	} nmeaHealth;

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
	
	void GetTime()
	{
		switch(charPoint)
		{
			case 7:
			nmeaDateTime.centiSeconds = Hex2Int()*10;
			break;
			case 8:
			nmeaDateTime.centiSeconds += Hex2Int();
			break;
			case 5:
			updateFlag |= UpdateTime;
			default:
			GetFixedDigits(nmeaDateTime.seconds, nmeaDateTime.centiSeconds, 2, 6);
			break;
		}
	}

	void GetDate()
	{
		switch(charPoint)
		{
			case 0:
			nmeaDateTime.month = 0;
			nmeaDateTime.year = 0;
			nmeaDateTime.day = Hex2Int();
			break;
			case 1:
			nmeaDateTime.day *= 10;
			nmeaDateTime.day += Hex2Int();
			break;
			case 2:
			nmeaDateTime.month = Hex2Int();
			break;
			case 3:
			nmeaDateTime.month *= 10;
			nmeaDateTime.month += Hex2Int();
			break;
			case 4:
			nmeaDateTime.year = Hex2Int();
			break;
			case 5:
			nmeaDateTime.year *= 10;
			nmeaDateTime.year += Hex2Int();
			
			if(nmeaDateTime.DateTimeCalc()) parseError = ReturnCode::Error;
			updateFlag |= UpdateDate;
			break;
		}
	}

	void GetLatitude()
	{
		GetFixedDigits(nmeaPosition.latitudeMinutes, nmeaPosition.latitudeDivisor, 2, 4);
		if(charPoint == 3) updateFlag |= UpdateLatitude;
	}

	void GetLongitude()
	{
		GetFixedDigits(nmeaPosition.longitudeMinutes,nmeaPosition.longitudeDivisor, 3, 5);
		if(charPoint == 4) updateFlag |= UpdateLongitude;
	}
	
	
	void TsipPushDle(u8 c)
	{
		tsipPushRaw(DLE);
		tsipPushRaw(c);
	}
	void TsipPush(u8 c)
	{
		if(c == DLE) tsipPushRaw(DLE);
		tsipPushRaw(c);
	}
	void TsipPushDleEtx()
	{
		tsipPushRaw(DLE);
		tsipPushRaw(ETX);
	}
	

	void FloatPush(float &arg)
	{
		u8 *f2byte = ((u8*) &arg) +3;
		for(u8 i = 0; i < 4; ++i, --f2byte)
		{
			TsipPush(*f2byte);
		}
	}
	void Int16Push(u16 &arg)
	{
		u8 *i2byte = ((u8*) &arg) +1;
		for(u8 i = 0; i < 2; ++i, --i2byte)
		{
			TsipPush(*i2byte);
		}
	}

	public:
	
	void HealthSend()
	{
		nmeaHealth.HealthCalc();
		TsipPushDle(0x46); //0x46 �������� ���������
		TsipPush(nmeaHealth.statusCode);
		TsipPush(0x00);
		TsipPushDleEtx();
		
		TsipPushDle(0x4B); //0x4B �������������� ������
		TsipPush(0x5A);
		TsipPush(0x00);
		TsipPush(0x01);
		TsipPushDleEtx();
		nmeaHealth.HealthClear();
	}
	
	
	ReturnCode Parse(u8 c)
	{
		data = c;
		static union
		{
			u16 msg16;
			u8 msg8[2];
		} msgType;
		
		parseError = ReturnCode::Ok;
		if(byteCount < 6) checkSum ^= data;
		
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
				checkSum ^= data;
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
						case 1: GetTime(); break; //UTC ����� hhmmss.ss
						case 2: nmeaHealth.isTimeValid = data == 'A'; break; //������: �A� � ������ ����������, �V� � ������������.
						case 3: GetLatitude(); break; //������
						case 4: if(data == 'S') nmeaPosition.latitudeMinutes *= -1; break; //�N� ��� �������� ��� �S� ��� ����� ������
						case 5: GetLongitude(); break; //�������
						case 6: if(data == 'W') nmeaPosition.longitudeMinutes *= -1; break; //�E� ��� ��������� ��� �W� ��� �������� �������
						case 7: GetFloat(nmeaVelocity.speedKnots); updateFlag |= UpdateSpeed; break; //�������� ������������ ����� � �����
						case 8: GetFloat(nmeaVelocity.courseDegrees); updateFlag |= UpdateCourse; break; // ������� ���� (����������� ��������) � �������� �� ������� �� ������
						case 9: GetDate(); break; //����  ddmmyy
						case 10: break; //��������� ��������� � ��������
						case 11: break;//��� ��������� ���������� ����� �E� � �������, �W� � ���������
						case 12: break;//��������� ������: �A� � ����������, �D� � ����������������, �E� � �������������, �N� � ������������� ������
					}
					
					break;
					case MSG_ENCODE('G','A'): //GGA - ������ � ��������� ����������� ��������������
					switch(comaPoint)
					{
						case 1: GetTime(); break; //UTC ����� hhmmss.ss
						case 2:	GetLatitude(); break; //������
						case 3:	if(data == 'S') nmeaPosition.latitudeMinutes *= -1; break; //�N� ��� �������� ��� �S� ��� ����� ������
						case 4:	GetLongitude(); break; //�������
						case 5:	if(data == 'W') nmeaPosition.longitudeMinutes *= -1; break; //�E� ��� ��������� ��� �W� ��� �������� �������
						case 6:	nmeaHealth.qualityIndicator = Hex2Int(); updateFlag |= UpdateQuality; break; //�������� �������� �������: 0 = No GPS, 1 = GPS, 2 = DGPS
						case 7:	nmeaHealth.nSatellitesUse = Hex2Int(); break; //���������� ������������ ���������
						case 8:	 break; //HDOP
						case 9:	GetFloat(nmeaPosition.mslAltitudeMeters); nmeaPosition.mslAboveHae = 0; updateFlag |= UpdateAltitude; break; //������ ��� ������� ����
						case 10: break; //M - �����
						case 11: GetFloat(nmeaPosition.mslAboveHae); break; //������ ������ ���� ��� ����������� WGS 84
						case 12: break; //M - �����
						case 13: break; //�����, ��������� � ������� ��������� ��������� DGPS ��������
						case 14: break; //����������������� ����� ������� ������� DGPS
					}
					break;
					case MSG_ENCODE('S','A'): //GSA - ����� ���������� � ���������.
					switch(comaPoint)
					{
						case 1: nmeaPrecision.dimension = (data == 'M')?_BV(3):0; updateFlag |= UpdateDimension;  break;//������ ����� 2D � 3D: A - ��������������, M - ������
						case 2:	nmeaPrecision.dimension += (Hex2Int()==3)?4:3; break;//�����: 1 = ������ �� ��������, 2 = 2D, 3 = 3D
						
						case 15: GetFloat(nmeaPrecision.pDop); updateFlag |= UpdatePDop;	 break;//PDOP
						case 16: GetFloat(nmeaPrecision.hDop); updateFlag |= UpdateHDop;	 break;//HDOP
						case 17: GetFloat(nmeaPrecision.vDop); updateFlag |= UpdateVDop;	 break;//VDOP
						
						case 3: nmeaPrecision.numberSv = 0; updateFlag |= UpdatePrn; //PRN ���� ������������ � �������� ������� ��������� (12 �����)
						default: if(comaPoint < 15)	 
						{
							if(charPoint == 0) nmeaPrecision.svprn[nmeaPrecision.numberSv] = Hex2Int()<<4;
							else nmeaPrecision.svprn[nmeaPrecision.numberSv++] |= Hex2Int();
						}
						break;
					}
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
			checkSum ^= Hex2Int();
			if(checkSum) //������ ����������� �����
			{
				byteCount = 0;
				parseError = ReturnCode::CheckSumError;
			}
			break;
			default: //�������� TSIP
			byteCount = 0;
			
			if((updateFlag & UpdateDateTime) == UpdateDateTime) //0x41 GPS �����
			{
				//������ ���������� � ������� ������ ����
				
				TsipPushDle(0x41);
				FloatPush(nmeaDateTime.gpsTimeOfWeek); //������ � �����������
				Int16Push(nmeaDateTime.gpsWeekNumber); //������ � January 6, 1980
				FloatPush(nmeaDateTime.gpsUtcOffset); //������� GPS � UTC � ��������
				TsipPushDleEtx();
			}
			if((updateFlag & UpdatePosition) == UpdatePosition) //0x4A �������
			{
				nmeaPosition.PositionCalc();
				nmeaDateTime.TimeOfFixCalc();
				
				TsipPushDle(0x4A);
				FloatPush(nmeaPosition.latitudeRadians);  //������ � ��������
				FloatPush(nmeaPosition.longitudeRadians); //������� � ��������
				FloatPush(nmeaPosition.haeAltitudeMeters); //������ ��� ����������� WGS-84 � ������
				
				FloatPush(nmeaPosition.clockBiasMeters); //������ �� �������� �� �������� �����
				FloatPush(nmeaDateTime.timeOfFix); //�����, ����� ��� ������ �����, � �������� � �����������
				TsipPushDleEtx();
			}
			if((updateFlag & UpdateVelocity) == UpdateVelocity) //0x56 ��������
			{
				nmeaVelocity.VelocityCalc();
				nmeaDateTime.TimeOfFixCalc();
				
				TsipPushDle(0x56);
				FloatPush(nmeaVelocity.eastVelocityMps); //�������� �� ������ � �/�
				FloatPush(nmeaVelocity.northVelocityMps); //�������� �� ����� � �/�
				FloatPush(nmeaVelocity.upVelocityMps); //�������� ����� � �/�
				
				FloatPush(nmeaVelocity.clockBiasRateMps); //�������� ��������� Clock Bias
				FloatPush(nmeaDateTime.timeOfFix); //�����, ����� ��� ������ �����, � �������� � �����������
				TsipPushDleEtx();
			}
			if((updateFlag & UpdatePrecision) == UpdatePrecision) //0x6D �������� � PRN
			{
				if(!(updateFlag & UpdatePrn)) nmeaPrecision.numberSv = 0;
				nmeaPrecision.PrecisionCalc();
				
				TsipPushDle(0x6D);
				TsipPush(nmeaPrecision.dimension); //���������� ��������� � ����� ������
				FloatPush(nmeaPrecision.pDop);
				FloatPush(nmeaPrecision.hDop);
				FloatPush(nmeaPrecision.vDop);
				FloatPush(nmeaPrecision.tDop);
				for(u8 i = 0; i < nmeaPrecision.numberSv; i++)
				{ 
					TsipPush(nmeaPrecision.svprn[i]); //PRN ���������
				}
				TsipPushDleEtx();
			}
			if((updateFlag & UpdateQuality) == UpdateQuality) //0x82 ����� �������� ���������
			{
				TsipPushDle(0x82);
				TsipPush((nmeaHealth.qualityIndicator == 2)?3:2); //DGPS/GPS
				TsipPushDleEtx();
			}
			break;
		}
		if(parseError != ReturnCode::Ok) 
		{
			byteCount = 0;
		}
		return parseError;
	}
	
};

#endif /*NMEA_PARSER_H_*/