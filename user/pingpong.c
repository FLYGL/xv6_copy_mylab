#include "kernel/types.h" 
#include "user/user.h"
#include "user/util.h"

int main(int argc, char* argv[])
{
    int nRetCode = 0;
    int pipeObject[2];
    char buffer[2];
    buffer[1] = '\0';
    nRetCode = pipe(pipeObject);
    CheckFalse(nRetCode == 0,Exit);
    if(fork() == 0)
    {
        read(pipeObject[0],buffer,1);
        close(pipeObject[0]);
        printf("%d: received ping\n",getpid());
        buffer[0] = '2';
        write(pipeObject[1],buffer,1);
        close(pipeObject[1]);
    }
    else
    {
        buffer[0] = '1';
        write(pipeObject[1],buffer,1);
        close(pipeObject[1]);
        read(pipeObject[0],buffer,1);
        close(pipeObject[0]);
        printf("%d: received pong\n",getpid());
    }

Exit:
    exit(nRetCode);
}