#include <sys/sem.h>
#include <sys/shm.h>

#include "../data.h"
#include "modelo.h"
#include <ssocket.h>
#include "../system.h"
#include "../functions.h"
#include "../comunicacao.h"
#include <stdio.h>
#include <cstring>


#define T_AMOSTR 0.033
using namespace std;

/* ***************************************************************
   Funções
   *************************************************************** */

static void* controle_soquetes(void*);  // main da thread dos sockets
static void* robos(void*);              // main da thread de simulação


static void atualiza_controle(PWM_ROBOTS&, TEAM, Modelo &);

/* ****************************************************************
   Variáveis globais (das duas threads)
   **************************************************************** */

static Modelo est;
static SITUACAO *psit = NULL;
static int sem_compart = -1;
static int mem_compart = -1;
static GAME_STATE estado_simulacao = PLAY_STATE;

// sockets de comunicacao com os clientes
udpSocket socket_azul,socket_amrl;
// filas de controle (para teste da existência de dados a ler)
fsocket filaServidores,filaClientes;

/* ****************************************************************
   PROGRAMA PRINCIPAL
   ***************************************************************** */

static void erro(SOCKET_STATUS err, const char *msg){}

int main(void)
{
  // Para que a biblioteca de sockets não faça nada em caso de "erros"
  // (por exemplo, de timeouts). Ela deve apenas retornar o código de
  // erro, para que eu trate aqui no programa principal.
  socket_error = erro;

  // Threads
  pthread_t thrd_controle;
  pthread_t thrd_simular;
  SITUACAO minhaSit;

  cout << "****************************************************\n";
  cout << "* Programa simulador dos robôs do futebol de robôs *\n";
  cout << "****************************************************\n";

  /* ************************************************
     Criação dos semáforos, memória compart e threads
     ************************************************ */

  // Criação do semáforo de exclusão mútua do compartilhamento dos dados
  if ((sem_compart=semget(KEY_SEM_COMPART,1,IPC_EXCL|IPC_CREAT|0600)) == -1) {
    cerr << "Erro na criação do semáforo de compartilhamento\n";
    goto FIM;
  }
  if (semctl(sem_compart,0,SETVAL,1) == -1) {
    cerr << "Erro na inicialização do semáforo de compartilhamento\n";
    goto FIM;
  }

  // Criação da memória compartilhada para o compartilhamento de dados
  if ((mem_compart=shmget(KEY_MEM_COMPART,sizeof(SITUACAO),
			  IPC_EXCL|IPC_CREAT|0600))==-1) {
    cerr << "Erro na criação da memória compartilhada\n";
    goto FIM;
  }
  if ((psit=(SITUACAO *)shmat(mem_compart,NULL,0)) == (void*)-1) {
    cerr << "Erro no acoplamento da memória compartilhada\n";
    goto FIM;
  }

  // Criação da thread de controle dos soquetes
  // verifica conexões
  if (pthread_create(&thrd_controle, NULL, controle_soquetes, NULL) != 0) {
    cerr << "Erro na criação da thread de controle das conexões\n";
    goto FIM;
  }
  if (pthread_detach(thrd_controle) != 0) {
    cerr << "Erro na dissociação da thread de conexões\n";
    goto FIM;
  }

  // Criação da thread de simulação (simulador <-> controle),
  // que engloba as funções de envio (câmera), recepção (porta paralela)
  // e simular_modelo (modelo dos robôs e modelagem de colisões)
  if (pthread_create(&thrd_simular, NULL, robos, NULL) != 0) {
    cerr << "Erro na criação da thread de simulação\n";
    goto FIM;
  }
  if (pthread_detach(thrd_simular) != 0) {
    cerr << "Erro na dissociação da thread de simulação\n";
    goto FIM;
  }
  
  /* *********************************
     Processo pai (programa principal)
     ********************************* */

  sleep(1);       // Para dar tempo às threads de se lançarem
  //set_keypress(); // Para não precisar digitar <ENTER> após a tecla

  do {
    char tecla;

    cout << "\nOpções disponíveis:\n";
    cout << "I - Voltar ao estado (i)nicial\n";
    cout << "P - Im(p)rimir estado\n";
    cout << "S - (S)uspender simulação\n";
    cout << "C - (C)ontinuar simulação\n";
    cout << "M - (M)udar posicao da bola\n";
    cout << "T - (T)erminar\n";
    cout << "SUA OPÇÃO: ";
    do {
      tecla = getchar();
    } while (tecla==EOF && estado_simulacao!=FINISH_STATE);

    if (estado_simulacao==FINISH_STATE) {
      tecla = 'T';
      cout << tecla;
    }
    else {
      tecla = toupper(tecla);
    }
    cout << '\n';
    switch(tecla) {
    case 'I':
      est.posicao_inicial();
      break;
    case 'P':
      // Imprime posições dos robôs e da bola
      // A informação é lida na memória compartilhada
      if (sem_compart!=-1 && entrar_regiao_critica(sem_compart)) {
	cerr << "Erro obtendo acesso à memória compartilhada\n";
	sem_compart = -1;
      }
      if (sem_compart!=-1 && psit!=NULL) {
	// Ler na memória compartilhada
	memcpy(&minhaSit,psit,sizeof(SITUACAO));
      }
      if (sem_compart!=-1 && sair_regiao_critica(sem_compart)) {
	cerr << "Erro liberando acesso à memória compartilhada\n";
	sem_compart = -1;
      }
      // Escrevendo a informação na tela
      cout << "+------------------------------------"
	   << "-----------------------------------+\n";
      printf( "| %6lu ", minhaSit.id);
      //      printf( "|           AZUL       %3u ", minhaSit.gols.azul);
      //      printf( "x %-3u      AMARELO         ", minhaSit.gols.amrl);
      printf( "|           AZUL       %3u ", 0);
      printf( "x %-3u      AMARELO         ", 0);

      printf( "|        |\n");
      // Nunca se imprime quando o estado é FINISH_STATE
      printf( "| %6.6s ", estado_simulacao==PLAY_STATE ? "PLAY_STATE" : "PAUSE_STATE");
      cout << "| 0 CIAN | 1 ROSA | 2 VERD "
	   << "| 0 CIAN | 1 ROSA | 2 VERD "
	   << "|   BOLA |\n";
      cout << "+------------------------------------"
	   << "-----------------------------------+\n";
      printf( "| Pos x  ");
      printf( "| %+6.3f | %+6.3f | %+6.3f ", minhaSit.pos.azul[0].x(),
	      minhaSit.pos.azul[1].x(), minhaSit.pos.azul[2].x());
      printf( "| %+6.3f | %+6.3f | %+6.3f ", minhaSit.pos.amrl[0].x(),
	      minhaSit.pos.amrl[1].x(), minhaSit.pos.amrl[2].x());
      printf( "| %+6.3f |\n", minhaSit.pos.bola.x());
      printf( "| Pos y  ");
      printf( "| %+6.3f | %+6.3f | %+6.3f ", minhaSit.pos.azul[0].y(),
	      minhaSit.pos.azul[1].y(), minhaSit.pos.azul[2].y());
      printf( "| %+6.3f | %+6.3f | %+6.3f ", minhaSit.pos.amrl[0].y(),
	      minhaSit.pos.amrl[1].y(), minhaSit.pos.amrl[2].y());
      printf( "| %+6.3f |\n", minhaSit.pos.bola.y());
      printf( "| Theta  ");
      printf( "| %+6.3f | %+6.3f | %+6.3f ", minhaSit.pos.azul[0].theta(),
	      minhaSit.pos.azul[1].theta(), minhaSit.pos.azul[2].theta());
      printf( "| %+6.3f | %+6.3f | %+6.3f ", minhaSit.pos.amrl[0].theta(),
	      minhaSit.pos.amrl[1].theta(), minhaSit.pos.amrl[2].theta());
      printf( "| ------ |\n");
      cout << "+------------------------------------"
	   << "-----------------------------------+\n";
      break;
    case 'M':
      {double x, y;
	cout << "coordenada x da bola:";
	cin >> x;
	cout << "coordenada y da bola:";
	cin >> y;
	est.set_posicao_bola (x, y);
      } break;
    case 'T':
    case 'S':
    case 'C':
      if (tecla=='C') {
	if (estado_simulacao==PAUSE_STATE) est.set_tempo(relogio());
	estado_simulacao=PLAY_STATE;
      }
      else {
	estado_simulacao=(tecla=='S' ? PAUSE_STATE : FINISH_STATE);
      }
      cout << "+----------------------------------+\n";
      cout << "| ESTADO: ";
      switch (estado_simulacao) {
      case PLAY_STATE:
	cout << "Simulação ativa          |\n";
	break;
      case PAUSE_STATE:
	cout << "Simulação suspensa       |\n";
	break;
      case FINISH_STATE:
	cout << "Simulação terminada      |\n";
	break;
      }
      cout << "+----------------------------------+\n";
      break;
    }
  } while (estado_simulacao!=FINISH_STATE);

  //se estado_simulacao == terminado ou acontecer qualquer erro na criação 
  //de semáforos, memória compartilhada ou threads, fim do simulador.
 FIM:
  estado_simulacao=FINISH_STATE;
  reset_keypress();
  
  // Desacoplamento e destruição da memória compartilhada
  if (psit != NULL) {
    if (shmdt(psit) == -1) {
      cerr << "Erro no desacoplamento da memória compartilhada\n";
    }
    psit = NULL;
  }
  if (mem_compart != -1) {
    if (shmctl(mem_compart,IPC_RMID,NULL) == -1) {
      cerr << "Erro na destruição da memória compartilhada\n";
    }
    mem_compart = -1;
  }

  // Destruição do semáforo de compartilhamento
  if (sem_compart != -1) {
    if (semctl(sem_compart,0,IPC_RMID,0) == -1) {
      cerr << "Erro na destruição do semáforo de compartilhamento\n";
    }
    sem_compart = -1;
  }
}

