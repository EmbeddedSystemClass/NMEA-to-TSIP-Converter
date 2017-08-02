/*
 * NEMA to TSIP Converter
 * ATmega328P 16 MHz
 * Created: 28.06.2017 7:30:19
 */ 

#include "stdafx.h"
#include "HardwareUART.cpp"
#include "SoftwareUART.cpp"
#include "NmeaParser.cpp"

//���������
#define GPS_UART_RX_PIN _BV(3) //�������� ������������ RX, � �������� ��������� GPS TX
#define GPS_UART_TX_PIN _BV(4) //�������� ������������ TX, � �������� ��������� GPS RX
#define PPS_PIN _BV(2)	//�������� ����, � �������� ��������� PPS
SoftUart nmeaUart = SoftUart(PIND, GPS_UART_RX_PIN, PORTD, GPS_UART_TX_PIN); //����������� UART
HardUart tsipUart = HardUart(9600, ParityAndStop::Odd1); //���������� UART
RingBuffer<128> nmeaBuffer = RingBuffer<128>(); //��������� ����� ������� NMEA
RingBuffer<64> tsipBuffer = RingBuffer<64>(); //��������� ����� ������� TSIP

//��. �������� WaitAndTransmit
inline void TsipPushRaw(u8 data)
{
	tsipBuffer.Push(data);
}
NmeaParser parser = NmeaParser(&TsipPushRaw); //��������������� ������� NMEA � TSIP

//����� ����� ���������� � ����������� �� �������� ���� PPS
#if PPS_PIN == _BV(3) 
#define PPS_FLAG _BV(INTF1)
#else
#define PPS_FLAG _BV(INTF0)
#endif



volatile u16 parseTime;
static u8 EEMEM eepromResetCount[4];
struct SDebugInfo
{
	u8 powerOnResetCount;
	u8 externalResetCount;
	u8 brownOutResetCount;
	u8 watchdogResetCount;

	u8 parseErrorCount;
	u8 checkSumErrorCount;
	u16 lastErrorMsgId;

	u16 maxParseTime;
	u8 BufferOverflowCount;
	u8 resetFlags;
	u16 nDebugMessage;

	const u8 size = 4+4+3*2;
	SDebugInfo()
	{
		parseErrorCount=0;
		checkSumErrorCount=0;
		lastErrorMsgId=0;
		maxParseTime=0;
		BufferOverflowCount=0;
		nDebugMessage=0;
	}

	void ResetSourceCalc()
	{
		resetFlags = MCUSR;
		MCUSR = 0;

		if(resetFlags & _BV(PORF))
		{
			powerOnResetCount++;
		}
		if(resetFlags & _BV(EXTRF))
		{
			externalResetCount++;
		}
		if(resetFlags & _BV(BORF))
		{
			brownOutResetCount++;
		}
		if(resetFlags & _BV(WDRF))
		{
			watchdogResetCount++;
		}
	}

	void CheckBufferOverflow()
	{
		if(nmeaBuffer.isOverflow)
		{
			BufferOverflowCount += 0x01;
			nmeaBuffer.isOverflow = false;
		}
		if(tsipBuffer.isOverflow)
		{
			BufferOverflowCount += 0x10;
			tsipBuffer.isOverflow = false;
		}
	}

	void CheckParse(ErrorCode returnCode)
	{
		if(returnCode == ErrorCode::Error)
		{
			parseErrorCount++;
			lastErrorMsgId = parser.msgId;
		}
		if(returnCode == ErrorCode::CheckSumError)
		{
			checkSumErrorCount++;
			lastErrorMsgId = parser.msgId;
		}
	}

	void CalcParseTime(u16 curParseTime)
	{
		if(curParseTime > maxParseTime) maxParseTime = curParseTime;
	}
} debugInfo;

//������� ���������� ��������������� ����������
void DebugSend()
{
	debugInfo.CheckBufferOverflow();
	TsipPushRaw(0xAA);
	TsipPushRaw(0xB1);
	for(u8 *ptr = reinterpret_cast<u8*>(&debugInfo); ptr < ptr+debugInfo.size; ++ptr)
	{
		TsipPushRaw(*ptr);
	}
	TsipPushRaw(0xAA);
	TsipPushRaw(0xB0);
	debugInfo.maxParseTime = 0;
}



//ppsTime5ms: �����, � 5�� ����������, �� ������ PPS
//ppsTime1s: ����� �������� PPS
//timer5ms: �����, � 5�� ����������, �� ���������� �������� ������� TSIP
//timerTick: ������� ���������� ������� ��� ������������ 5�� ����������
volatile u8 ppsTime5ms, ppsTime1s, timer5ms;
u8 timerTick; 

//���������� ������� � �������� 9600*3 ��
ISR(TIMER1_CAPT_vect) 
{	
	u8 data; //�������� ����
	if(nmeaUart.RxProcessing(data))
	{
		nmeaBuffer.Push(data);
	}
	if(tsipUart.TxProcessing() && !tsipBuffer.Empty())
	{
		tsipUart.Transmit(tsipBuffer.Pop());
	}

	if(--timerTick == 0)
	{
		timerTick = 144;
		++ppsTime5ms;
		if(++timer5ms == 0) timer5ms = 255;
	}

	if(EIFR & PPS_FLAG) //�������� ����� ����������� ���������� �� PPS
	{
		LED_PORT ^= LED_PIN;
		ppsTime5ms = 0;
		++ppsTime1s;
		EIFR |= PPS_FLAG;
	}

	++parseTime;
}

