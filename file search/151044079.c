
#include <unistd.h> 
#include <stdio.h> 
#include <dirent.h> 
#include <string.h>
#include <ctype.h>
#include <sys/stat.h> 
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h> 

#include <stdbool.h>




int flag = 0;
void parse_arg(int argc, char *const argv[], char *inputPath , char *filename , int *file_size , char *file_type , char *permissions, int* num_of_links);
void removeChar(char str[], char c) ;
void find(const char *dir, int depth,char *filename , int file_size , char file_type , char *permissions, int num_of_links);
int regex(char *filename , char* targetfilename);
int filePermStr(mode_t perm, char* permissions);
int compare(struct dirent *entry,struct stat statbuf,char *filename , int file_size , char file_type , char *permissions, int num_of_links);
void findchild(const char *dir,bool* ischild,DIR *dp,char* filename,int file_size , char file_type , char *permissions, int num_of_links);


static volatile int signal_arrived;

static void sigint_handler()
{
   signal_arrived = 1;
   fprintf(stderr, "\nSignal was handled. Please wait. \n");
}
int main (int argc, char *const argv[]){
		//Signal handling
   struct sigaction sig_action;
   memset(&sig_action, 0, sizeof(sig_action));
   sig_action.sa_sigaction = sigint_handler;
   sig_action.sa_flags = SA_RESTART | SA_SIGINFO;
   sigemptyset(&sig_action.sa_mask);
   sigaction(SIGINT, &sig_action, 0);



  	char dirpath[2048] ;
  	char filename [100] =" ";
  	int file_size = -1;
  	char file_type = ' ';
  	char permissions[20] = " ";
  	int num_of_links = -1;
  
	parse_arg(argc, argv, dirpath,filename,&file_size,&file_type,permissions,&num_of_links);
  	find(dirpath,0,filename,file_size , file_type , permissions,num_of_links);
  
  	if(flag == 0){
  		printf("no file found\n");
  	}
   

	return 0;

}
void parse_arg(int argc, char *const argv[], char *inputPathA , char *filename , int *file_size , char *file_type , char *permissions, int* num_of_links)
{
   int t = 0;
   int opt ;
   char optlist[] = "fbtpl";
   while ((opt = getopt(argc, argv, "w:f:b:t:p:l:")) != -1)
   {
   	
      if (opt == 'w' && optind == 3)
      {
         strcpy(inputPathA, optarg);
         t = 1;
      }
      else if(strchr(optlist,opt) != NULL){
      	if(opt == 'f'){
      		strcpy(filename,optarg);

      	}
      	else if(opt =='b'){
      		*file_size = atoi(optarg);
      	}
      	else if(opt == 't'){
      		*file_type = optarg[0];
      	}
      	else if(opt == 'p'){
      		strcpy(permissions , optarg);
      	}
      	else if(opt == 'l'){
      		*num_of_links = atoi(optarg);
      	}
      	removeChar(optlist , opt);
      	
      }
      else
      {
         printf("Repeated or Unknown option: %c\n", opt);
         exit(1);
      }
   }
   if (t == 0 || argc < 5)
   {
      printf("Somerthing is missing in the input line ,,, please check the input line again\n");
      exit(1);
   }
}
void removeChar(char str[], char c) {
	int i = 0;
	int j = 0;

	while (str[i]) {
	    if (str[i] != c) {
	        str[j++] = str[i];
	      }
	    i++;
	}
	str[j]=0;
	
}

void find(const char *dir, int depth,char *filename , int file_size , char file_type , char *permissions, int num_of_links){ 
	DIR *dp = NULL; 
	struct dirent *entry; 
  	struct stat statbuf; 
  	//int strbuffersize=0;
  	bool ischild = false;
  	if((dp = opendir(dir)) == NULL){ 
    	fprintf(stderr,"cannot open directory: %s\n", dir);
        return;
     } 
    chdir(dir); 
    while((entry = readdir(dp)) != NULL && signal_arrived == 0) {
   		lstat(entry->d_name,&statbuf);
  
    	if(S_ISDIR(statbuf.st_mode)){ // Found a directory, but ignore . and .. /
      		if(	strcmp(".",entry->d_name) == 0 || strcmp("..",entry->d_name) == 0) 
      			continue; 
     		findchild(entry->d_name,&ischild,dp,filename,file_size , file_type , permissions, num_of_links);
     		if(ischild ||compare(entry,statbuf,filename,file_size , file_type , permissions,num_of_links) == 1 ){
      			int i =0 ;
      			flag =1;
	      		while(i<depth){
	      			printf("-");
	      			i++;
	      		}
	      		printf("%s\n",entry->d_name );
      			
	      		ischild = false;
      		}
      		find(entry->d_name,depth+4,filename,file_size , file_type , permissions,num_of_links );
      	} 
      	else {
      		if(compare(entry,statbuf,filename,file_size , file_type , permissions,num_of_links) == 1){
      			int i =0 ;
      			flag =1;
	      		while(i<depth){
	      			printf("-");
	      			i++;
	      		}
	      		printf("%s\n",entry->d_name );
      			
     		}
        }
    

     } 
    chdir("..");
    closedir(dp); 
    } 



 int regex(char *filename , char* targetfilename){
 	int i = 0;
 	int j = 0;
 	char str[100]=" ";
 	char str2[100]=" ";
 	int sizestr2 =0;
 	int sizestr=0;
 	while (filename[i])
	{
		str[i] = tolower(filename[i]);i++;
		
	}
	str[i] = '\0';
	while (targetfilename[j])
	{
		str2[j] = tolower(targetfilename[j]);j++;
	}
	str2[j] = '\0';
	i=0;
	j=0;
	sizestr =strlen(str);
	sizestr2 = strlen(str2);
	while(str[i] || str2[j]){
		if(str2[j] == '+' ){
			if(j< sizestr2 && str2[j+1] == '+'){
				printf("double + in the file name \n");
				exit(1);
			}
			while(str[i] == str2[j-1] && i < sizestr) {i++;}
			j++;	
		}
		if(str[i] == str2[j]){
			i++;j++;
		}
		else {
			return -1;
		}
	}
	if(i >= sizestr && j >= sizestr2 ){
		return 1;
	}
	else
		return -1;
 }


