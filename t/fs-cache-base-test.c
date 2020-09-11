#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <error.h>
#include <stdbool.h>
#include "./fs-cache-base-files.h"
#define MAX_READ_PER (1024*1024*8)

int get_random_step(){
   /*return rand() % MAX_READ_PER;*/
    return 1024*4;
}

int get_random_file(){
   /*return rand() % MAX_READ_PER;*/
    return rand() % g_file_list_len;
}

char* readbuff = NULL;

int test_file(const char* file, bool verbose){
    int fd,nread;
    long  i;
    struct stat sb;
    char *mapped=0;
    
    int read_per = MAX_READ_PER;

    if ((fd = open (file, O_RDONLY)) < 0)
    {
        printf("open failed %s\n",file);
        return -1;
    }
    /* 获取文件的属性 */
    if ((fstat (fd, &sb)) == -1)
    {
        printf("fstat failed %s\n",file);
        return -1;
    }
    /* 将文件映射至进程的地址空间 */
    if ((mapped = (char *) mmap (NULL, sb.st_size, PROT_READ , MAP_SHARED, fd, 0)) == (void *) -1)
    {
        printf("mapped failed %s\n",file);
        return -1;
    }
    /* 映射完后, 关闭文件也可以操纵内存 */
    close (fd);
    if(verbose){
        printf ("mmap %s,size=%ld, map addr=%p\n",file, sb.st_size,mapped);
    }
    for(i=0;i<sb.st_size;)
    {
        nread = get_random_step();
        if(i+nread>sb.st_size){
            nread = sb.st_size-i;
        }
        //printf ("------read i=%ld,step=%d\n",i,nread);
        memcpy(readbuff,mapped+i,nread);
        i += nread;
    }
    if(verbose){
        printf ("mmap %s,read total=%ld\n",file, i);
    }
}

int main (int argc, char **argv)
{
    int usleep_time=0;
    int verbose=0;
    int idx =0 ;
    readbuff = (char*)malloc(MAX_READ_PER);
  
    if ( argc >= 2 )
    {
        usleep_time = atoi(argv[1]);
        printf("usleep =%d\n",usleep_time);
    }
    if ( argc >= 3 )
    {
        verbose = atoi(argv[2]);
        printf("verbose =%d\n", verbose);
    }
    srand((unsigned)time(NULL));
    while(true){
        idx = get_random_file();
        test_file(g_file_list[idx],verbose>0);

        if(usleep_time>0){
            usleep(usleep_time);
        }else{
            usleep(5000); /*5ms*/
        }
    }
    return 0;
}