//ppsTimeOfDtFix: ����� PPS, ����� ��� ������ ����� � ����� � ��������
//time5s: ���������� ������, ��������� � ��������� �������� ������ � GPS ��������
//nPacketSend: ����� ������������� ������ TSIP
//isDtFixOld: ����, ��� ����������� ��������� ppsTimeOfDtFix � �������� ������ � ��������� ������ � ����� � ��������
u8 ppsTimeOfDtFix, time5s, nPacketSend;
bool isDtFixOld;

//������� ��������� �������� �����
void MainLoop()
{
	if(!nmeaBuffer.Empty())
	{
		parseTime = 0;
		debugInfo.CheckParse(parser.Parse(nmeaBuffer.Pop()));
		debugInfo.CalcParseTime(parseTime);
		
		bool isDtFix = (parser.updateFlag & parser.UpdateDateTime) == parser.UpdateDateTime; //���� ������� ����� parser.UpdateDateTime
		if(isDtFix && !isDtFixOld)
		{
			ppsTimeOfDtFix = ppsTime1s;
		}
		isDtFixOld = isDtFix;
	}
	
	//���� ������ > 990�� � ��������� ������� && < 100�� � ������ PPS && �� �� � �������� ������� ������
	if(timer5ms > 990/5 && ppsTime5ms < 100/5 && parser.dataType != parser.MsgData)
	{
		timer5ms = 0;
		++time5s;
		nPacketSend = 1;
	}
	if(nPacketSend > 0 && tsipBuffer.Size() < 64-(4*4*2+1+12+4))
	{
		switch(nPacketSend)
		{
			case 1: parser.PositionSend(); break;
			case 2: parser.VelocitySend(); break;
			case 3: if(time5s < 5) break;
				parser.gpsTime.DateTimeAdd(ppsTime1s - ppsTimeOfDtFix);
				parser.GpsTimeSend();
			break;
			case 4: if(time5s < 5) break;
				time5s = 0;
				parser.HealthSend();
			break;
			case 5: parser.SatelliteViewSend(); break;
			case 6: parser.FixModeSend(); break;
			case 7: if(time5s == 1) DebugSend(); break;
			default: nPacketSend = 0; break;
		}
		++nPacketSend;
	}
}


int main()
{
	//��������� �������� ������� ������ � 1
	clock_prescale_set(clock_div_1); 

	//������������� ������
	PORTB = 0x00; 
	DDRB = LED_PIN;

	PORTC=0x00;
	DDRC=0x00;

	PORTD = GPS_UART_TX_PIN | GPS_UART_RX_PIN | PPS_PIN;
	DDRD = GPS_UART_TX_PIN;
	
	//��������� ������ 0
  TCCR0A=0x00;
  TCCR0B=0x00;
  TCNT0=0x00;
  OCR0A=0x00;
  OCR0B=0x00;

	//������ 1 � ������ CTC (��������� ������� �� ����������)
  TCCR1A=0x00;
  TCCR1B=0x19;
  TCNT1H=0x00;
  TCNT1L=0x00;
	ICR1 = F_CPU / (9600UL * 3) - 1;
  OCR1AH=0x00;
  OCR1AL=0x00;
  OCR1BH=0x00;
  OCR1BL=0x00;

	//��������� ������ 2
  ASSR=0x00;
  TCCR2A=0x00;
  TCCR2B=0x00;
  TCNT2=0x00;
  OCR2A=0x00;
  OCR2B=0x00;

	EICRA=0x0F; //���������� INT0 � INT1 �� ������������ ������
	EIMSK=0x00; //��������� ����� ����������� ����������
	EIFR=0x03; //�������� ����� INTF1 INTF0 
	PCICR=0x00; //��������� ��� PCINT

  // Timer/Counter 0 Interrupt(s) initialization
  TIMSK0=0x00;

  // Timer/Counter 1 Interrupt(s) initialization
  TIMSK1=0x20; //�������� ����� ����������� ���������� �� CTC

  // Timer/Counter 2 Interrupt(s) initialization
  TIMSK2=0x00;

	//��������� ���������� ����������
  ACSR=0x80;
  ADCSRB=0x00;
  DIDR1=0x00;

	//��������� ���
  ADCSRA=0x00;

	//��������� SPI
  SPCR=0x00;

	//��������� TWI
  TWCR=0x00;

	//���������� ������ � ���������� 120��
  wdt_enable(WDTO_120MS);

	//��������� �� EEPROM �������� ���������� ������, �������� ������� ����� � �������� �������
  eeprom_read_block(&debugInfo, eepromResetCount, 4);
  wdt_reset();
  debugInfo.ResetSourceCalc();
  eeprom_write_block(&debugInfo, eepromResetCount, 4);
  wdt_reset();

	//��������� ��������� ���������
  sei();

	//����� � ������� ��������
	static const u8 softwareVersion[15] PROGMEM = {0x10, 0x45, 0x01, 0x10, 0x10, 0x02, 0x02, 0x06, 0x02, 0x19, 0x0C, 0x02, 0x05, 0x10, 0x03};
	for(u8 i=0; i<15; i++)
	{
		TsipPushRaw(pgm_read_byte(&softwareVersion[i]));
	}
	parser.HealthSend();
	wdt_reset();
	set_sleep_mode(SLEEP_MODE_IDLE); //����� ��� idle (� ������ ������ ���������� �� ������ �� ��������)

  while (1)
  {
    MainLoop();
	wdt_reset();
	sleep_enable();
	sleep_cpu(); //�����
	sleep_disable();
  }
  return 0;
}
