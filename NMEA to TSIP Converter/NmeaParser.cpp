#ifndef NMEA_PARSER_H_
#define NMEA_PARSER_H_
#include "stdafx.h"


enum class ErrorCode:u8
{
	Ok = 0,
	Error,
	CheckSumError,
	NoData
};

class NmeaParser
{
	public: //�������������� ������ �������� � ���
	#define MSG_ENCODE(_p) u16(u16(*(_p)) << 10 ^ u16(*((_p)+1)) << 5 ^ u16(*((_p)+2)))
	
	ErrorCode result; //��������� ������� ������ NMEA
	void(*tsipPushRaw)(u8); //������� �������� ����� TSIP 
	
	static const float clockBiasConst; //�������� Clock Bias ��� ��������� �������
	static const float clockBiasRateConst; //�������� ��������� Clock Bias
	static const float gpsUtcOffsetConst; //�������� ������� GPS ������������ UTC

	const u8 DLE = 0x10; //���� DLE ��������� TSIP
	const u8 ETX = 0x03; //���� ETX ��������� TSIP

	NmeaParser(void(*tsipPushRawA)(u8)) : tsipPushRaw(tsipPushRawA) //������� �������� ����� TSIP 
	{
	}

	u8 Hex2Int(u8 c) //������� ASCII � ����� / ����� / ������ ASCII
	{
		if (isDigit(c))
			return c - '0';
		if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;
		if (c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		result = ErrorCode::Error;
		return 0;
	}

	void GetFloat(float &param) //������� ASCII ������������� ����� � ����� ��������� �������� IEEE 754 / ����� ��������� �������� IEEE 754
	{
		static bool isSign; //���� �������������� �����
		static bool isDot; //���� �����
		static float divisor; //�������� ������� ����� �����
		if (iCharCmd == 0)
		{
			isSign = false;
			isDot = false;
			param = 0;
			divisor = 1.0;
		}
		else if(!(isDigit(dataCmd) || (!isDot && dataCmd == '.')))
		{
			result = ErrorCode::Error;
			return;
		}

		switch (dataCmd)
		{
			case ' ':
			case '+': break;
			case '-': isSign = true; break;
			case '.': isDot = true; break;
			default:
			if (!isDigit(dataCmd))
			{
				result = ErrorCode::Error;
				return;
			}
			float temp = Dec2Int(dataCmd);
			if (isDot)
			{
				divisor *= 10;
				temp /= divisor;
			}
			else
			{
				param *= 10;
			}
			if (isSign) param -= temp;
			else param += temp;
			break;
		}
	}

	enum
	{
		MsgStart,
		MsgID,
		MsgData,
		MsgCSh,
		MsgCSl,
		MsgEnd
	} dataType = MsgStart; //��� ����� ������ NMEA

	u8 iCmd, iCharCmd, dataCmd, checkSum; //����� ������� � ������ / ����� ������� � ������� / ������ ������� / ����������� �����
	u16 msgId; //��� ID ������ NMEA
	char msgName[3]; //ID ������ NMEA

	enum UpdateFlag //����� ���������� ������
	{
		UpdateNone = 0,
		UpdateTime = 1,
		UpdateDate = 2,
		UpdateLatitude = 4,
		UpdateLongitude = 8,
		UpdateAltitude = 16,
		UpdateSpeed = 32,
		UpdateCourse = 64,
		UpdateDateTime = UpdateTime | UpdateDate,
		UpdatePosition = UpdateLatitude | UpdateLongitude | UpdateAltitude,
		UpdateVelocity = UpdateSpeed,
		UpdatePrn = 128,
		UpdateQuality = 256
	} updateFlag;

	void RmcParse() //RMC - ��������������� ����������� ����� GPS ������
	{
		switch (iCmd) //RMC,hhmmss,status,latitude,N,longitude,E,spd,cog,ddmmyy,mv,mvE,mode
		{
			case 1: result |= gpsTime.GetTime(iCharCmd, dataCmd, updateFlag); break; //UTC ����� hhmmss.ss
			case 2: break; //������: �A� � ������ ����������, �V� � ������������.
			case 3: result |= llaPosition.GetLatitude(iCharCmd, dataCmd, updateFlag); break; //������
			case 4: if (dataCmd == 'S') llaPosition.latitudeMinutes *= -1; break; //�N� ��� �������� ��� �S� ��� ����� ������
			case 5: result |= llaPosition.GetLongitude(iCharCmd, dataCmd, updateFlag); break; //�������
			case 6: if (dataCmd == 'W') llaPosition.longitudeMinutes *= -1; break; //�E� ��� ��������� ��� �W� ��� �������� �������
			case 7: GetFloat(enuVelocity.speedKnots); enuVelocity.courseDegrees = 0; updateFlag |= UpdateSpeed; break; //�������� ������������ ����� � �����
			case 8: GetFloat(enuVelocity.courseDegrees); updateFlag |= UpdateCourse; break; // ������� ���� (����������� ��������) � �������� �� ������� �� ������
			case 9: result |= gpsTime.GetDate(iCharCmd, dataCmd, updateFlag); break; //���� ddmmyy
			case 10: break; //��������� ��������� � ��������
			case 11: break; //��� ��������� ���������� ����� �E� � �������, �W� � ���������
			case 12: break; //��������� ������: �A� � ����������, �D� � ����������������, �E� � �������������, �N� � ������������� ������
		}
	}

	void GgaParse() // GGA - ������ � ��������� ����������� ��������������
	{
		switch (iCmd) //GGA,hhmmss.ss,Latitude,N,Longitude,E,FS,NoSV,HDOP,msl,m,Altref,m,DiffAge,DiffStation
		{
			case 1: result |= gpsTime.GetTime(iCharCmd, dataCmd, updateFlag); break; //UTC ����� hhmmss.ss
			case 2:	result |= llaPosition.GetLatitude(iCharCmd, dataCmd, updateFlag); break; //������
			case 3:	if (dataCmd == 'S') llaPosition.latitudeMinutes *= -1; break; //�N� ��� �������� ��� �S� ��� ����� ������
			case 4:  result |= llaPosition.GetLongitude(iCharCmd, dataCmd, updateFlag); break; //�������
			case 5:	if (dataCmd == 'W') llaPosition.longitudeMinutes *= -1; break; //�E� ��� ��������� ��� �W� ��� �������� �������
			case 6: result |= healthReceiver.GetQualityIndicator(dataCmd); updateFlag |= UpdateQuality; break; //�������� �������� �������: 0 = No GPS, 1 = GPS, 2 = DGPS
			case 7: result |= healthReceiver.GetNumberSv(iCharCmd, dataCmd); break; //���������� ������������ ���������
			case 8:	 break; //HDOP
			case 9:	GetFloat(llaPosition.mslAltitudeMeters); llaPosition.mslAboveHae = 0; updateFlag |= UpdateAltitude; break; //������ ��� ������� ����
			case 10: break; //M - �����
			case 11: GetFloat(llaPosition.mslAboveHae); break; //������ ������ ���� ��� ����������� WGS 84
			case 12: break; //M - �����
			case 13: break; //�����, ��������� � ������� ��������� ��������� DGPS ��������
			case 14: break; //����������������� ����� ������� ������� DGPS
		}
	}

	void GsaParse() //GSA - ����� ���������� � ���������.
	{
		switch (iCmd) //GSA,Smode,FS{,sv},PDOP,HDOP,VDOP
		{
			case 1: satelliteView.GetSmode(dataCmd); break; //������ ����� 2D � 3D: A - ��������������, M - ������
			case 2: satelliteView.GetFixStatus(dataCmd);  //�����: 1 = ������ �� ��������, 2 = 2D, 3 = 3D
			satelliteView.numberSv = 0;  updateFlag |= UpdatePrn;
			break;

			case 15: GetFloat(satelliteView.pDop); break;//PDOP
			case 16: GetFloat(satelliteView.hDop); break;//HDOP
			case 17: GetFloat(satelliteView.vDop); break;//VDOP

			//PRN ���� ������������ � �������� ������� ��������� (12 �����)
			default: if (iCmd < 15) result |= satelliteView.GetSvPrn(iCharCmd, dataCmd);
			break;
		}
	}

	void Nmea2Tsip() //���������� ��� �������������� ������ NMEA � TSIP
	{
		if((updateFlag & UpdateDateTime) == UpdateDateTime)
		{
			gpsTime.DateTimeCalc();
		}
		if((updateFlag & UpdatePosition) == UpdatePosition)
		{
			llaPosition.PositionCalc();
			gpsTime.TimeOfFixCalc(llaPosition.timeOfFixSec);
		}
		if ((updateFlag & UpdateVelocity) == UpdateVelocity)
		{
			enuVelocity.VelocityCalc();
			gpsTime.TimeOfFixCalc(enuVelocity.timeOfFixSec);
		}
		if ((updateFlag & UpdatePrn) == UpdatePrn)
		{
			satelliteView.NumberSvCalc();
		}
		if ((updateFlag & UpdateQuality) == UpdateQuality)
		{
			healthReceiver.HealthCalc();
		}
	}

	ErrorCode Parse(u8 c) //������� ������� ������� ������� NMEA
	{
		result = ErrorCode::Ok;
		if (c < 0x20 || c >= 0x7F)
		{
			if (dataType != MsgEnd && dataType != MsgStart)
			{
				result = ErrorCode::Error;
			}
			dataType = MsgStart;
			return result;
		}
		switch (dataType)
		{
			case MsgStart:
			if (c == '$')
			{
				dataType = MsgID;
				iCmd = 0;
				iCharCmd = -1;
				checkSum = 0;
				updateFlag = UpdateNone;
			}
			break;
			case MsgID:
			switch (iCharCmd)
			{
				case 0: if (c != 'G')  dataType = MsgStart; break;
				case 1: if (c != 'P' && c != 'N') dataType = MsgStart; break; //�GP� - GPS, �GN� - �������+GPS
				default:
				if (c < 'A' || c > 'Z')
				{
					dataType = MsgStart;
					break;
				}
				msgName[iCharCmd - 2] = c;
				if (iCharCmd == 4)
				{
					msgId = MSG_ENCODE(msgName);
					dataType = MsgData;
				}
				break;
			}
			checkSum ^= c;
			break;
			case MsgData:
			if (c == '*')
			{
				dataType = MsgCSh;
				break;
			}
			checkSum ^= c;
			dataCmd = c;
			if (c != ',')
			{
				switch (msgId)
				{
					case MSG_ENCODE("RMC"): RmcParse(); break;
					case MSG_ENCODE("GGA"): GgaParse(); break;
					case MSG_ENCODE("GSA"): GsaParse(); break;
					default:;
				}
			}
			break;
			case MsgCSh:
			dataType = MsgCSl;
			checkSum ^= Hex2Int(c) << 4;
			break;
			case MsgCSl:
			dataType = MsgEnd;
			checkSum ^= Hex2Int(c);
			if(checkSum == 0)
			{
				Nmea2Tsip();
			}
			else result = ErrorCode::CheckSumError;
			break;
			case MsgEnd: break;
			default: ;
		}

		++iCharCmd;
		if (c == ',')
		{
			++iCmd;
			iCharCmd = 0;
		}
		if (result != ErrorCode::Ok) dataType = MsgStart;
		return result;
	}

	#pragma pack(push, 1)

	struct SGpsTime //0x41 - GPS Time
	{
		float gpsUtcOffset; //�������� ������� GPS UTC ������
		s16 gpsWeekNumber; //����������� ����� ������� GPS ������
		float gpsTimeOfWeek; //������ � ������� GPS ������
		const u8 size = 4 + 2 + 4; //���������� ���� ����� ����

		SGpsTime()
		{
			gpsTimeOfWeek = 0;
			gpsWeekNumber = 1958;
			gpsUtcOffset = gpsUtcOffsetConst;
			dayOfWeek = 0;
		}

		s32 second; //������ � ������� ���
		u8 centiSecond; //����� ������
		u8 day; //�����
		u8 month; //�����
		u8 year; //���
		u8 dayOfWeek; //���� ������
		const u32 secInDay = u32(24) * 60 * 60; //������ � ������

		void DateTimeCalc() //���������� ������� ������ � ������ ������ GPS
		{
			u16 m = month; //�����
			u16 y = year; //���
			if (m > 2) { m -= 3; }
			else { m += 12 - 3; --y; }
			//https://ru.stackoverflow.com/a/455866
			//���������� ���� � January 6, 1980
			u16 daysPassed = day + (153 * m + 2) / 5 + 365 * y + (y >> 2) + 7359; //- y / 100 + y / 400 - 723126;
			gpsWeekNumber = daysPassed / 7;
			dayOfWeek = daysPassed % 7;
			gpsTimeOfWeek = dayOfWeek*secInDay + second + float(centiSecond) / 100 + gpsUtcOffset;
			if (gpsTimeOfWeek >= 7 * secInDay)
			{
				gpsTimeOfWeek -= 7 * secInDay;
				gpsWeekNumber += 1;
			}
		}

		void DateTimeAdd(u8 sec) //����������� ������ � ������� GPS / ������
		{
			if (sec == 0) return;
			gpsTimeOfWeek += sec;
			if (gpsTimeOfWeek >= 7 * secInDay)
			{
				gpsTimeOfWeek -= 7 * secInDay;
				gpsWeekNumber += 1;
			}
		}

		void TimeOfFixCalc(float &timeOfFix) const //���������� GPS ������� ������� ��������� / ����������� GPS ����� 
		{
			timeOfFix = dayOfWeek*secInDay + second + float(centiSecond) / 100 + gpsUtcOffset;
			if (timeOfFix >= 7 * secInDay)
			{
				timeOfFix -= 7 * secInDay;
			}
		}

		ErrorCode GetTime(u8 iCharCmd, u8 c, UpdateFlag &flag) //hhmmss.ss // ������� ASCII ������� � ������� / ��� ������ / ����� ������� � ������� / ������ ������� / ���� ���������� ������
		{
			if(!(isDigit(c) || (iCharCmd == 6 && c == '.')))
			{
				return ErrorCode::Error;
			}
			switch (iCharCmd)
			{
				case 0:
				second = Dec2Int(c);
				centiSecond = 0;
				break;
				case 5:
				flag |= UpdateTime;
				case 1:
				case 3:
				second *= 10;
				second += Dec2Int(c);
				break;
				case 2:
				case 4:
				second *= 6;
				second += Dec2Int(c);
				break;
				case 7:
				centiSecond = Dec2Int(c) * 10;
				break;
				case 8:
				centiSecond += Dec2Int(c);
				break;
				default: ;
			}
			return ErrorCode::Ok;
		}

		ErrorCode GetDate(u8 iCharCmd, u8 c, UpdateFlag &flag) //ddmmyy // ������� ASCII ���� � ���, ����� � ����� / ��� ������ / ����� ������� � ������� / ������ ������� / ���� ���������� ������
		{
			if (!isDigit(c))
			{
				return ErrorCode::Error;
			}
			switch (iCharCmd)
			{
				case 0:
				month = 0;
				year = 0;
				day = Dec2Int(c);
				break;
				case 1:
				day *= 10;
				day += Dec2Int(c);
				break;
				case 2:
				month = Dec2Int(c);
				break;
				case 3:
				month *= 10;
				month += Dec2Int(c);
				break;
				case 4:
				year = Dec2Int(c);
				break;
				case 5:
				year *= 10;
				year += Dec2Int(c);
				flag |= UpdateDate;
				break;
				default: return ErrorCode::Error;
			}
			return ErrorCode::Ok;
		}

	} gpsTime; //GPS �����

	struct SLlaPosition //0x4A - LLA Position
	{
		float timeOfFixSec; //GPS ����� ������� ���������
		float clockBiasMeters; //Clock Bias
		float haeAltitudeMeters; //������ ��� �����������
		float longitudeRadians; //�������
		float latitudeRadians; //������
		const u8 size = 4 * 5; //���������� ���� ����� ����

		SLlaPosition()
		{
			timeOfFixSec = 0;
			clockBiasMeters = clockBiasConst;
			haeAltitudeMeters = 0;
			longitudeRadians = 0;
			latitudeRadians = 0;
			mslAltitudeMeters = 0;
			mslAboveHae = 0;
		}

		s32 latitudeMinutes;
		u8 latitudeDivisor;
		s32 longitudeMinutes;
		u8 longitudeDivisor;
		float mslAltitudeMeters;
		float mslAboveHae;

		float RDCalc(const u16 divisor) const { return float(M_PI / 180 / 60 / divisor); } //���������� �������� ��� ������ / �������� / ���������� ��������
		const float RadiansDivisor[5] = { RDCalc(1),RDCalc(10),RDCalc(100),RDCalc(1000),RDCalc(10000) }; //������ ��������� ��� ������
		const u8 MaxDivisor = 4; //������������ ������ ��������
		
		void PositionCalc() //������� ����� � ������� � ���������� ������ ��� ������� ����
		{
			latitudeRadians = latitudeMinutes * RadiansDivisor[latitudeDivisor];
			longitudeRadians = longitudeMinutes * RadiansDivisor[longitudeDivisor];
			haeAltitudeMeters = mslAltitudeMeters + mslAboveHae;
			clockBiasMeters += clockBiasRateConst;
		}

		ErrorCode GetLatitude(u8 iCharCmd, u8 c, UpdateFlag &flag) //llmm.mmm // ������� ASCII ������ � ������ / ��� ������ / ����� ������� � ������� / ������ ������� / ���� ���������� ������
		{
			if (!(isDigit(c) || (iCharCmd == 4 && c == '.')))
			{
				return ErrorCode::Error;
			}
			switch (iCharCmd)
			{
				case 0:
				latitudeMinutes = Dec2Int(c);
				latitudeDivisor = 0;
				break;
				case 3:
				flag |= UpdateLatitude;
				case 1:
				latitudeMinutes *= 10;
				latitudeMinutes += Dec2Int(c);
				break;
				case 2:
				latitudeMinutes *= 6;
				latitudeMinutes += Dec2Int(c);
				break;
				case 4:
				break;
				default:
				if (latitudeDivisor < MaxDivisor)
				{
					latitudeMinutes *= 10;
					latitudeMinutes += Dec2Int(c);
					++latitudeDivisor;
				}
				break;
			}
			return ErrorCode::Ok;
		}

		ErrorCode GetLongitude(u8 iCharCmd, u8 c, UpdateFlag &flag) //yyymm.mmm // ������� ASCII ������� � ������ / ��� ������ / ����� ������� � ������� / ������ ������� / ���� ���������� ������
		{
			if (!(isDigit(c) || (iCharCmd == 5 && c == '.')))
			{
				return ErrorCode::Error;
			}
			switch (iCharCmd)
			{
				case 0:
				longitudeMinutes = Dec2Int(c);
				longitudeDivisor = 0;
				break;
				case 4:
				flag |= UpdateLongitude;
				case 1:
				case 2:
				longitudeMinutes *= 10;
				longitudeMinutes += Dec2Int(c);
				break;
				case 3:
				longitudeMinutes *= 6;
				longitudeMinutes += Dec2Int(c);
				break;
				case 5:
				break;
				default:
				if (longitudeDivisor < MaxDivisor)
				{
					longitudeMinutes *= 10;
					longitudeMinutes += Dec2Int(c);
					++longitudeDivisor;
				}
				break;
			}
			return ErrorCode::Ok;
		}

	} llaPosition; //�������

	struct SEnuVelocity //0x56 - ENU Velocity
	{
		float timeOfFixSec; //GPS ����� ������� ���������
		float clockBiasRateMps; //�������� ��������� Clock Bias
		float upVelocityMps; //�������� �����
		float northVelocityMps; //�������� �� �����
		float eastVelocityMps; //�������� �� ������
		const u8 size = 4 * 5; //���������� ���� ����� ����

		SEnuVelocity()
		{
			timeOfFixSec = 0;
			clockBiasRateMps = clockBiasRateConst;
			upVelocityMps = 0;
			northVelocityMps = 0;
			eastVelocityMps = 0;
		}

		float speedKnots; //�������� � �����
		float courseDegrees; //������� ���� � ��������
		const float knots2m = 0.514f; //��������� ����� � ������

		void VelocityCalc() //������� �������� ��������� � ���������� � ��������� ����� � �����
		{
			float fi = courseDegrees * float(M_PI / 180); //������� ���� � ��������
			float speedMps = speedKnots * knots2m; //�������� � ������
			eastVelocityMps = speedMps * sin(fi);
			northVelocityMps = speedMps * cos(fi);
		}

	} enuVelocity; //��������

	struct SSatelliteView //0x6D - All-In-View Satellite Selection
	{
		float tDop; //�������� �������� �� �������
		float vDop; //�������� �������� � ������������ ���������
		float hDop; //�������� �������� � �������������� ���������
		float pDop; //�������� �������� �� ��������������
		u8 dimension; //���������� ������������ ���������, 2D ��� 3D � �������������� ��� ������ �����
		const u8 size = 4 * 4 + 1; //���������� ���� ����� ����
		u8 svPrn[12]; //ID ���������

		SSatelliteView()
		{
			dimension = 3;
			tDop = 1.0;
			vDop = 99.0;
			hDop = 99.0;
			pDop = 99.0;
			numberSv = 0;
		}

		u8 numberSv; //���������� ���������

		void NumberSvCalc() //��������� ���������� ��������� � dimension
		{
			dimension &= 0x0F;
			dimension |= numberSv << 4;
		}

		ErrorCode GetSvPrn(u8 iCharCmd, u8 c) //������� ASCII ID �������� � ����� / ��� ������ / ����� ������� � ������� / ������ �������
		{
			if (!isDigit(c)) return ErrorCode::Error;
			if (iCharCmd == 0)
			{
				++numberSv;
				svPrn[numberSv - 1] = 0;
			}
			svPrn[numberSv - 1] *= 10;
			svPrn[numberSv - 1] += Dec2Int(c);
			return ErrorCode::Ok;
		}

		ErrorCode GetSmode(u8 c) //������ ������ / ��� ������ / ������ �������
		{
			dimension &= 0xF7;
			if (c == 'M') dimension |= _BV(3);
			return ErrorCode::Ok;
		}

		ErrorCode GetFixStatus(u8 c) //������ ������� / ��� ������ / ������ �������
		{
			if (!isDigit(c)) return ErrorCode::Error;
			dimension &= 0xF8;
			dimension |= (c == '3') ? 4 : 3;
			return ErrorCode::Ok;
		}

	} satelliteView; //���������� � ��������� � �������� ��������

	struct SHealthReceiver //0x46 - Health of Receiver
	{
		u8 errorCode; //��� ���������� ������
		u8 statusCode; //��� �������
		const u8 size = 2; //���������� ���� ����� ����

		SHealthReceiver()
		{
			errorCode = 0;
			statusCode = 1;
			qualityIndicator = 0;
		}

		u8 numberSv; //���������� ���������
		u8 qualityIndicator; //��������� ��������

		void HealthCalc() //���������� ���� �������
		{
			if(qualityIndicator == 0)
			{
				statusCode = 1;
			}
			else if(numberSv < 4)
			{
				statusCode = 0x08 + numberSv;
			}
			else
			{
				statusCode = 0;
			}
		}

		ErrorCode GetQualityIndicator(u8 c) //������ ���������� �������� / ��� ������ / ������ �������
		{
			if (!isDigit(c)) return ErrorCode::Error;
			qualityIndicator = Dec2Int(c);
			return ErrorCode::Ok;
		}

		ErrorCode GetNumberSv(u8 iCharCmd, u8 c) //������ ���������� ��������� / ��� ������ / ����� ������� � ������� / ������ �������
		{
			if (!isDigit(c)) return ErrorCode::Error;
			if (iCharCmd == 0) numberSv = 0;
			numberSv *= 10;
			numberSv += Dec2Int(c);
			return ErrorCode::Ok;
		}

	} healthReceiver; //�������� ���������

	#pragma pack(pop)
	
	//_TsipSend
	#pragma region _TsipSend
	
	void TsipPushDle(u8 c) const //�������� ��������� ������ TSIP
	{
		tsipPushRaw(DLE);
		tsipPushRaw(c);
	}
	void TsipPush(u8 c) const //�������� ������ ������ TSIP
	{
		if (c == DLE) tsipPushRaw(DLE);
		tsipPushRaw(c);
	}
	void TsipPushDleEtx() const //�������� ������ ������ TSIP
	{
		tsipPushRaw(DLE);
		tsipPushRaw(ETX);
	}

	void TsipPayload(void *sptr, u8 size) const //�������� ��������� � ������� ������ TSIP / ��������� �� ��������� / ���������� ������������ ����
	{
		u8 *ptr = static_cast<u8*>(sptr); //��������� �� ���������
		ptr += size;
		while(size--)
		{
			--ptr;
			TsipPush(*ptr);
		}
	}

	void PositionAndVelocitySend() //�������� ��������� � ��������
	{
		TsipPushDle(0x4A); //0x4A �������
		TsipPayload(&llaPosition, llaPosition.size);
		TsipPushDleEtx();

		TsipPushDle(0x56); //0x56 ��������
		TsipPayload(&enuVelocity, enuVelocity.size);
		TsipPushDleEtx();
	}

	void GpsTimeSend() //�������� GPS �������
	{
		TsipPushDle(0x41); //0x41 GPS �����
		TsipPayload(&gpsTime, gpsTime.size);
		TsipPushDleEtx();
	}

	void HealthSend() //�������� �������� ��������� � ��������������� �������
	{
		TsipPushDle(0x46); //0x46 �������� ���������
		TsipPayload(&healthReceiver, healthReceiver.size);
		TsipPushDleEtx();

		TsipPushDle(0x4B); //0x4B �������������� ������
		TsipPush(0x5A);
		TsipPush(0x00);
		TsipPush(0x01);
		TsipPushDleEtx();
	}

	void SatelliteViewSend() //�������� �������� ��������, ID ��������� � ������ ��������
	{
		TsipPushDle(0x6D); //0x6D �������� � PRN
		TsipPayload(&satelliteView, satelliteView.size);
		for (u8 i = 0; i < satelliteView.numberSv; i++)
		{
			TsipPush(satelliteView.svPrn[i]); //PRN ���������
		}
		TsipPushDleEtx();

		TsipPushDle(0x82); //0x82 ����� �������� ���������
		TsipPush((healthReceiver.qualityIndicator == 2) ? 3 : 2); //DGPS/GPS
		TsipPushDleEtx();
	}
	
	#pragma endregion _TsipSend
};


#endif /*NMEA_PARSER_H_*/