#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#define shmname "mysharedmem"

struct Queue {
    int front, rear, size;
    int capacity;
    int* array;
    int* pid_array;
    int pid_array_size;
    int* how_many_times;
    int Remaining;
};


struct shm{
    int vac1 ; // counter to hold the number of vac1 in the clinic
    int vac2 ; // counter to hold the number of vac2 in the clinic
    int total_vac1; //counter to hold all the vac1 in the clinic
    int total_vac2; //counter to hold all the vac2 in the clinic
    sem_t Kempty ; // semaphore to keep tracking empty spaces in the buffer/clinic , initial value = b
    sem_t Kmutex ; // binary semaphore to lock and unlock the vaccines counter vac1 and vac2 , initial value = 1 
    int vaccinator_loop_counter ;
    sem_t Vmutex;//
    sem_t N_V_mutex;//mutex between the nureses ans the vaccinators , when there is at least on dose or 1 and 2 the nurse should inform the vaccinator
    sem_t Qmutex; // mutex to inform the vaccinators that there are citizens waiting in the queue
    struct Queue Citizines_queue;
    int detector;
    int* vaccinators_;
    int* how_many_times_vaccinated;
    int vaccinators_size;
    
};



int parsing_commandline(int argc, char *const argv[] , int* n ,int* v , int* c , int* b ,int* t , char* fn);
int check_constraints(int n ,int v , int c , int b ,int t );
int isfilevalide(int fd,int tc2);
int nurse(int fd,int pid , int i);
int isFull(struct Queue* queue);
void enqueue(struct Queue* queue, int item);
int dequeue(struct Queue* queue);
struct Queue createQueue(int capacity,int c);
int vaccinator(int i , int vaccinator_id,int number_of_vaccinators , int t);
int min(int a , int b);
int citizen(int t , int pid );
int found(int item , int* arr , int size);

