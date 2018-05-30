#include "camera.h"
#include "../../../program/system.h"

using namespace std;

class TesteCam:public Camera
{
public:
  TesteCam(unsigned index = 0):Camera(index){this->capturando = true;}
  inline bool capture(){ return Camera::captureimage(); }
  inline bool wait(){return Camera::waitforimage(); }
  inline void save(const char* arq){ ImBruta.save(arq); }
  inline void toRGB(ImagemRGB &dest){ Camera::toRGB(dest); }

};

int main(){
  TesteCam cam(1);
  ImagemRGB imrgb(0,0);
  char key;

  int gain = 77;
  int ExpAbs = 292;

  struct controler qGain;
  struct controler qExpAbs;

  if(!cam.queryGain(qGain) ) std::cout << "Camera nao possui o controles" << '\n';
  else std::cout << "Gain min - max " <<qGain.min << ' ' <<qGain.max << '\n';
  if ( !cam.setGain(gain) ) std::cout << "falha na alteracao dos controles" << '\n';

  if(!cam.queryExposureAbs(qExpAbs) ) std::cout << "Camera nao possui o controles" << '\n';
  else  std::cout << "Exposure(Absolute) min  - max " <<qExpAbs.min << ' ' <<qExpAbs.max << '\n';
  if ( !cam.setExposureAbs( ExpAbs ) ) std::cout << "falha na alteracao do controle" << '\n';


  while(true){
    cout << "q - Quit \n ENTER - Capture "<<endl;
    cin.get(key);
    if(key == 'q'){
      cout << "Quit\n";
      break;
    }
    if(key == '\n'){
      double start = relogio();
      cam.wait();
      cam.capture();
      double end = relogio();

      cam.save("CamSaveTeste.ppm");

      double start_2RGB = relogio();
      cam.toRGB(imrgb);
      double end_2RGB = relogio();

      imrgb.save("RGBtest.ppm");

      cout << "Captura time : " << end - start << endl;
      cout << "To RGB time  : " << end_2RGB - start_2RGB << endl;
    }

  };

  return 0;
}
