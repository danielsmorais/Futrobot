#include <time.h>
#include <cstdlib> //exit()
#include <errno.h>

#include "camera.h"
#include "../../../program/system.h"

using namespace std;
//
// class TesteCam:public Camera
// {
// public:
//   TesteCam(unsigned index = 0):Camera(index){this->capturando = true;}
//
//   inline bool capture(){ return Camera::captureimage(); }
//   inline bool wait(){return Camera::waitforimage(); }
//   inline void save(const char* arq){ ImBruta.save(arq); }
//   inline void toirgb(ImagemRGB &dest){ Camera::toRGB(dest); }
//
//   PxRGB getRGB(unsigned lin,unsigned col) { return ImBruta.getRGB(lin,col); } ;
//
// };

//V4L2_CID_MIN_BUFFERS_FOR_CAPTURE

#define CLEAR(A)  memset(&A, 0, sizeof(A))

int main(){
  //TesteCam cam(1);
  struct v4l2_queryctrl queryctrl;
  // struct v4l2_capability cap;
  // int fd = open("/dev/video0", O_RDWR | O_NONBLOCK , 0);
  // CLEAR(cap);

  char* name;
  std::string dev;
  int fd;
  bool alguma = false;

  for(unsigned i = 0; i < 9; i++){
    dev =  std::string("/dev/video") + std::string(new (char)(i+48));
    name = (char*)dev.c_str();

    fd = open(name,O_RDWR | O_NONBLOCK , 0);
    if(fd == -1)continue;

    alguma = true;

    struct v4l2_capability cap;
    CLEAR(cap);
    ioctl(fd,VIDIOC_QUERYCAP,&cap);

    std::cout << "index: "<< i <<" --> Nome: "<<cap.card << '\n';
    close(fd);
  }
  return 0;
}
