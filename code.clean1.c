//CMSIS
#include "TM4C123GH6PM.h"						//CMSIS Cortex-M4 Peripheral Access Layer

//TIVAWARE
#include <stdint.h>									//Definição de tipos inteiros, máximos e mínimos
#include <stdbool.h>								//Definição de tipo booleano
//#include "inc/hw_memmap.h"					//Macros defining the memory map of the device (mesmas definições de TM4C123GH6PM.h)
#include "driverlib/fpu.h"					//Prototypes for the floatint point manipulation routines
#include "driverlib/sysctl.h"				//Prototypes for the system control driver
#include "driverlib/rom.h"					//Macros to facilitate calling functions in the ROM
#include "driverlib/pin_map.h"			//Mapping of peripherals to pins for all parts
#include "driverlib/uart.h"					//Defines and Macros for the UART
#include "grlib/grlib.h"						//Prototypes for the low level primitives provided by the graphics library
#include "utils/uartstdio.h"				//Prototypes for the UART console functions
#include "driverlib/gpio.h"					//Defines and Macros for GPIO API
#include "driverlib/timer.h"				//Prototypes for the timer
#include <math.h>
#include <stdlib.h>
//Valvano
#include "tm4c123gh6pmX.h"
#include "BSP.h"										//Foi adaptada para esses projetos

//SD Card
#include "sdcard.h"
#include <stdio.h>

//int main()
//{
//	while(1)
//	{
//	
//	}
//}

//GPIOF	4		3			2		 1	 0
//			SW1	Green	Blue Red SW2

//Máscara de AND para testar um bit
//Exemplo bit 4 (GPIOF SW1)
//Valor     xxxx xxxx
//Máscara   0001 0000
//          --------- AND
//Resultado 000x 0000
//Se Resultado > 0 o bit está setado
//Se Resultado = 0 o bit está resetado
//Máscara de OR/AND para setar/resetar bit
//Exemplo bit 2 (GPIOF Red)
//Liga: 	 GPIOF->DATA = GPIOF->DATA | Red;
//Desliga: GPIOF->DATA = GPIOF->DATA & ~Red;

#define SW1 0x10
#define SW2 0x01
#define Red 0x02
#define Green 0x08
#define Blue 0x04

uint32_t msTicks = 0; //Quant. ms
uint32_t FallingEdges = 0;

uint32_t BitmapStart = 0;

extern const unsigned short eu_2016[];
extern const unsigned short eu_2016_inv[];
extern const unsigned char eu_2016_128x128_inv_16_cores[];
//extern const unsigned short eu_2016_128x128_inv[]; //excede o limite de memória

//Tratamento de interrupção
//void SysTick_Handler(void)  
//{
//	msTicks++;
//	if(!(msTicks%=1000))
//		GPIOF->DATA ^= Red;
//}

//Inicializa o PORTF (leds e chaves)
void GPIOF_Inicializa()
{
	//1 - Ativar o clock (RCGCGPIO, RCGC2 só para compatibilidade)
	SYSCTL->RCGCGPIO = 0x20;
	//2 - Verificar se o port está pronto (PRGPIO)
	while(!(SYSCTL->PRGPIO & 0x20));
	//3 - Destravar o port (LOCK e CR)
	GPIOF->LOCK = 0x4C4F434B;
	GPIOF->CR = 0x1F;
	//4 - Desabilitar função analógica (AMSEL)
	GPIOF->AMSEL = 0x00;
	//5 - Selecionar a função dos pinos (PCTL)
	GPIOF->PCTL = 0x00;
	//6 - Definir o sentido dos pinos (DIR)
	GPIOF->DIR = 0x0E;
	//7 - Desabilitar funções alternativas (AFSEL)
	GPIOF->AFSEL = 0x00;
	//8 - Habilitar pull-ups (PUR)
	GPIOF->PUR = 0x11;
	//9 - Habilitar função digital (DEN)
	GPIOF->DEN = 0x1F;
}

