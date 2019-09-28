#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "printf_tools.h"
#include "lcd.h"
#include <avr/eeprom.h>
#include "PCF8563.h"

/* Sets intuitive names to the I/O so the code can be easily understood*/
#define C1 PC0
#define C2 PC1
#define C3 PC2
#define C4 PD5
#define L1 PD6
#define L2 PD7
#define L3 PB4
#define L4 PB5
#define PORTA PC3

#define OPEN 20 //time during which the door is unlock
#define SHORT 10 //time transition from one screen message to another

#define SIGNATURE 122 //eeprom signature

char Teclado();
uint8_t password_check(uint8_t numpass, char cod[], uint8_t enter);
uint8_t master_check(char cod[]);
void password_del(uint8_t numpass, char cod[]);
void password_add(uint8_t numpass, char cod[], uint8_t temp[], uint8_t date[]);
void password_change_code(uint8_t pos, char cod[]);
void password_change_temp(uint8_t pos, uint8_t temp[]);
uint8_t password_change_pos(uint8_t numpass, char cod[]);
uint8_t check_time(uint8_t pos);
uint8_t char_to_int(char a);
uint8_t timed_del(uint8_t npass);
void delete(uint8_t i, uint8_t npass, uint8_t j);
uint8_t check_date(uint8_t date[]);
void print_report(uint8_t nrep);
void reset_eeprom();
void t1_init(void);


#define	baud 57600  // baud rate
// Caution: erroneous result if F_CPU is not correctly defined
#define baudgen ((F_CPU/(16*baud))-1)  //baud divider

void init_USART(void)
{
  UBRR0 = baudgen;
  UCSR0B = (1<<RXEN0)|(1<<TXEN0);
  UCSR0C = (1<<USBS0)|(3<<UCSZ00);
}

#define T1TOP 6250 /*Set interruption time period to 100ms at the frequency at which our hardware works*/

uint8_t timer1=0;
uint16_t timer2=50;


ISR(TIMER1_COMPA_vect)
{
	if(timer1>0) timer1--;
}



