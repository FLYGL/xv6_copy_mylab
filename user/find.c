#include "kernel/types.h" 
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"
#include "user/util.h"

static int find(const char* pcszPath,const char* pcszFileName)
{
    int nRetCode = -1;
    int fd;
    struct stat st;
    struct dirent dirInfo;
    char buf[512];
    char* pos;
    uint pathLength = strlen(pcszPath);
    uint nameLength = strlen(pcszFileName);
    if( (fd=open(pcszPath,0)) < 0)
    {
        fprintf(2,"find: cannot open %s\n",pcszPath);
        goto Exit;
    }

    if( fstat(fd,&st) < 0)
    {
        fprintf(2,"find: cannot stat %s\n",pcszFileName);
        goto Exit;
    }

    switch (st.type)
    {
    case T_DEVICE:
    case T_FILE:
        if(pathLength < nameLength)
        {
            goto Exit;
        }
        if(memcmp(pcszPath + pathLength - nameLength,pcszFileName,nameLength) != 0)
        {
            goto Exit;
        }
        if(pathLength > nameLength && pcszPath[pathLength - nameLength - 1] != '/')
        {
            goto Exit;
        }
        printf("%s\n",pcszPath);
        nRetCode = 0;
        break;
    case T_DIR:
        if(pathLength + 1 + DIRSIZ + 1 > sizeof buf){
            printf("ls: path too long\n");
            break;
        }
        strcpy(buf,pcszPath);
        // printf("%d %s\n",__LINE__,buf);
        pos = buf + strlen(buf);
        *pos = '/';
        pos++;
        // *pos = 0;
        // printf("%d %s\n",__LINE__,buf);
        while(read(fd,&dirInfo,sizeof(dirInfo)) == sizeof(dirInfo))
        {
            // printf("Line:%d Inode:%d Name:%s\n",__LINE__,dirInfo.inum,dirInfo.name);
            if(dirInfo.inum == 0 || strcmp(dirInfo.name,".") == 0 || strcmp(dirInfo.name,"..") == 0)
                continue;
            memmove(pos,dirInfo.name,DIRSIZ);
            pos[DIRSIZ] = 0;
            // printf("%d %s\n",__LINE__,buf);
            find(buf,pcszFileName);
        }
        nRetCode = 0;
    }
Exit:
    if(fd >= 0)
    {
        close(fd);
    }
    return nRetCode;
}

int
main(int argc, char *argv[])
{
  int nRetCode = -1;
  if(argc >= 3)
  {
    // printf("Input %s %s\n",argv[1],argv[2]);
    nRetCode = find(argv[1],argv[2]);
  }
  exit(nRetCode);
}
