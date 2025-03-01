#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void sieve(int pfather[2]){
    //从左邻居（父管道）读取整数
    int p;
    read(pfather[0], &p, sizeof(p));
    if(p==-1){
        exit(0);    //读取到-1表示结束
    }

    printf("prime %d\n", p);

    //创建子管道
    int pchild[2];
    pipe(pchild);

    if(fork()==0){  //子管道
        close(pchild[1]);
        close(pfather[0]);  
        sieve(pchild);  //创建子进程递归执行
    } else {
        close(pchild[0]);
        //从父管道接收数据
        int buf;
        while(read(pfather[0], &buf, sizeof(buf)) && buf != -1)
        {
            if(buf % p != 0){
                write(pchild[1], &buf, sizeof(buf));
            }
        }
        buf = -1;
        write(pchild[1], &buf, sizeof(buf));
        wait(0);
        exit(0);
    }
}


int main(int argc, char** argv){
    int input_pipe[2];
    pipe(input_pipe);
    if(fork()==0){
        close(input_pipe[1]);
        sieve(input_pipe);
        exit(0);
    } else {
        close(input_pipe[0]);
        int i;
        for ( i = 2; i <= 35; i++)
        {
            write(input_pipe[1], &i, sizeof(i));
        }
        i=-1;
        write(input_pipe[1], &i, sizeof(i));
    }
    wait(0);
    exit(0);
}