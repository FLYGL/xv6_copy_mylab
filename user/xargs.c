#include "kernel/types.h" 
#include "kernel/stat.h"
#include "user/user.h"


void RunCmd(int argc,char* argv[],char* buffer)
{
    int nProcId = fork();
    if(nProcId == 0)
    {
        char** nextArgvs = (char**)malloc( (argc + 1)  * sizeof(char*));
        for(int i = 0; i < argc - 1; ++i)
        {
            nextArgvs[i] = argv[i+1];
        }
        nextArgvs[argc - 1] = buffer;
        nextArgvs[argc] = 0;
        exec(nextArgvs[0],nextArgvs);
    }
    else
    {
        while(wait(&nProcId) < 0);
    }
}

int
main(int argc, char* argv[])
{
    int inputFd = 0;
    const int bufferSize = 512;
    char buffer[bufferSize];
    int curPos = 0;
    while(read(inputFd,buffer + curPos,1) == 1)
    {
        if(buffer[curPos] == 10)
        {
            buffer[curPos] = 0;
            RunCmd(argc,argv,buffer);
            curPos = 0;
        }
        else
        {
            ++curPos;
        }
    }
    exit(0);
    return 0;
}