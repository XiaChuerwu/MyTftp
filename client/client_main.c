/*
 * @creater: XiaChuerwu 1206575349@qq.com
 * @since: 2023-09-09 15:09:46
 * @lastTime: 2023-09-10 09:08:53
 * @LastAuthor: XiaChuerwu 1206575349@qq.com
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


#include "client.h"

//  gcc -I ./ commen.c ./client/client.c ./client/client_main.c -o client_test -l sqlite3
// gcc -I ../inc client.c client_main.c ../src/commen.c -o client_test -l sqlite3


int main (){
    login();
    return 0;
}