static void* controle_soquetes(void *pt_c)
{
  int servidoresAtivos;
  SOCKET_STATUS result;

  /* ************************************************
     Espera pela conexão dos sockets
     ************************************************ */

  while (estado_simulacao != FINISH_STATE) {
    // Inicializacao dos sockets de controle
    if (socket_azul.closed() &&
	socket_azul.listen( PORTA_AZUL ) != SOCKET_OK) {
      cerr << "Falha na abertura do servidor azul\n";
      estado_simulacao=FINISH_STATE;
    }
    if (socket_amrl.closed() &&
	socket_amrl.listen( PORTA_AMRL ) != SOCKET_OK) {
      cerr << "Falha na abertura do servidor amarelo\n";
      estado_simulacao=FINISH_STATE;
    }

    filaServidores.clean();
    servidoresAtivos = 0;
    if (socket_azul.accepting()) {
      filaServidores.include(socket_azul);
      servidoresAtivos++;
    }
    if (socket_amrl.accepting()) {
      filaServidores.include(socket_amrl);
      servidoresAtivos++;
    }
    if (servidoresAtivos > 0) {
      result = filaServidores.wait_connect(1000);  // 1000 ms
      if( result == SOCKET_OK ) {
	if( socket_azul.accepting() &&
	    filaServidores.had_activity(socket_azul) ) {
	  if( socket_azul.accept() == SOCKET_OK ) {
	    cout << "Conexão aceita com time azul\n"; 
	  }  // fim do if( socket_azul.accept() == SOCKET_OK )
	  else {
	    cerr << "Conexão não aceita pelo servidor azul\n";
	    estado_simulacao=FINISH_STATE;
	  }
	} // fim if( filaServidores.had_activity(servidor_azul) )
	if( socket_amrl.accepting() &&
	    filaServidores.had_activity(socket_amrl) ) {
	  if( socket_amrl.accept() == SOCKET_OK ) {
	    cout << "Conexão aceita com time amarelo\n"; 
	  }  // fim if( socket_amrl.accept() == SOCKET_OK )
	  else {
	    cerr << "Conexão não aceita pelo servidor amarelo\n";
	    estado_simulacao=FINISH_STATE;
	  }
	} // fim if( filaServidores.had_activity(servidor_amrl) )
      } 
      else if (result != SOCKET_TIMEOUT) {  // else if ( result == SOCKET_OK )
	cerr << "Erro na fila de sockets servidores\n";
	estado_simulacao=FINISH_STATE;
      } // fim if( result == SOCKET_OK )
    }
    else { // else if( servidoresAtivos > 0 )
      sleep(1);
    } // fim if( servidoresAtivos > 0 )
  } // fim_while

  socket_azul.close();
  socket_amrl.close();

  return NULL;
}