//Inicializa o PF1 (led vermelho)
void LedRed_Inicializa()
{
	//1 - Ativar o clock (RCGCGPIO, RCGC2 só para compatibilidade)
	SYSCTL->RCGCGPIO |= 0x20;
	//2 - Verificar se o port está pronto (PRGPIO)
	while(!(SYSCTL->PRGPIO & 0x20));
	//3 - Destravar o port (LOCK e CR)
	GPIOF->LOCK = 0x4C4F434B;
	GPIOF->CR |= 0x02;
	//4 - Desabilitar função analógica (AMSEL)
	GPIOF->AMSEL &= 0x02;
	//5 - Selecionar a função dos pinos (PCTL)
	GPIOF->PCTL = (GPIOF->PCTL & 0xFFFFFF0F) + 0x00000000;
	//6 - Definir o sentido dos pinos (DIR)
	GPIOF->DIR |= 0x02;
	//7 - Desabilitar funções alternativas (AFSEL)
	GPIOF->AFSEL &= ~0x02;
	//8 - Desabilitar pull-ups (PUR)
	GPIOF->PUR &= ~0x02;
	//9 - Habilitar função digital (DEN)
	GPIOF->DEN |= 0x02;
	//10 - Iniciar desligado
	GPIOF->DATA &= ~0x02;
}

//Initializa UART0 a 115.200 bps (considerando clock de 80 MHz),
//8 bits de dados, sem bit de paridade, um stop bit, FIFOs habilitadas
void UART0_Inicializa_115200bps_80MHz(void)
{
	//CMSIS
	//1 -  Ativar o clock da UART0
  SYSCTL->RCGCUART |= 0x01;
	//2 -  Ativar o clock do PORTA
	SYSCTL->RCGCGPIO |= 0x01;
	//3 - Desabilitar a UART0
  UART0->CTL = 0x00;
	//4 - Definir a parte inteira do baud rate
	UART0->IBRD = 43;					// IBRD = int(80.000.000 / (16 * 115.200)) = int(43,4028)
	//5 - Definir a parte fracionária do baud rate
	UART0->FBRD = 26;      		// FBRD = int(0,4028 * 64 + 0,5) = 26
	//6 - Formato do pacote (8 bits de dados, sem bit de paridade, um stop bit, FIFOs habilitadas)
	UART0->LCRH = 0x70;
	//7 - Habilitar a UART0
	UART0->CTL = 0x301;
	//8 - Habilitar funções alternativas para os pinos PA1-0
	GPIOA->AFSEL |= 0x03;
	//9 - Habilitar função digital nos pinos PA1-0
	GPIOA->DEN |= 0x03;
	//10 - Configurar a função alternativa dos pinos PA1-0 como UART
	GPIOA->PCTL = (GPIOA->PCTL&0xFFFFFF00)+0x00000011;
	//11 - Desabilitar função analógica dos pinos PA1-0
	GPIOA->AMSEL &= ~0x03;
}

//Initializa UART0 a 115.200 bps (considerando clock de 16 MHz),
//8 bits de dados, sem bit de paridade, um stop bit, FIFOs habilitadas
void UART0_Inicializa_115200bps_16MHz(void)
{
	//1 -  Ativar o clock da UART0
  SYSCTL->RCGCUART |= 0x01;
	//2 -  Ativar o clock do PORTA
	SYSCTL->RCGCGPIO |= 0x01;
	//3 - Desabilitar a UART0
  UART0->CTL = 0x00;
	//4 - Definir a parte inteira do baud rate
	UART0->IBRD = 8;					// IBRD = int(16.000.000 / (16 * 115.200)) = int(8,6805)
	//5 - Definir a parte fracionária do baud rate
	UART0->FBRD = 44;      		// FBRD = int(0,6805 * 64 + 0,5) = 44
	//6 - Formato do pacote (8 bits de dados, sem bit de paridade, um stop bit, FIFOs habilitadas)
	UART0->LCRH = 0x70;
	//7 - Habilitar a UART0
	UART0->CTL = 0x301;
	//8 - Habilitar funções alternativas para os pinos PA1-0
	GPIOA->AFSEL |= 0x03;
	//9 - Habilitar função digital nos pinos PA1-0
	GPIOA->DEN |= 0x03;
	//10 - Configurar a função alternativa dos pinos PA1-0 como UART
	GPIOA->PCTL = (GPIOA->PCTL&0xFFFFFF00)+0x00000011;
	//11 - Desabilitar função analógica dos pinos PA1-0
	GPIOA->AMSEL &= ~0x03;
}


//A diretiva #define SYSDIV2 inicializa o PLL com a frequencia desejada
#define SYSDIV2 4
//A frequência do barramento é de 400MHz/(SYSDIV2+1) = 400MHz/(4+1) = 80 MHz

