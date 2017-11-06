/* 
 * File:   main.c
 * Author: renat
 *
 * Created on October 16, 2017, 1:32 PM
 */
#include <xc.h>
#include "Definitions.h"
#include "XLCD.H"
#include "adc.h"
#include <stdio.h>
#include "i2c.h"
#include "timers.h"
#include "pwm.h"

union FLOAT
{
 float number;
 unsigned char bytes[3];
};

char eeprom_laddress = 0x00;
char eeprom_haddress = 0x00;
int block_size = 6;                 //3 bytes for temperature, 3 bytes for date
int temp_size = 3;
int time_size = 3;
unsigned char result[6];                        //result

union FLOAT converted_temp;
float max_temp = 18.5f;
float min_temp = 18.0f;
unsigned char msec,sec,min,hour = 0x00;

       
char str_tmp[10];
char time[20];

int counter = 0;


/**
 * Function from Mr. Gontean
 * @param ControlByte
 * @param HighAdd
 * @param LowAdd
 * @param data
 * @return 
 */
unsigned char HDByteWriteI2C( unsigned char ControlByte, unsigned char HighAdd, unsigned char LowAdd, unsigned char data )
{
  IdleI2C();                      // ensure module is idle
  StartI2C();                     // initiate START condition
  while ( SSPCON2bits.SEN );      // wait until start condition is over 
  WriteI2C( ControlByte );        // write 1 byte - R/W bit should be 0
  IdleI2C();                      // ensure module is idle
  WriteI2C( HighAdd );            // write address byte to EEPROM
  IdleI2C();                      // ensure module is idle
  WriteI2C( LowAdd );             // write address byte to EEPROM
  IdleI2C();                      // ensure module is idle
  WriteI2C ( data );              // Write data byte to EEPROM
  IdleI2C();                      // ensure module is idle
  StopI2C();                      // send STOP condition
  while ( SSPCON2bits.PEN );      // wait until stop condition is over 
  while (EEAckPolling(ControlByte));  //Wait for write cycle to complete
  return ( 0 );                   // return with no error
}

/********************************************************************
*     Function Name:    HDByteReadI2C                               *
*     Parameters:       EE memory ControlByte, address, pointer and *
*                       length bytes.                               *
*     Description:      Reads data string from I2C EE memory        *
*                       device. This routine can be used for any I2C*
*                       EE memory device, which only uses 1 byte of *
*                       address data as in the 24LC01B/02B/04B/08B. *
*                                                                   *  
********************************************************************/
unsigned char HDByteReadI2C( unsigned char ControlByte, unsigned char HighAdd, unsigned char LowAdd, unsigned char *data, unsigned char length )
{
  IdleI2C();                      // ensure module is idle
  StartI2C();                     // initiate START condition
  while ( SSPCON2bits.SEN );      // wait until start condition is over 
  WriteI2C( ControlByte );        // write 1 byte 
  IdleI2C();                      // ensure module is idle
  WriteI2C( HighAdd );            // WRITE word address to EEPROM
  IdleI2C();                      // ensure module is idle
  while ( SSPCON2bits.RSEN );     // wait until re-start condition is over 
  WriteI2C( LowAdd );             // WRITE word address to EEPROM
  IdleI2C();                      // ensure module is idle
  RestartI2C();                   // generate I2C bus restart condition
  while ( SSPCON2bits.RSEN );     // wait until re-start condition is over 
  WriteI2C( ControlByte | 0x01 ); // WRITE 1 byte - R/W bit should be 1 for read
  IdleI2C();                      // ensure module is idle
  getsI2C( data, length );       // read in multiple bytes
  NotAckI2C();                    // send not ACK condition
  while ( SSPCON2bits.ACKEN );    // wait until ACK sequence is over 
  StopI2C();                      // send STOP condition
  while ( SSPCON2bits.PEN );      // wait until stop condition is over 
  return ( 0 );                   // return with no error
}

void DelayFor18TCY( void ) //18+ cycles delay
{
    __delay_us(20);   
}

void DelayPORXLCD(void)         // 15ms delay
{
    __delay_ms(15);                 // 15ms delay with 4MHz Clock
}

void DelayXLCD(void)            // 5ms delay
{
    __delay_ms(5);                  // 5ms delay with 4MHz Clock
}
 
