#include <time.h>
#include "camera.h"
#include "../../../program/system.h"

using namespace std;

class TesteCam:public Camera
{
public:
  TesteCam():Camera(){ this->capturando = true; }

  inline bool capture(){ return Camera::captureimage(); }
  inline bool wait(){return Camera::waitforimage(); }
  inline void save(const char* arq){ ImBruta.save(arq); }
  PxRGB getPixel(unsigned lin,unsigned col){ return ImBruta[lin][col];}
};

int main()
{
  TesteCam cam;

  char key;
  while(true)
  {
    cout << "q - Quit or ENTER - Capture\n";
    cin.get(key);
    if(key == 'q'){
      cout << "Quit\n";
      break;
    }
    if(key == '\n'){
      //tick[0] = clock();
      double on = relogio();
      cam.wait();
      cam.capture();
      double end = relogio();
      cout << "Tempo de captura " << (end - on) << endl;
      //tick[1] = clock();
      //cout << "Tempo de captura " << (tick[1]-tick[0])*1000/CLOCKS_PER_SEC << "ms\n";
      //tick[0] = clock();

      cam.save("CamSaveTeste.ppm");
      //tick[1] = clock();
      //cout << "Tempo pra salvar" << (tick[1]-tick[0])*1000/CLOCKS_PER_SEC << "ms\n";
    }

  };

  return 0;
}
