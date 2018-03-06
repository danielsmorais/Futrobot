#include <cstdlib>
#include <iostream>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include "ssocket.h"
#include "teste.h"

using namespace std;

// Normalmente, a biblioteca de socket para o programa sempre que
// acontece um erro. Para evitar este comportamento, vamos redefinir
// a fun��o tratadora de erros por uma que apenas imprime a mensagem
// de erro, mas n�o encerra o programa. O usu�rio � que deve analisar
// os erros retornados pelas fun��es e decidir o que fazer.

static void imprime_msg(SOCKET_STATUS err, const char *msg)
{
  //std::cout << "Erro no programa cliente: " << msg << endl;
}

/* Este programa tem uma thread principal que espera que o usu�rio
   digite alguma coisa e envia esta string para um servidor via
   socket. Uma outra thread imprime todas as strings recebidas pelo
   socket. Para toda mensagem recebida ou enviada, inicialmente �
   transmitido o n�mero "n" de bytes da mensagem (contando com o \0
   final) e em seguida os "n" bytes da mensagem */

// Aqui v�o as vari�veis globais das duas threads

tcpSocket s;
bool fim = false;

// Esta � a thread que escreve as mensagens recebidas

void *le_msg(void *x)
{
  char buffer[110];
  int n;

  while (!fim) {
    if ( s.read(&n,sizeof(n)) != SOCKET_OK ||
	 s.read(buffer,n) != SOCKET_OK) {
      if (!fim) {
	cout << "Problema na leitura do socket...\n";
      }
      fim = true;
    }
    if (!fim) {
      cout << endl
	   << "=========================================================\n"
	   << "Mensagem recebida: " << buffer << endl
	   << "=========================================================\n";
      fim = (strcasecmp(buffer,"fim")==0);
    }
  }
  return(NULL);
}

// O programa principal cont�m o ciclo que envia as mensagens digitadas

int main(int argc, char **argv)
{
  char buffer[100];
  int n;

  socket_error = imprime_msg;

  if( argc<2 ) {
    cout << "M�quina onde est� rodando o servidor (IP ou nome): ";
    cin >> buffer;
  }
  else {
    strcpy(buffer,argv[1]);
  }

  if (s.connect(buffer, PORTA_TESTE) != SOCKET_OK) {
    cout << "Problema na conex�o do socket...\n";
    exit(1);
  }

  pthread_t tid;
  // Cria a thread que escreve as mensagens recebidas
  pthread_create(&tid,NULL,le_msg,NULL);
  
  // Este � o ciclo que envia as mensagens digitadas
  while (!fim) {
    cout << "Mensagem a enviar: ";
    cin >> buffer;
    n = strlen(buffer)+1;

    if (!fim && n>1) {
      fim = (strcasecmp(buffer,"fim")==0); 
      if ( s.write(&n,sizeof(n)) != SOCKET_OK ||
	   s.write(buffer,n) != SOCKET_OK ) {
	cout << "Problema na escrita no socket...\n";
	fim = true;
      }
    }
    sleep(1); // Para as mensagens n�o se sobreporem no terminal
  }
  s.close();
  // Encerra a thread de recep��o
  pthread_kill(tid,SIGTERM);
  pthread_join(tid,NULL);
}