//Configuração de sistema para a receber este clock do PLL
void PLL_Inicializa(void)
{
	//CMSIS
	// 1) Habilitar o divisor fe frequência SYSDIV
	SYSCTL->RCC |= 0x000400000;
  // 2) Configurar o sistema para usar o RCC2 para recursos avançados como
  //    PLL de 400 MHz e divisão do clock por valores não inteiros
  SYSCTL->RCC2 |= 0x80000000;		// USERCC2
  // 3) Bypass do PLL enqunato inicializar
  SYSCTL->RCC2 |= 0x00000800;		// BYPASS2, PLL bypass
  // 4) Selecionar o valor do cristal e a fonto do oscilador
  SYSCTL->RCC &= ~0x000007C0;		// limpar o campo XTAL
  SYSCTL->RCC += 0x00000540;		// configurar para um cristal de 16 MHz
  SYSCTL->RCC2 &= ~0x00000070;	// limpar o campo do oscilador
  SYSCTL->RCC2 += 0x00000000;		// configurar a fonte como main oscillator
  // 5) Ativar o PLL limpando o PWRDN
  SYSCTL->RCC2 &= ~0x00002000;
  // 6) Ajustar o divisor desejado e o LSB do divisor do sistema (bit 22 para 400 MHz de PLL)
  SYSCTL->RCC2 |= 0x40000000;		// usar 400 MHz de PLL
  SYSCTL->RCC2 = (SYSCTL->RCC2 & ~0x1FC00000) // limpar o campo do divisor
                  + (SYSDIV2<<22);      			// configurar para clock de 80 MHz
  // 7) Aguardar o PLL começar a operar fazendo polling no PLLLRIS (PLL Raw Interrupt Status)
  while((SYSCTL->RIS & 0x00000040)==0){};			// Aguargar pelo bit PLLLRIS
  // 8) Habilitar o PLL limpando o BYPASS
  SYSCTL->RCC2 &= ~0x00000800;
	
	//TIVAWARE
	//Ajusta a frequência dos uC
//	ROM_SysCtlClockSet(SYSCTL_SYSDIV_2_5 |	//Divide 200 MHz/2,5 = 80 MHz
//										 SYSCTL_USE_PLL |			//Usa o PLL
//										 SYSCTL_OSC_MAIN |		//Oscilador selecionado é o main
//										 SYSCTL_XTAL_16MHZ);	//Cristal externo de 16 MHz
	
	//Retorna a frequência do uC
//	ROM_SysCtlClockGet();
}

//Inicialização do SysTick com busy wait running usando o clock do barramento
void SysTick_Inicializa(void) 
{
	SysTick->CTRL = 0;            // 1) Desabilita SysTick durante o setup
  SysTick->LOAD = 0x00FFFFFF; 	// 2) Valor máximo de recarga
  SysTick->VAL = 0;             // 3) Escreve qualquer valor para limpar o atual
  SysTick->CTRL = 0x00000005; 	// 4) Habilita SysTick com clock do sistema
}

//Inicialização do SysTick com busy wait running usando o clock do barramento
void SysTick_Inicializa_Int(unsigned long period) 
{
	SysTick->CTRL = 0;            			// 1) Desabilita SysTick durante o setup
  SysTick->LOAD = 0x00FFFFFF; 				// 2) Valor máximo de recarga
  SysTick->VAL = 0;             			// 3) Escreve qualquer valor para limpar o atual
	NVIC_SetPriority(SysTick_IRQn, 2); 	// 4) Define a prioridade da interrupção
  SysTick->CTRL = 0x00000007; 				// 5) Habilita SysTick com clock do sistema e interrupções
  __enable_irq();											// 6) Habilita interrupções (chave geral)
}

//Delay de tempo usando modo busy wait
//O parâmetro de delay é a unidade de clock do sistema (unidade de 20 ns para clock de 50 MHz)
void SysTick_Wait(unsigned long delay) 
{
	volatile unsigned long elapsedTime;
  unsigned long startTime = SysTick->VAL;
  do
  {
		elapsedTime = (startTime-SysTick->VAL)&0x00FFFFFF;
  }
  while(elapsedTime <= delay);
}

