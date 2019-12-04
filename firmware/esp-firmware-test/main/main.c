/**
 *  Programa teste para o controle local FeedForward
 *  O programa ira identificar a relacao entre PWM e Velocidade de cada motor para cada
 *  sentido de rotacao
 **/

 //Doc util: https://www.pjrc.com/teensy/td_libs_Encoder.html#optimize

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/timer.h"
#include "driver/mcpwm.h"

#include "nvs.h"
#include "nvs_flash.h"

//Bluetooth
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"

#include "esp_attr.h"

#include "common.h"

// #define DEVICE_NAME "ESP_ROBO_TEST"
// #define DEVICE_NAME "ESP_ROBO_1"
#define DEVICE_NAME "ESP_ROBO_2"

#define GPIO_OUTPUT_PIN_SEL ((1ULL << GPIO_STBY)      | \
                             (1ULL << GPIO_A1N1_LEFT) | \
                             (1ULL << GPIO_A1N2_LEFT) | \
                             (1ULL << GPIO_B1N1_RIGHT)| \
                             (1ULL << GPIO_B1N2_RIGHT))

#define GPIO_INPUT_PIN_SEL ( (1ULL << GPIO_OUTA_LEFT)  | \
                             (1ULL << GPIO_OUTB_LEFT)  | \
                             (1ULL << GPIO_OUTA_RIGHT) | \
                             (1ULL << GPIO_OUTB_RIGHT))

/*************************************************************************************/
/********************************* CABEÇALHOS ****************************************/
/*************************************************************************************/
// static void task_Encoder(void*arg);

static void config_bluetooth();
static void config_gpio();

//interruptions and callbacks
static void IRAM_ATTR isr_EncoderLeft();
static void IRAM_ATTR isr_EncoderRight();
static void esp_spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
static void periodic_controller();
//my functions
static void func_controlSignal(const float pwmL,const float pwmR);
static void func_calibration();
//options = 0xXY, sendo X 1 bit para indicar o motor, e os demais bits 7bits da parte Y indicando opcoes para o controlador utilizado,
//por enquanto Y serão:
// 0 => controlador padrao
// 1 => bypass controlador
// a coleta as velocidades medidas pelos sensores em intervalos de step_time até
// um tempo total de timeout, retorna um vetor de omegas medidos por referencias e
// o tamanho do vetor por retorno de funcao
static void func_identify(const uint8_t options,
                          const float setpoint,  // -1.0 a 1.0
                          const float step_time, //em segundos
                          const float timeout);  //em segundos
// static void func_identify(void* arg);  //em segundos
/*************************************************************************************/
/****************************** VARIAVEIS GLOBAIS ************************************/
/*************************************************************************************/
static xQueueHandle bt_queue = NULL;
static xQueueHandle encoder_queue[2] = {NULL,NULL};

static bool bypass_controller = false;

struct CoefLine coefL[2];       //coef. das retas PWM(omega) para o motor esquerdo
struct CoefLine coefR[2];       //coef. das retas PWM(omega) para o motor direito
static float omega_max = 0.0;   //modulo da velocidade maxima do robo

static float omega_ref[2]       = {0.0, 0.0}; //-1.0 a 1.0
static float omega_current[2]   = {0.0, 0.0}; //-1.0 a 1.0
static float omega_beta[2]      = {0.0, 0.0};
static int32_t pulse_counter[2]= {0, 0};
//Identificador do Bluetooth
static uint32_t bt_handle = 0;
static TaskHandle_t controller_xHandle = 0;