void initXLCD()
{
    OpenXLCD( FOUR_BIT & LINES_5X7 );	
    while(BusyXLCD());              // Check if the LCD controller is not busy                              
    WriteCmdXLCD(0x06);            // move cursor right, don?t shift display
    WriteCmdXLCD(0x0C);            // turn display on without cursor    
}

void initADC(void)
{
    unsigned char channel=0x00,adc_config1=0x00,adc_config2=0x00,config3=0x00,portconfig=0x00,i=0;
    TRISAbits.RA0 = 1;
    adc_config1 = ADC_FOSC_4 & ADC_RIGHT_JUST & ADC_4_TAD ;
    adc_config2 = ADC_CH0 & ADC_INT_OFF & ADC_REF_VDD_VSS ;
    portconfig = ADC_1ANA ;
    OpenADC(adc_config1,adc_config2,portconfig);    
}

void initTimer(void)
{    
    // set bits to configure Timer
    T1CON = 0x8F;
    // set timer register half full
    TMR1 = 0x8000;
    TMR1IE = 1;
    // enable global interrupt
    GIE = 1;
    PEIE = 1;
    
}

/**
 * writes size bytes from d into the eeprom. start positions are eeprom_laddress / eeprom_haddress
 * eeprom_l/h address are increased.
 * @param d
 * @param size
 * @return 
 */
int write_one_block(unsigned char* d, int size)
{
    int i = 0;
    // warum & Operator hier?
    while(eeprom_laddress < 0xFF & i < size)
    {
        HDByteWriteI2C(0xA0,eeprom_haddress,eeprom_laddress++,*d);
        i++;d++;
    }
    if( i != size)
    {
        eeprom_laddress = 0;
        if(eeprom_haddress < 0xFF)
        {
            eeprom_haddress++;
            while(i < size){
                HDByteWriteI2C(0xA0,eeprom_haddress,eeprom_laddress++,*d);
                i++;d++;
            }
        }
        else
        {
            //memory full
            return -1;
        }
    }
    return 0;
}

int write_data(unsigned char * temp, unsigned char * date)
{
     return write_one_block(temp,temp_size) + write_one_block(date,time_size);
}
 
 /**
  * Puts the last 6 Bytes from the eeprom into "result" 
  * if there are none returns -1
  * @return 
  */
 int read_data(void)
 {
     int counter = block_size;
     int tla = eeprom_laddress;
     int tha = eeprom_haddress;
     while(tla > 0x00 && counter > 0)
     {
         counter--;
         tla--;
     }
     if(counter > 0)
     {
         tla = 0xFF;
         if(tha > 0)
         {
             tha--;
             while(counter > 0)
             {
                 tla--;
             }
         }
         else
         {
             //already on block 0
             return -1;
         }
     }     
     if(HDByteReadI2C(0xA0,tha,tla,result,block_size) == 0)
         return 0;
     else return -1;
 }
 
 /**
  * read the temperature into converted_temp
  */
 void read_temperature(void)
 {
    ConvertADC();
    while(BusyADC());
    converted_temp.number = ((float)ReadADC() *100/255);
 }
 
 /**
  * print a formatted string from "result"
  */
 void print_data()
 {
    int j = 0;
    union FLOAT funion2;
    for(; j<3; j++)
        funion2.bytes[j] = result[j];

    char temp[3];
    sprintf(temp,"%.2f",funion2.number);
    
    unsigned char time[3];
    for(; j<6; j++)
        time[j-3] = result[j];
    
    initXLCD();
    unsigned char str_temp2[3];
    sprintf(str_temp2,"%s %d:%d:%d",temp, time[0],time[1],time[2]);
    putsXLCD(str_temp2);     
 }
 
 /**
  * prints the value from "result" (temperature and time)
  */
void test_readwrite()
{
    //if test data is needed, uncomment. writes 16.0f and 'abc' into the eeprom.
    /* 
    union FLOAT funion;
    funion.number = 16.0f;
    char test2[3] = {'a','b','c'};    
    write_data(funion.bytes,test2);
    __delay_ms(1000);
    */ 
    initXLCD();
    putsXLCD("SHOW DATA");
    __delay_ms(2000);
    initXLCD();
    
    print_data();
    
    __delay_ms(1000);
    initXLCD();
    putsXLCD("END");
    __delay_ms(1000);  
}

/**
 * print all values from the eeprom
 */