/* Simulação (envio+modelo+recepção). Esta é a função "main" da thread
   mais importante, que faz efetivamente a simulação. */

static void* robos(void *pt_c)
{
  SITUACAO minhaSit;
  SINAL_RADIO ctrl_azul, ctrl_amrl, *meuCtrl;
  udpSocket *meuSocket;
  SOCKET_STATUS result;
  double t,dt,dsusp,damostr=0.0;
  int clientesAtivos;

  est.posicao_inicial();
  minhaSit.id = 0;      // Vai gerar a primeira imagem
  ctrl_azul.id = ctrl_amrl.id = 0;   // Não recebeu nenhum controle
  damostr = 0.0;

  /* ***************************************
     laço (enquanto simulador estiver ativo)
     *************************************** */
  while (estado_simulacao!=FINISH_STATE) {
    // Lê a hora atual
    t = relogio();

    // Fila de transmissores de sinais de controle que estão conectados
    filaClientes.clean();
    clientesAtivos = 0;
    if( socket_azul.connected() ) {
      filaClientes.include(socket_azul);
      clientesAtivos++;
    }
    if( socket_amrl.connected() ) {
      filaClientes.include(socket_amrl);
      clientesAtivos++;
    }
    // Leitura de eventuais sinais de controle
    if (clientesAtivos > 0) {
      result = filaClientes.wait_read(0);  // timeout=0
      if ( result == SOCKET_OK ) {
	for( int ii=0; ii<2; ii++ ) {
	  meuCtrl = (ii==0 ? &ctrl_amrl : &ctrl_azul);
	  meuSocket = (ii==0 ? &socket_amrl : &socket_azul);
	  if( meuSocket->connected() &&
	      filaClientes.had_activity(*meuSocket) ) {
	    do {
	      result = meuSocket->read((void *)meuCtrl, sizeof(SINAL_RADIO));
	      if( result == SOCKET_OK ) {
		fsocket fila;
		fila.include(*meuSocket);
		result = fila.wait_read(0);
		if( result != SOCKET_TIMEOUT && result != SOCKET_OK ) {
		  cerr << "Erro na fila interna de socket" << endl;
		  estado_simulacao=FINISH_STATE;
		  return (void*)-1;
		}
	      }
	      else {
		cout << "Conexão fechada com transmissor do time "
		   << ((ii==0)?"amarelo":"azul")
		   << endl;
		meuSocket->close();
	      }
	      if (result==SOCKET_OK) {
		cerr << "Sinal de controle desprezado: "
		     << meuCtrl->id << endl;
	      }
	    } while( result == SOCKET_OK );
	    atualiza_controle(meuCtrl->c, 
			      (ii==0) ? YELLOW_TEAM : BLUE_TEAM,
			      est);
	    if (meuCtrl->id+1 != minhaSit.id) {
	      cerr << "Controle atrasado para o time "
		   << ((ii==0)?"amarelo":"azul")
		   << ": ctrl=" << meuCtrl->id
		   << ": img=" << minhaSit.id << endl;
	    }
	  } // fim if( meuSocket->connected() && ...
	}// fim for
      } // fim if ( result == SOCKET_OK ) {
      else if( result != SOCKET_TIMEOUT ) {
	cerr << "Erro na fila de sockets" << endl;
	estado_simulacao=FINISH_STATE;
	return (void*)-1;
      }

    } // fim if (clientesAtivos != 0)

    // Intervalo desde o último ciclo
    dt = t-est.le_tempo();
    if (estado_simulacao==PLAY_STATE) {
      est.simular(dt);
    }
    else {  // SUSPESO
      est.avancar_sem_simular(dt);
      dsusp += dt;
    }
    damostr += dt;
    if (estado_simulacao==PAUSE_STATE) {
      dsusp += dt;
    }
    else {
      dsusp = 0.0;
    }

    // Envio de imagem a cada período de amostragem
    if ( damostr >= T_AMOSTR ) {
      est.le_posicao(minhaSit.pos);
      //normalização dos ângulos
      for (int i=0; i<3; i++) {
	minhaSit.pos.azul[i].theta() = ang_equiv(minhaSit.pos.azul[i].theta());
	minhaSit.pos.amrl[i].theta() = ang_equiv(minhaSit.pos.amrl[i].theta());
      }
      // Escrever na memória compartilhada (para visualizador)
      if (estado_simulacao!=FINISH_STATE && sem_compart!=-1 &&
	  entrar_regiao_critica(sem_compart)) {
	cerr << "Erro obtendo acesso à memória compartilhada\n";
	sem_compart = -1;
      }
      if (estado_simulacao!=FINISH_STATE && sem_compart!=-1 && psit!=NULL) {
	// Escrever na memória compartilhada
	memcpy(psit, &minhaSit, sizeof(SITUACAO));
      }
      if (estado_simulacao!=FINISH_STATE && sem_compart!=-1 &&
	  sair_regiao_critica(sem_compart)) {
	cerr << "Erro liberando acesso à memória compartilhada\n";
	sem_compart = -1;
      }
      // Se houver algum cliente conectado, manda imagem nova
      if ( socket_amrl.connected() ) {
	if (socket_amrl.write((void*)&minhaSit,
			       sizeof(SITUACAO))!=SOCKET_OK) {
	  cout << "Conexão fechada com câmera amarela\n";
	  socket_amrl.close();
	}
      }
      if ( socket_azul.connected() ) {
	if (socket_azul.write((void*)&minhaSit,
			       sizeof(SITUACAO))!=SOCKET_OK) {
	  cout << "Conexão fechada com câmera azul\n";
	  socket_azul.close();
	}
      }
      damostr -= T_AMOSTR;
      minhaSit.id++;  // id do quadrado gerado
    } // fim if ( damostr >= T_AMOSTR )

    // Verifica se houve gols marcados
    /*
    if (estado_simulacao == PLAY_STATE &&
	fabs(minhaSit.pos.bola.x()) > FIELD_WIDTH/2.0) {
      if (minhaSit.pos.bola.x() > 0.0) {
	minhaSit.gols.azul++;
      }
      else {
	minhaSit.gols.amrl++;
      }
      estado_simulacao = minhaSit.estado = PAUSE_STATE;
      est.posicao_inicial();
    }
    */
    // Verifica se já deve sair do estado PAUSE_STATE
    if (estado_simulacao == PAUSE_STATE && dsusp > 10) {
      estado_simulacao = PLAY_STATE;
      dsusp = 0.0;
    }

    usleep(9999); // 10ms = tick do Linux
  } //fim while (estado_simulacao != FINISH_STATE)
  return(NULL);
}

// atualiza sinal de controle
static void atualiza_controle(PWM_ROBOTS &controle, TEAM cor, Modelo &est)
{
  for (int id=0; id<3; id++) {
    // Esquerdo
    est.atualizar_controle(cor, id, MOTOR_ESQUERDO, controle.me[id].left);
    // Direito
    est.atualizar_controle(cor, id, MOTOR_DIREITO, controle.me[id].right);
  }
}
