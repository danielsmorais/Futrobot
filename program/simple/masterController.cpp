#include <string>
#include <fstream>
#include <iostream>          //cout, cerr, cin
#include <cstdio>           //printf
#include <unistd.h>          //sleep
#include <bluetoothAction.h>
#include <cmath>
#include <omp.h>

// #include "../../firmware/esp-firmware-test/main/common.h"

#define MAC_ESP_TEST   "30:AE:A4:3B:A4:26"
#define MAC_ESP_ROBO_1 "30:AE:A4:20:0E:12"
#define MAC_ESP_ROBO_2 "30:AE:A4:13:F8:AE"
#define MAC_ESP_ROBO_3 "30:AE:A4:20:0E:12"

#define F_IS_NEG(x) (*(uint32_t*)&(x) >> 31)
#define ABS_F(x) (((x)<0.0)?-(x):(x))

#define CMD_HEAD           0xA0

#define CMD_REQ_CAL        0x00
#define CMD_REQ_OMEGA      0x03

#define CMD_CALIBRATION    0x04
#define CMD_IDENTIFY       0x05

#define CMD_SET_POINT      0x0A
#define CMD_CONTROL_SIGNAL 0x0B

#define CMD_PING           0x0F

#define RADIUS    1.5/100 //metros
#define REDUCTION 30 //30x1

using namespace std;

BluetoothAction btAction;

string getData(){
  time_t rawtime;
  tm *timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  std::string strTime = asctime(timeinfo);
  strTime.replace(strTime.find(' '), 1, "_");
  strTime.replace(strTime.find(' '), 1, "_");
  strTime.replace(strTime.find(' '), 1, "_");
  strTime.replace(strTime.find(' '), 1, "_");
  return strTime;
}

void saveToFile(const float *datas, const int size, const float timeout, const char* fileName);

void bytes2float(const uint8_t *bitstream, float*f, uint32_t num_float)
{
  memcpy((float*)bitstream, f, num_float*sizeof(float));
}

void float2bytes(const float*f, uint8_t *bitstream, uint32_t num_float)
{
  memcpy((uint8_t*)f, bitstream, num_float*sizeof(float));
}

void encodeFloat(const float* vec_f, uint8_t *bitstream)
{
  uint16_t ref_b[2];

	ref_b[0] = (!F_IS_NEG(vec_f[0]) << 15)  | (uint16_t)(ABS_F(vec_f[0])*32767.0);
  ref_b[1]= (!F_IS_NEG(vec_f[1]) << 15)   | (uint16_t)(ABS_F(vec_f[1])*32767.0);

  bitstream[0] = (ref_b[0] & 0xFF00) >> 8;
  bitstream[1] = (ref_b[0] & 0x00FF);
  bitstream[2] = (ref_b[1] & 0xFF00) >> 8;
  bitstream[3] = (ref_b[1] & 0x00FF);
}

void _printMainMenu(){
  printf("****************MENU DE AÇÕES*************\n");
  printf("01 -> PEDIR OMEGAS ATUAIS\n");
  printf("02 -> CONECTAR\n");
  printf("03 -> DESCONECTAR\n");
  printf("04 -> PING\n");
  printf("05 -> ENVIAR REFERÊNCIAS\n");
  printf("06 -> ENVIAR SINAL DE CONTROLE\n");
  printf("07 -> INICIAR CALIBRAÇÃO DO CONTROLADOR\n");
  printf("08 -> IDENTIFICAÇÃO\n");
  printf("09 -> VISUALIZAR GRAFICOS\n");
  printf("10 -> PEDIR DADOS DA CALIBRACAO\n");
  printf("11 -> ENCERRAR O PROGRAMA\n");
};

void _pause(const char* msg = ""){
  printf("%s\n(PRESSIONE QUALQUER TECLA PARA CONTINUAR)\n", msg);
  cin.ignore(1,'\n');
  cin.get();
}

void _printListMACs(){
  printf("****************MACs Conhecidos*************\n");
  vector<string> addrs = btAction.getDest();
  for(int i = 0; i < (int)addrs.size(); i++)
  {
    printf("%d -> Robo_%d = %s\n",i, i, addrs[i].c_str());
  }
};

