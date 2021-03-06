/*
串口0负责接收磁罗盘输出的数据，串口1负责将系统的数据输出到上位机。

串口0采用中断接收。发送都采用查询模式。


SPI负责接收两个单轴倾角传感器的输出信号。

时序为方式0，即常态为低电平，上升沿读写。

*/

#include "C8051F120.h"
#include <math.h>
#include <intrins.h>			 // _nop_()的头文件

//延时函数
void Delay_us(unsigned int times);//最大65535 uS
void Delay_ms(unsigned int times);//最大65535 mS

//控制信号定义
sbit CS0 = P4^0; 	 //片选0
sbit CS1 = P4^1; 	 //片选1

//SPI通信函数
void SPI_WriteData(unsigned char val);
void SPI_Read();

//命令定义
#define RDAX 0x10 
#define RDAY 0x11

//串口0子程序定义
void UART0_isr ();                     //串口0接收中断服务子程序
void send_char_com0(unsigned char ch0); //串口0发送程序(单字节命令) 

//接收数据定义
#define  INBUF_LEN 20                  //数据帧长度
unsigned char inbuf0[INBUF_LEN];	  //字符串定义
unsigned char count0 ;	        	  //计数值
unsigned char read_flag0;		     //串口0取数标志

//串口1子程序定义
void send_char_com1(unsigned char ch1); //串口1发送程序(单字节) 
void send_string_com1(unsigned char *str1,unsigned int strlen1);	//向串口1发送一个字符串

//发送数据定义
#define  OUTBUF_LEN 10                  //输出字符串的字节数
unsigned char outbuf1[OUTBUF_LEN];		//输出字符串定义，名为字符数组，实为五个无符号整数组成的数组
unsigned char outtemp1[4];	        	//临时存放空间
unsigned char OUTBUF_P;                //当前位置

//测量函数
void Measure_angle();           //测倾角
void Measure_MEMS();           //测磁偏角

//***********************************************
//串口0接收中断函数 
void UART0_isr () interrupt 4  	   //中断号
{									   //中断自动跳转和返回寄存器页
    if(RI0)							  //检测到输入
    {
        RI0 = 0;					  //清标志位
  		inbuf0[count0] = SBUF0;       //读缓存
		count0++;

        if(count0==(INBUF_LEN))
        {						
         read_flag0 = 1;  //如果串口接收的数据达到INBUF_LEN个，就置位取数标志                                 
         }
    }
}
//向串口0发送一个字符 
void send_char_com0(unsigned char ch0)  
{
    char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE

    SFRPAGE   = UART0_PAGE;   //跳转到串口0的寄存器页
    SBUF0 = ch0;
    while(TI0==0);
    TI0 = 0;

	SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}

//***********************************************

//向串口1发送一个字符 
void send_char_com1(unsigned char ch1)  
{
    char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE

    SFRPAGE   = UART1_PAGE;   //跳转到串口1的寄存器页
    SBUF1 = ch1;
    while(TI1==0);
    TI1 = 0;

	SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}

//向串口1发送一个字符串，strlen为该字符串长度(字节的个数) 
void send_string_com1(unsigned char *str1,unsigned int strlen1)
{
	char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE

    unsigned int j=0;

    do 
    {
        send_char_com1(*(str1 + j));
        j++;
    } while(j < strlen1);

	SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}

//***********************************************
// the SPI protocol.  SCK is idle-low, and bits are latched on SCK rising.

//向SPI器件写入单字节数据
void SPI_WriteData(unsigned char val)
{
    char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE

	SFRPAGE   = SPI0_PAGE;
	SPI0DAT = val;
    while(!SPIF);
    SPIF = 0;

	SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}
//从SPI器件读取两个字节数据（因为数据长度是11位）
void SPI_Read()
{
	 char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE

	 unsigned int i;
	 	  
	 SFRPAGE   = SPI0_PAGE;
	 for(i=0;i<2;i++)
	 {
		SPI0DAT  = 0;                       // Dummy write to output serial clock
    	while (!SPIF);                      // Wait for the value to be read
    	SPIF = 0;
    	outtemp1[OUTBUF_P++] = SPI0DAT;		//先存入临时存储区
	 }

	 SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}
//***********************************************

//重置源初始化
void Reset_Sources_Init()
{
	char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE

    WDTCN     = 0xDE;
    WDTCN     = 0xAD;
    SFRPAGE   = LEGACY_PAGE;
    RSTSRC    = 0x02;

	SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}

//计数器初始化
//串口0波特率9600，串口1波特率115200
void Timer_Init()
{
    char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE

    SFRPAGE   = TIMER01_PAGE;
    TCON      = 0x40;
    TMOD      = 0x20;
    CKCON     = 0x10;
    TH1       = 0x96;
    SFRPAGE   = TMR2_PAGE;
    TMR2CN    = 0x04;
    TMR2CF    = 0x08;
    RCAP2L    = 0x60;
    RCAP2H    = 0xFF;

	SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}

//串口初始化
void UART_Init()
{
	char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE

    SFRPAGE   = UART0_PAGE;
    SCON0     = 0x50;
    SSTA0     = 0x05;
    SFRPAGE   = UART1_PAGE;
    SCON1     = 0x50;

	SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}


//SPI初始化，200k
void SPI_Init()
{
	char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE

    SFRPAGE   = SPI0_PAGE;
    SPI0CFG   = 0x40;
    SPI0CN    = 0x0D;
    SPI0CKR   = 0x3C;

	SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}