/*************************************************************************************/
/****************************** ROTINAS PRINCIPAIS ***********************************/
/*************************************************************************************/
void app_main()
{
  bt_data btdata;
  bt_queue   = xQueueCreate(1, sizeof(bt_data));
  encoder_queue[LEFT]  = xQueueCreate(1, sizeof(int8_t));
  encoder_queue[RIGHT] = xQueueCreate(1, sizeof(int8_t));

  /** configuracoes iniciais **/
  //interrupcoes no nucleo 0
  xTaskCreatePinnedToCore(config_gpio, "config_gpio", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(config_bluetooth, "config_bluetooth", 2048, NULL, 5, NULL, 0);
  xTaskCreatePinnedToCore(periodic_controller, "periodic_controller", 2048, NULL, 4, &controller_xHandle, 0);
  // //controlador no nucleo 1
  // xTaskCreatePinnedToCore(task_Encoder, "task_Encoder", 2048, LEFT, 3, NULL, 1);
  // xTaskCreatePinnedToCore(task_Encoder, "task_Encoder", 2048, RIGHT, 3, NULL, 1);

  uint8_t  head, cmd;
  uint8_t  *bitstream;
  float    *vec_float;
  while(1){
      xQueueReceive(bt_queue, &btdata, portMAX_DELAY);

      head = btdata.data[0] & 0xF0;
      if(head != CMD_HEAD)
        continue;

      cmd = btdata.data[0] & 0x0F;

      switch (cmd) {
      case CMD_SET_POINT:
        decodeFloat(btdata.data, &omega_ref[LEFT], &omega_ref[RIGHT]);
        if(bypass_controller)
          bypass_controller = false;
        break;
      case CMD_CONTROL_SIGNAL:
        bypass_controller = true;
        decodeFloat(btdata.data, &omega_ref[LEFT], &omega_ref[RIGHT]);
        break;
      case CMD_PING:
        esp_spp_write(bt_handle, btdata.len, btdata.data);
        break;
      case CMD_IDENTIFY:
        func_identify((uint8_t)btdata.data[1],                       //options
                      *(float*)&btdata.data[2+0*sizeof(float)],      //setpoint
                      *(float*)&btdata.data[2+1*sizeof(float)],      //steptime
                      *(float*)&btdata.data[2+2*sizeof(float)]);     //timeout
        break;
      case CMD_CALIBRATION:
        func_calibration();
        break;
      case CMD_REQ_OMEGA:
        esp_spp_write(bt_handle, 2*sizeof(float), (uint8_t*)omega_current);
        break;
      case CMD_REQ_CAL:
        vec_float = (float*)malloc(9*sizeof(float));

        vec_float[0] = omega_max;

        vec_float[1] = coefL[FRONT].alpha;
        vec_float[2] = coefL[FRONT].beta;
        vec_float[3] = coefL[BACK].alpha;
        vec_float[4] = coefL[BACK].beta;

        vec_float[5] = coefR[FRONT].alpha;
        vec_float[6] = coefR[FRONT].beta;
        vec_float[7] = coefR[BACK].alpha;
        vec_float[8] = coefR[BACK].beta;

        esp_spp_write(bt_handle, 9*sizeof(float), (uint8_t*)vec_float);
        free(vec_float);
        break;
      default:
        break;
      }
  }
}
//***************************************************************************************
static void IRAM_ATTR isr_EncoderLeft()
{
  static timeval timev;
  static int8_t  lookup_table[] = { 0,1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0 };
  static uint8_t enc_v = 0;
  static struct Encoder_data my_data = {0, 0.0};

    gettimeofday(&t, NULL);
    new_time =  t.tv_usec/1000000.0;

  enc_v <<= 2;
  enc_v |= (REG_READ(GPIO_IN1_REG) >> (GPIO_OUTB_LEFT - 32)) & 0b0011;
  xQueueSendFromISR(encoder_queue[LEFT], &lookup_table[enc_v & 0b111], NULL);
}
static void IRAM_ATTR isr_EncoderRight()
{
  static int8_t  lookup_table[] = { 0,-1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
  static uint8_t  enc_v = 0;
  static uint32_t reg_read;
  static struct Encoder_data my_data = {0, 0.0};
  static float t_last = 0.0;

  enc_v <<= 2;
  reg_read = REG_READ(GPIO_IN_REG);
  enc_v |= (reg_read & LS(GPIO_OUTA_RIGHT)) >> (GPIO_OUTA_RIGHT-1);
  enc_v |= (reg_read & LS(GPIO_OUTB_RIGHT)) >> GPIO_OUTB_RIGHT;
  xQueueSendFromISR(encoder_queue[RIGHT], &lookup_table[enc_v & 0b111], NULL);
}

static void func_controlSignal(const float pwmL,const float pwmR)
{
  #define SAT(x) (((x) > 1.0)?1.0:(x))

  static bool front[2]  = {false, false};
  front[LEFT]  = !F_IS_NEG(pwmL);
  front[RIGHT] = !F_IS_NEG(pwmR);

  gpio_set_level(GPIO_STBY, 0);
  gpio_set_level(GPIO_A1N1_LEFT, !front[LEFT]);
  gpio_set_level(GPIO_A1N2_LEFT, front[LEFT]);
  //set modo de rotacao do motor direito
  gpio_set_level(GPIO_B1N1_RIGHT, !front[RIGHT]);
  gpio_set_level(GPIO_B1N2_RIGHT, front[RIGHT]);

  //set PWM motor esquerdo
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM0A, ABS_F(pwmL)*100.0);
  //set PWM motor direito
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM0B, ABS_F(pwmR)*100.0);

  //driver on
  gpio_set_level(GPIO_STBY, (pwmL != 0.0) || (pwmR != 0.0)); //se algum pwm for diferente de zero, liga o drive (1)
}
//Funcao de tratamento de eventos do bluetooth
static void
esp_spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
  bt_data btdata;
    switch (event){
    case ESP_SPP_INIT_EVT:
        esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
        esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, "ESP32_SPP_SERVER");
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
        break;
    case ESP_SPP_OPEN_EVT:
        bt_handle = param->open.handle;
        break;
    case ESP_SPP_CLOSE_EVT:
        bt_handle = 0;
        memset(omega_ref, 0, 2*sizeof(float));
        bypass_controller = true;
        vTaskSuspend(controller_xHandle);
        break;
    case ESP_SPP_START_EVT:
        vTaskResume(controller_xHandle);
        break;
    case ESP_SPP_CL_INIT_EVT:
        break;
    case ESP_SPP_DATA_IND_EVT:
        // param->data_ind.len e param->data_ind.data
        btdata.data = param->data_ind.data;
        btdata.len  = param->data_ind.len;
        xQueueSend(bt_queue, &btdata, NULL);
        break;
    case ESP_SPP_CONG_EVT:
        break;
    case ESP_SPP_WRITE_EVT:
        break;
    case ESP_SPP_SRV_OPEN_EVT:
      bt_handle = param->open.handle;
        break;
    default:
        break;
    }
}
static void
periodic_controller()
{
  float pwm[2];
  float e[2] = {0.0, 0.0};

  const float k = (M_PI/6.0)*1000.0/(TIME_CONTROLLER); //constante pra converter de pulsos/s para rad/s

  while(1)
  {
    omega_beta[RIGHT] = (pulse_counter[RIGHT]*k + omega_beta[RIGHT])/2.0;
    omega_beta[LEFT]  = (pulse_counter[LEFT]*k  + omega_beta[LEFT])/2.0;

    memset(pulse_counter, 0, 2*sizeof(int32_t));

    if(bypass_controller)
    {
      //talvez colocar um taskYield aqui para permitir troca de contexto para outra trask executar
      func_controlSignal(omega_ref[LEFT], omega_ref[RIGHT]);
      vTaskDelay(TIME_CONTROLLER/portTICK_PERIOD_MS);
      continue;
    }

    //controlador FeedForWard
    pwm[LEFT]  = coefL[F_IS_NEG(omega_ref[LEFT])].alpha*omega_ref[LEFT]*omega_max  +
      coefL[F_IS_NEG(omega_ref[LEFT])].beta*(omega_ref[LEFT]  != 0 );
    pwm[RIGHT] = coefR[F_IS_NEG(omega_ref[RIGHT])].alpha*omega_ref[RIGHT]*omega_max +
      coefR[F_IS_NEG(omega_ref[RIGHT])].beta*(omega_ref[RIGHT] != 0 );

    // func_controlSignal(pwm[LEFT], pwm[RIGHT]);
    /****
     *aqui ficara o controlador BackWard (PID)
     ****/
    // e[LEFT]  = omega_ref[LEFT]*omega_max - omega_current[LEFT];
    // e[RIGHT] = omega_ref[RIGHT]*omega_max - omega_current[RIGHT];
    //
    // pwm[LEFT]  += e[LEFT]*1.0*(!!omega_ref[LEFT]);
    // pwm[RIGHT] += e[RIGHT]*1.0*(!!omega_ref[RIGHT]);

    //aplicar sinal de controle
    func_controlSignal(pwm[LEFT], pwm[RIGHT]);
    vTaskDelay(TIME_CONTROLLER/portTICK_PERIOD_MS);
  }

}

