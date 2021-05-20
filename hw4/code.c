# include <stdio.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <sys/types.h>
# include <sys/sem.h>
# include <pthread.h>
# include <semaphore.h>
# include <string.h>
# include <stdlib.h>
#include <signal.h>

int moneyamount ;
struct homework hm_queue;
int students_count =0;
sem_t mutex1 ;//read writing to hm_queue 
sem_t sem1 ; // to count the number of homeworks in the queue
sem_t sem2;//sync between stdudent to hire and the main thread wh is reading a char
sem_t student_finished;// to make sure that at least there is one student not working 
sem_t homeworkTaken_sem;
int read_bit = 10;
int total_cost = 0;
int total_hm_done = 0;
char next_homework_ = ' ';
static volatile int signal_arrived;

static void sigint_handler()
{
	signal_arrived = 1;
	//fprintf(stderr, "Signal was handled. Please wait. All thread will have exited in a second.");
}


struct student_to_hire{
	char name[20];
	int speed ;
	int price;
	int quality;
	int hmcount; // nmber of homework have been done by this student
	sem_t thread;
	pthread_t thread_id;
	int exit;
	int flag;

};
struct homework{
	char* hm;
	int size;
	int capacity;
	int front;
	int rear;
	
};
int parsing_commandline(int argc,const char *argv[] , char* hmFile,char* stFile );
struct student_to_hire *read_stfile (int fd,int *students_count);
struct student_to_hire *read_students_from_file(char *buffer, int *students_count);
int isFull(struct homework* hm_queue);
void add_hm(struct homework* hm_queue, char item);
char read_hm(struct homework* hm_queue);
void *h_thread(void *arg);
void *student_to_hire_job(void *arg);
int find_student(struct student_to_hire* student);
int isfilevalide(int fd);
int main(int argc , const char* argv[] ){

	//signal handling
	struct sigaction sig_action;
	memset(&sig_action, 0, sizeof(sig_action));
	sig_action.sa_sigaction = sigint_handler;
	sig_action.sa_flags = SA_RESTART | SA_SIGINFO;
	sigemptyset(&sig_action.sa_mask);
	sigaction(SIGINT, &sig_action, 0);
	///////
	int stFile_fd;
	int hmFile_fd;
	struct student_to_hire* students ;
	char* hmFile = (char*) malloc(50*sizeof(char));
	char* stFile = (char*) malloc(50*sizeof(char));
	pthread_t h_thread_id;
	if( parsing_commandline(argc,argv,hmFile,stFile) == -1 )
		return -1;
	if((stFile_fd = open(stFile,O_RDONLY)) == -1){
		perror("open file ERROR :");
		return -1 ;
	}
	if((hmFile_fd = open(hmFile,O_RDONLY)) == -1){
		perror("open file ERROR :");
		return -1 ;
	}
	if(isfilevalide(hmFile_fd) == -1){
		close(hmFile_fd);
		close(stFile_fd);
		return -1;
	}
	students = read_stfile(stFile_fd, &students_count);

	if(sem_init(&mutex1 ,0,1) == -1 || sem_init(&sem1 ,0,0) == -1  ||sem_init(&sem2 ,0,0)  || sem_init(&student_finished ,0,students_count) == -1
		|| sem_init(&homeworkTaken_sem ,0,1) == -1){
		perror("sem_init ERROR :");
		free(hmFile);
		free(stFile);
		free(students);
		close(hmFile_fd);
		close(stFile_fd);
		return -1 ;
	}
	if( pthread_create(&h_thread_id , NULL , h_thread ,&hmFile_fd ) != 0 ){
		printf("ERROR creating H thread\n");
		free(hmFile);
		free(stFile);
		free(students);
		close(hmFile_fd);
		close(stFile_fd);
		return -1 ;
	}
	int i = 0;
	for(i = 0 ; i < students_count ; i++){
		if(sem_init(&students[i].thread ,0,0) == -1 ){
			perror("sem_init ERROR :");
			exit(0) ;
		}
		if( pthread_create(&students[i].thread_id , NULL , student_to_hire_job  , (void *)&(students[i])) != 0  ){
			printf("ERROR creating a thread\n");
			free(hmFile);
			free(stFile);
			free(students);
			close(hmFile_fd);
			close(stFile_fd);
			return -1 ;
		}

	}
	printf("%d students-for-hire threads have been created.\nName  Q  S  C ",students_count );
	int h;
	for(h = 0 ; h < students_count ; h++){
		printf("%s %d %d %d \n",students[h].name ,students[h].quality, students[h].speed ,students[h].price   );
	}
	
	int ret_val=0;
	while(1){
		sem_wait(&mutex1);
		if((hm_queue.size == 0 && read_bit == 0) || signal_arrived == 1){
			
			int y =0;
			sem_post(&mutex1);
			for(y = 0 ; y < students_count ; y++){
				students[y].exit =1;
				sem_post(&students[y].thread);
				pthread_join(students[y].thread_id , NULL);
			}
			if(signal_arrived == 1){
				printf("Termination signal received, closing.\n");
			}
			else if(hm_queue.size == 0 && read_bit == 0){
				printf("No more homeworks left or coming in, closing. \n");
			}
			break;
		}
		sem_post(&mutex1);
		
		sem_wait(&homeworkTaken_sem);

		sem_wait(&sem1);
		next_homework_ = read_hm(&hm_queue);
		ret_val =  find_student(students);
		if(ret_val == -1){
			printf("Money is over, closing.\n");
			int i = 0;
			for(i = 0 ; i < students_count ; i++){
				students[i].exit = 1;
				sem_post(&students[i].thread);
				pthread_join(students[i].thread_id , NULL);
			}
			break;
		}
		sem_wait(&student_finished);

	}
	pthread_detach(h_thread_id);
	 i = 0;
	if(sem_destroy(&mutex1 ) == -1 || sem_destroy(&sem1 )  == -1 || sem_destroy(&sem2 ) == -1 || sem_destroy(&homeworkTaken_sem) == -1 
  	|| sem_destroy(&student_finished) ){
  		perror("sem_destroy ERROR :");
		return -1 ;
  	}
  	printf("Homeworks solved and money made by the students: \n");
  	for(i = 0 ; i < students_count ; i++){
  		printf("%s   %d  %d\n",students[i].name , students[i].hmcount , students[i].hmcount* students[i].price );
  	}
  	printf("Total cost for %d homeworks %dTL\n",total_hm_done , total_cost );
  	printf("money left at G’s account: %dtl\n",moneyamount );
  	
  	free(hm_queue.hm);
  	free(hmFile);
	free(stFile);
	free(students);
	close(hmFile_fd);
	close(stFile_fd);
	return 0;
}