int main()
{
	timer1=OPEN;

	if(eeprom_read_byte((void*)1023) != SIGNATURE)
	{
		reset_eeprom();
	}

	char a='z', cod[6], a_ant='z';
	uint16_t state = 0, ndig = 0;
	uint8_t npass = eeprom_read_byte((void *)1020); /*EEPROM ADDRESS 1020 saves number of passwords*/
	uint8_t nrep = eeprom_read_byte((void*)1021); /*EEPROM ADDRESS 1021 saves number of reported events*/
	uint8_t temp[4], ntemp=0, k=0, change=0, nmax=25, ant_min, date[3], ndate=0;

	init_USART();
	sei();
	init_printf_tools();

	t1_init(); /*Initializes TC1*/

	DDRC = DDRC & (~(1<<C1)) & (~(1<<C2))& (~(1<<C3));
	DDRD = DDRD & (~(1<<C4));
	DDRD = DDRD | (1<<L1) | (1<<L2);
	DDRB = DDRB | (1<<L3) | (1<<L4);
	DDRC = DDRC | (1<<PORTA);

	/*Initializes RTC*/
	PCF8563_init();
	PCF8563_start_RTC();

	_delay_ms(500);

	/*Initializes LCD*/
	InitLCD(0);
	_delay_ms(500);
	LCDClear();
	_delay_ms(500);

	while(1)
	{
		/*If the reset button is pressed for more than 2 seconds the eeprom is reseted*/

		a_ant = a; //records previous state of the keyboard
		a = Teclado();// Reads Keypad Inputs

		if(a=='*' && a_ant=='z') //when "*" is pressed on the keyboard the system goes back to the initial screen
		{
			state=0;
			LCDClear();
			ndig=0;
		}
		ant_min=min;

		read_time(); //reads
		read_date();

		if(ant_min != min)
		{
			k = timed_del(npass);
			npass=npass-k;
			eeprom_update_byte((void*)1020, npass);
		}

		switch(state) //nao esquecer de adicionar a condicao do * para voltar ao estado 0
		{
			/* BASIC BEHAVIOUR:
			 * --> Cancel all operations and get back to state = 0
			 # --> ENTER
			 Button A --> Add Password
			 Button B --> Change Password
			 Button C --> Remove Password
			 */

			 /* KEYPAD:
			 Number 0 to 9 --> a=0 to a=9
			 * --> a=13
			 # --> a=14
			 Button A
			 Button B
			 Button C
			 Button D
			 */


			case 0:																
			{
				/* Display: "Password:/" */
				PORTC &= (0<<PORTA);

				LCDWriteStringXY(0, 0, "Password:");
				LCDWriteIntXY(11, 0, hr, 2);
				LCDWriteIntXY(14, 0, min, 2);
				LCDWriteStringXY(13, 0, ":");
				LCDWriteIntXY(8, 1, dt, 2);
				LCDWriteIntXY(11, 1, mt, 2);
				LCDWriteIntXY(14, 1, yr, 2);
				LCDWriteStringXY(10, 1, "/");
				LCDWriteStringXY(13, 1, "/");
				if(a<58 && a>47 && a_ant=='z')
				{
					cod[5-ndig] = a;
					LCDWriteStringXY(ndig, 1, "*");
					ndig++;
				}

				else if(a==35 && a_ant=='z') /*Cardinal pressionado*/
				{
					if(ndig == 4)
					{
						cod[1] = 0;
						cod[0] = 0;
					}
					if(ndig == 5)
					{
						cod[0] = 0;
					}

					if(password_check(npass, cod, 1) && ndig>3 && ndig<7)
					{
						state=1;
						timer1=OPEN;
						LCDClear();
					}

					else
					{
						state=2;
						timer1=SHORT;
						LCDClear();
					}

				}


				else if(a=='a')
				{
					if(npass==nmax)
					{
						state=3;
						LCDClear();
						timer1=SHORT;
					}
					else
					{
						state=100;
						LCDClear();
						cod[0]=0;
						cod[1]=0;
						ndig=0;
					}
				}

				else if(a=='b')
				{
					state=200;
					LCDClear();
					cod[0]=0;
					cod[1]=0;
					ndig=0;
				}

				else if(a=='c')
				{
					state=300;
					LCDClear();
					cod[0]=0; cod[1] = 0;
					ndig=0;
				}
				else if(a=='d')
				{
					state = 404;
					LCDClear();
					ndig=0;
				}

				break;
			}

			case 1:	/* Falta output de abrir porta */
			{
				/* Display: "Correct/Password" */
				LCDWriteStringXY(0,0, "Correct");
				LCDWriteStringXY(0,1, "Password");
				PORTC |= (1<<PORTA);

				if(!timer1)
				{
					if(nrep<50)
					{
						eeprom_update_byte((void*)252+nrep*3, 0);
						eeprom_update_byte((void*)251+nrep*3, min);
						eeprom_update_byte((void*)250+nrep*3, hr);
						nrep++;
						eeprom_update_byte((void*)1021, nrep);
					}
					state=0;
					LCDClear();
					cod[0]=0;
					cod[1]=0;
					ndig=0;
				}

				break;
			}

			case 2:
			{
				/* Display: "Incorret/Password" */
				LCDWriteStringXY(0,0,"Incorrect");
				LCDWriteStringXY(0,1,"Password");
				if(!timer1)
				{
					state=0;
					LCDClear();
					cod[0]=0;
					cod[1]=0;
					ndig=0;
				}
				break;
			}

			case 3:
			{
				/*Display: "Can't add more/Passwords" */
				LCDWriteStringXY(0,0,"Can't add more");
				LCDWriteStringXY(0,1,"Passwords");
				if(!timer1)
				{
					state=0;
				}
				break;
			}


			/* A button behavior */
			case 100:
			{
				/* Display: "Add Password/" */
				LCDWriteStringXY(0,0,"Add Password");
				LCDWriteStringXY(0,1,"Master Password:");

				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					ndig++;
					state=101;
					LCDClear();
				}

				break;
			}

			case 101:
			{
				/* Display: "Master Pass:/" */
				LCDWriteStringXY(0,0,"Master Password:");
				LCDWriteStringXY(0,1, "*");
				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					LCDWriteStringXY(ndig, 1, "*");
					ndig++;
				}

				if(a=='#' && a_ant=='z')
				{
					LCDClear();
					ndig=0;
					if(master_check(cod)){
						state=103;
						timer1=SHORT;
					}
					else
					{
						state=102;
						timer1=SHORT;
					}
				}
				break;
			}

			case 102:
			{
				/* Display: "Wrong Password/" */
				LCDWriteStringXY(0,0,"Incorrect");
				LCDWriteStringXY(0,1,"Password");
				if(!timer1)
				{
					state=0;
					LCDClear();
				}
				break;
			}

			case 103:
			{
				/* Display: "New Password:/*/
				LCDWriteStringXY(0,0,"New Password:");
				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					LCDWriteStringXY(ndig, 1, "*");
					ndig++;
				}

				else if(a == '#' && ndig < 4 && a_ant=='z')
				{
					state=104;
					LCDClear();
					timer1=SHORT;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}

				else if(a=='#' && ndig > 6 && a_ant=='z')
				{
					state=105;
					LCDClear();
					timer1=SHORT;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}

				else if(a=='#' && ndig>3 && ndig<7 && a_ant=='z')
				{

					if(ndig==4)
					{
						cod[0]=0;
						cod[1]=0;
					}
					if(ndig==5)
					{
						cod[0]=0;
					}
					if(password_check(npass, cod, 0))
					{
						state=107;
						ndig=0;
						cod[0]=0;
						cod[1]=0;
						timer1=SHORT;
					}
					else
					{
						state=108;
					}
				}
				break;
			}

			case 104:
			{
				/* Display: "Too short/" */
				LCDWriteStringXY(0,0,"Password Is");
				LCDWriteStringXY(0,1,"Too Short");
				if(!timer1)
				{
					LCDClear();
					state=103;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}
				break;
			}

			case 105:
			{
				/* Display: "Too long/" */
				LCDWriteStringXY(0,0,"Password Is");
				LCDWriteStringXY(0,1,"Too Long");
				if(!timer1)
				{
					LCDClear();
					state=103;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}
				break;
			}

			case 106:
			{
				/* Display: "New Password/Accepted" */
				LCDWriteStringXY(0,0,"New Password");
				LCDWriteStringXY(0,1,"Accepted");
				if(!timer1)
				{
					if(nrep<50)
					{
						eeprom_update_byte((void*)252+nrep*3, 1);
						eeprom_update_byte((void*)251+nrep*3, min);
						eeprom_update_byte((void*)250+nrep*3, hr);
						nrep++;
						eeprom_update_byte((void*)1021, nrep);
					}
					npass=npass+1;
					eeprom_update_byte((void*)1020, npass);
					state=0;
					LCDClear();
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}
				break;
			}

			case 107:
			{
				/*Display: Password already exists*/
				LCDWriteStringXY(0,0,"Password Already");
				LCDWriteStringXY(0,1,"Exists");
				if(!timer1)
				{
					state=103;
					LCDClear();
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}
				break;
			}

			case 108:
			{
				LCDWriteStringXY(0,0,"(0) Full Access");
				LCDWriteStringXY(0,1,"(1) Restrict");
				if(a_ant=='z' && a=='0')
				{
					temp[0]=99;
					temp[1]=99;
					temp[2]=99;
					temp[3]=99;
					date[0]=99;
					date[1]=99;
					date[2]=99;
					password_add(npass, cod, temp, date);
					state=106;
					LCDClear();
					timer1=SHORT;
					ndig=0;
				}
				else if(a_ant=='z' && a=='1')
				{
					state=109;
					LCDClear();
				}

				break;
			}

			case 109:
			{
				LCDWriteStringXY(0,0,"0-Daily  2-Timed");
				LCDWriteStringXY(0,1,"1-Schedule");
				if(a_ant=='z' && a=='0')
				{
					temp[0]=59;
					temp[1]=23;
					temp[2]=88;
					temp[3]=88;
					date[0]=dt;
					date[1]=mt;
					date[2]=yr;
					password_add(npass, cod, temp, date);
					state=106;
					LCDClear();
					timer1=SHORT;
					ndig=0;
				}
				else if(a_ant=='z' && a=='1')
				{
					state = 110;
					temp[3]=0;
					temp[2]=0;
					temp[1]=0;
					temp[0]=0;
					date[0]=99;
					date[1]=99;
					date[2]=99;
					LCDClear();
				}
				else if(a_ant=='z' && a=='2')
				{
					state = 115;
					temp[3]=88;
					temp[2]=88;
					temp[1]=0;
					temp[0]=0;
					LCDClear();
					ndate=0;
				}

				break;
			}

			case 110:
			{
				LCDWriteStringXY(0,0,"Entry Time:");
				LCDWriteStringXY(2,1,":");


				if(a<58 && a>47 && a_ant=='z')
				{
					if(ntemp==0)
					{
						temp[3]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==1)
					{
						temp[3]=char_to_int(a)+temp[3];
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==2)
					{
						temp[2]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==3)
					{
						temp[2]=char_to_int(a)+temp[2];
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}
				}

				if(ntemp==4 && a=='#' && a_ant=='z')
				{
					if(temp[3]>23 || temp[2]>59)
					{
						state=113;
						LCDClear();
						timer1=SHORT;
						ntemp=0;
					}
					else
					{
						state=111;
						LCDClear();
						ntemp=0;
					}
				}

				break;
			}

			case 111:
			{
				LCDWriteStringXY(0,0,"Out Time:");
				LCDWriteStringXY(2,1,":");


				if(a<58 && a>47 && a_ant=='z')
				{
					if(ntemp==0)
					{
						temp[1]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==1)
					{
						temp[1]=char_to_int(a)+temp[1];
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==2)
					{
						temp[0]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==3)
					{
						temp[0]=char_to_int(a)+temp[0];
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}
				}

				if(ntemp==4 && a=='#' && a_ant=='z')
				{
					if(temp[1]>23 || temp[0]>59)
					{
						state=114;
						LCDClear();
						timer1=SHORT;
						ntemp=0;
					}
					else
					{
						state=112;
						LCDClear();
						ntemp=0;
					}
				}

				break;
			}

			case 112:
			{
				password_add(npass, cod, temp, date);
				state=106;
				LCDClear();
				timer1=SHORT;
				ndig=0;
				break;
			}

			case 113:
			{
				/*Display Invalid time*/
				LCDWriteStringXY(0,0,"Invalid Time");
				if(!timer1)
				{
					state=110;
					LCDClear();
				}
				break;
			}

			case 114:
			{
				/*Display Invalid time*/
				LCDWriteStringXY(0,0,"Invalid Time");
				if(!timer1)
				{
					state=111;
					LCDClear();
				}
				break;
			}

			case 115:
			{
				/*Display "Valid Until:*/
				LCDWriteStringXY(0,0,"Valid Until:");
				LCDWriteStringXY(2, 1,"/");
				LCDWriteStringXY(5, 1,"/");

				if(a<58 && a>47 && a_ant=='z')
				{
					if(ndate==0)
					{
						date[0]=char_to_int(a)*10;
						LCDWriteIntXY(ndate, 1, char_to_int(a), 1);
						ndate++;
					}

					else if(ndate==1)
					{
						date[0]=char_to_int(a)+date[0];
						LCDWriteIntXY(ndate, 1, char_to_int(a), 1);
						ndate++;
					}

					else if(ndate==2)
					{
						date[1]=char_to_int(a)*10;
						LCDWriteIntXY(ndate+1, 1, char_to_int(a), 1);
						ndate++;
					}

					else if(ndate==3)
					{
						date[1]=char_to_int(a)+date[1];
						LCDWriteIntXY(ndate+1, 1, char_to_int(a), 1);
						ndate++;
					}

					else if(ndate==4)
					{
						date[2]=char_to_int(a)*10;
						LCDWriteIntXY(ndate+2, 1, char_to_int(a), 1);
						ndate++;
					}

					else if(ndate==5)
					{
						date[2]=char_to_int(a)+date[2];
						LCDWriteIntXY(ndate+2, 1, char_to_int(a), 1);
						ndate++;
					}
				}

				if(ndate==6 && a=='#' && a_ant=='z')
				{
					if(check_date(date))
					{
						state=117;
						LCDClear();
						ndate=0;
						ntemp=0;
					}
					else
					{
						state=116;
						LCDClear();
						timer1=SHORT;
						ndate=0;
					}
				}

				break;
			}

			case 116:
			{
				/*Display: "Invalid Date"*/
				LCDWriteStringXY(0,0,"Invalid Date");
				if(!timer1)
				{
					LCDClear();
					ndate=0;
					state=115;
				}
				break;
			}

			case 117:
			{
				LCDWriteStringXY(0,0,"Valid Until:");
				LCDWriteStringXY(2,1,":");


				if(a<58 && a>47 && a_ant=='z')
				{
					if(ntemp==0)
					{
						temp[1]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==1)
					{
						temp[1]=char_to_int(a)+temp[1];
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==2)
					{
						temp[0]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==3)
					{
						temp[0]=char_to_int(a)+temp[0];
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}
				}

				if(ntemp==4 && a=='#' && a_ant=='z')
				{
					if(temp[1]>23 || temp[0]>59 || (temp[1]==hr && temp[0]<min) || temp[1]<hr)
					{
						state=118;
						LCDClear();
						timer1=SHORT;
						ntemp=0;
					}
					else
					{
						password_add(npass, cod, temp, date);
						state=106;
						LCDClear();
						timer1=SHORT;
						ndig=0;
						ntemp=0;
						ndate=0;
					}
				}

				break;
			}

			case 118:
			{
				LCDWriteStringXY(0,0, "Invalid Time")
				if(!timer1)
				{
					LCDClear();
					state=117;
					ndig=0;
				}
				break;
			}


			/* B button behavior */
			case 200:
			{
				/* Display: "Change Password/" */
				LCDWriteStringXY(0,0,"Change Password");
				LCDWriteStringXY(0,1,"Master Password:");

				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					LCDClear();
					ndig++;
					state=201;
				}

				break;
			}


			case 201:
			{
				/* Display: "Master Pass:/" */
				LCDWriteStringXY(0,0,"Master Password:");
				LCDWriteStringXY(0,1, "*");
				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					LCDWriteStringXY(ndig, 1, "*");
					ndig++;
				}

				if(a=='#' && a_ant=='z')
				{
					LCDClear();
					ndig=0;
					if(master_check(cod)){
						state=203;
						timer1=SHORT;
					}
					else
					{
						state=202;
						timer1=SHORT;
					}
				}
				break;
			}

			case 202:
			{
				/* Display: "Wrong Password" */
				LCDWriteStringXY(0,0,"Incorrect");
				LCDWriteStringXY(0,1,"Password");
				if(!timer1)
				{
					state=0;
					LCDClear();
				}
				break;
			}

			case 203:
			{
				/* Display: "Change Password:" */
				LCDWriteStringXY(0,0,"Old Password:");
				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					LCDWriteStringXY(ndig, 1, "*");
					ndig++;
				}

				else if(a == '#' && ndig < 4 && a_ant=='z')
				{
					state=206;
					LCDClear();
					timer1=SHORT;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}

				else if(a=='#' && ndig > 6 && a_ant=='z')
				{
					state=207;
					LCDClear();
					timer1=SHORT;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}

				else if(a=='#' && ndig>3 && ndig<7 && a_ant=='z')
				{
					if(ndig == 4)
					{
						cod[1] = 0;
						cod[0] = 0;
					}
					if(ndig == 5)
					{
						cod[0] = 0;
					}

					if(master_check(cod))
					{
						state=209;
						LCDClear();
						timer1=SHORT;
						ndig=0;
					}
					else if(password_check(npass,cod, 0))
					{
						change = password_change_pos(npass, cod);
						state=205;
						LCDClear();
						timer1=SHORT;
						ndig=0;
					}
					else if(!password_check(npass,cod, 0))
					{
						state=204;
						LCDClear();
						timer1=SHORT;
						ndig=0;
					}
				}


				break;
			}

			case 204:
			{
				/* Display: "Doesn't exist/" */
				LCDWriteStringXY(0,0,"Password");
				LCDWriteStringXY(0,1,"Doesn't exist");
				if(!timer1)
				{
					state=203;
					LCDClear();
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}
				break;
			}

			case 205:
			{
				/* Display: "New Password:/" */
				LCDWriteStringXY(0,0,"New Password:");
				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					LCDWriteStringXY(ndig, 1, "*");
					ndig++;
				}

				else if(a == '#' && ndig < 4 && a_ant=='z')
				{
					state=206;
					LCDClear();
					timer1=SHORT;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}

				else if(a=='#' && ndig > 6 && a_ant=='z')
				{
					state=207;
					LCDClear();
					timer1=SHORT;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}

				else if(a=='#' && ndig>3 && ndig<7 && a_ant=='z')
				{
					if(ndig==4)
					{
						cod[0]=0;
						cod[1]=0;
					}
					if(ndig==5)
					{
						cod[0]=0;
					}
					password_change_code(change, cod);
					state=208;
					LCDClear();
					timer1=SHORT;
					ndig=0;
				}
				break;
			}

			case 206:
			{
				/* Display: "Minimum 4/characters" */
				LCDWriteStringXY(0,0,"Password Is");
				LCDWriteStringXY(0,1,"Too Short");
				if(!timer1)
				{
					LCDClear();
					state=205;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}
				break;
			}

			case 207:
			{
				/* Display: "Maximum 6/characters" */
				LCDWriteStringXY(0,0,"Password Is");
				LCDWriteStringXY(0,1,"Too Long");
				if(!timer1)
				{
					LCDClear();
					state=205;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}
				break;
			}

			case 208:
			{
				/* Display: "Password changed" */
				LCDWriteStringXY(0,0,"Password");
				LCDWriteStringXY(0,1,"Changed");
				if(!timer1)
				{
					if(eeprom_read_byte((void*)150+4*change) != 99 && eeprom_read_byte((void*)150+4*change) != 88)
					{
						if(nrep<50)
						{
							eeprom_update_byte((void*)252+nrep*3, 2);
							eeprom_update_byte((void*)251+nrep*3, min);
							eeprom_update_byte((void*)250+nrep*3, hr);
							nrep++;
							eeprom_update_byte((void*)1021, nrep);
						}
						state=210;
						LCDClear();
						cod[0] = 0;
						cod[1] = 0;
						ndig=0;
					}
					else
					{
						LCDClear();
						state=0;
						cod[0] = 0;
						cod[1] = 0;
						ndig=0;
					}
				}
				break;
			}

			case 209:
			{
				/*Display: "Change Master?"*/
				LCDWriteStringXY(0,0,"Change Master?");
				LCDWriteStringXY(0,1,"(1)Yes   (0)NO");
				if(a_ant=='z' && a == '1')
				{
					state = 205;
					cod[0]=0;
					cod[1]=0;
					ndig=0;
					LCDClear();
				}
				if(a_ant=='z' && a == '0')
				{
					state=0;
					cod[0]=0;
					cod[1]=0;
					ndig=0;
					LCDClear();
				}
				break;
			}

			case 210:
			{
				/*Display: "Change time?*/
				LCDWriteStringXY(0,0,"Change Time?");
				LCDWriteStringXY(0,1,"(1)Yes   (0)NO");
				if(a_ant=='z' && a == '1')
				{
					state=211;
					LCDClear();
				}
				if(a_ant=='z' && a == '0')
				{
					state=0;
					LCDClear();
				}

				break;
			}

			case 211:
			{
				LCDWriteStringXY(0,0,"New Entry Time:");
				LCDWriteStringXY(2,1,":");

				if(a<58 && a>47 && a_ant=='z')
				{
					if(ntemp==0)
					{
						temp[3]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==1)
					{
						temp[3]=char_to_int(a)+temp[3];
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==2)
					{
						temp[2]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==3)
					{
						temp[2]=char_to_int(a)+temp[2];
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}

				}

				if(ntemp==4 && a=='#' && a_ant=='z')
				{
					if(temp[3]>23 || temp[2]>59)
					{
						state=213;
						LCDClear();
						timer1=SHORT;
						ntemp=0;
					}
					else
					{
						state=212;
						LCDClear();
						ntemp=0;
					}
				}

				break;
			}

			case 212:
			{
				LCDWriteStringXY(0,0,"New Out Time:");
				LCDWriteStringXY(2,1,":");

				if(a<58 && a>47 && a_ant=='z')
				{
					if(ntemp==0)
					{
						temp[1]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==1)
					{
						temp[1]=char_to_int(a)+temp[1];
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;

					}

					else if(ntemp==2)
					{
						temp[0]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==3)
					{
						temp[0]=char_to_int(a)+temp[0];
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}
				}

				if(ntemp==4 && a=='#' && a_ant=='z')
				{
					if(temp[1]>23 || temp[0]>59)
					{
						state=214;
						LCDClear();
						timer1=SHORT;
						ntemp=0;
					}
					else
					{
						state=215;
						LCDClear();
						ntemp=0;
					}
				}
				break;
			}

			case 213:
			{
				/*Display Invalid time*/
				LCDWriteStringXY(0,0,"Invalid Time");
				if(!timer1)
				{
					state=210;
					LCDClear();
				}
				break;
			}

			case 214:
			{
				/*Display Invalid time*/
				LCDWriteStringXY(0,0,"Invalid Time");
				if(!timer1)
				{
					state=211;
					LCDClear();
				}
				break;
			}

			case 215:
			{
				password_change_temp(change, temp);
				state=216;
				LCDClear();
				timer1=SHORT;
				ndig=0;
				break;
			}

			case 216:
			{
				/*Display: "Password and/Time Changed"*/
				LCDWriteStringXY(0,0,"Password and");
				LCDWriteStringXY(0,1,"Time Changed");
				if(!timer1)
				{
					state=0;
					LCDClear();
				}
				break;
			}

			/* C button behavior */
			case 300:
			{
				/* Display: "Remove Password" */
				LCDWriteStringXY(0,0,"Remove Password");
				LCDWriteStringXY(0,1,"Master Password:");

				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					LCDClear();
					ndig++;
					state=301;
				}

				break;
			}

			case 301:
			{
				/* Display: "Master Pass:/" */
				LCDWriteStringXY(0,0,"Master Password:");
				LCDWriteStringXY(0,1, "*");
				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					LCDWriteStringXY(ndig, 1, "*");
					ndig++;
				}


				if(a=='#' && a_ant=='z')
				{
					LCDClear();
					ndig=0;
					if(master_check(cod)){
						state=303;
						timer1=SHORT;
					}
					else
					{
						state=302;
						timer1=SHORT;
					}
				}
				break;
			}

			case 302:
			{
				/* Display: "Wrong Password/" */
				LCDWriteStringXY(0,0,"Incorrect");
				LCDWriteStringXY(0,1,"Password");
				if(!timer1)
				{
					state=0;
					LCDClear();
				}
				break;
			}

			case 303:
			{
				/* Display: "Pass to Remove:/" */
				LCDWriteStringXY(0,0,"Pass to Remove:");
				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					LCDWriteStringXY(ndig, 1, "*");
					ndig++;
				}

				else if(a == '#' && ndig < 4 && a_ant=='z')
				{
					state=304;
					LCDClear();
					timer1=SHORT;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}

				else if(a=='#' && ndig > 6 && a_ant=='z')
				{
					state=304;
					LCDClear();
					timer1=SHORT;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}

				else if(a=='#' && ndig>3 && ndig<7 && a_ant=='z')
				{
					if(ndig==4)
					{
						cod[0]=0;
						cod[1]=0;
					}
					else if(ndig==5)
					{
						cod[0]=0;
					}

					if(password_check(npass, cod, 0) && !master_check(cod))
					{
						password_del(npass, cod);
						npass--;
						eeprom_update_byte((void *)1020, npass);
						state=305;
						LCDClear();
						timer1=SHORT;
						ndig=0;
					}
					else if(!password_check(npass, cod, 0) && !master_check(cod))
					{
						state=304;
						LCDClear();
						timer1=SHORT;
						ndig=0;
					}

					else if(master_check(cod))
					{
						state=306;
						LCDClear();
						timer1=SHORT;
						ndig=0;
					}
				}

				break;
			}

			case 304:
			{
				/* Display: "Password does/not exist" */
				LCDWriteStringXY(0,0,"Password does");
				LCDWriteStringXY(0,1,"Not exist");
				if(!timer1)
				{
					LCDClear();
					state=303;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}
				break;
			}

			case 305:
			{
				/* Display: "Sucessfully Removed" */
				LCDWriteStringXY(0,0,"Successfully");
				LCDWriteStringXY(0,1,"Removed");
				if(!timer1)
				{
					if(nrep<50)
					{
						eeprom_update_byte((void*)252+nrep*3, 3);
						eeprom_update_byte((void*)251+nrep*3, min);
						eeprom_update_byte((void*)250+nrep*3, hr);
						nrep++;
						eeprom_update_byte((void*)1021, nrep);
					}
					LCDClear();
					state=0;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}
				break;
			}

			case 306:
			{
				LCDWriteStringXY(0,0,"Can't Remove");
				LCDWriteStringXY(0,1,"Master Password");
				if(!timer1)
				{
					LCDClear();
					state=0;
					cod[0] = 0;
					cod[1] = 0;
					ndig=0;
				}
				break;
			}

			case 400:
			{
				/*Display: "Print Report/Master Password"*/
				LCDWriteStringXY(0,0,"Print Report");
				LCDWriteStringXY(0,1,"Master Password:");

				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					ndig++;
					state=401;
					LCDClear();
				}

				break;
			}

			case 401:
			{
				/* Display: "Master Pass:/" */
				LCDWriteStringXY(0,0,"Master Password:");
				LCDWriteStringXY(0,1, "*");
				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					LCDWriteStringXY(ndig, 1, "*");
					ndig++;
				}

				if(a=='#' && a_ant=='z')
				{
					LCDClear();
					ndig=0;
					if(master_check(cod)){
						state=403;
						timer1=SHORT;
					}
					else
					{
						state=402;
						timer1=SHORT;
					}
				}
				break;
			}

			case 402:
			{
				/* Display: "Wrong Password/" */
				LCDWriteStringXY(0,0,"Incorrect");
				LCDWriteStringXY(0,1,"Password");
				if(!timer1)
				{
					state=0;
					LCDClear();
				}
				break;
			}

			case 403:
			{
				/* Display Time Report/ Printing..." */
				LCDWriteIntXY(0, 0, hr, 2);
				LCDWriteStringXY(2, 0, ":");
				LCDWriteIntXY(3, 0, min, 2);
				LCDWriteStringXY(5, 0, ":");
				LCDWriteIntXY(6, 0, sec, 2);
				LCDWriteStringXY(10, 0, "Report");
				LCDWriteStringXY(0, 1, "  Printing...");

				print_report(nrep);
				nrep=0;
				eeprom_update_byte((void*)1021, nrep);
				state=0;
				LCDClear();
				break;
			}

			case 404:
			{
				/*Display (0) Print Report/(1)Clear Memory*/
				LCDWriteStringXY(0,0,"(0)Print Report");
				LCDWriteStringXY(0,1,"(1)Settings");
				if(a_ant=='z' && a=='0')
				{
					state = 400;
					LCDClear();
				}

				if(a_ant=='z' && a=='1')
				{
					state=406;
					LCDClear();
					timer1=SHORT;
				}
				break;
			}

			case 405:
			{
				/*Display Memory Clear*/
				LCDWriteStringXY(0,0,"Memory Is Clear");
				if(!timer1)
				{
					reset_eeprom();
					npass=1;
					state=0;
					LCDClear();
				}
				break;
			}

			case 406:
			{
				/*Display: "Settings/Master Password"*/
				LCDWriteStringXY(0,0,"Settings");
				LCDWriteStringXY(0,1,"Master Password:");

				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					ndig++;
					state=407;
					LCDClear();
				}

				break;
			}

			case 407:
			{
				/* Display: "Master Pass:/" */
				LCDWriteStringXY(0,0,"Master Password:");
				LCDWriteStringXY(0,1, "*");

				if(a<58 && a>47 && a_ant=='z'){
					cod[5-ndig] = a;
					LCDWriteStringXY(ndig, 1, "*");
					ndig++;
				}

				if(a=='#' && a_ant=='z')
				{
					LCDClear();
					ndig=0;
					if(master_check(cod)){
						state=408;
						timer1=SHORT;
					}
					else
					{
						state=402;
						timer1=SHORT;
					}

				}
				break;
			}

			case 408:
			{
				/*Display:"(0)Clear Memory/(1)Change Time"*/
				LCDWriteStringXY(0,0,"(0)Clear Memory");
				LCDWriteStringXY(0,1,"(1)Change Time");
				if(a_ant=='z' && a=='0')
				{
					state = 405;
					LCDClear();
					timer1=SHORT;
				}

				if(a_ant=='z' && a=='1')
				{
					state=409;
					LCDClear();
					ntemp=0;
					temp[0]=0;
					temp[1]=0;
				}
				break;
			}

			case 409:
			{
				/*Display:"(0)Clear Memory/(1)Change Time"*/
				LCDWriteStringXY(0,0,"New Time:");
				LCDWriteStringXY(2,1,":");
				if(a<58 && a>47 && a_ant=='z')
				{
					if(ntemp==0)
					{
						temp[0]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==1)
					{
						temp[0]=char_to_int(a)+temp[0];
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==2)
					{
						temp[1]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==3)
					{
						temp[1]=char_to_int(a)+temp[1];
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}

				}

				if(ntemp==4 && a=='#' && a_ant=='z')
				{
					if(hr>23 || min>59)
					{
						state=410;
						LCDClear();
						timer1=SHORT;
						ntemp=0;
						timer1=SHORT;
					}
					else
					{
						sec=0;
						state=411;
						LCDClear();
						ntemp=0;
						date[2]=0;
						date[1]=0;
						date[0]=0;
					}
				}
				break;
			}

			case 410:
			{
				/*Display: "Invalid Time"*/
				LCDWriteStringXY(0,0,"Invalid Time");
				if(!timer1)
				{
					LCDClear();
					state=409;
				}
				break;
			}

			case 411:
			{
				/*Display:"New Date:"*/
				LCDWriteStringXY(0,0,"New Date:");
				LCDWriteStringXY(2,1,"/");
				LCDWriteStringXY(5,1,"/");
				if(a<58 && a>47 && a_ant=='z')
				{
					if(ntemp==0)
					{
						date[0]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==1)
					{
						date[0]=char_to_int(a)+date[0];
						LCDWriteIntXY(ntemp, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==2)
					{
						date[1]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==3)
					{
						date[1]=char_to_int(a)+date[1];
						LCDWriteIntXY(ntemp+1, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==4)
					{
						date[2]=char_to_int(a)*10;
						LCDWriteIntXY(ntemp+2, 1, char_to_int(a), 1);
						ntemp++;
					}

					else if(ntemp==5)
					{
						date[2]=char_to_int(a)+date[2];
						LCDWriteIntXY(ntemp+2, 1, char_to_int(a), 1);
						ntemp++;
					}

				}

				if(ntemp==6 && a=='#' && a_ant=='z')
				{
					hr=temp[0];
					min=temp[1];
					dt=date[0];
					mt=date[1];
					yr=date[2];
					sec=0;
					if(mt==1 || mt==3 || mt==5 || mt==7 || mt==8 || mt==10 || mt==12)
					{
						if(dt>31)
						{
							state=412;
							timer1=SHORT;
						}
						else
						{
							state=413;
							timer1=SHORT;
							write_date();
							write_time();
						}
					}
					else if(mt==2)
					{
						if(dt>28){state=412; timer1=SHORT;}
						else
						{
							state=413;
							timer1=SHORT;
							write_date();
							write_time();
						}
					}
					else if(mt==4 || mt==6 || mt==9 || mt==11)
					{
						if(dt>30){state=412; timer1=SHORT;}
						else
						{
							state=413;
							timer1=SHORT;
							write_date();
							write_time();
						}
					}
					else
					{
						state=412;
						timer1=SHORT;
					}
					LCDClear();
				}

				break;
			}

			case 412:
			{
				/*Display: "Invalid Date"*/
				LCDWriteStringXY(0,0, "Invalid Date");
				if(!timer1)
				{
					LCDClear();
					state=411;
					ntemp=0;
				}
				break;
			}

			case 413:
			{
				/*Display: "Date Changed/With Success"*/
				LCDWriteStringXY(0,0, "Date Changed");
				LCDWriteStringXY(0,2, "With Success");
				if(!timer1)
				{
					LCDClear();
					state=0;
					ntemp=0;
				}
				break;
			}

			default:
			{
				state = 0;
				break;
			}
		}
	}
}