//交叉开关端口定义
void Port_IO_Init()
{
    // P0.0  -  TX0 (UART0), Push-Pull,  Digital
    // P0.1  -  RX0 (UART0), Open-Drain, Digital
    // P0.2  -  SCK  (SPI0), Push-Pull,  Digital
    // P0.3  -  MISO (SPI0), Open-Drain, Digital
    // P0.4  -  MOSI (SPI0), Push-Pull,  Digital
    // P0.5  -  NSS  (SPI0), Open-Drain, Digital
    // P0.6  -  TX1 (UART1), Push-Pull,  Digital
    // P0.7  -  RX1 (UART1), Open-Drain, Digital

	char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE

    SFRPAGE   = CONFIG_PAGE;	 //总体允许弱上拉
    P0MDOUT   = 0x55;
    P4MDOUT   = 0x03;			 //P0.0和P0.1配置成推挽输出
    XBR0      = 0x06;
    XBR2      = 0x44;

	SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}

//晶振初始化
void Oscillator_Init()			
{
	char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE

    SFRPAGE   = CONFIG_PAGE;
    OSCICN    = 0x83;

	SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}


//器件初始化
void Init_Device(void)
{
    Reset_Sources_Init();
    Timer_Init();
    UART_Init();
    SPI_Init();
    Port_IO_Init();
    Oscillator_Init();
}


//延时函数
/*************************************************************************************
* 函数名称：Delay_us;
*
* 函数功能描述：延时程序(采用外部22M晶振时)，延时时间范围：1~65535us;
*
* 输入参数：times: unsigned int, 延时时间变量；
*
* 返回数据：none；
*
* 注意： 延时时间最大是65535us，不要超过这个值;
**************************************************************************************/
void Delay_us(unsigned int times)
{
    unsigned int i;

	for (i=0; i<times; i++)
	{
		_nop_();
		_nop_();
		_nop_();
		_nop_();
		_nop_();
		_nop_();
		_nop_();

		_nop_();//24.5M晶振时
		_nop_();
	}	     
}

/**************************************************************************************
* 函数名称：Delay_ms;
*
* 函数功能描述：延时程序，延时时间范围：1~65535ms;
*              
* 输入参数：times: unsigned int, 延时时间变量；
*
* 返回数据：none；
*
* 注意： 延时时间最大是65535ms，不要超过这个值;
**************************************************************************************/
void Delay_ms(unsigned int times)
{
    unsigned int i;

	for (i=0; i<times; i++)	
 		Delay_us(1000); 
}

//***********************************************

//测量倾角    
void Measure_angle()
{
    char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE
	unsigned int t0,t1;
	
	SFRPAGE   = CONFIG_PAGE;		   //跳转到端口配置页面
	CS0 = 0;					   //X轴
	SPI_WriteData(RDAX);
	SPI_Read();
	CS0 = 1;
	Delay_ms(1);

	CS0 = 0;
	SPI_WriteData(RDAY);
	SPI_Read();
	CS0 = 1;
	t0 = outtemp1[0]*256 + outtemp1[1];	  //数据整理
	t1 = outtemp1[2]*256 + outtemp1[3];
	t0 = t0 >> 5;
	outbuf1[0] = t0 / 256;
	outbuf1[1] = t0 % 256;//取余数
	t1 = t1 >> 5;
	outbuf1[2] = t1 / 256;
	outbuf1[3] = t1 % 256;//取余数

	OUTBUF_P = 0;             //清0

	CS1 = 0;						//Y轴  
	SPI_WriteData(RDAX);			
	SPI_Read();
	CS1 = 1;                     //拉低后再开始下一次测量
	Delay_ms(1);                 //延时确保稳定

	CS1 = 0;
	SPI_WriteData(RDAY);
	SPI_Read();
	CS1 = 1;
	t0 = outtemp1[0]*256 + outtemp1[1];	  //数据整理
	t1 = outtemp1[2]*256 + outtemp1[3];
	t0 = t0 >> 5;
	outbuf1[4] = t0 / 256;
	outbuf1[5] = t0 % 256;//取余数
	t1 = t1 >> 5;
	outbuf1[6] = t1 / 256;
	outbuf1[7] = t1 % 256;//取余数

	OUTBUF_P = 0;             //清0

	SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}
//测量磁偏角
void Measure_MEMS()
{
    char old_SFRPAGE = SFRPAGE;      // Store current SFRPAGE

	IE = 0x90;		                 //开中断;all pages 

     send_char_com0(0xa2);            //设置为单次输出方式
     Delay_ms(30);     			    //等待20个字节数据接收完成
	 if(read_flag0 == 1)              //如果取数完成
	  {
	   read_flag0 = 0;				 //清取数标志
	   count0 = 0;					 //清计数值
      }
  	 else                           //如果取数没有完成
        read_flag0 = 2;               //取数未完成标志	  
   
	IE = 0x00;	                     //关中断
	outbuf1[8] = inbuf0[16];		 //转存
	outbuf1[9] = inbuf0[17];

	SFRPAGE = old_SFRPAGE;           // restore SFRPAGE
}
//***********************************************
//主函数
//***********************************************

void main()
{ 
   Init_Device();
   read_flag0 = 0;
   count0=0;
	
   Delay_ms(2000);     //等待器件稳定
   while(1)
   { 
    Measure_MEMS();
    if(read_flag0 == 2)              //如果取数未完成
	   continue;					 //进入下次循环
	Measure_angle();
	send_string_com1(outbuf1,OUTBUF_LEN);		 //输出
    send_char_com1(0x0a);
    send_char_com1(0x0d);
    Delay_ms(200);			 
   }								 //end of while
}									 //end of main


