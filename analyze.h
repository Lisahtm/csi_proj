#ifndef __ANALYZE_H__
#define __ANALYZE_H__

#define sample_T 2
#define sample_f 15
#define sample_shift 2
#define Group_num 90 
#define ChannelNum 3
#define RefreshNum 30


typedef struct calculateParam
{
	double buffer[3][sample_f*sample_T][RefreshNum];
	double *group_t;
	double *group_f;
	int type;
	int hasTimestamp;
	int group_index;
	int wave_index;
	int seq;
	pthread_mutex_t *pre_lock;
	pthread_mutex_t *own_lock;
} calculateParam;

void beepManager();
// filter
double enlarge_tranform(double t,double f);
int binary_transform(double data);
void print(FILE *fp,int timestamp,int value);
void print_next_filter(int timestamp,int current,int previous,int type,int wave_index);

void initCalculate();
void terminate();
double calculate_median(double* arr,int n);
int getProjectedIndex(int seq);
double get_corre(double buffer[][RefreshNum],int x,int y,int n,int isRow);

double get_selected_row(double* arr,int n);
double get_selected_column(double* arr,int n);

void push_sampling(double buffer[][RefreshNum],int seq,double data[]);
int isFull(int seq);
void adjust_buffer(double buffer[][RefreshNum]);

void quiksort(double* a,int low,int high);
int getJudgeIndex(double buffer[][RefreshNum],int type,int wave_index,int seq);
int getSelectedJudgeIndex(int data[]);
void calculateM(void *arg);//multi thread
void calculate(double buffer[][RefreshNum],double group_t[],double group_f[],int type,int hasTimestamp,int group_index,int wave_index);

double getSkewness(double arr[]);
double getKurtosis(double arr[]);

void insert_group(double time_g,double frequency_g,double arr_t[],double arr_f[],int group_index);

void startSocket();
void *read_socket();
void *write_socket();
/**************new version********************/
void push_sampling_plus(double buffer[][RefreshNum],int seq,double data[]);

#endif 