char Teclado()
{
	char b = 'z';

	PORTD |= (1<<L1);
	_delay_us(100);		/*Gives time for the pins value to stabilize*/
	if(PINC & (1<<C1)){b='1'; PORTD &= (0<<L1);}
	else if(PINC & (1<<C2)){b='2'; PORTD &= (0<<L1);}
	else if(PINC & (1<<C3)){b='3'; PORTD &= (0<<L1);}
	else if(PIND & (1<<C4)){b='a'; PORTD &= (0<<L1);}

	else{
		PORTD &= (0<<L1);
		PORTD |= (1<<L2);
		_delay_us(100);
		if(PINC & (1<<C1)){b='4'; PORTD &= (0<<L2);}
		else if(PINC & (1<<C2)){b='5'; PORTD &= (0<<L2);}
		else if(PINC & (1<<C3)){b='6'; PORTD &= (0<<L2);}
		else if(PIND & (1<<C4)){b='b'; PORTD &= (0<<L2);}

		else{
			PORTD &= (0<<L2);
			PORTB |= (1<<L3);
			_delay_us(100);
			if(PINC & (1<<C1)){b='7'; PORTB &= (0<<L3);}
			else if(PINC & (1<<C2)){b='8'; PORTB &= (0<<L3);}
			else if(PINC & (1<<C3)){b='9'; PORTB &= (0<<L3);}
			else if(PIND & (1<<C4)){b='c'; PORTB &= (0<<L3);}

			else{
				PORTB &= (0<<L3);
				PORTB |= (1<<L4);
				_delay_us(100);
				if(PINC & (1<<C1)){b='*'; PORTB &= (0<<L4);}
				else if(PINC & (1<<C2)){b='0'; PORTB &= (0<<L4);}
				else if(PINC & (1<<C3)){b='#'; PORTB &= (0<<L4);}
				else if(PIND & (1<<C4)){b='d'; PORTB &= (0<<L4);}
				else PORTB &= (0<<L4);
				}
			}
		}
	return b;
}