static void func_calibration()
{
  vTaskSuspend(controller_xHandle);

  #define PWM_MIN_INIT         0.10
  #define TIME_MS_WAIT_PWM_MIN  250
  #define TIME_MS_WAIT_VEL_MAX 1000
  const float STEP = 5.0/32767.0;    //por causa da resolucao de 15 bits na transmissao da ref

  float omega_max_tmpL[2];
  float omega_max_tmpR[2];
  float pwmMinL[2];
  float pwmMinR[2];
  float minOmegaMax = 0.0;
  //##################################### Calibrar motor direito ##########################################//
  /*************************************** LEFT FRONT *************************************************************/
  //Aplica maximo PWM
  //aguarda 500ms e armazena as 100 ultimas velocidades do motor esquerdo
  //calcula a media dessas velocidades e armazena como sendo a maior velocidade desse motor para essa rotacao
  func_controlSignal(1.0, 0.0);
  vTaskDelay(TIME_MS_WAIT_VEL_MAX/portTICK_PERIOD_MS);
  omega_max_tmpL[FRONT] = omega_current[LEFT];
  minOmegaMax = omega_max_tmpL[FRONT];
  //Encontrando o PWM minimo
  for(float pwm = PWM_MIN_INIT; pwm > 0.0; pwm = pwm - STEP)
  {
    func_controlSignal(pwm, 0.0);
    vTaskDelay(TIME_MS_WAIT_PWM_MIN/portTICK_PERIOD_MS);
    if(omega_current[LEFT] == 0.0)
    {
      pwmMinL[FRONT] = pwm;
      pwm = 0.0;
    }
  }
  func_controlSignal(0.0, 0.0);
  /*************************************** LEFT BACK *************************************************************/
  //Aplica maximo PWM
  //aguarda 500ms e armazena as 100 ultimas velocidades do motor esquerdo
  //calcula a media dessas velocidades e armazena como sendo a maior velocidade desse motor para essa rotacao
  func_controlSignal(-1.0, 0.0);
  vTaskDelay(TIME_MS_WAIT_VEL_MAX/portTICK_PERIOD_MS);
  omega_max_tmpL[BACK] = ABS_F(omega_current[LEFT]);

  //Encontrando o PWM minimo
  for(float pwm = PWM_MIN_INIT; pwm > 0; pwm = pwm - STEP)
  {
    func_controlSignal(-pwm, 0.0);
    vTaskDelay(TIME_MS_WAIT_PWM_MIN/portTICK_PERIOD_MS);
    if(omega_current[LEFT] == 0.0)
    {
      pwmMinL[BACK] = pwm;
      pwm = 0.0;
    }
  }
  func_controlSignal(0.0, 0.0);
  //##################################### Calibrar motor direito ##########################################//
  /*************************************** RIGHT FRONT *************************************************************/
  //Aplica maximo PWM
  //aguarda 500ms e armazena as 100 ultimas velocidades do motor esquerdo
  //calcula a media dessas velocidades e armazena como sendo a maior velocidade desse motor para essa rotacao
  func_controlSignal(0.0, 1.0);
  vTaskDelay(TIME_MS_WAIT_VEL_MAX/portTICK_PERIOD_MS);
  omega_max_tmpR[FRONT] = omega_current[RIGHT];
  //Encontrando o PWM minimo
  for(float pwm = PWM_MIN_INIT; pwm > 0; pwm = pwm - STEP)
  {
    func_controlSignal(0.0, pwm);
    vTaskDelay(TIME_MS_WAIT_PWM_MIN/portTICK_PERIOD_MS);
    if(omega_current[RIGHT] == 0)
    {
      pwmMinR[FRONT] = pwm;
      pwm = 0;
    }
  }
  func_controlSignal(0.0, 0.0);
  /*************************************** RIGHT BACK *************************************************************/
  //Aplica maximo PWM
  //aguarda 500ms e armazena as 100 ultimas velocidades do motor esquerdo
  //calcula a media dessas velocidades e armazena como sendo a maior velocidade desse motor para essa rotacao
  func_controlSignal(0.0, -1.0);
  vTaskDelay(TIME_MS_WAIT_VEL_MAX/portTICK_PERIOD_MS);
  omega_max_tmpR[BACK] = ABS_F(omega_current[RIGHT]);
  //Encontrando o PWM minimo
  for(float pwm = PWM_MIN_INIT; pwm > 0; pwm = pwm - STEP)
  {
    func_controlSignal(0.0, -pwm);
    vTaskDelay(TIME_MS_WAIT_PWM_MIN/portTICK_PERIOD_MS);
    if(omega_current[RIGHT] == 0)
    {
      pwmMinR[BACK] = pwm;
      pwm = 0;
    }
  }

  func_controlSignal(0.0, 0.0);

  for(int i = 0; i < 2; i++)
  {
    //Menor das Maximas velocidades
    minOmegaMax = (omega_max_tmpL[i] < minOmegaMax)?omega_max_tmpL[i]:minOmegaMax;
    minOmegaMax = (omega_max_tmpR[i] < minOmegaMax)?omega_max_tmpR[i]:minOmegaMax;
  }

  //Calculo dos coef.
  coefL[FRONT].alpha = (1.0 - pwmMinL[FRONT])/(omega_max_tmpL[FRONT]);
  coefL[FRONT].beta  = pwmMinL[FRONT];

  coefL[BACK].alpha = (1.0 - pwmMinL[BACK])/(omega_max_tmpL[BACK]);
  coefL[BACK].beta  = -pwmMinL[BACK];

  coefR[FRONT].alpha = (1.0 - pwmMinR[FRONT])/(omega_max_tmpR[FRONT]);
  coefR[FRONT].beta  = pwmMinR[FRONT];

  coefR[BACK].alpha = (1.0 - pwmMinR[BACK])/(omega_max_tmpR[BACK]);
  coefR[BACK].beta  = -pwmMinR[BACK];

  //Maior velocidade considerada
  omega_max = 0.90 * minOmegaMax;

  //retoma a rotina de controle
  vTaskResume(controller_xHandle);
}