// Supondo clock do sistema de 16 MHz ou 80 MHz
void SysTick_Wait10ms(unsigned long delay) 
{
	unsigned long i;
	for(i=0; i<delay; i++)
	{
		//SysTick_Wait(160000); 	// Espera 10ms 
		SysTick_Wait(800000); 	// Espera 10ms 
	}
}

//Aguarda um dado na porta serial
//Retorna: código ASCII
unsigned char UART0_RxChar(void)
{
    while((UART0->FR&0x10) > 0);				//Aguarde enquanto RXFE = 1 (buffer vazio)
    return((unsigned char)(UART0->DR&0xFF));
}

//Envia um dado pela porta serial
//Recebe: caractere de 8 bits ASCII
void UART0_TxChar(unsigned char data)
{
    while((UART0->FR&0x20) > 0);	//Aguarde enquanto TXFF = 1 (buffer cheio)
    UART0->DR = data;
}

//Envia uma string pela porta serial
//Recebe: caractere de 8 bits ASCII
void UART0_TxString(unsigned char* data)
{
    while(*data != NULL)
		{
			UART0_TxChar(*data);
			data++;
		}
}

// ------------Profile_Init------------
// Initialize all Profile pins for GPIO output,
// which correspond with unused BoosterPack pins.
// The Profile pins can be set, cleared, or toggled at
// strategic times and observed with a logic analyzer.
// This function should be called once at the beginning.
// Input: none
// Output: none
void Profile_Init(void){
  SYSCTL_RCGCGPIO_R |= 0x00000004; // 1) activate clock for Port C
  while((SYSCTL_PRGPIO_R&0x04) == 0){};// allow time for clock to stabilize
                                   // 2) no need to unlock PC7,5
  GPIO_PORTC_AMSEL_R &= ~0xA0;     // 3) disable analog on PC7,5
                                   // 4) configure PC7,5 as GPIO
  GPIO_PORTC_PCTL_R = (GPIO_PORTC_PCTL_R&0x0F0FFFFF)+0x00000000;
  GPIO_PORTC_DIR_R |= 0xA0;        // 5) make PC7,5 output
  GPIO_PORTC_AFSEL_R &= ~0xA0;     // 6) disable alt funct on PC7,5
  GPIO_PORTC_PUR_R &= ~0xA0;       // disable pull-up on PC7,5
  GPIO_PORTC_PDR_R &= ~0xA0;       // disable pull-down on PC7,5
  GPIO_PORTC_DEN_R |= 0xA0;        // 7) enable digital I/O on PC7,5
  GPIO_PORTC_DATA_R &= ~0xA0;      // 8) initialize PC7,5 low
  SYSCTL_RCGCGPIO_R |= 0x00000010; // 1) activate clock for Port E
  while((SYSCTL_PRGPIO_R&0x10) == 0){};// allow time for clock to stabilize
                                   // 2) no need to unlock PE3-0
  GPIO_PORTE_AMSEL_R &= ~0x0F;     // 3) disable analog on PE3-0
                                   // 4) configure PE3-0 as GPIO
  GPIO_PORTE_PCTL_R = (GPIO_PORTE_PCTL_R&0xFFFF0000)+0x00000000;
  GPIO_PORTE_DIR_R |= 0x0F;        // 5) make PE3-0 output
  GPIO_PORTE_AFSEL_R &= ~0x0F;     // 6) disable alt funct on PE3-0
  GPIO_PORTE_PUR_R &= ~0x0F;       // disable pull-up on PE3-0
  GPIO_PORTE_PDR_R &= ~0x0F;       // disable pull-down on PE3-0
  GPIO_PORTE_DEN_R |= 0x0F;        // 7) enable digital I/O on PE3-0
  GPIO_PORTE_DATA_R &= ~0x0F;      // 8) initialize PE3-0 low
  SYSCTL_RCGCGPIO_R |= 0x00000020; // 1) activate clock for Port F
  while((SYSCTL_PRGPIO_R&0x20) == 0){};// allow time for clock to stabilize
                                   // 2) no need to unlock PF1
  GPIO_PORTF_AMSEL_R &= ~0x02;     // 3) disable analog on PF1
                                   // 4) configure PF1 as GPIO
  GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFFFF0F)+0x00000000;
  GPIO_PORTF_DIR_R |= 0x02;        // 5) make PF1 output
  GPIO_PORTF_AFSEL_R &= ~0x02;     // 6) disable alt funct on PF1
  GPIO_PORTF_PUR_R &= ~0x02;       // disable pull-up on PF1
  GPIO_PORTF_PDR_R &= ~0x02;       // disable pull-down on PF1
  GPIO_PORTF_DEN_R |= 0x02;        // 7) enable digital I/O on PF1
  GPIO_PORTF_DATA_R &= ~0x02;      // 8) initialize PF1 low
}

