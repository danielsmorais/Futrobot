#include <imagem.h>

#include "camera.h"
#include "../../../program/system.h"


using namespace std;

class TesteCam:public Camera
{
public:
  TesteCam(){ this->capturando = true; }
  TesteCam(unsigned index):Camera(index){this->capturando = true;}
  inline void save(const char* arq) { ImagemRGB(ImBruta).save(arq);}
  inline bool capture(){return Camera::captureimage();}
  inline bool wait(){return Camera::waitforimage(); }
  inline bool Open(unsigned index){ Camera::Open(index); this->capturando = true; }
  inline PxRGB  atRGB(unsigned lin, unsigned col){ return ImBruta.atRGB(lin,col); }
  void toRGB( ImagemRGB &imgRGB) {imgRGB=ImBruta;}
  void histogram(ImagemRGB &dest){
    unsigned histo[256];
    for(unsigned i=0; i<=255; i++)
      histo[i] = 0;

    for(unsigned i = 120; i< 360; i++)for(unsigned j = 160; j < 480; j++)
      histo[dest[i][j].r] +=1;

    ofstream histograma;
    histograma.open("grafico.txt");
    for(unsigned i=0; i<=255; i++){
      histograma << histo[i]<< " ";
    }
    histograma.close();
  }
};

void segmentacao (ImagemRGB &dest, uint8_t ref){
  for(unsigned i = 0; i<dest.nlin(); i++)
  for(unsigned j = 0; j<dest.ncol(); j++){
    if(dest[i][j].r <= ref){
      dest[i][j].r = (uint8_t)0;
      dest[i][j].g = (uint8_t)0;
      dest[i][j].b = (uint8_t)0;
    }
    else{
    dest[i][j].r = (uint8_t)255;
    dest[i][j].g = (uint8_t)255;
    dest[i][j].b = (uint8_t)255;
    }
  }
};

void detlinhas(ImagemRGB &dest){
  unsigned coef[240][320];
  for(unsigned i = 0; i<240; i++)
    for(unsigned j = 0; j<320; j++)
      coef[i][j] = 0;
  for(unsigned i = 120; i<360; i++)
    for(unsigned j = 160; j<480; j++){
      if(dest[i][j].r >= (uint8_t)80)
        coef[i-120][j-160] += 1;
  }
  ofstream f;
  f.open("coef.txt");
  for(unsigned i = 0; i<240; i++){
    for(unsigned j = 0; j<320; j++)
      f << coef[i][j] << " ";
    f<<"\n";
  }
  f.close();
};


int main(){
  TesteCam cam;
  unsigned numDevices = 0;
  unsigned index = 0;
  ImagemRGB imrgb(0,0), segRGB(0,0), linha(100,100);
  char key;
  uint8_t ref = 124;

  do{
    numDevices = cam.listDevices();
    std::cout << "\nInforme um index valido da Camera : " << '\n';
    cin >> index;
  }while(index >= numDevices);
  cam.Open(index);
  cin.ignore(1,'\n');


  while(true){
    cout << "q - Quit \nENTER - Capture "<<endl;
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

      //cam.save("CamSaveTeste.ppm");

      double start_2RGB = relogio();
      cam.toRGB(imrgb);
      double end_2RGB = relogio();
      //Imgbruta.toGray();
      imrgb.save("RGBtest.ppm");
      imrgb.toGray();
      imrgb.save("RGBgray.ppm");
      cam.histogram(imrgb);

      std::cout << "Valor atual da ref " << ref << '\n';
      segmentacao(imrgb, 58);
      imrgb.save("RGB_seg.ppm");
      // detlinhas(imrgb);
      cout << "Captura time : " << end - start << endl;
      cout << "To RGB time  : " << end_2RGB - start_2RGB << endl;
    }
  };


  return 0;
}