int parsing_commandline(int argc,const char *argv[] , char* hmFile , char* stFile ){

	if(argc < 4){
		printf("INVAILD COMMAND LINE ARGUMENT\n");
		return -1;
	}
	else{
		strcpy(hmFile,argv[1]);
		strcpy(stFile,argv[2]);
		moneyamount = atoi(argv[3]);
	}
	return 0;

}
struct student_to_hire *read_stfile(int fd, int *students_count)
{
	char buf;
	int size = 1024;
	int counter = 0;
	struct student_to_hire *students = NULL;
	char *buffer = (char *)malloc(size * sizeof(char));
	//struct student_to_hire* temp_student ;
	int bytes_read;
	while (((bytes_read = read(fd, &buf, 1)) != -1))
	{


		if (bytes_read == 0)
		{
			students = read_students_from_file(buffer, students_count);
			free(buffer);
			counter =0;
			buffer = NULL;
			return students;
			
		}
		buffer[counter] = buf;
		buffer[counter + 1] = '\0';
		counter++;
		if (size == counter)
		{
			size *= 2;
			buffer = (char *)realloc(buffer, size * sizeof(char));
		}

	}
	free(buffer);
	return NULL;

}


struct student_to_hire *read_students_from_file(char *buffer, int *students_count)
{
	int size = 10;

	int count = 0;
	char name[20];
	int speed, quality,price;
	struct student_to_hire *temp_student = (struct student_to_hire *)malloc(size * sizeof(struct student_to_hire));

	char *a = strtok(buffer, "\n");
	//Murat (-10,8; 1.1) : violet, daffodil, orchid
	while (a != NULL)
	{

		*students_count += 1;
		sscanf(a, "%s %d %d %d ", name,&quality,&speed,&price);
		temp_student[count].quality = quality;
		temp_student[count].speed = speed;
		strcpy(temp_student[count].name, name);	
		temp_student[count].price = price;
		temp_student[count].hmcount = 0;
		temp_student[count].thread_id = 0;
		temp_student[count].flag = 0;
		temp_student[count].exit = 0;
		
		count++;
		if (size == count)
		{
			size *= 2;
			temp_student = (struct student_to_hire *)realloc(temp_student, size * sizeof(struct student_to_hire));
		}
		a = strtok(NULL, "\n");
	}