// ------------Profile_Get------------
// Return the current status of all Profile pins.
// Profile 0 is in bit 0. Profile 1 is in bit 1, and
// so on.
// The most significant bit is set to tell the PC
// that these values are data and not standard ASCII
// characters.
// Input: none
// Output: 7-bit status of all Profile pins
// Assumes: Profile_Init() has been called
uint8_t Profile_Get(void){
  return (((GPIO_PORTE_DATA_R)&0x0E)>>1) | // Profiles 2, 1, 0
         (((GPIO_PORTF_DATA_R)&0x02)<<2) | // Profile 3
         (((GPIO_PORTE_DATA_R)&0x01)<<4) | // Profile 4
         (((GPIO_PORTC_DATA_R)&0x20))    | // Profile 5
         (((GPIO_PORTC_DATA_R)&0x80)>>1) | // Profile 6
         0x80;                             // set most significant bit
}

//Função de inicialização do módulo ADC0
void ADC0_InitSWTriggerSeq3_Ch1(void)
{ 
    volatile unsigned long delay;
    SYSCTL->RCGCGPIO |= 0x00000010;   // 1) ativa o clock do Port E
    while(!(SYSCTL->PRGPIO & 0x10));	// tempo necessário para o clock estabilizar
    GPIOE->DIR &= ~0x04;      				// 2) faz PE2 input
    GPIOE->AFSEL |= 0x04;     				// 3) habilita alternate function em PE2
    GPIOE->DEN &= ~0x04;      				// 4) desabilita digital I/O em PE2
    GPIOE->AMSEL |= 0x04;     				// 5) habilita analog function on PE2
    
		SYSCTL->RCGCADC |= 0x0001;		   	// 6) ativa o clock do ADC0 
    while((SYSCTL->PRADC&0x01) == 0); // tempo necessário para o clock estabilizar       
    ADC0->PC = 0x0001;								// 7) configura amostragem para 125 Ksps
		ADC0->SSPRI = 0x0123;          		// 8) sequenciador 3 tem a mais alta prioridade
    ADC0->ACTSS &= ~0x0008;	        	// 9) desabilita amostragem do sequenciador 3
    ADC0->EMUX &= ~0xF000;	         	// 10) seq3 é trigado por software
    ADC0->SSMUX3 = (ADC0->SSMUX3 & 0xFFFFFFF0)+1; // 11) seleciona canal Ain1 (PE2)
    //ADC0->SSCTL3 = 0x0006;	         	// 12) não TS0 e D0, sim IE0 e END0
	  ADC0->SSCTL3 = 0x000E;	         	// 12) não  D0, sim TS0, IE0 e END0 (lê o sensor de temperatura)
    ADC0->ACTSS |= 0x0008;	         	// 13) habilita amostragem do sequenciador 3
}

// Conversão analógico para digital busy-wait
// Saída: resultado da conversão ADC com 12bits
unsigned long ADC0_InSeq3(void)
{  
    unsigned long result;
    ADC0->PSSI = 0x0008;           		// 1) inicializa SS3
    while((ADC0->RIS & 0x08)==0){};   // 2) espera a conversão terminar
    result = ADC0->SSFIFO3 & 0xFFF;	  // 3) lê o resultado de 12 bits
    ADC0->ISC = 0x0008;             	// 4) reconhece o término
    return result;
}

