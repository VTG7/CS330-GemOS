#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
int main(int argc, char **argv)
{
	int num_files=0;
	char cmd[10];
	strcpy(cmd, argv[1]);
	if(!strcmp(cmd,"-c")) {    											// creating the archive
		DIR *dir = opendir(argv[2]);
		struct dirent* de;
		if(!dir){
			printf("Failed to complete creation operation");
			return 0;
		}
		chdir(argv[2]);
		int tarfd;
		if((tarfd = open(argv[3], O_RDWR | O_CREAT, 0644))==-1) {
			printf("Failed to complete creation operation");
			return 0;
		}
	 	while((de=readdir(dir))!=NULL){
	   		if((de->d_name)[0]!='.' && strcmp(de->d_name, argv[3])) { // here see if the '\.' files are really to be excluded
  				num_files++;
				int fd = open(de->d_name,O_RDONLY);
				char filename[256]={'\0'};// made it 256 to avoid the warning at compilation time
				sprintf(filename,"%s", de->d_name);
				//filename = de->d_name;
				write(tarfd, filename, 17);
				struct stat st;
				fstat(fd, &st);
				char filesize[10]={'\0'};
				sprintf(filesize, "%lu" ,st.st_size);
				write(tarfd, filesize, 10);
				char *contents;
				contents = (char*)malloc(st.st_size*sizeof(char));
				read(fd, contents, st.st_size);
				write(tarfd, contents, st.st_size);
				free(contents);
				//read(fd, buf, 32);
				close(fd);

			}	
		}
		char buf[6]={'\0'};
		sprintf(buf,"%d", num_files);
		write(tarfd, buf, 6);
		close(tarfd);
		closedir(dir);
	}	




	if(!strcmp(cmd,"-l")) {													//listing the contents
		int tarfd = open(argv[2], O_RDWR);
		if(tarfd==-1) {
			printf("Failed to complete list operation");
			return 0;
		}
		int n = strlen(argv[2]); // length of the directory name;
		char *dir;
		
		dir = (char*)malloc((n+1)*sizeof(char));
		strcpy(dir, argv[2]);// the argv[2] is gonna get messed up so...

		char *temp = strtok(argv[2], "/");
		if(temp==NULL) {
			printf("Failed to complete listoperation");
			return 0;
		}
		char * tok = temp;
		while(temp!=NULL) {
			tok = temp;
			temp = strtok(NULL, "/");
		}	

		// tok contains the absolute tar file name now;
		int size_tok = strlen(tok);
		dir[n-size_tok]='\0';

		
		if(chdir(dir)){		//working directory is now dir
			printf("Failed to complete list operation");
			return 0;
		}
	
		int tar_structure = open("tarStructure", O_RDWR | O_CREAT, 0644);
		struct stat st;
		fstat(tarfd, &st);
		char buf[15] = {'\0'}; ;	//reading the size of the tar file
		sprintf(buf, "%lu", st.st_size);

		write(tar_structure, buf, strlen(buf)); // writing the same in the tarStructure file.
		write(tar_structure, "\n", 1); 	//adding a new line

		unsigned long long end = lseek(tarfd, -6, SEEK_END);
		char temp1[6]={'\0'};		
		read(tarfd, temp1, 6);
		write(tar_structure, temp1,strlen(temp1) );		//writing the number of files 
		write(tar_structure, "\n", 1);		//adding a new line
		unsigned long long curr = lseek(tarfd, 0, SEEK_SET);
		 while(curr!=end) {
		 	char name[17]={'\0'};
		 	read(tarfd, name, 17);
		 	write(tar_structure,name, strlen(name));	//writing the name of a file
		 	write(tar_structure, " ",1 );	// adding a space
		 	char size[10]={'\0'};
		 	read(tarfd, size, 10);
		 	write(tar_structure,  size, strlen(size)); // writing its size
		 	write(tar_structure,"\n",1);	// adding a new line
		 	curr = lseek(tarfd,atol(size),SEEK_CUR); 
		// 	//printf("Current pointer at %llu", curr);
		 }
		close(tar_structure);
		close(tarfd);
	}




	if(!strcmp(cmd,"-d")) {													//extracting all files
		int tarfd = open(argv[2], O_RDWR);
		if(tarfd==-1) {
			printf("Failed to complete extraction operation");
			return 0;
		}
		int n = strlen(argv[2]); // length of the directory name;
		char *dir;
		
		dir = (char*)malloc((n+1)*sizeof(char));
		strcpy(dir, argv[2]);// the argv[2] is gonna get messed up so...

		char *temp = strtok(argv[2], "/");
		if(temp==NULL) {
			printf("Failed to complete extraction operation");
			return 0;
		}
		char * tok = temp;
		while(temp!=NULL) {
			tok = temp;
			temp = strtok(NULL, "/");
		}

		// tok contains the absolute tar file name now;
		int size_tok = strlen(tok);
		dir[n-size_tok]='\0';

		
		if(chdir(dir)){		//working directory is now dir
			printf("Failed to complete extracation operation");
			return 0;
		}
		//tarfd = open(tok, O_RDWR);
		tok[size_tok-4] = 'D';
		tok[size_tok-3] = 'u';
		tok[size_tok-2] = 'm';
		tok[size_tok-1] = 'p';
		if(mkdir(tok,0777)) {
			printf("Failed to complete extracation operation");
			return 0;
		}
		if(chdir(tok)){
			printf("Failed to complete extracation operation");
			return 0;			
		}	//working directory now the DUMP folder
		if(tarfd==-1) {
			printf("Failed to complete extraction operation");
			return 0;
		}
		free(dir);
		unsigned long long end = lseek(tarfd, -6, SEEK_END);
		unsigned long long curr = lseek(tarfd, 0, SEEK_SET);
		while(curr!=end) {
			char name[17]={'\0'};
			read(tarfd, name, 17);
			int fd = open(name, O_RDWR | O_CREAT, 0644);
			if(fd==-1) {
				
				printf("Failed to complete extraction operation");
				return 0;
			}
			char size[10]={'\0'};
			read(tarfd, size, 10);
			int file_size = atoi(size);
			char *buf;
			buf = (char*)malloc(file_size*sizeof(char));
			read(tarfd, buf, file_size);	
			write(fd, buf, file_size);
			curr = lseek(tarfd,0,SEEK_CUR); 
			free(buf);
			close(fd);
		}
		close(tarfd);		
	}



	if(!strcmp(cmd,"-e")){														//extracting single file
		int tarfd = open(argv[2], O_RDWR);
		if(tarfd==-1) {
			printf("Failed to complete extraction operation");
			return 0;
		}
		int n = strlen(argv[2]); // length of the directory name;
		char *dir;
		
		dir = (char*)malloc((n+1)*sizeof(char));
		strcpy(dir, argv[2]);// the argv[2] is gonna get messed up so...

		char *temp = strtok(argv[2], "/");
		if(temp==NULL) {
			printf("Failed to complete extraction operation");
			return 0;
		}
		char * tok = temp;
		while(temp!=NULL) {
			tok = temp;
			temp = strtok(NULL, "/");
		}

		// tok contains the absolute tar file name now;
		int size_tok = strlen(tok);
		dir[n-size_tok]='\0';

		
		if(chdir(dir)){		//working directory is now dir
			printf("Failed to complete extracation operation");
			return 0;
		}		
		struct stat st;

		if (stat("IndividualDump", &st) == -1) {
    		if(mkdir("IndividualDump", 0777)) {
				printf("Failed to complete extraction operation");
				return 0;
			}
		}
		if(chdir("IndividualDump")) {	//in IndividualDump directory now;
				printf("Failed to complete extraction operation");
				return 0;			
		}
		free(dir);
		int found = 0;
		unsigned long long end = lseek(tarfd, -6, SEEK_END);
		unsigned long long curr = lseek(tarfd, 0, SEEK_SET);
		while(curr!=end) {
			char name[17]={'\0'};
			read(tarfd, name, 17);
			if(!strcmp(name, argv[3])) {
				found = 1;
				int fd = open(name, O_RDWR | O_CREAT, 0644);
				if(fd==-1) {	
					printf("Failed to complete extraction operation");
					return 0;
				}
				char size[10]={'\0'};
				read(tarfd, size, 10);
				int file_size = atoi(size);
				char *buf;
				buf = (char*)malloc(file_size*sizeof(char));
				read(tarfd, buf, file_size);	
				write(fd, buf, file_size);
				curr = lseek(tarfd,0,SEEK_CUR); 
				free(buf);
				close(fd);
				break;
			}
			char size[10]={'\0'};
			read(tarfd, size, 10);
			int file_size = atoi(size);
			curr = lseek(tarfd,file_size,SEEK_CUR); 
		}
		close(tarfd);	
		if(found==0) {
			printf("No such file is present in tar file.\n");
		}			

	}						

	return 0;
}
