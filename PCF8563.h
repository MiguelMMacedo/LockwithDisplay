
                     
#define PCF8563_read_address							                             0xA3
#define PCF8563_write_address														 0xA2
                 
#define PCF8563_TIMER   															 0x0F    
#define PCF8563_CONTROL1															 0x00     
#define PCF8563_CONTROL2															 0x01                                                                
#define PCF8563_SECONDS 															 0x02    
#define PCF8563_MINUTES 															 0x03     
#define PCF8563_HOURS   														     0x04    
#define PCF8563_DATE																 0x05     
#define PCF8563_WEEKDAYS															 0x06
#define PCF8563_MONTHS  															 0x07                   
#define PCF8563_YEARS   															 0x08 
#define PCF8563_MINUTE_ALARM														 0x09  
#define PCF8563_HOUR_ALARM  														 0x0A  
#define PCF8563_DAY_ALARM   														 0x0B  
#define PCF8563_WEEKDAY_ALARM														 0x0C   
#define PCF8563_CLKOUT  															 0x0D     
#define PCF8563_TCONTROL															 0x0E 

#define SUN																			 0x00
#define MON																			 0x01
#define TUE																			 0x02
#define WED																			 0x03
#define THR																			 0x04
#define FRI																			 0x05
#define SAT																			 0x06 


unsigned char bcd_to_decimal(unsigned char value);
unsigned char decimal_to_bcd(unsigned char value); 
void PCF8563_init(); 
void PCF8563_start_RTC();
void PCF8563_stop_RTC();
void PCF8563_write(unsigned char address, unsigned char value);
unsigned char PCF8563_read(unsigned char address);  
void read_time();
void read_date();
void write_time();  
void write_date();    

extern unsigned char sec;
extern unsigned char min;
extern unsigned char hr;
extern unsigned char dt;
extern unsigned char wk;
extern unsigned char mt;
extern unsigned char yr;
extern unsigned char al_hr;
extern unsigned char al_min;
extern unsigned char al_state;
