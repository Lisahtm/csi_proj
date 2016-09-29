#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include <float.h>
#include<string.h>
#include"analyze.h"
#include"iwl_define.h"
#include"sys/time.h"
#include"sys/stat.h"
#include<unistd.h>
#include <pthread.h>  

extern char paramGraphic[20];
extern FILE *basep[3],*skewp[3],*fpR[3];;
extern int timestamp_current;
extern char testChannel[4];
extern int hasManChannelNumber;
extern int isWriteFile;

//create related files
void initCalculate(){

	if(access("data_out",F_OK)==-1){
		if(mkdir("data_out",0777)){
			printf("make file failed!\n");
		}
	}
	basep[0] = fopen("./data_out/baseA.txt","w");
	basep[1] = fopen("./data_out/baseB.txt","w");
	basep[2] = fopen("./data_out/baseC.txt","w");
	// skewp[0] = fopen("./data_out/skewA.txt","wa");
	// skewp[1] = fopen("./data_out/skewB.txt","wa");
	// skewp[2] = fopen("./data_out/skewC.txt","wa");

	fpR[0] = fopen("./data_out/wave.txt","w");
}
void terminate(){
	fclose(basep[0]);
	fclose(basep[1]);
	fclose(basep[2]);
	fclose(fpR[0]);	
}
//skewness
double getSkewness(double arr[]){
	int i;
	double avg=0,var=0,S=0,skewness=0;
	for (i = 0; i < Group_num; i++){
		avg+=arr[i];
	}
	avg = avg/Group_num;

	for (i = 0; i < Group_num; i++)
    {
        var += (arr[i] - avg)*(arr[i] - avg);
    }
	var = (var)/(Group_num - 1);
    S = sqrt(var);
	for (i = 0; i < Group_num; i++)
        skewness += (arr[i] - avg)*(arr[i] - avg)*(arr[i] - avg);
	if(S==0){
		return -DBL_MAX;
	}
	skewness = skewness/(Group_num * S * S * S);
	return skewness;
}
//kurtosis
double getKurtosis(double arr[]){
	int i;
	double avg=0,var=0,S=0,k=0;
	for (i = 0; i < Group_num; i++){
		avg+=arr[i];
	}
	avg = avg/Group_num;

	for (i = 0; i < Group_num; i++)
    {
        var += (arr[i] - avg)*(arr[i] - avg);
    }
	var = (var)/(Group_num - 1);
    S = sqrt(var);

	for (i = 0; i < Group_num; i++)
        k += (arr[i] - avg)*(arr[i] - avg)*(arr[i] - avg)*(arr[i] - avg);
	if(S==0){
		return -DBL_MAX;
	}
	k = k/(Group_num*S*S*S*S);
    k -= 3; 
	return k;
}
//get correlation 
double get_corre(double buffer[][RefreshNum],int x,int y,int n,int isRow){
	double r,nr=0,dr_1=0,dr_2=0,dr_3=0,dr=0;
	double *xx,*xy,*yy;
	double sum_y=0,sum_yy=0,sum_xy=0,sum_x=0,sum_xx=0,sum_x2=0,sum_y2=0;
	int i;

	//initialize
	xx =  (double*)malloc(n*sizeof(double));
	xy =  (double*)malloc(n*sizeof(double));
	yy =  (double*)malloc(n*sizeof(double));

	for(i=0;i<n;i++)
	{
		if(isRow == 1 ){
			xx[i]=buffer[x][i]*buffer[x][i];
			yy[i]=buffer[y][i]*buffer[y][i];
		}else{
			xx[i]=buffer[i][x]*buffer[i][x];
			yy[i]=buffer[i][y]*buffer[i][y];		
		}

	}
	for(i=0;i<n;i++)
	{
		if(isRow == 1 ){
			sum_x+=buffer[x][i];
			sum_y+=buffer[y][i];
			sum_xy+=buffer[x][i]*buffer[y][i]; 
		}else{
			sum_x+=buffer[i][x];
			sum_y+=buffer[i][y];	
			sum_xy+=buffer[i][x]*buffer[i][y]; 
		}

		sum_xx+= xx[i];
		sum_yy+=yy[i];
		    
	}
	nr=(n*sum_xy)-(sum_x*sum_y);
	sum_x2=sum_x*sum_x;
	sum_y2=sum_y*sum_y;

	dr_1=(n*sum_xx)-sum_x2;
	dr_2=(n*sum_yy)-sum_y2;
	dr_3=dr_1*dr_2;
	dr=sqrt(dr_3);
	if(dr == 0){
		return -DBL_MAX;
	}
	r=(nr/dr);

	free(xx);
	free(yy);
	free(xy);
	//printf("Correlation Coefficient:%.2lf\n",r);
	return fabs(r);
}
//get median row
double get_selected_row(double* arr,int n){
	return calculate_median(arr,n);
}
//get median column
double get_selected_column(double* arr,int n){
	return calculate_median(arr,n);
}
//push buffer from new version (just add the new one in the front)
void push_sampling_plus(double buffer[][RefreshNum],int seq,double data[]){
	int window_size = sample_f*sample_T;
	int index = seq % window_size,i;
	for(i = 0;i<RefreshNum;i++){
		buffer[index][i] = data[i];
	}
}
//judge whether the seq is full 
int isFull(int seq){
	int window_size =  sample_f*sample_T;
	int shift = sample_shift;
	if(seq < window_size-1){
		return 0;
	}
	if(seq == window_size-1){
		return 1;
	}
	if((seq-window_size) % shift == shift-1){
		return 1;
	}else{
		return 0;
	}
}
void quiksort(double* a,int low,int high)
{
	int i = low;
	int j = high;  
	double temp = a[i]; 

	if( low < high)
	{          
		while(i < j) 
		{
			while((a[j] >= temp) && (i < j))
			{ 
				j--; 
			}
			a[i] = a[j];
			while((a[i] <= temp) && (i < j))
			{
				i++; 
			}  
			a[j]= a[i];
		}
		a[i] = temp;
		quiksort(a,low,i-1);
		quiksort(a,j+1,high);
	}
	else
	{
		return;
	}
}
//Get the median from an array
double calculate_median(double* arr,int n)
{
	//sort first
	int i;
	double* number = (double*)malloc(n*sizeof(double)),result;
	for(i = 0;i<n;i++){
		number[i] = arr[i];
	}
	quiksort(number,0,n-1);
	//then find median
	if(n%2==0)
		result = (number[n/2-1]+number[n/2])/2.0;
	else
		result =  number[n/2];
	free(number);
	return result;
}
//User can select the channel calculated
int isTestChannel(int index){
	if(testChannel[index] == '1'){
		return 1;
	}else{
		return 0;
	}
}
//previous binary form of the data
int previousFT[3] = {0,0,0};
//multi thread calculation
void calculateM(void *arg){
	int ABC_index;
	int currentFT=0;
	int previousTmp =0;
	calculateParam *param;
	int type;
	int FTtmp[3];
	struct timeval tpstart,tpend;
	float timeuse;

	param = (calculateParam*)arg;	
	type = param->type;

	//get time params
	gettimeofday(&tpstart,NULL);
	if(param->hasTimestamp){
		timestamp_current -= sample_T*30;
		// fprintf(basep[type], "%d ", timestamp_current);
	}else{
		timestamp_current = -1;
	}
	/*****start filter the wave************/
	printf("%d:",param->wave_index);
	for(ABC_index = 0;ABC_index<ChannelNum;ABC_index++){	
		/**************calculate correlation coeffient******************/
		/**************We can select the calculated channel ourselves******************/
		if(isTestChannel(ABC_index)){
			FTtmp[ABC_index] = getJudgeIndex(param->buffer[ABC_index],ABC_index,param->wave_index,param->seq);
		}
	}
	currentFT = getSelectedJudgeIndex(FTtmp);
	// printf("wave:%d\n",currentFT);
	/***LOCK to ensure that the wave is calculated is in order********/
	if(param->pre_lock&&param->wave_index!=0){
		pthread_mutex_lock(param->pre_lock);//make sure previous one is done
	}
	previousTmp = previousFT[type];
	previousFT[type] = currentFT;
	print_next_filter(timestamp_current,currentFT,previousTmp,type,param->wave_index);

	/*********Destroy the lock****************/
	pthread_mutex_unlock(param->own_lock);  //release own lock to let next to use
	if(param->pre_lock){
		pthread_mutex_unlock(param->pre_lock);
		pthread_mutex_destroy(param->pre_lock);//previous lock is useless,free it;		
	}
	/**********end filter the wave**************/
	gettimeofday(&tpend,NULL);
	timeuse = 1000000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec;
	timeuse/=1000000;

	printf("time:%.3lf\n",timeuse);

	free(arg);
	// printf("%d\n",param->wave_index);
}
//We get three values from Channel A,B,C, and we have a rule to achieve final result from the values of A,B,C
int getSelectedJudgeIndex(int data[]){
	int sum = 0,i;
	for(i = 0;i<ChannelNum;i++){
		sum+=data[i];
	}
	if(sum >= hasManChannelNumber){
		return 1;
	}else{
		return 0;
	}
}

