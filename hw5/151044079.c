///** the program takes time to read all finish all the requests 
/// 




#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>

#define QUEUE_SIZE 50
#define DEBUG 0

struct requests
{
	char client_name[20];
	double X, Y;
	char flower_type[20];
	double distance;
};
struct queue
{
	int write_index;
	int read_index;
	int buffer_size;
	int size;
	struct requests request[QUEUE_SIZE];
};
struct Florists
{
	char name[20];
	double X;
	double Y;
	int delivered_requests;
	double speed;
	char **flowerTypes;
	int exit;
	int flowerTypes_counter;
	int flowerType_size;
	pthread_t thread_id;
	double delivery_time ; 
	struct queue request_queue;
	pthread_mutex_t mutex;
	pthread_cond_t cond_var;
};

// QUEUE Functions
int queue_init(struct queue *Q);
int enqueue(struct queue *Q, char *name, double x, double y, char *flower_type, double distance);
int dequeue(struct queue *Q, struct requests *req);

void parse_arg(int argc, char *const argv[], char *inputPathA);
struct Florists *readFile(int fd, int *florisits_size);
struct Florists *read_florists_from_file(char *buffer, int *florists_count);
int min_dis(double x, double y, char *flower_type, struct Florists *F, int florists_count, double *min_distance);
double find_distance(double x1, double y1, double x2, double y2);
int compare(char *flower_name, char **flowerTypes, int number_of_flowers);
void parssing_requests(char *buffer, char *clients_name, double *client_x, double *client_y, char *flower);

//Florist functions
void *florists_job(void *queue);
double sleep_duration(double distance, double speed);

//signal handler
void handle_prof_signal();
static volatile int signal_arrived;

static void sigint_handler()
{
	signal_arrived = 1;
	fprintf(stderr, "Signal was handled. Please wait. All thread will have exited in a second.");
}

int main(int argc, char *const argv[])
{
	char inputPathA[20];
	int inputFile;
	int florists_count = 0;

	//Signal handling
	struct sigaction sig_action;
	memset(&sig_action, 0, sizeof(sig_action));
	sig_action.sa_sigaction = sigint_handler;
	sig_action.sa_flags = SA_RESTART | SA_SIGINFO;
	sigemptyset(&sig_action.sa_mask);
	sigaction(SIGINT, &sig_action, 0);

	parse_arg(argc, argv, inputPathA);
	if ((inputFile = open(inputPathA, O_RDONLY)) == -1)
	{
		perror("failed to open input file");
		return -1;
	}
	struct Florists *florists = readFile(inputFile, &florists_count);
	int i, return_val, bytes_read, size = 1024, counter = 0, j = 0, min_index = 0;
	char *req = (char *)malloc(size * sizeof(char));
	char clients_name[20], flower[20], buf;
	double client_x = 0, client_y = 0, min_distance = 0;

	for (i = 0; i < florists_count; i++)
	{
		return_val = pthread_cond_init(&florists[i].cond_var, NULL);
		if (return_val != 0)
		{
			printf("pthread_cond_init return %d", return_val);
		}
		return_val = pthread_mutex_init(&florists[i].mutex, NULL);
		if (return_val != 0)
		{
			printf("pthread_mutex_init return %d", return_val);
		}
		pthread_create(&florists[i].thread_id, NULL, florists_job, (void *)&(florists[i]));
#if DEBUG == 0
		fprintf(stderr, "%s florist thread created \n", florists[i].name);
#endif
	}

	while (signal_arrived == 0)
	{
		bytes_read = read(inputFile, &buf, 1);

		if (buf == '\n' || bytes_read == 0)
		{

			parssing_requests(req, clients_name, &client_x, &client_y, flower);

			min_index = min_dis(client_x, client_y, flower, florists, florists_count, &min_distance);
			if (min_index != -1)
			{
				pthread_mutex_lock(&(florists[min_index].mutex));
				while (enqueue(&(florists[min_index].request_queue), clients_name, client_x, client_y, flower, min_distance) == -1)
				{
					pthread_cond_wait(&(florists[min_index].cond_var), &(florists[min_index].mutex));
				}
				pthread_cond_broadcast(&(florists[min_index].cond_var));
				pthread_mutex_unlock(&(florists[min_index].mutex));
			}
			else
			{
#if DEBUG
				fprintf(stderr, "Error occured. Florist not found\n");
#endif
			}
#if DEBUG

			fprintf(stderr, "Parse Florist: %s  %lf  %lf  %s \n", clients_name, client_x, client_y, flower);

#endif
			counter = 0;
			req[0] = '\0';
			clients_name[0] = '\0';
			flower[0] = '\0';
			if (bytes_read == 0)
				break;
		}
		req[counter] = buf;
		req[counter + 1] = '\0';
		counter++;
	}

	i = 0;
	do
	{
		if (florists[i].request_queue.size == 0 || signal_arrived == 1)
		{
			florists[i].exit = 1;
			pthread_mutex_lock(&(florists[i].mutex));
			pthread_cond_broadcast(&(florists[i].cond_var));
			pthread_mutex_unlock(&(florists[i].mutex));
			pthread_join(florists[i].thread_id, NULL);
			++i;
		}
		if (signal_arrived == 0)
			usleep(500000);
#if DEBUG
		fprintf(stderr, "Waiting %s %d %d %d %d\n", florists[i].name, florists[i].request_queue.size, i, (florists[i].request_queue.size != 0), (i < florists_count));
#endif
	} while ((florists[i].request_queue.size != 0) || (i < florists_count));
	printf("All requests processed.\n");
	for(i = 0 ; i  < florists_count ; i++){
		printf("%s closing shop \n",florists[i].name );
	}
	printf("Sale statistics for today: \n");
	printf("----------------------------------------\n");
	printf("Florist  # of sales    Total time\n");
	for(i = 0 ; i  < florists_count ; i++ ){
		printf("%s       %d      %lf  \n",florists[i].name,florists[i].delivered_requests,florists[i].delivery_time );
	}


	for (i = 0; i < florists_count; i++)
	{
		for (j = 0; j < florists[i].flowerType_size; j++)
		{
			free(florists[i].flowerTypes[j]);
			florists[i].flowerTypes[j] = NULL;
		}
		free(florists[i].flowerTypes);
		florists[i].flowerTypes = NULL;
	}
	free(florists);
	free(req);
	florists = NULL;
	req = NULL;
	close(inputFile);
#if DEBUG
	fprintf(stderr, "Main thread exited\n");
#endif

	return 0;
}