int main(int argc, char *const argv[]){

int n , v , c ,  b ,t; 
char filename[50];
int input;
struct shm *myshared ;
    if(parsing_commandline(argc,argv,&n,&v,&c,&b,&t,filename)==-1){
        printf("INVAILD COMMAND LINE ARGUMENT.\n");
        return -1;
    }
    if(check_constraints(n , v , c ,  b ,t)==-1){
        printf("constraints imposed \n");
        return -1 ;
    }
    int fd = shm_open(shmname , O_CREAT | O_RDWR , S_IRWXU );
    if(fd == -1){
        perror("open shared mem object ERROR :");
        return -1 ;
    }
    if(ftruncate(fd, sizeof(struct shm)) == -1){  /**/
        perror("ftruncate ERROR :");
        shm_unlink(shmname);
        return -1 ;
    }
    if((input = open(filename,O_RDONLY)) == -1){
        perror("open file ERROR :");
        shm_unlink(shmname);
        return -1 ; 
    }
    if(isfilevalide(input,2*t*c) == -1){
        shm_unlink(shmname);
        close(input);
        return -1 ;
    }

    myshared =(struct shm*) mmap(NULL , sizeof(struct shm) , PROT_READ | PROT_WRITE , MAP_SHARED , fd ,  0);
    myshared->total_vac1=0;
    myshared->total_vac2=0;
    myshared->vac1=0;
    myshared->vac2=0;
    myshared->vaccinator_loop_counter = t *c; 
    myshared->Citizines_queue = createQueue(t*c,c);
    myshared->detector = 0;
    myshared->vaccinators_ = mmap(NULL , (v)*sizeof(int) , PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,-1, 0);
    myshared->vaccinators_size = 0;
    myshared->how_many_times_vaccinated = mmap(NULL , (v)*sizeof(int) , PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,-1, 0);
   

    if(sem_init(&myshared->Kmutex ,1,1) == -1  || sem_init(&myshared->Kempty ,1,b) == -1
        ||sem_init(&myshared->Vmutex ,1,1) == -1||sem_init(&myshared->N_V_mutex ,1,0) == -1
        ||sem_init(&myshared->Qmutex ,1,0) == -1  ){
        perror("sem_init ERROR :");
        shm_unlink(shmname);
        return -1 ;
    }
    //creating nurses process
    int nurse_processes=0;
    for(nurse_processes = 0 ; nurse_processes < n ; nurse_processes++){
        int pid = fork();
        if(pid == -1){
            perror("unable to fork !! ");
            return -1;
        }
        else if(pid == 0){
            nurse(input,getpid(),nurse_processes);
            return 0 ;
        }
    }
    
    int vaccinators;
    for(vaccinators = 0 ; vaccinators < v; vaccinators++){
        int pid = fork();
        if(pid == -1){
            perror("unable to fork !! ");
            return -1;
        }
        else if(pid == 0){
           vaccinator(vaccinators,getpid(),v,t);
            return 0 ;
        }
    }
    //creating citizens 
    int citizens;
    for(citizens = 0 ; citizens < c ; citizens++){
        int pid = fork();
        if(pid == -1){
            perror("unable to fork !! ");
            return -1;
        }
        else if(pid == 0){
           citizen(t,getpid());
            return 0 ;
        }
    }
    int i;

    for(i = 0 ; i < n+v+c ; i++){
        wait(NULL);
    }
    for( i = 0 ; i < v ; i++)
        printf("Vaccinator %d (pid=%d) vaccinated %d doses." ,i+1,myshared->vaccinators_[i],myshared->how_many_times_vaccinated[i]+1) ;
    
    printf("The clinic is now closed. Stay healthy. \n"); 
    if(sem_destroy(&myshared->Kmutex ) == -1 || sem_destroy(&myshared->Kempty ) == -1 || sem_destroy(&myshared->N_V_mutex ) == -1 ||
        sem_destroy(&myshared->Vmutex ) == -1 || sem_destroy(&myshared->Qmutex) == -1  ){
        perror("sem_destroy ERROR :");
        shm_unlink(shmname);
        return -1 ;
    }   
    if(close(input)==-1){
        perror("close file ERROR :");
    }
    if(shm_unlink(shmname) ==-1){
        perror("close shared mem object ERROR :");
        return -1 ;
    }


return 0;

}

int parsing_commandline(int argc, char *const argv[] , int *n ,int* v , int* c , int* b ,int* t , char* fn){
    int opt;
        while((opt = getopt(argc, argv, ":n:v:c:b:t:i:")) != -1){   
        if(opt == 'n' && optind == 3){
            *n =atoi(optarg);
        }else if (opt == 'v' && optind == 5){
            *v = atoi(optarg);
        }else if (opt == 'c' && optind == 7){
            *c = atoi(optarg);
        }else if (opt == 'b' && optind == 9){
           *b = atoi(optarg);
        }else if (opt == 't' && optind == 11){
            *t = atoi(optarg);
        
        }else if (opt == 'i' && optind == 13){
            strcpy(fn,optarg); 
        }else{
            return -1 ;
        }   
    }
    if(optind != 13)
        return -1;        
    return 0 ;
}