uint8_t password_check(uint8_t numpass, char cod[], uint8_t enter)
{
	uint8_t i=0, j=0;
	while(i<numpass)
	{
		if(eeprom_read_byte((void *)(6*i)) == cod[5] && eeprom_read_byte((void *)(6*i + 1)) == cod[4] && eeprom_read_byte((void *)(6*i + 2)) == cod[3] && eeprom_read_byte((void *)(6*i + 3)) == cod[2] && eeprom_read_byte((void *)(6*i + 4)) == cod[1] && eeprom_read_byte((void *)(6*i + 5)) == cod[0])
		{
			if(enter) j=check_time(i);
			else j=1;
			return j;
		}
		i++;
	}
	return 0;
}

uint8_t master_check(char cod[])
{
	if(eeprom_read_byte((void *)0) == cod[5] && eeprom_read_byte((void *)1) == cod[4] && eeprom_read_byte((void *)2) == cod[3] && eeprom_read_byte((void*)3) == cod[2] && eeprom_read_byte((void *)4) == cod[1] && eeprom_read_byte((void *)5) == cod[0])
	{
		return 1;
	}
	else return 0;
}

void password_del(uint8_t numpass, char cod[])
{
	uint8_t i=1;
	while(i<numpass)
	{
		if(eeprom_read_byte((void *)(6*i)) == cod[5] && eeprom_read_byte((void *)(6*i + 1)) == cod[4] && eeprom_read_byte((void *)(6*i + 2)) == cod[3] && eeprom_read_byte((void *)(6*i + 3)) == cod[2] && eeprom_read_byte((void *)(6*i + 4)) == cod[1] && eeprom_read_byte((void *)(6*i + 5)) == cod[0])
		{
			eeprom_update_byte((void*)(6*i), eeprom_read_byte((void*)(6*(numpass-1))));
			eeprom_update_byte((void*)(6*i+1), eeprom_read_byte((void*)(6*(numpass-1)+1)));
			eeprom_update_byte((void*)(6*i+2), eeprom_read_byte((void*)(6*(numpass-1)+2)));
			eeprom_update_byte((void*)(6*i+3), eeprom_read_byte((void*)(6*(numpass-1)+3)));
			eeprom_update_byte((void*)(6*i+4), eeprom_read_byte((void*)(6*(numpass-1)+4)));
			eeprom_update_byte((void*)(6*i+5), eeprom_read_byte((void*)(6*(numpass-1)+5)));
			eeprom_update_byte((void*)(4*i+150), eeprom_read_byte((void*)(4*(numpass-1)+150)));
			eeprom_update_byte((void*)(4*i+151), eeprom_read_byte((void*)(4*(numpass-1)+151)));
			eeprom_update_byte((void*)(4*i+152), eeprom_read_byte((void*)(4*(numpass-1)+152)));
			eeprom_update_byte((void*)(4*i+153), eeprom_read_byte((void*)(4*(numpass-1)+153)));
			eeprom_update_byte((void*)(400+i*3), eeprom_read_byte((void*)(3*(numpass-1)+400)));
			eeprom_update_byte((void*)(401+i*3), eeprom_read_byte((void*)(3*(numpass-1)+401)));
			eeprom_update_byte((void*)(402+i*3), eeprom_read_byte((void*)(3*(numpass-1)+402)));
			break;
		}
		i++;
	}
	return;
}