void EdgeCounter_Init(void)
{                          
  SYSCTL->RCGCGPIO |= 0x00000010; 	// (a) activate clock for port E
  FallingEdges = 0;             		// (b) initialize counter
  GPIOE->DIR &= ~0x10;    					// (c) make PE4 in (built-in button)
  GPIOE->AFSEL &= ~0x10;  					//     disable alt funct on PE4
  GPIOE->DEN |= 0x10;     					//     enable digital I/O on PE4   
  GPIOE->PCTL &= ~0x000F0000; 			// configure PE4 as GPIO
  GPIOE->AMSEL = 0;       					//     disable analog functionality on PE
  GPIOE->PUR |= 0x10;     					//     enable weak pull-up on PE4
  GPIOE->IS &= ~0x10;     					// (d) PE4 is edge-sensitive
  GPIOE->IBE &= ~0x10;    					//     PE4 is not both edges
  GPIOE->IEV &= ~0x10;    					//     PE4 falling edge event
  GPIOE->ICR = 0x10;      					// (e) clear flag4
  GPIOE->IM |= 0x10;      					// (f) arm interrupt on PE4
  
	NVIC_SetPriority(GPIOE_IRQn, 5); 	// (g) define a prioridade da interrupção
	NVIC_EnableIRQ(GPIOE_IRQn);				// (h) enable interrupt 4 in NVIC
  __enable_irq();										// (i) clears the I bit
}

void GPIOE_Handler(void)
{
  GPIOE->ICR = 0x10;      					// acknowledge flag4
  FallingEdges = FallingEdges + 1;
}

//Configura operação do servo como Timer 3A (PB2)
//Duas etapas:
//1a configurar o GPIO
//2a configurar o Timer como PWM
void Servo_Init(void)
{
	//1a etapa: configurar o GPIO------------------------------------------------
	//Tivaware
	//Ativa o clock do PORTB
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	//Espera o clock estabilizar
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB))
	{
	}
	//Seleciona a função de timer para o pino
	//GPIOPinTypeTimer(GPIO_PORTB_BASE, GPIO_PIN_2); //GPIO_PORTB_BASE declarado em hw_memmap.h do Tivaware
	GPIOPinTypeTimer(GPIOB_BASE, GPIO_PIN_2);	//GPIOB_BASE declarado em TM4C123GH6PM.h do CMSIS
	//Habilita funcionamento do pino como Timer3
	GPIOPinConfigure(GPIO_PB2_T3CCP0);

//	//CMSIS
//	//Configuração do Port B
//	SYSCTL->RCGCGPIO |= 0x002;     			//1-Ativa o clock do Port B
//  while((SYSCTL->PRGPIO&0x02) == 0);	//2-Aguarda o clock estabilizar
//  GPIOB->AFSEL |= 0x04;				      	//3-Habilita função alternativa no PB2
//  GPIOB->DEN |= 0x04;		        			//4-Habilita I/O difgital no PB2
//  GPIOB->PCTL = (GPIOB->PCTL & 0xFFFFF0FF) + 0x00000700;	//5-Seleciona a função T1CCP0 no PB2
//  GPIOB->AMSEL &= ~0x04;			 	    	//5-Desabilita função analógica no PB2

	//2a etapa: configurar o Timer como PWM--------------------------------------
	//Tivaware
	//Ativa o clock do Timer3
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
	//Aguarda o clock estabilizar
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER3))
	{
	}
	//Desabilita o Timer3A
	TimerDisable(TIMER3_BASE, TIMER_A);	
	//Configura o Timer3 como 2 de 16 bits (A e B), TimerA peródico e PWM
	TimerConfigure(TIMER3_BASE, (TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PERIODIC | TIMER_CFG_A_PWM));
	//Ajusta a saída como invertida (isso porque o contador a decrescente, o mach define o ponto para ligar o GPIO)
	TimerControlLevel(TIMER3_BASE, TIMER_A, true);
	//Define o prescale = 5-1 (divide o clock por 5)
	//f=16 MHz, t=62,5 ns, ciclo timer=t*2^16=4,096 ms
	//com prescale de 5: f=3,2 MHz, t=312,5 ns, ciclo timer=20,48 ms
	TimerPrescaleSet(TIMER3_BASE, TIMER_A, 5-1);
	//Ajusta o intervalo (período) para ~20 ms (50 Hz)
	//i=20ms/312,5ns=62208,4
	TimerLoadSet(TIMER3_BASE, TIMER_A, 62208);
	//Ajusta o match (duty cycle)
	//Atenção: o match ocorre antes do prescaler!!!
	//-90 graus=0,6ms -> 9600 ciclos
	//+90 graus=2,4ms -> 38400 ciclos
	//match=a*angulo+b
	//a=(38400-9600)/(90-(-90))=160
	//b=38400-160*90=24000
	//0 graus=24000 ciclos -> 1,5ms
	TimerMatchSet(TIMER3_BASE, TIMER_A, 24000);
	//Habilita o Timer3A
	TimerEnable(TIMER3_BASE, TIMER_A);
	
}

