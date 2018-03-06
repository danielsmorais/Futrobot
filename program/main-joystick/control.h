#ifndef _CONTROL_H_
#define _CONTROL_H_

#include "futdata.h"


//################definicoes do Controle############
// Dist�ncia na qual se considera que o rob� atingiu o alvo
#define EPSILON_L 0.01
// Dist�ncia na qual se considera que o rob� n�o mais est� no alvo
#define DELTA_L (2*EPSILON_L)


// Dist�ncia a partir da qual o controle de orienta��o come�a a atuar
#define DIST_ORIENT 0.15
// Valor m�nimo do PWM abaixo do qual o rob� n�o anda...
//#define PWM_MINIMO (1.0/8.0)
#define PWM_MINIMO 0.07//CHECAR: fazer pwms diferenciados pra cada roda

// valor abaixo do qual o sinal enviado do pwm eh zero
#define PWM_ZERO (1.0/127.0)

class PID {
private:
  double K, Ti, Td, N;
  double e_ant, I_ant, I_ant2, D_ant;
public:
  // A rela��o com as constantes tradicionais do PID � a seguinte:
  // Kp=k, Ki=k/ti, Kd=k*td
  // Para eliminar a parte integral (Ki=0), ti=infinito
  // Para eliminar a parte derivativa (Kd=0), td=0.0
  // O n�mero n � o limitador de altas freq�encias da parte derivativa.
  // Normalmente, 3<=n<=20. Para eliminar a filtragem, n=infinito
  inline PID(double k=85.0, double ti=0.44/*1E+6*/, double td=0.11/*0.0*/, double n=1E+6) {
    fixa_constantes(k,ti,td,n);
  }
  void fixa_constantes(double k, double ti, double td, double n=1E+6);
  void anti_windup();
  void reset();
  double controle(double erro, double h);
};

class Control : public virtual FutData
{
private:
  bool controle_orientacao;
  PID lin[3],ang[3];
  bool chegou[3];
  int sentidoGiro[3];
public:
  Control(TEAM team, SIDE side, GAME_MODE gameMode);
  ~Control();
  bool control();
  bool stop_control();
};

#endif