	return temp_student;
}
int isFull(struct homework* hm_queue){
	return hm_queue->size == hm_queue->capacity;
}
void add_hm(struct homework* hm_queue, char item){

	if(isFull(hm_queue)){
		hm_queue->capacity *= 2;
		hm_queue->hm = (char*)realloc(hm_queue->hm,hm_queue->capacity * sizeof(char));
	}
	hm_queue->rear = (hm_queue->rear + 1) % hm_queue->capacity;
    hm_queue->hm[hm_queue->rear] = item;
    hm_queue->size = hm_queue->size + 1;
}
char read_hm(struct homework* hm_queue){

    char item = hm_queue->hm[hm_queue->front];
    hm_queue->front = (hm_queue->front + 1) % hm_queue->capacity;
    hm_queue->size = hm_queue->size - 1;
    return item;
}

struct homework initialize_homework_queue()
{
    struct homework queue ;
    queue.capacity = 1000;
    queue.front = queue.size = 0;
    queue.rear = queue.capacity - 1;
    queue.hm = (char*)malloc(queue.capacity*sizeof(char));
    return queue;
}
void *h_thread(void *arg){
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	pthread_sigmask(SIG_BLOCK, &mask, NULL);
	int fd = *(int*) arg ;
	hm_queue = initialize_homework_queue();
	int X;
    char ch;
    while((X =read(fd , &ch , 1) ) != -1 ){
	    
	    if(ch == '\n')
	   		continue;
	    sem_wait(&mutex1);
	    if(X == 0 ){
	    	read_bit  = X ;
	    	sem_post(&mutex1);
	    	pthread_exit(NULL);
	    }
	    read_bit = X ; 
	    add_hm(&hm_queue,ch);
	    sem_post(&mutex1);
	    printf("H has a new homework %c; remaining money is %dTL\n",ch,moneyamount );
	    sem_post(&sem1);    	
	    
	}
	
	pthread_exit(NULL);

}
void *student_to_hire_job(void *arg){
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	pthread_sigmask(SIG_BLOCK, &mask, NULL);

	struct student_to_hire *S = (struct student_to_hire*)arg;
	while(1){
		sem_wait(&S->thread);
		if(S->exit == 1){
			pthread_exit(NULL);
		}
		printf("%s is waiting for a homework \n",S->name );
		S->flag = 1;
		printf("%s is solving homework %c for %d, H has %dTL left. \n",S->name , next_homework_ , S->price , moneyamount - S->price );
		moneyamount = moneyamount - S->price;
		sem_post(&homeworkTaken_sem);
		total_hm_done += 1;
		total_cost += S->price;
		S->hmcount += 1;
		sleep( 6-(S->speed ));
		sem_post(&student_finished);
		S->flag = 0;

	}
	
	
	
}
int find_student(struct student_to_hire* student){
	int i = 0;
	int position = 0 ;
	int temp_price = 1001;
	
	if(next_homework_ == 'C'){
		while(i < students_count){
			
			if(student[i].price < temp_price   &&student[i].flag == 0  ){
				position = i;
				
				temp_price = student[i].price;
			}
			i++;
		}
		

	}
	else if(next_homework_ == 'Q'){
		int temp_quality = 0;
		while(i < students_count){
			if(student[i].quality > temp_quality   &&student[i].flag == 0  ){
				position = i;
				
				temp_quality = student[i].quality;
			}
			i++;

		}


	}
	if(next_homework_ == 'S'){
		int temp_speed = 5;
		while(i < students_count){
			if(student[i].speed < temp_speed   &&student[i].flag == 0  ){
				position = i;
				
				temp_speed = student[i].speed;
			}
			i++;
		}
		
	}

	if(student[position].price < moneyamount ){	
		sem_post(&student[position].thread);
		return 1;
	}
	else{
		return -1;
	}
	
}
int isfilevalide(int fd){
  
    char ch ;
    while(read(fd,&ch,1)== 1){
        if(ch == 'C' || ch == 'S' || ch == 'Q' || ch == '\n'){
        	continue;
        }else{
            printf("unknown element found in the input file =>> %c\n !!!\n",ch );
            return -1 ;
        }
    }
    
    lseek(fd,0L,SEEK_SET);
    return 1 ;

}