static void func_identify(const uint8_t options,
  const float setpoint,
  const float step_time,
  const float timeout)
{

  omega_ref[LEFT]  = 0;
  omega_ref[RIGHT] = 0;

  int size       = ABS_F(ceil(timeout/step_time));

  uint8_t motor  = ((options & 0x80) >> 7) & 0x01;
  uint8_t typeC  = options & 0x7F;

  if(size == 0.0)
    return;

  float *vec_omegas = (float*)malloc(size*sizeof(float));
  memset(vec_omegas, 0, size*sizeof(float));

  bypass_controller = !!typeC; //bypass ou nao o controlador

  int step_time_ticks = (step_time*1000)/portTICK_PERIOD_MS;

  omega_ref[motor] = setpoint;

  for(int i = 0; i < size; i++)
  {
    vec_omegas[i] = omega_current[motor];
    vTaskDelay(step_time_ticks);
  }

  omega_ref[LEFT]  = 0;
  omega_ref[RIGHT] = 0;

  esp_spp_write(bt_handle, size*sizeof(float), (uint8_t*)vec_omegas);

  free(vec_omegas);
}

// static void task_Encoder(void*arg)
// {
//   int motor = (int)arg;
//   struct timeval t;
//   double old_time = 0.0, new_time;
//   int8_t sense, old_sense = 0;
//   while(1)
//   {
//     if(!xQueueReceive(encoder_queue[motor], &sense, TIME_TEST_OMEGA_ZERO/portTICK_PERIOD_MS))
//     {
//       omega_current[motor] = 0;
//       continue;
//     }
//
//     if(sense == 0)
//       sense = old_sense;
//     gettimeofday(&t, NULL);
//     new_time =  t.tv_usec/1000000.0;
//     //12 interrupcoes, por revolucao, para interrupcoes do tipo change nos canais A e B.
//     //(2pi/12)/dt => (pi/6)/dt rad/s
//     pulse_counter[motor]+=sense;
//     omega_current[motor] = sense*(M_PI/6.0)/(new_time - old_time);
//     old_time = new_time;
//     if(sense != 0)
//       old_sense = sense;
//   }
// }



