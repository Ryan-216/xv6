#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char** argv){
    //创建两个管道：pp2c 用于父进程到子进程的通信，pc2p用于子进程到父进程的通信
    int pp2c[2], pc2p[2];
    pipe(pp2c);
    pipe(pc2p);

    /*
    当调用 fork() 时，操作系统会创建一个与父进程几乎完全相同的子进程。
    父子进程的同步点：父子进程的代码执行路径在 fork() 的返回点分叉。父进程继续执行 fork() 之后的代码，而子进程也从同一个位置开始执行。
    在父进程中，fork() 返回子进程的进程 ID（PID），这是一个正整数。在子进程中，fork() 返回 0。如果失败，fork() 返回 -1，并设置 errno。
    */
    if(fork()!=0){  //父进程
        write(pp2c[1], ".", 1); //写端
        close(pp2c[1]);

        char buf;
        read(pc2p[0], &buf, 1); //读端
        printf("%d: received pong\n", getpid());
        //等待子进程结束
        // printf("child %d exit\n", wait(0));
        // printf("parent %d exit\n", getpid());
        wait(0);
        
    } else {    //子进程
        char buf;
        read(pp2c[0], &buf, 1);
        printf("%d: received ping\n", getpid());

        write(pc2p[1], ".", 1);
        close(pc2p[1]);
    }
    close(pp2c[0]);
    close(pc2p[0]);
    exit(0);
}