void parse_arg(int argc, char *const argv[], char *inputPathA)
{
	int t = 0;
	int opt;
	while ((opt = getopt(argc, argv, "i:")) != -1)
	{

		if (opt == 'i' && optind == 3)
		{
			strcpy(inputPathA, optarg);
			t = 1;
		}
		else
		{
			printf("unknown option: %c\n", opt);
			exit(1);
		}
	}
	if (t == 0)
	{
		printf("input path is missing ,,, please check the input line again\n");
		exit(1);
	}
}

struct Florists *readFile(int fd, int *florists_count)
{
	char buf;
	char prev;
	int size = 1024;
	int counter = 0;
	struct Florists *florists = NULL;
	char *buffer = (char *)malloc(size * sizeof(char));
	//struct Florists* temp_florists ;
	int bytes_read;
	while (((bytes_read = read(fd, &buf, 1)) == 1))
	{
		if (prev == '\n' && buf == '\n')
		{
			florists = read_florists_from_file(buffer, florists_count);
			free(buffer);
			buffer = NULL;
			return florists;
		}

		prev = buf;
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
struct Florists *read_florists_from_file(char *buffer, int *florists_count)
{
	int size = 10;
	int size1 = 10;
	int count = 0;
	char name[20];
	char c;
	int i = 0;
	int j;
	double x, y;
	double speed;
	struct Florists *temp_florists = (struct Florists *)malloc(size * sizeof(struct Florists));

	char *a = strtok(buffer, "\n");
	//Murat (-10,8; 1.1) : violet, daffodil, orchid
	while (a != NULL)
	{
		*florists_count += 1;
		sscanf(a, "%s %c %lf %c %lf %c %lf %c %c ", name, &c, &x, &c, &y, &c, &speed, &c, &c);
		temp_florists[count].X = x;
		temp_florists[count].Y = y;
		strcpy(temp_florists[count].name, name);
		temp_florists[count].speed = speed;
		temp_florists[count].exit = 0;
		temp_florists[count].delivered_requests = 0;
		temp_florists[count].flowerTypes_counter = 0;
		temp_florists[count].delivery_time = 0;
		temp_florists[count].flowerType_size = size1;
		queue_init(&(temp_florists[count].request_queue));
		//pthread_mutex_init(&(temp_florists[count].mutex), NULL);
		//pthread_cond_init(&(temp_florists[count].cond_var), NULL);

		temp_florists[count].flowerTypes = (char **)malloc(size1 * sizeof(char *));
		for (i = 0; i < size1; ++i)
		{
			temp_florists[count].flowerTypes[i] = (char *)malloc(25 * sizeof(char));
		}
		i = 0;
		char *temp;
		char *a1 = (char *)malloc(strlen(a) * sizeof(char));
		char *temp_a1;
		strcpy(a1, a);
		temp = strtok_r(a1, ":", &temp_a1);
		temp = strtok_r(temp_a1, ", ", &temp_a1);
		if (temp == NULL)
		{
			free(a1);
			a1 = NULL;
			continue;
		}
		while (temp != NULL)
		{
			strcpy(temp_florists[count].flowerTypes[i], temp);
			temp_florists[count].flowerTypes_counter++;
			temp = strtok_r(temp_a1, ", ", &temp_a1);

			i++;
			if (i == size1)
			{
				int temp_size = size1;
				size1 *= 2;
				temp_florists[count].flowerType_size = size1;
				temp_florists[count].flowerTypes = realloc(temp_florists[count].flowerTypes, size1 * sizeof(char *));
				for (j = temp_size - 1; j < size1; ++j)
				{
					temp_florists[count].flowerTypes[j] = (char *)malloc(25 * sizeof(char));
				}
			}
		}
		free(a1);
		a1 = NULL;
		i = 0;
		count++;
		if (size == count)
		{
			size *= 2;
			temp_florists = (struct Florists *)realloc(temp_florists, size * sizeof(struct Florists));
		}
		a = strtok(NULL, "\n");
	}

	return temp_florists;
}

int min_dis(double x, double y, char *flower_type, struct Florists *F, int florists_count, double *min_distance)
{
	int i = 0;
	double temp_distance = 0;
	int min_index = -1;
	int check_if_contain_flower = 0;

	while (i < florists_count)
	{
		check_if_contain_flower = compare(flower_type, F[i].flowerTypes, F[i].flowerTypes_counter);
		if (check_if_contain_flower == 1)
		{
			temp_distance = find_distance(x, y, F[i].X, F[i].Y);
			if (temp_distance < *min_distance || min_index == -1)
			{
				*min_distance = temp_distance;
				min_index = i;
			}
		}
		i++;
	}
	return min_index;
}
double find_distance(double x1, double y1, double x2, double y2)
{
	double a = x1 - x2;

	double b = y1 - y2;

	if (abs(a) < abs(b))
		return abs(b);
	else
		return abs(a);
}
int compare(char *flower_name, char **flowerTypes, int number_of_flowers)
{
	int i = 0;
	while (i < number_of_flowers)
	{
		if (strcmp(flower_name, flowerTypes[i]) == 0)
			return 1;
		i++;
	}
	return 0;
}
void parssing_requests(char *buffer, char *clients_name, double *client_x, double *client_y, char *flower)
{
	char c;
	sscanf(buffer, "%s %c %lf %c %lf %c %c %s ", clients_name, &c, client_x, &c, client_y, &c, &c, flower);
}

//-----------------------FLORIST Functions-----------------------------

void *florists_job(void *queue)
{
	struct Florists *thread_arg = (struct Florists *)queue;
	int ret_val;
	struct requests req;
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	pthread_sigmask(SIG_BLOCK, &mask, NULL);

#if DEBUG
	fprintf(stderr, "Florist %s started\n", thread_arg->name);
#endif
	while (thread_arg->exit == 0)
	{
#if DEBUG
		fprintf(stderr, "Running %s\n", thread_arg->name);
#endif
		pthread_mutex_lock(&(thread_arg->mutex));
		if (thread_arg->request_queue.size == 0)
		{
			pthread_cond_wait(&(thread_arg->cond_var), &(thread_arg->mutex));
		}
		ret_val = dequeue(&(thread_arg->request_queue), &req);
		pthread_cond_broadcast(&(thread_arg->cond_var));
		pthread_mutex_unlock(&(thread_arg->mutex));
		if (ret_val == 0)
		{
			double delivery_time = sleep_duration(req.distance, thread_arg->speed);
			thread_arg->delivered_requests += 1;
			thread_arg->delivery_time += delivery_time * 1000;
			printf("Florist %s has delivered a %s to %s in %.1fms\n", thread_arg->name, req.flower_type, req.client_name, delivery_time * 1000);
		}
	}
	
	pthread_exit(NULL);
}

double sleep_duration(double distance, double speed)
{
	srand(time(0));
	double preparation_time = ((rand() % (251))) ;
	double delivery_time = (distance / speed) ;

#if DEBUG
	fprintf(stderr, "Distance:%f\nDelivery Time: %f\nPreparation Time: %f\nSleep Duration: %f\n", distance, delivery_time, preparation_time, 1000*(delivery_time + preparation_time));
#endif
	usleep(1000*(preparation_time + delivery_time));
	return (preparation_time + delivery_time)/1000 ;
}

//-----------------------QUEUE Functions-----------------------------

int queue_init(struct queue *Q)
{
	if (Q == NULL)
	{
		return -1;
	}
	Q->write_index = 0;
	Q->read_index = 0;
	Q->buffer_size = QUEUE_SIZE;
	Q->size = 0;
	return 0;
}
int enqueue(struct queue *Q, char *name, double x, double y, char *flower_type, double distance)
{
	if (Q->size >= Q->buffer_size)
	{
		return -1;
	}
	Q->request[Q->write_index].X = x;
	Q->request[Q->write_index].Y = y;
	Q->request[Q->write_index].distance = distance;
	strcpy(Q->request[Q->write_index].flower_type, flower_type);
	strcpy(Q->request[Q->write_index].client_name, name);
	Q->write_index = (Q->write_index + 1) % QUEUE_SIZE;
	Q->size += 1;
	return 0;
}

int dequeue(struct queue *Q, struct requests *req)
{

	if (Q->size <= 0)
	{
		return -1;
	}
	req->X = Q->request[Q->read_index].X;
	req->Y = Q->request[Q->read_index].Y;
	req->distance = Q->request[Q->read_index].distance;
	strcpy(req->client_name, Q->request[Q->read_index].client_name);
	strcpy(req->flower_type, Q->request[Q->read_index].flower_type);
	Q->read_index = (Q->read_index + 1) % QUEUE_SIZE;
	Q->size -= 1;
	return 0;
}