/*************************************************************************************/
/****************************** CONFIGURACOES ****************************************/
/*************************************************************************************/
static void config_bluetooth()
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      nvs_flash_erase();
      nvs_flash_init();
  }
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_bt_controller_init(&bt_cfg);
  esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
  esp_bluedroid_init();
  esp_bluedroid_enable();
  esp_spp_register_callback(esp_spp_callback);
  esp_spp_init(ESP_SPP_MODE_CB);
  esp_bt_dev_set_device_name(DEVICE_NAME);

  vTaskDelete(NULL);
}

static void config_gpio(){
  //##### SET GPIO CONFIG ########
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  gpio_config(&io_conf);

  io_conf.intr_type = GPIO_INTR_ANYEDGE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 1;
  io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
  gpio_config(&io_conf);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(GPIO_OUTA_LEFT, isr_EncoderLeft, NULL);
  gpio_isr_handler_add(GPIO_OUTB_LEFT, isr_EncoderLeft, NULL);

  gpio_isr_handler_add(GPIO_OUTA_RIGHT,isr_EncoderRight, NULL);
  gpio_isr_handler_add(GPIO_OUTB_RIGHT,isr_EncoderRight, NULL);

  mcpwm_pin_config_t pin_configLeft = {
      .mcpwm0a_out_num = GPIO_PWM_LEFT,
      .mcpwm0b_out_num = GPIO_PWM_RIGHT,
  };
  mcpwm_set_pin(MCPWM_UNIT_0, &pin_configLeft);

  mcpwm_config_t pwm_config;
  pwm_config.frequency = 10000;    //frequency = 10kHz
  pwm_config.cmpr_a = 0.0;       //duty cycle of PWMxA = 0.0%
  pwm_config.cmpr_b = 0.0;       //duty cycle of PWMxA = 0.0%
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);   //Configure PWM0A & PWM0B with above settings

  vTaskDelete(NULL);
}