int main(int argc, char** argv)
{
  double time_stemp[2];
  uint8_t *bitstream;
  float   *vec_float;
  int     idBt = -1;
  int     choice;

  btAction.setBluetoothAddr(MAC_ESP_TEST);
  btAction.setBluetoothAddr(MAC_ESP_ROBO_1);
  btAction.setBluetoothAddr(MAC_ESP_ROBO_2);
  btAction.setBluetoothAddr(MAC_ESP_ROBO_3);


  bool run = true;
  int  send = 0,rec = 0;
  while(run)
  {

    system("clear");
    _printMainMenu();
    cin >> choice;

    if(choice != 2 && idBt == -1 && choice != 9 && choice != 11)
    {
      _pause("Nenhum dispositivo conectado!");
      continue;
    }

    switch (choice){
    case 1://solicitar omegas
      bitstream = new uint8_t[1];
      vec_float = new float[2];

      memset(vec_float, 0, 2*sizeof(float));

      bitstream[0] = CMD_HEAD | CMD_REQ_OMEGA;
      btAction.sendBluetoothMessage(idBt, bitstream, 1*sizeof(uint8_t));
      rec = btAction.recvBluetoothMessage(idBt, (uint8_t*)vec_float, 2*sizeof(float), 2);

      if(rec != 2*sizeof(float))
      {
        _pause("Erro na leitura");
        break;
      }
      printf("   Omega Left:%f rad/s     Omega Right:%f rad/s\n", vec_float[0], vec_float[1]);
      printf("Velocity Left:%f m/s   Velocity Right:%f m/s\n", vec_float[0]*RADIUS/REDUCTION, vec_float[1]*RADIUS/REDUCTION);
      _pause();

      delete[] bitstream;
      delete[] vec_float;

      break;
    case 2://conectar
      _printListMACs();
      cin >> idBt;
      btAction.initBluetoothById(idBt);
      _pause();
      break;
    case 3://desconectar
      btAction.closeBluetoothById(idBt);
      _pause("Dispositivo desconectado!");
      idBt = -1;
      break;
    case 4: //ping
      bitstream = new uint8_t[64];

      memset(bitstream, 'A', 64);
      bitstream[0] = CMD_HEAD | CMD_PING;

      time_stemp[0] = omp_get_wtime();
      send = btAction.sendBluetoothMessage(idBt, bitstream, 64);
      rec  = btAction.recvBluetoothMessage(idBt, bitstream, 64, 1);
      time_stemp[1] = omp_get_wtime();

      printf("send:%d  \t rec:%d bytes \t time=%0.2fms\n",send, rec, (time_stemp[1] - time_stemp[0])*1000.0);
      delete[] bitstream;
      _pause();

      break;
    case 5://omegas ref
    case 6://omegas sinal de controle
      vec_float = new float[2];
      bitstream = new uint8_t[5];
      memset(bitstream, 0, 5*sizeof(uint8_t));
      memset(vec_float, 0, 2*sizeof(float));

      printf("Referencia do motor esquerdo ? (-1.0 a 1.0)\n");
      cin >> vec_float[0];
      printf("Referencia do motor direito ? (-1.0 a 1.0)\n");
      cin >> vec_float[1];

      encodeFloat(vec_float, bitstream+1);

      if(choice == 5){
        bitstream[0] = CMD_HEAD | CMD_SET_POINT;
      }else{ //control signal
        bitstream[0] = CMD_HEAD | CMD_CONTROL_SIGNAL;
      }

      btAction.sendBluetoothMessage(idBt, bitstream, 5*sizeof(uint8_t));

      delete[] bitstream;
      delete[] vec_float;

      break;
    case 7://calibrar
      bitstream = new uint8_t;
      bitstream[0] = CMD_HEAD | CMD_CALIBRATION;
      btAction.sendBluetoothMessage(idBt, bitstream, 1*sizeof(uint8_t));
      delete bitstream;
      break;
    case 8://identificar
    {
      uint8_t motor, typeC;
      float setpoint;
      const int size = (2.0/0.01);
      int sumRec = 0;

      cout << "Motor ? (0 -> Left \t 1 -> Right): ";
      cin >> motor;

      cout << "Setpoint: ";
      cin >> setpoint;

      cout << "Desabilitar controlador ? 0(nao), 1(sim): ";
      cin >> typeC;

      vec_float  = new float[size];
      bitstream  = new uint8_t[2 + 1*sizeof(float)];

      bitstream[0]                   = CMD_HEAD | CMD_IDENTIFY;
      bitstream[1]                   = ((motor << 7)  | typeC) & 0b10000001;
      *(float*)&bitstream[2 + 0*sizeof(float)] = setpoint;
      // printf("Options: %x \t setpoint: %f \t steptime: %f \t timeout: %f\n",
      //         bitstream[1],
      //         *(float*)&bitstream[2 + 0*sizeof(float)],
      //         *(float*)&bitstream[2 + 1*sizeof(float)],
      //         *(float*)&bitstream[2 + 2*sizeof(float)]);

      btAction.sendBluetoothMessage(idBt, bitstream, 2 + 1*sizeof(float));

      printf("Esperando...\n");
      time_stemp[0] = omp_get_wtime();
      // rec = btAction.recvBluetoothMessage(idBt, (uint8_t*)vec_float, size*sizeof(float), 6);
      for(int i = 0; i < size; i += 200)
      {
        // printf("i = %d\n",i);
        rec = btAction.recvBluetoothMessage(idBt, (uint8_t*)&vec_float[i], 200*sizeof(float), 6);
        printf("Recebi:%d bytes\n", rec);
        if(rec == -1)
          puts("timeout!");
        sumRec += rec;
      }
      time_stemp[1] = omp_get_wtime();
      printf("Tempo decorrido: %f s\n", time_stemp[1] - time_stemp[0]);

      printf("%d Bytes recebidos, deseja salvar ? (0/1)", sumRec);
      cin >> choice;

      if(choice == 1)
      {
        char fileName[50];
        cout << "Que nome devo colocar no arquivo ? ";
        scanf("%50s", fileName);

        saveToFile(vec_float, size, 2.0, (string("etc/") + string(fileName)).c_str() );
        cout << "Salvando...\n";
        printf("Salvo! Em: %s\n", (string("etc/") + string(fileName)).c_str());
        _pause();
      }else{
        printf("Tudo bem então...\n");
        _pause();
      }

      delete[] bitstream;
      delete[] vec_float;

      break;
    }
    case 9://graficos
    system("python3 etc/_pyplotter.py");
    break;
    case 10://dados da calibracao
      bitstream = new uint8_t[1];
      vec_float = new float[9];
      bitstream[0] = CMD_HEAD | CMD_REQ_CAL;
      btAction.sendBluetoothMessage(idBt, bitstream, 1*sizeof(uint8_t));
      rec = btAction.recvBluetoothMessage(idBt, (uint8_t*)vec_float, 9*sizeof(float), 5);

      printf("Coeficientes da calibracao tamanho total:%d bytes\n", rec);
      printf("Omega Max: %f rad/s = %f m/s\n", vec_float[0], vec_float[0]*RADIUS/(REDUCTION));//reducao de 30 e 24 interrupcoes por volta
      printf("Left  Front  => a = %f , b = %f \n", vec_float[1], vec_float[2]);
      printf("Left  Back   => a = %f , b = %f \n", vec_float[3], vec_float[4]);
      printf("Right Front  => a = %f , b = %f \n", vec_float[5], vec_float[6]);
      printf("Right Back   => a = %f , b = %f \n", vec_float[7], vec_float[8]);
      _pause();

      delete[] bitstream;
      delete[] vec_float;
      break;
    case 11: //terminar
      run = false;
      break;
    default:
      break;
    }
  }

  printf("Encerrando conexao e Fechando o programa\n");
  if(idBt != -1)
    btAction.closeBluetoothById(idBt);

  return 0;
}

void saveToFile(const float *datas, const int size, const float timeout, const char* fileName)
{
  ofstream out(fileName);

  out << size << ',' << timeout << ',';
  out << datas[0];
  for(int i = 0; i < size; i++){
    out << ',' << datas[i];
  }

  out.close();
}