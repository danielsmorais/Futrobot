/****************************************************************/
/*   Defini��es dos par�metros de comunica��o entre processos   */
/****************************************************************/

#ifndef _COMUNICACAOH_
#define _COMUNICACAOH_

// Par�metros da comunica��o via socket entre
// o programa principal e o simulador

#define PORTA_AMRL 0x3000
#define PORTA_AZUL 0x4000

// Par�metros da comunica��o via mem�ria compartilhada entre
// o simulador e o visualizador

#define KEY_SEM_COMPART 0x2001
#define KEY_MEM_COMPART 0x2002

// Par�metros da comunica��o via mem�ria compartilhada entre
// o futrobot e a interface

#define KEY_SEM_BLUE 715896
#define KEY_MEM_BLUE 123473

#define KEY_SEM_YELLOW 752478
#define KEY_MEM_YELLOW 138912

#endif
