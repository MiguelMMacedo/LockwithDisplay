#include "PCF8563.h"
#include "i2cmaster.h"


unsigned char sec = 0;
unsigned char min = 0;
unsigned char hr = 0;
unsigned char dt = 0;
unsigned char wk = 0;
unsigned char mt = 0;
unsigned char yr = 0;
unsigned char al_hr = 0;
unsigned char al_min = 0;
unsigned char al_state = 0;



unsigned char bcd_to_decimal(unsigned char value)                
{                                                                                          
	return ((value & 0x0F) + (((value & 0xF0) >> 0x04) * 0x0A));
}                                          
                                                             
        
unsigned char decimal_to_bcd(unsigned char value) 
{                                                            
	return (((value / 0x0A) << 0x04) & 0xF0) | ((value % 0x0A) & 0x0F);   
}                                 


void PCF8563_init()
{   		              
	i2c_init();
    PCF8563_stop_RTC();
}  


void PCF8563_start_RTC() 
{  
	PCF8563_write(PCF8563_CONTROL1, 0x08);
}


void PCF8563_stop_RTC()
{
	PCF8563_write(PCF8563_CONTROL1, 0x28); 
}


void PCF8563_write(unsigned char address, unsigned char value)
{
	i2c_start(PCF8563_write_address);
	i2c_write(address);
    i2c_write(value);
    i2c_stop();

}

                                  
unsigned char PCF8563_read(unsigned char address)
{                 
	unsigned char value = 0;
   	
	i2c_start(PCF8563_write_address);
	i2c_write(address);  
	i2c_start(PCF8563_read_address);
	value = i2c_read(0);
	i2c_stop();    
   	
	return value;
}    


void read_time()
{                                       
	sec = PCF8563_read(PCF8563_SECONDS);       
	sec &= 0x7F;
	sec = bcd_to_decimal(sec);
	                                     
	min = PCF8563_read(PCF8563_MINUTES);        
	min &= 0x7F;
	min = bcd_to_decimal(min);   
   	
	hr = PCF8563_read(PCF8563_HOURS);
	hr &= 0x3F;
	hr = bcd_to_decimal(hr); 
}    


void read_date()
{              
	dt = PCF8563_read(PCF8563_DATE);
	dt &= 0x3F;             
	dt = bcd_to_decimal(dt); 
   	
	wk = PCF8563_read(PCF8563_WEEKDAYS);
	wk &= 0x07;
	wk = bcd_to_decimal(wk);   	
   	
	mt = PCF8563_read(PCF8563_MONTHS);
	mt &= 0x1F;
	mt = bcd_to_decimal(mt);
	                    
	                   
	yr = PCF8563_read(PCF8563_YEARS);
	yr = bcd_to_decimal(yr);
}                                             


void write_time()
{      
	sec = decimal_to_bcd(sec);	  
	PCF8563_write(PCF8563_SECONDS, sec);
   	
	min = decimal_to_bcd(min);   	
	PCF8563_write(PCF8563_MINUTES, min); 
   	
	hr = decimal_to_bcd(hr);   	
	PCF8563_write(PCF8563_HOURS, hr);  
}

                                                                      
void write_date()
{    
	dt = decimal_to_bcd(dt);   	
	PCF8563_write(PCF8563_DATE, dt);
	                   
	wk = decimal_to_bcd(wk);	     
	PCF8563_write(PCF8563_WEEKDAYS, wk); 
   	
	mt = decimal_to_bcd(mt);   	
	PCF8563_write(PCF8563_MONTHS, mt);
   	
	yr = decimal_to_bcd(yr);	                                 
	PCF8563_write(PCF8563_YEARS, yr);  
}
