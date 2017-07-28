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
#define GPS_UART_RX_PIN _BV(4) //����� ������������ RX, � �������� ��������� GPS TX
#define GPS_UART_TX_PIN _BV(5) //����� ������������ TX, � �������� ��������� GPS RX
#define PPS_PIN _BV(3)	//����� ����, � �������� ��������� PPS
SoftUart nmeaUart = SoftUart(PIND, GPS_UART_RX_PIN, PORTD, GPS_UART_TX_PIN); //����������� UART
HardUart tsipUart = HardUart(9600, ParityAndStop::Odd1); //���������� UART
RingBuffer<128> nmeaBuffer = RingBuffer<128>(); //��������� ����� ������� NMEA

inline void TsipPushRaw(u8 data) //�������� ����� TSIP / ������������ ����
{
	tsipUart.WaitAndTransmit(data);
}
NmeaParser parser = NmeaParser(&TsipPushRaw); //��������������� ������� NMEA � TSIP

#if PPS_PIN == _BV(3) //����� ����� ���������� � ����������� �� ������ ����
#define PPS_FLAG _BV(INTF1)
#else
#define PPS_FLAG _BV(INTF0)
#endif

volatile u8 ppsTime5ms, ppsTime1s, timer5ms; //�����, � 5�� ����������, �� ������ PPS / ����� �������� PPS / �����, � 5�� ����������, �� ���������� �������� ������� TSIP
u8 timerTick; //������� ���������� ������� ��� ������������ 5�� ����������

ISR(TIMER1_CAPT_vect) //9600*3 //���������� ������� � �������� 9600*3 ��
{	
	u8 data; //�������� ����
	if(nmeaUart.RxProcessing(data))
	{
		nmeaBuffer.Push(data);
	}

	if(--timerTick == 0) //5.004ms
	{
		timerTick = 144;
		++ppsTime5ms;
		++timer5ms;
		 //995.796ms
	}

	if(EIFR & PPS_FLAG) //PPS detect //�������� ����� ����������� ���������� �� PPS
	{
		LED_PORT ^= LED_PIN;
		ppsTime5ms = 0;
		++ppsTime1s;
		EIFR |= PPS_FLAG;
	}
}

u8 ppsTimeOfDtFix, time5s; //����� PPS, ����� ��� ������ ����� � ����� � �������� / ���������� ������, ��������� � ��������� �������� ������ � GPS ��������
bool isDtFixOld; //���� ��� ����������� ��������� ppsTimeOfDtFix � �������� ������ � ��������� ������ � ����� � ��������

void MainLoop() //�������� ������� ����
{
	if(nmeaBuffer.Size())
	{
		wdt_reset();
		parser.Parse(nmeaBuffer.Pop());
		
		bool isDtFix = (parser.updateFlag & parser.UpdateDateTime) == parser.UpdateDateTime; //���� ����� parser.UpdateDateTime
		if(isDtFix && !isDtFixOld)
		{
			ppsTimeOfDtFix = ppsTime1s;
		}
		isDtFixOld = isDtFix;
	}
	
	//���� ������ > 990�� � ��������� ������� && < 200�� � ������ PPS && �� �� � �������� ������� ������
	if(timer5ms > 990/5 && ppsTime5ms < 200/5 && parser.dataType != parser.MsgData)
	{
		timer5ms = 0;
		wdt_reset();
		parser.PositionAndVelocitySend();
		if(++time5s >= 5)
		{
			time5s = 0;
			wdt_reset();
			parser.gpsTime.DateTimeAdd(ppsTime1s - ppsTimeOfDtFix);
			parser.GpsTimeSend();
			parser.HealthSend();
		}
		wdt_reset();
		parser.SatelliteViewSend();
	}
}


int main()
{
	// Declare your local variables here
	
	//��������� �������� ������� ������ � 1
	// Crystal Oscillator division factor: 1
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
  // Timer/Counter 1 initialization
  // Clock source: System Clock
  // Clock value: 16000,000 kHz
  // Mode: CTC top=ICR1
  // OC1A output: Discon.
  // OC1B output: Discon.
  // Noise Canceler: Off
  // Input Capture on Falling Edge
  // Timer1 Overflow Interrupt: Off
  // Input Capture Interrupt: On
  // Compare A Match Interrupt: Off
  // Compare B Match Interrupt: Off
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

 // External Interrupt(s) initialization
 // INT0 Mode: Rising Edge
 // INT1 Mode: Rising Edge
 // Interrupt on any change on pins PCINT0-7: Off
 // Interrupt on any change on pins PCINT8-14: Off
 // Interrupt on any change on pins PCINT16-23: Off
 EICRA=0x0F; //���������� INT0 � INT1 �� ������������ ������
 EIMSK=0x00; //��������� ����� ����������� ����������
 EIFR=0x03; //�������� ����� INTF1 INTF0 
 PCICR=0x00;

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

	//�������� ���������� ������ � ���������� 120��
  wdt_enable(WDTO_120MS);

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
	if(nmeaBuffer.Empty()) sleep_cpu(); //���� ����� NMEA ������, �����
	sleep_disable();
  }
  return 0;
}