int check_constraints(int n ,int v , int c , int b ,int t ){
    if(n>=2 && v>=2 && c>=3 && b >= t*c+1 && t>=1){
        return 1 ;
    }
    return -1 ;
}
int isfilevalide(int fd,int tc2){
    int vac1 = 0 ;
    int vac2 = 0 ;
    char ch ;
    while(read(fd,&ch,1)== 1){
        if(ch == '1'){
            vac1++;
        }else if (ch == '2'){
            vac2++;
        }else if(ch == '\n'){
        }else{
            printf("unknown element found in the input file =>> %c\n !!!\n",ch );
            return -1 ;
        }
    }
    if(vac2+vac1 < tc2){
        printf("the input file contains less than (2*t*c) bytes, can not continue !!!\n");
        return -1 ;
    }else if(vac2+vac1 > tc2){
        printf("the input file contains more than (2*t*c) bytes, can not continue !!!\n");
        return -1 ;
    }
    if( vac1 != vac2 ){
        printf("the input file input file should contain equal amont of all vaccines !!! vac1:%3d  vac2:%3d",vac1,vac2);
        return -1 ;
    }
    lseek(fd,0L,SEEK_SET);
    return 1 ;

}
int nurse(int filed,int pid , int i){

    int fd ;
    struct shm *myshared ;
    int  temp = 0 ,r;
    char ch;
    
    fd = shm_open(shmname , O_RDWR , S_IRWXU );
    if(fd == -1){
        perror("open shared mem object ERROR :");
        return -1 ;
    }

    myshared = (struct shm*)mmap(NULL , sizeof(struct shm) , PROT_READ | PROT_WRITE , MAP_SHARED , fd ,  0);
    
    while(1){
        //printf("nurse  %d waiting \n",i );
        sem_wait(&myshared->Kempty);
        sem_wait(&myshared->Kmutex);
        //printf("nurse %d is in ->>>>>>>>>>>>>>>> \n",i );
        if((r = read(filed,&ch,1)) == 0){
            sem_post(&myshared->Kmutex);
            //printf("Nurses have carried all vaccines to the buffer, terminating.\n");           
            return 0 ;
        }
        if(r == -1 ){
            
            perror("ERROR read file :");
            return -1 ;
        }
        if(ch == '1'){
            myshared->vac1++;
            myshared->total_vac1++;
            printf("Nurse %d (pid= %d) has brought vaccine 1: the clinic has %d vaccine1 and %d vaccine2.\n",i+1,pid,myshared->vac1,myshared->vac2); 
            
        }else if( ch == '2'){
            myshared->vac2++;
            myshared->total_vac2++;
            printf("Nurse %d (pid= %d) has brought vaccine 2: the clinic has %d vaccine1 and %d vaccine2\n",i+1,pid,myshared->vac1,myshared->vac2);        
        }else if(ch == '\n'){
           
        }else{
            printf("unknown kind of meals was found ( %c ) in input file !!!\n",ch );
            return -1 ;
        }
        //temp = minimum(myshared->total_vac1 , myshared->total_vac2 );
        
        temp = min(myshared->total_vac1 , myshared->total_vac2 );
        if(temp != myshared->detector){
            sem_post(&myshared->N_V_mutex);//if there are enough doeses then vaccinator should wake up
            myshared->detector = temp;
        }
        sem_post(&myshared->Kmutex);
       
    }


    return 0 ;  
}