//Use transformed data to calculate correlations of both row and column, then turn results into binary value
int getJudgeIndex(double buffer[][RefreshNum],int type,int wave_index,int seq){
	int row_num = sample_f*sample_T;
	int i,j;
	double column_result[RefreshNum*(RefreshNum-1)/2],row_result[sample_f*sample_T*(sample_f*sample_T-1)/2];
	// double *row_result = row_result_copy[type];
	int row_index=0,column_index=0;
	//get final result
	double row_final,column_final;
	double tmpvalue;
	int result;
	//time
	time_t timeT;
	struct tm *p;
	char typechar;

	//row calculate
	for(i = 0;i < row_num;i++ ){
		for(j = i+1;j < row_num;j++){
			row_result[row_index++]=get_corre(buffer,i,j,RefreshNum,1);
		}
	}
	//get median
	row_final = get_selected_row(row_result,row_index);
	//column calculate
	for(i = 0;i < RefreshNum;i++ ){
		for(j = i+1;j < RefreshNum;j++){
			column_result[column_index++]=get_corre(buffer,i,j,row_num,0);
		}
	}
	//get median
	column_final = get_selected_row(column_result,column_index);
	//
	// insert_group(row_final,column_final,param->group_t,param->group_f,param->group_index);

	tmpvalue = enlarge_tranform(row_final,column_final);
	result = binary_transform(tmpvalue);

	//print
	time(&timeT);
	p = localtime(&timeT);
	if(isWriteFile){
		// fprintf(basep[type],"%02d:%02d:%02d %d %.6lf %d\n",p->tm_hour,p->tm_min,p->tm_sec,wave_index,tmpvalue,result);
		fprintf(basep[type],"%.4lf %.4lf\n",row_final,column_final);
		fflush(basep[type]);		
	}
	/************Print the data to the screen************************/
	if(type == 0 ){
		typechar = 'A';
	}else if(type == 1 ){
		typechar = 'B';
	}else if(type == 2 ){
		typechar = 'C';
	}
	printf("%.4lf(%c) ",tmpvalue,typechar);
	fflush(stdout);

	return result;

}
//Calculate in single thread
void calculate(double buffer[][RefreshNum],double group_t[],double group_f[],int type,int hasTimestamp,int group_index,int wave_index){
	int row_num = sample_f*sample_T;
	int i,j;
	double row_result[sample_f*sample_T*(sample_f*sample_T-1)/2],column_result[RefreshNum*(RefreshNum-1)/2];
	int row_index=0,column_index=0;
	//get final result
	double row_final,column_final;
	struct timeval tpstart,tpend;
	float timeuse;
	// char testStr[100]="";
	int currentFT=0;
	double tmpvalue;

	gettimeofday(&tpstart,NULL);
	//row calculate
	for(i = 0;i < row_num;i++ ){
		for(j = i+1;j < row_num;j++){
			if(i != j){
				row_result[row_index++]=get_corre(buffer,i,j,RefreshNum,1);
			}
		}
	}
	//getmedian
	row_final = get_selected_row(row_result,row_index);
	//column calculate
	for(i = 0;i < RefreshNum;i++ ){
		for(j = i+1;j < RefreshNum;j++){
			if(i != j){
				column_result[column_index++]=get_corre(buffer,i,j,row_num,0);
			}
		}
	}
	column_final = get_selected_row(column_result,column_index);
	//
	insert_group(row_final,column_final,group_t,group_f,group_index);

	gettimeofday(&tpend,NULL);
	timeuse = 1000000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec;
	timeuse/=1000000;
	if(hasTimestamp){
		timestamp_current -= sample_T*30;
		// fprintf(basep[type], "%d ", timestamp_current);
	}else{
		timestamp_current = -1;
	}
	// printf("(%.4lf,%.4lf,%.4lf) ",row_final,column_final,timeuse);
	// fprintf(basep[type],"%.4lf %.4lf\n",row_final,column_final);
	//*****start filter the wave************
	tmpvalue = enlarge_tranform(row_final,column_final);
	currentFT = binary_transform(tmpvalue);
	//to ensure the wave calculating is in order


	print_next_filter(timestamp_current,currentFT,previousFT[type],type,wave_index);
	previousFT[type] = currentFT;
	//**********end filter the wave**************


	fflush(basep[type]);

	//socket param
	// memset(paramGraphic,0,sizeof(paramGraphic));
	// gcvt(row_final,4,testStr);
	// strcat(paramGraphic,testStr);
	// strcat(paramGraphic," ");
	// gcvt(column_final,4,testStr);
	// strcat(paramGraphic,testStr);
	// strcat(paramGraphic,"\n");



}
//insert time_group,frequency group
void insert_group(double time_g,double frequency_g,double arr_t[],double arr_f[],int group_index){
	arr_t[group_index] = time_g;
	arr_f[group_index]=frequency_g;
}