void print_all_data()
{
    int pad_counter = 1;
    int a = 0;
    initXLCD();
    putsXLCD("SHOW ALL DATA");
    __delay_ms(500);
    
    while(1)
    {
        //initXLCD();
        for(;a < pad_counter; a ++)
        {
            if(read_data() == -1){
                initXLCD();
                putsXLCD("END");
                __delay_ms(100);
                return;
            }
        }
        __delay_ms(10);
        print_data();
        __delay_ms(200);
        initXLCD();
        putsXLCD("NEXT");
        __delay_ms(200);
        pad_counter++;
        a = 0;
    }
}
 

void alarm (){
    OpenPWM1(((100000/2.0f)/4)-1);
    SetDCPWM1((100000/2.0f)/2);
    SetOutputPWM1(SINGLE_OUT,PWM_MODE_1);
    __delay_ms(1000);
    ClosePWM1();
    __delay_ms(100);
    return;
}

/*
void interrupt ISR()
{
 if(TMR0IE && TMR0IF)
 {
    counter ++;  
    if(counter >= 1)
    {
        sec++;
        if(sec>=60)
        {
            min++;
            sec=0;
            if(min>=60)
            {
                hour++;
                min=0;
            }
        }
    
        read_temperature();  
        sprintf(time, "%d:%d:%d", hour,min,sec);
        sprintf(str_tmp, "%.2f %s",converted_temp.number, time);        
 
        
        //check every 30 sec for temperature
        if((sec == 30 || sec == 0) && msec == 1)
        {
            if(converted_temp.number > max_temp || converted_temp.number < min_temp){
                alarm();alarm();alarm();
            }
        }


        //save the data every 2 minutes
        if(min % 2 == 0 && sec == 0 && msec == 1)
        {
            unsigned char date[3];
            date[0] = hour;
            date[1] = min;
            date[2] = sec;
            if(write_data(converted_temp.bytes,date) < 0)
            {
                initXLCD();
                putsXLCD("memory full");
                __delay_ms(100);
            }        
            
            //read_data();
            //test_readwrite();
       
        }
        counter = 0;
    }
    putsXLCD(str_tmp);
    initXLCD();
    
    
    TMR0IF = 0;
 }
}

*/
int testmain()
{
       //Setup Timer0
   T0PS0=1; //Prescaler is divide by 256

   T0PS1=1;
   T0PS2=1;

   PSA=0;      //Timer Clock Source is from Prescaler

   T0CS=0;     //Prescaler gets clock from FCPU (5MHz)

   T08BIT=1;   //8 BIT MODE

   TMR0IE=1;   //Enable TIMER0 Interrupt
   PEIE=1;     //Enable Peripheral Interrupt

   GIE=1;      //Enable INTs globally

   TMR0ON=1;      //Now start the timer!
   initXLCD();
  
   
   while(1);
}


void high_priority interrupt TIMER1(void){
    char str_tmp[10];
    char time[20];
            
    if(TMR1IF == 1){
        initXLCD();
        TMR1IF = 0;
        TMR1 = 0x8000;
        sec++;

        read_temperature();  
       
        sprintf(time, "%d:%d:%d", hour,min,sec);
        sprintf(str_tmp, "%.2f %s",converted_temp.number, time);
        putsXLCD(str_tmp);
        //check every 30 sec for temperature
        /*
        if(sec == 30 || sec == 0)
        {
            
            if(converted_temp.number > max_temp || converted_temp.number < min_temp){
                alarm();alarm();alarm();
            }
        }    */
        if(sec==60)
        {
            //save the data every 2 minutes
            /*
            if (min % 2 == 0){
                unsigned char date[3];
                date[0] = hour;
                date[1] = min;
                date[2] = sec;
                if(write_data(converted_temp.bytes,date) < 0)
                {
                    initXLCD();
                    putsXLCD("memory full");
                    __delay_ms(100);
                }

                //testing 

                read_data();
                test_readwrite();
                //print_all_data();
            }
            */
            min++;
            sec=0;
            if(min>=60)
            {
                hour++;
                min=0;
            }
        }


    }
}

int main() 
{
    LATA = 0xFF;
    LATB = 0xFF;
    TRISC = 0xFF;
    SSPADD = 0x27;
    initTimer();        
    initADC();
    initXLCD();
    OpenI2C(MASTER, SLEW_OFF); 
    
    while(1);
    CloseTimer1();
  //  testmain();
                   

    
}
 
