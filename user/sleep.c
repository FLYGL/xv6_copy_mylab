#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int nRetCode = -1;
  int sleepTicksNumber = 0;
  if(argc < 2){
    fprintf(2, "Usage: sleep [a number]\n");
    exit(1);
  }
  sleepTicksNumber = atoi(argv[1]);
  if(sleepTicksNumber > 0)
  {
    nRetCode = sleep(sleepTicksNumber);
  }
  exit(nRetCode);
}