int filePermStr(mode_t perm, char* permissions)
{

	 static char str[10];
	 snprintf(str, 10, "%c%c%c%c%c%c%c%c%c",
	 (perm & S_IRUSR) ? 'r' : '-',
	 (perm & S_IWUSR) ? 'w' : '-',
	 (perm & S_IXUSR) ?
	 	((perm & S_ISUID)  ? 's' : 'x') :
	 	((perm & S_ISUID)  ? 'S' : '-'),
	 (perm & S_IRGRP) ? 'r' : '-',
	 (perm & S_IWGRP) ? 'w' : '-',
	 (perm & S_IXGRP) ?
	 	((perm & S_ISGID)  ? 's' : 'x') :
	 	((perm & S_ISGID)  ? 'S' : '-'),
	 (perm & S_IROTH) ? 'r' : '-',
	 (perm & S_IWOTH) ? 'w' : '-',
	 (perm & S_IXOTH) ?
	 	((perm & S_ISVTX) ? 't' : 'x') :
	 	((perm & S_ISVTX)  ? 'T' : '-'));
	 if(strcmp(permissions,str) != 0 )
	 	return -1;
	 return 1;
}

int compare(struct dirent *entry,struct stat statbuf,char *filename , int file_size , char file_type , char* permissions, int num_of_links){
	if(strcmp(filename, " ") != 0){
      	if(regex(entry->d_name,filename) != 1){
      		return 0;
      	}
    }
    if(file_size != -1){
    	if(file_size != statbuf.st_size )
    		return 0;
    }
    if(strcmp(permissions , " ") != 0) {
    	if(filePermStr (statbuf.st_mode , permissions) != 1)
    		return 0;
    }
	if(file_type != ' '){
		if(!S_ISREG(statbuf.st_mode) && file_type == 'f')
			return 0;
		if(!S_ISDIR(statbuf.st_mode) && file_type == 'd')
			return 0;
		if(!S_ISSOCK(statbuf.st_mode) && file_type == 's')
			return 0;
		if(!S_ISBLK(statbuf.st_mode) && file_type == 'b')
			return 0;
		if(!S_ISCHR(statbuf.st_mode) && file_type == 'c')
			return 0;
		if(!S_ISFIFO(statbuf.st_mode) && file_type == 'p')
			return 0;
		if(!S_ISLNK(statbuf.st_mode) && file_type == 'l')
			return 0;

	}
	if(num_of_links != -1){
		if(statbuf.st_nlink != (unsigned) num_of_links)
			return 0;
	}
	return 1;


}
void findchild(const char *dir,bool* ischild,DIR *dp,char* filename,int file_size , char file_type , char *permissions, int num_of_links){
	
    struct dirent *entry; 
    struct stat statbuf; 
    if((dp = opendir(dir)) == NULL){ 
        return;
     } 
    chdir(dir); 
    while((entry = readdir(dp)) != NULL && signal_arrived == 0) {
      lstat(entry->d_name,&statbuf);
  
      if(S_ISDIR(statbuf.st_mode)){ // Found a directory, but ignore . and .. /
          if(strcmp(".",entry->d_name) == 0 || strcmp("..",entry->d_name) == 0) 
            continue; 
        if(compare(entry,statbuf,filename,file_size , file_type , permissions,num_of_links) == 1){
        	
          	*ischild = true ;
        }
        findchild(entry->d_name,ischild,dp,filename,file_size , file_type , permissions, num_of_links);
        } 
        else {
          if(compare(entry,statbuf,filename,file_size , file_type , permissions,num_of_links) == 1){
          
          	 *ischild = true;
        	}
        }

     } 
    chdir("..");
    closedir(dp); 
   

}

