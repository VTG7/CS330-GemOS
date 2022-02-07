#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
int main(int argc, char **argv)
{
	
	if(argc==1) {
		//but doesn't work when chained...gotta do it
		printf("UNABLE TO EXECUTE");
		return 0;
	}
	if(isalpha(argv[argc-1][0])) {
		printf("UNABLE TO EXECUTE");
		return 0;
	}
	unsigned long long num = strtoull(argv[argc-1], NULL, 10);
	char  check[25] = {'\0'};
	sprintf(check, "%llu", num);
	if(strlen(check)!=strlen(argv[argc-1])) {
		printf("UNABLE TO EXECUTE");
		return 0;
	}	
	unsigned long long ans = num*num;
	char buf[65]={'\0'};
	sprintf(buf,"%llu",ans);
	//if(num<0) {
	//	printf("UNABLE TO EXECUTE");
	//	return 0;
	//}
	if(argc==2) {
		printf("%llu\n", ans);
	}
	else {
		argv[argc-1] = buf;
		char next_cmd[10] = "./";
		strcat(next_cmd, argv[1]);
		if(execv(next_cmd, argv+1));
			printf("UNABLE TO EXECUTE");
	}
	return 0;
}
