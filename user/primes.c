#include "kernel/types.h" 
#include "user/user.h"
#include "user/util.h"

int filterNumber(int);
int GenNextPipline(int);
int TransferNext(int);
int WaitNumber();

int filter = -2;
int readFd = -1;
int writeFd = -1;
int nextProId = -1;

int WaitNumber()
{
    int number = -1;
    int childProcId = -1;

    while(read(readFd,&number,sizeof(int)) != 0)
    {
        filterNumber(number);
    }

    if(writeFd != -1)
    {
        close(writeFd);
        writeFd = -1;
    }

    while(wait(&childProcId) != -1 && childProcId != nextProId);

    return 0;
}

int filterNumber(int number)
{
    if(number % filter != 0)
    {
        TransferNext(number);
    }

    return 0;
}

int TransferNext(int number)
{
    if(writeFd != -1)
    {
        write(writeFd,&number,sizeof(int));
    }
    else
    {
        GenNextPipline(number);
    }

    return 0;
}

int GenNextPipline(int filterType)
{
    int tmpPipeObject[2];
    int nRetCode = pipe(tmpPipeObject);
    writeFd = tmpPipeObject[1];
    printf("prime %d\n",filterType);
    nRetCode = fork();
    if( nRetCode == 0)
    {
        filter = filterType;
        close(tmpPipeObject[1]);
        readFd = tmpPipeObject[0];
        writeFd = -1;
        nextProId = -1;
    }
    else
    {
        close(tmpPipeObject[0]);
        nextProId = nRetCode;
    }
    return nRetCode;
}

int main(int argc, char* argv[])
{
    if( GenNextPipline(2) == 0)
    {
        WaitNumber();
    }
    else
    {
        int childProcId;
        for(int input = 2; input <= 35; ++input)
        {
            TransferNext(input);
            // write(writeFd,&input,sizeof(int));
        }
        close(writeFd);
        //wait child process
        while(wait(&childProcId) != -1 && childProcId != nextProId);
    }

    exit(0);
}