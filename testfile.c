#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv){
	printf("Hello!!\n");
	for(int i = 0; i < 1000000; i++){
		// printf("%d\n", i);
	}
	return 0;
}