void password_add(uint8_t numpass, char cod[], uint8_t temp[], uint8_t date[])
{
	eeprom_update_byte((void*)(6*numpass), cod[5]);
	eeprom_update_byte((void*)(6*numpass+1), cod[4]);
	eeprom_update_byte((void*)(6*numpass+2), cod[3]);
	eeprom_update_byte((void*)(6*numpass+3), cod[2]);
	eeprom_update_byte((void*)(6*numpass+4), cod[1]);
	eeprom_update_byte((void*)(6*numpass+5), cod[0]);
	eeprom_update_byte((void*)(4*numpass+150), temp[3]);
	eeprom_update_byte((void*)(4*numpass+151), temp[2]);
	eeprom_update_byte((void*)(4*numpass+152), temp[1]);
	eeprom_update_byte((void*)(4*numpass+153), temp[0]);
	eeprom_update_byte((void*)(400+3*numpass), date[0]);
	eeprom_update_byte((void*)(401+3*numpass), date[1]);
	eeprom_update_byte((void*)(402+3*numpass), date[2]);
	return;
}

void password_change_code(uint8_t pos, char cod[])
{
	eeprom_update_byte((void*)(6*pos), cod[5]);
	eeprom_update_byte((void*)(6*pos+1), cod[4]);
	eeprom_update_byte((void*)(6*pos+2), cod[3]);
	eeprom_update_byte((void*)(6*pos+3), cod[2]);
	eeprom_update_byte((void*)(6*pos+4), cod[1]);
	eeprom_update_byte((void*)(6*pos+5), cod[0]);
	return;
}

