#include"kernel/types.h"
#include"user/user.h"

void solve(int leftp[2]){
    int now;
    if(!read(leftp[0],&now,sizeof(now)))
        exit(0);
    printf("prime %d\n",now);
    int p[2];
    pipe(p);
    if(fork() == 0){
        close(p[1]);
        solve(p);
    }else {
        close(p[0]);
        int x;
        while(read(leftp[0],&x,sizeof(x))){
            if(x % now != 0){
                write(p[1],&x,sizeof(x));
            }
        }
        close(leftp[0]);
        close(p[1]);
        wait(0);
        exit(0);
    }
}

int main(int argc,char* argv[]){
    int p[2];
    pipe(p);
    if(fork() == 0){
        close(p[1]);
        solve(p);
    }else {
        close(p[0]);
        int i;
        for(i = 2;i <= 35;i++){
            write(p[1],&i,sizeof(i));
        }
        close(p[1]);
        wait(0); 
        exit(0);
    }
    exit(0);
}