//Configura operação do servo como Timer 3A
void Servo_Set_Angle(int angulo)	
{
	int match = 0;
	//Desabilita o timer3A
	TimerDisable(TIMER3_BASE, TIMER_A);	
	//Ajusta o intervalo (período) para ~20 ms (50 Hz)
	//i=20ms/312,5ns=62208,4
	TimerLoadSet(TIMER3_BASE, TIMER_A, 62208);
	//Ajusta o match (duty cycle)
	//Atenção: o match ocorre antes do prescaler!!!
	//-90 graus=0,6ms -> 9600 ciclos
	//+90 graus=2,4ms -> 38400 ciclos
	//match=a*angulo+b
	//a=(38400-9600)/(90-(-90))=160
	//b=38400-160*90=24000
	//0 graus=24000 ciclos -> 1,5ms
	match = 160 * angulo + 24000;
	TimerMatchSet(TIMER3_BASE, TIMER_A, match);
	//Habilita o timer3A
	TimerEnable(TIMER3_BASE, TIMER_A);
}

//Configura o Watchdog Timer
void WDT_Init()
{
	SYSCTL->RCGCWD = 0x01; 		//Habilita WDT0
	WATCHDOG0->LOAD = 160000;	//Carrega o valor do timer 1ms @ 16MHz
	while(!(WATCHDOG0->CTL & (1<<31)));	//Aguarda completar a escrita
	WATCHDOG0->CTL |= (1<<1);	//Habilita reset do sistema (RESEN)
	while(!(WATCHDOG0->CTL & (1<<31)));	//Aguarda completar a escrita
	WATCHDOG0->CTL |= (1<<0);	//Habilita WDT e interrupções (INTEN)
	//Escrever no registrador LOAD periódicamente para atender ao WDT e recarregar o valor
}
//Tratamento do WDT 0 e 1
void WDT0_Handler()
{
	WATCHDOG0->ICR = 0x00;		//Limpa a flag do WDT
	
	SYSCTL->SRWD = 0x01;		//Pulso de reset do módulo WDT0 (0pcional)
	SYSCTL->SRWD = 0x00;
}
//Conversão de inteiro para string decimal
void IntToString(unsigned int n, char* s)
{
	unsigned int i,aux1;
	char first = 1;
	for(i = 1000000000; i >= 10; i/=10)
	{
		aux1 = n/i;
		n = n%i;
		if((aux1 > 0) || (first == 0))
		{
			first = 0;
			*s = aux1 + '0';
			s++;
		}		
	}
	*s = n + '0';	//Unidade
	s++;
	*s = 0;	//Terminador nulo	
}
//Conversão de inteiro para string hexa
void IntToStringHexa(unsigned int n, char* s)
{
	int i;
	unsigned int aux1;
	for(i = 7; i >= 0; i--)
	{
		aux1 = n & 0x0000000F;
		if(aux1 <= 9)
			*(s+i) = aux1 + '0';
		else
			*(s+i) = aux1 + 55;
		n >>= 4;
	}
	*(s+8) = 0;	//Terminador nulo	
}
//Conversão de string para inteiro
unsigned int StringToInt(unsigned char* s)
{
	unsigned int i=0,num=0;
	unsigned char temp;
	temp = *s;
	while((temp!=0) && (i<=10))
	{
		num *= 10;
		num += temp - '0';
		i++;
		temp = *(s+i);
	}
	return num;
}

//Envia uma string para o LCD TFT
//Recebe: caractere de 8 bits ASCII
void LCD_TxString(unsigned char* data)
{
	static unsigned int tamanho = 0;
	static unsigned char sobrou[3] = {0,0,0};
	unsigned int i = 0;
	unsigned int setor = 512;
	unsigned short int cor;
	unsigned char tam[11];
}

//função de seno


uint32_t sinFunction (float x){
	//return (sin(x) +1) * 512;
		return (sin(x) * 1024/2) + 1024;
}

uint32_t squareFunction (float x){
	//return (!((int)x%2)) ? 1024:0;
	return (sin(x) > 0) ? 1024:0;
}

uint32_t getFunctionPoint (uint32_t(*func)(float),float dx){
	static float x = 0;
	
	x += dx;
	return func(x);
	
}