uint8_t password_change_pos(uint8_t numpass, char cod[])
{
	uint8_t k=0;
	while(k<numpass)
	{
		if(eeprom_read_byte((void *)(6*k)) == cod[5] && eeprom_read_byte((void *)(6*k + 1)) == cod[4] && eeprom_read_byte((void *)(6*k + 2)) == cod[3] && eeprom_read_byte((void *)(6*k + 3)) == cod[2] && eeprom_read_byte((void *)(6*k + 4)) == cod[1] && eeprom_read_byte((void *)(6*k + 5)) == cod[0])
		{
			return k;
		}
		k++;
	}
	return 167;		/*Unused memory position to prevent mistakes*/
}

void password_change_temp(uint8_t pos, uint8_t temp[])
{
	eeprom_update_byte((void*)(4*pos+150), temp[3]);
	eeprom_update_byte((void*)(4*pos+151), temp[2]);
	eeprom_update_byte((void*)(4*pos+152), temp[1]);
	eeprom_update_byte((void*)(4*pos+153), temp[0]);
	return;
}

void reset_eeprom()
{
	uint16_t i=6;
	while(i<1023)
	{
		eeprom_update_byte((void*)i, 0xFF);
		i++;
	}
	eeprom_update_byte((void*)0, '0');
	eeprom_update_byte((void*)1, '0');
	eeprom_update_byte((void*)2, '0');
	eeprom_update_byte((void*)3, '0');
	eeprom_update_byte((void*)4, '0');
	eeprom_update_byte((void*)5, '0');
	eeprom_update_byte((void*)150, 99);
	eeprom_update_byte((void*)151, 99);
	eeprom_update_byte((void*)152, 99);
	eeprom_update_byte((void*)153, 99);
	eeprom_update_byte((void*)1020,1);
	eeprom_update_byte((void*)1021,0);
	eeprom_update_byte((void*)1023, SIGNATURE);
}