int vaccinator(int i , int vaccinator_id,int number_of_vaccinators ,int tn){

    int fd ;
    struct shm *myshared ;
    
    fd = shm_open(shmname , O_RDWR , S_IRWXU );
    if(fd == -1){
        perror("open shared mem object ERROR :");
        return -1 ;
    }

    myshared = (struct shm*)mmap(NULL , sizeof(struct shm) , PROT_READ | PROT_WRITE , MAP_SHARED , fd ,  0);
    while(1){
        

        sem_wait(&myshared->N_V_mutex);
       
        sem_wait(&myshared->Kmutex);

          if(myshared->vaccinator_loop_counter == 0 ){
             sem_post(&myshared->Kmutex);
                 return 0;
          }
        myshared->vac2--;
        myshared->vac1--;
        sem_post(&myshared->Kempty);
        sem_post(&myshared->Kempty);
        sem_post(&myshared->Kmutex);
        sem_wait(&myshared->Qmutex); // cheching if the are citizenes waiting in the queue
        sem_wait(&myshared->Kmutex); //locking the critical region to read from the shared memory
        int pid = dequeue(&myshared->Citizines_queue);
        printf("Vaccinator %d (pid= %d ) is inviting citizen pid= %d to the clinic   \n",i+1,vaccinator_id, pid );
        int vac =found(vaccinator_id , myshared->vaccinators_ , myshared->vaccinators_size); 
        if(vac == -1){
            myshared->vaccinators_[ myshared->vaccinators_size] = vaccinator_id;
            myshared->vaccinators_size+=1;
        }
        else{
             myshared->how_many_times_vaccinated[vac] += 1;
        }
        int t = found(pid , myshared->Citizines_queue.pid_array , myshared->Citizines_queue.pid_array_size);
        int many_times = myshared->Citizines_queue.how_many_times[t];
        myshared->Citizines_queue.how_many_times[t]+= 1;
        if(many_times == tn-1){
            myshared->Citizines_queue.Remaining -= 1;
            printf("Citizen %d (pid= %d ) is vaccinated for the %dst time:  the clinic has %d vaccine1 and %d vaccine22. The citizen is leaving. Remaining citizens to vaccinate: %d \n",t+1,pid , many_times+1 , myshared->vac1 , myshared->vac2,myshared->Citizines_queue.Remaining);
       }
       else
        printf("Citizen %d (pid= %d ) is vaccinated for the %dst time:  the clinic has %d vaccine1 and %d vaccine2\n",t+1,pid , many_times+1 , myshared->vac1 , myshared->vac2);
        myshared->vaccinator_loop_counter--;
        if(myshared->vaccinator_loop_counter == 0 ){
            for(i = 0 ; i < number_of_vaccinators-1 ; i++)
                sem_post(&myshared->N_V_mutex);
            sem_post(&myshared->Kmutex);
            return 0;
        }
        sem_post(&myshared->Kmutex);
    }
    return 0;

}
int isFull(struct Queue* queue) // if the queue is full then there must be a problem 
{
    return (queue->size == queue->capacity);
}
void enqueue(struct Queue* queue, int item)
{
    if (isFull(queue)){
        perror("the queue cant be full ... something wrong has happend!! \n");
        return;
    }
   
    queue->rear = (queue->rear + 1)
                  % queue->capacity;
    queue->array[queue->rear] = item;
    if(found(item , queue->pid_array , queue->pid_array_size) == -1){
        queue->pid_array[queue->pid_array_size] = item;
        queue->pid_array_size  =queue->pid_array_size+1;
    }
    queue->size = queue->size + 1;
}

int dequeue(struct Queue* queue)
{
    int item = queue->array[queue->front];
    queue->front = (queue->front + 1)
                   % queue->capacity;
    queue->size = queue->size - 1;
    return item;
}
struct Queue createQueue(int capacity , int c)
{
    struct Queue queue ;
    queue.capacity = capacity;
    queue.front = queue.size = 0;
    queue.rear = capacity - 1;
    queue.array = mmap(NULL , (capacity)*sizeof(int) , PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,-1, 0);
    queue.pid_array =  mmap(NULL , (c)*sizeof(int) , PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,-1, 0);
    queue.how_many_times =  mmap(NULL , (c)*sizeof(int) , PROT_READ|PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,-1, 0);
    queue.pid_array_size = 0;
    queue.Remaining = c;
    return queue;
}
int min(int a , int b){
    if(a < b)
        return a;
    return b;
}
int citizen(int t , int pid ){
    struct shm *myshared ;
    
    int fd = shm_open(shmname , O_RDWR , S_IRWXU );
    if(fd == -1){
        perror("open shared mem object ERROR :");
        return -1 ;
    }

    myshared = (struct shm*)mmap(NULL , sizeof(struct shm) , PROT_READ | PROT_WRITE , MAP_SHARED , fd ,  0);
   
    int i = 0;
    for( i = 0 ; i < t ; i++){
        sem_wait(&myshared->Kmutex);
        enqueue(&myshared->Citizines_queue,pid);
        sem_post(&myshared->Kmutex);
        sem_post(&myshared->Qmutex);
        
    }
    return 0;
}
int found(int item , int* arr , int size){
    if(size == 0)
        return -1;
    else{
        int i;
        for(i = 0 ; i < size ; i++){
            if(arr[i] == item)
                return i;
        }
    }
    return -1;
}