int screen_init = 0;
void SysTick_Handler(void)  
{
	msTicks++;
	/*if(!(msTicks%=1000))
		GPIOF->DATA ^= Red;*/
	
	if(!(msTicks%10) && screen_init){
		BSP_LCD_PlotPoint(getFunctionPoint(sinFunction,0.10)/2,LCD_YELLOW);
		BSP_LCD_PlotIncrement();
	}
	
}

int main()
{
	//Variáveis locais
	unsigned char dado;
	int16_t x, y, select, txamos;
	long adc;
	long temp_int;
	int pAtual, pAnt;
	bool plot = false;
	int nextPoint;
	float grid;

	//Setup
	SysTick_Inicializa();
	SysTick_Config(SystemCoreClock / 1000);      // 1 int./ms (interrupção configurada intersecamente)
	
	BSP_LCD_Init();
  BSP_LCD_FillScreen(LCD_BLACK);
	
//	BSP_LCD_DrawString(3, 1, "Osciluzcopio", LCD_WHITE);
//	BSP_LCD_DrawString(2, 3, "By Lucas Teixeira", LCD_WHITE);
//	BSP_LCD_DrawString(1, 5, "Sistemas Embarcados", LCD_WHITE);
//	BSP_LCD_DrawString(1, 7, "Microcontroladores", LCD_WHITE);
//	BSP_LCD_DrawString(4, 9, " de 32 Bits", LCD_WHITE);
//	BSP_LCD_DrawString(5, 11, "SENAI-2019", LCD_WHITE);
//	BSP_Delay1ms(2000);
//	BSP_LCD_FillScreen(LCD_BLACK);
	
	//BSP_Joystick_Init();
	
	BSP_LCD_Drawaxes(LCD_YELLOW, LCD_BLACK, "Temp", "X", LCD_GREEN, "Y", LCD_CYAN, 1024, 0);
	
	BSP_LCD_DrawString(0, 0, "Amos=    ", LCD_YELLOW);
	BSP_LCD_DrawString(8, 0, "Ampl=    ", LCD_YELLOW);
	BSP_LCD_DrawString(16, 0, "Knal=    ", LCD_YELLOW);
	//screen_init = 1;
	
	pAtual = pAnt = getFunctionPoint(sinFunction,0.10)/2;
	nextPoint = 0;
	txamos = 10;
	
	//BSP_LCD_DrawFastVLine(20, 11, 60, LCD_WHITE);
	
	while(1)
	{
		while(msTicks<txamos); // taxa de amostragem. time/div = 10 vezes o valor definido aqui
		msTicks = 0;
		
		pAtual = getFunctionPoint(sinFunction,0.10*txamos/10)/2;
		
		if ((pAtual > 512) && (pAnt <=512) && (plot == false))
		plot = true;
		
		if (plot)
		{
	  BSP_LCD_PlotPoint(pAtual,LCD_BLUE);
		BSP_LCD_Plot_Vertical(pAtual,pAnt,LCD_BLUE);
		nextPoint = BSP_LCD_PlotIncrement();
			
		if (nextPoint == 0)
		{plot = false;}
		
		if ((nextPoint%10) == 9 )
			{
			for (y = 1; y<=9; y=y+1)
				{
				grid = 1023.0/10*y;
				BSP_LCD_PlotPoint(grid, LCD_YELLOW);
			//	BSP_LCD_PlotPoint(grid+1, LCD_BLACK);
				}
			}
		}
	pAnt=pAtual;
}
		//Lê o joystick da MKII
//		BSP_Joystick_Input(&x, &y, &select);
//		
//		
//		BSP_LCD_SetCursor(2, 0);
//		BSP_LCD_OutUDec4(x, LCD_YELLOW);
//		BSP_LCD_SetCursor(10, 0);
//		BSP_LCD_OutUDec4(y, LCD_YELLOW);
//		BSP_LCD_SetCursor(18, 0);
//		BSP_LCD_OutUDec4(select, LCD_YELLOW);
//				
//		//Pisca-pisca por interrupção do SysTick (usar SysTick_Config(_))
//		while(msTicks <= 10);
//		msTicks = 0;	
//		//BSP_LCD_PlotPoint(x, LCD_GREEN);
//		BSP_LCD_PlotPoint(y, LCD_CYAN);
//		BSP_LCD_PlotIncrement();

}