void t1_init(void)
{
	/*Stop TC1 and clear pending interrupts*/
	TCCR1B=0;
	TIFR1|=(7<<TOV1);

	/*Define mode CTC*/
	TCCR1A=0;
	TCCR1B=(1<<WGM12);

	/*Load BOTTOM and TOP values*/
	TCNT1=0;
	OCR1A=T1TOP;

	/*Enable COMPA interrupt*/
	TIMSK1=(1<<OCIE1A);

	/*Start TC1 with a prescaler of 256*/
	TCCR1B|=4;
}


uint8_t check_time(uint8_t pos)
{
	uint16_t init=0, end=0, current;
	init = eeprom_read_byte((void*)(4*pos+150))*100 + eeprom_read_byte((void*)(4*pos + 151));
	end = eeprom_read_byte((void*)(4*pos+152))*100 + eeprom_read_byte((void*)(4*pos + 153));
	current = hr*100 + min;

	if((init == 9999 && end == 9999) || (init == 8888))
	{
		return 1;
	}

	else if(init >= end)
	{
		if(current >= end || current <= init) return 1;
		else return 0;
	}

	else if(end > init)
	{
		if(current <= end && current >= init) return 1;
		else return 0;
	}
	return 0;
}

uint8_t char_to_int(char a)
{
	if(a=='0') return 0;
	if(a=='1') return 1;
	if(a=='2') return 2;
	if(a=='3') return 3;
	if(a=='4') return 4;
	if(a=='5') return 5;
	if(a=='6') return 6;
	if(a=='7') return 7;
	if(a=='8') return 8;
	if(a=='9') return 9;
	return 0;
}

uint8_t timed_del(uint8_t npass)
{
	uint8_t i=1, j=0, cmin=0, chr=0, cdt=0, cmt=0, cyr=0;
	while(i<=(npass-1-j))
	{
		cdt=eeprom_read_byte((void*)400+3*i);
		cmt=eeprom_read_byte((void*)401+3*i);
		cyr=eeprom_read_byte((void*)402+3*i);
		chr=eeprom_read_byte((void*)152+4*i);
		cmin=eeprom_read_byte((void*)153+4*i);
		if(eeprom_read_byte((void*)150+i*4)==88 && eeprom_read_byte((void*)151+i*4)==88 && i!=(npass-1-j))
		{
			if(cyr<yr)
			{
				delete(i, npass, j);
				j++;
				i--;
			}
			else if(cmt<mt && cyr==yr)
			{
				delete(i, npass, j);
				j++;
				i--;
			}
			else if(cdt<dt && mt==cmt && yr==cyr)
			{
				delete(i, npass, j);
				j++;
				i--;
			}
			else if(chr<hr && cdt==dt && mt==cmt && yr==cyr)
			{
				delete(i, npass, j);
				j++;
				i--;
			}
			else if(cmin<min && chr==hr && cdt==dt && mt==cmt && yr==cyr)
			{
				delete(i, npass, j);
				j++;
				i--;
			}

		}
		else if(eeprom_read_byte((void*)150+i*4)==88 && eeprom_read_byte((void*)151+i*4)==88 && i==(npass-1-j))
		{
			if(cyr<yr)
			{
				j++;
				i--;
			}
			else if(cmt<mt && cyr==yr)
			{
				j++;
				i--;
			}
			else if(cdt<dt && mt==cmt && yr==cyr)
			{
				j++;
				i--;
			}
			else if(chr<hr && cdt==dt && mt==cmt && yr==cyr)
			{
				j++;
				i--;
			}
			else if(cmin<min && chr==hr && cdt==dt && mt==cmt && yr==cyr)
			{
				j++;
				i--;
			}
		}
		i++;
	}

	return j;
}

void delete(uint8_t i, uint8_t npass, uint8_t j)
{
	eeprom_update_byte((void*)(6*i), eeprom_read_byte((void*)(6*(npass-1-j))));
	eeprom_update_byte((void*)(6*i+1), eeprom_read_byte((void*)(6*(npass-1-j)+1)));
	eeprom_update_byte((void*)(6*i+2), eeprom_read_byte((void*)(6*(npass-1-j)+2)));
	eeprom_update_byte((void*)(6*i+3), eeprom_read_byte((void*)(6*(npass-1-j)+3)));
	eeprom_update_byte((void*)(6*i+4), eeprom_read_byte((void*)(6*(npass-1-j)+4)));
	eeprom_update_byte((void*)(6*i+5), eeprom_read_byte((void*)(6*(npass-1-j)+5)));
	eeprom_update_byte((void*)150+i*4, eeprom_read_byte((void*)150+(npass-1-j)*4));
	eeprom_update_byte((void*)151+i*4, eeprom_read_byte((void*)151+(npass-1-j)*4));
	eeprom_update_byte((void*)152+i*4, eeprom_read_byte((void*)152+(npass-1-j)*4));
	eeprom_update_byte((void*)153+i*4, eeprom_read_byte((void*)153+(npass-1-j)*4));
	eeprom_update_byte((void*)(400+i*3), eeprom_read_byte((void*)(3*(npass-1-j)+400)));
	eeprom_update_byte((void*)(401+i*3), eeprom_read_byte((void*)(3*(npass-1-j)+401)));
	eeprom_update_byte((void*)(402+i*3), eeprom_read_byte((void*)(3*(npass-1-j)+402)));
	return;
}

void print_report(uint8_t nrep)
{
	uint8_t i=0;
	read_date();
	printf("System Report -> %d:%d:%d %d/%d/%d\n", hr,min,sec,dt,mt,yr);
	while(i<nrep)
	{

		if(eeprom_read_byte((void*)(252+3*i))==0)
		{
			printf("%02d:%02d -> Entry\n", eeprom_read_byte((void*)(250+i*3)), eeprom_read_byte((void*)(251+i*3)));
		}

		else if(eeprom_read_byte((void*)(252+3*i))==1)
		{
			printf("%02d:%02d -> Password Added\n", eeprom_read_byte((void*)(250+i*3)), eeprom_read_byte((void*)(251+i*3)));
		}

		else if(eeprom_read_byte((void*)(252+3*i))==2)
		{
			printf("%02d:%02d -> Password Changed\n", eeprom_read_byte((void*)(250+i*3)), eeprom_read_byte((void*)(251+i*3)));
		}

		else if(eeprom_read_byte((void*)(252+3*i))==3)
		{
			printf("%02d:%02d -> Password Deleted\n", eeprom_read_byte((void*)(250+i*3)), eeprom_read_byte((void*)(251+i*3)));
		}
		i++;
	}
	if(eeprom_read_byte((void*)1020)==1) printf("There is 1 password in the system.");
	else printf("There are %d passwords in the system.", eeprom_read_byte((void*)1020));

}

uint8_t check_date(uint8_t date[])
{
	if(date[2]<yr) return 0;
	else if(date[1]==1 || date[1]==3 || date[1]==5 || date[1]==7 || date[1]==8 || date[1]==10 || date[1]==12)
	{
		if(date[0]>31 || date[1]<mt || (date[1]==mt && date[0]<dt))return 0;
		else return 1;
	}
	else if(date[1]==4 || date[1]==6 || date[1]==9 || date[1]==11)
	{
		if(date[0]>30 || date[1]<mt || (date[1]==mt && date[0]<dt))return 0;
		else return 1;
	}
	else if(date[1]==2)
	{
		if(date[0]>29 || date[0]<dt) return 0;
		else return 1;
	}
	return 0;
}
