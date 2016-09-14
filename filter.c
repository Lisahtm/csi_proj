#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include <float.h>
#include "analyze.h"
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

extern int errno;

double binary_threshold = 0.9;
int short_len = 10;
int dist_len = 50;

int pre_index[3] = {-1,-1,-1};
int wave_len[3]={0,0,0};
int pre_wave_len[3] = {0,0,0};
int has_first_short[3] = {0,0,0};//此波之前是否有短波（长度没有达到阈值的波）
int isCurrentLong[3] = {0,0,0};
int isIntervalLong[3] = {0,0,0};//当前的波是否比阈值大;
int smallConjunLen[3] = {0,0,0};
extern FILE *fpR[3];
extern int isWriteFile;
pthread_t pid,pid2;

int hasMan = 0;//判断是否有人的状态符


double enlarge_tranform(double t,double f){
	return t*exp(-0.1*f);
}
int binary_transform(double data){
	if(data < binary_threshold)
		return 1;
	return 0;
}


void beepManager(){
	while(1){
		if(hasMan){	
			system("/root/beep0.sh");
			sleep(1);
		}		
	}

}

void print(FILE *fp,int timestamp,int value){
	// printf("%d\n",fp);
	if(timestamp == -1){
		// fprintf(fp,"%d ",value);
		if(fp == fpR[0]){
			// printf("%d ",value);
			// fflush(stdout);
			if(isWriteFile){
				fprintf(fp,"%d ",value);
				fflush(fp);
			}
			if(value==1&&hasMan==0){
				hasMan = 1;
				// pthread_create(&pid,NULL,(void *)beepStart,NULL);
				
			}
			if(value==0&&hasMan==1){
				hasMan = 0;
				// pthread_create(&pid2,NULL,(void *)beepStop,NULL);

			}
		}
		// fflush(stdout);
	}else{
		//fprintf(fp,"%d %d\n",timestamp,value);
		printf("%d ",value);
		fflush(stdout);
	}
	
}
void print_next_filter(int timestamp,int current,int previous,int type,int wave_index){

	int j;

	if(current == 1){
			wave_len[type]++;

			if(wave_len[type] >= short_len || isCurrentLong[type]){
				if(isCurrentLong[type]){					
					print(fpR[type],timestamp,1);
				}else{
					for(j = wave_index - wave_len[type] + 1;j<=wave_index;j++){
						print(fpR[type],timestamp,1);
					}
					isCurrentLong[type] = 1;				
				}

			}
			//deal with the previous short wave
			if(wave_index>=1 && previous==0){
				isIntervalLong[type] = 0;
				if(pre_index[type]!=-1 && wave_index-pre_index[type]-1 <= dist_len)//if the previous short wave's distance is less than threshold
				{					
					//如果前面有短波
					if(has_first_short[type]){
						//前面有短波，将这两个短波合并，变成稍微长的短波
						if(smallConjunLen[type] < short_len && smallConjunLen[type]+wave_index-pre_index[type]+pre_wave_len[type] < short_len){
							smallConjunLen[type] += (pre_wave_len[type]+wave_index-pre_index[type]-1);
						}else{
							//如果前面的短波到目前的波的长度已经大于阈值，则要一起输出
							//需要输出自己，并且保证接下来的波都可以直接输出
							isCurrentLong[type] = 1; 
							for(j = pre_index[type]-pre_wave_len[type]+1-smallConjunLen[type];j<=wave_index;j++){
								print(fpR[type],timestamp,1);
							}	
							smallConjunLen[type] = 0;
							has_first_short[type] = 0;
						}

					}else{
						//前面没有短波（是一个长波），则也把与前面波之前的夹着的0变成1，接着输出本身
						isCurrentLong[type] = 1; 

						for(j = pre_index[type]+1;j<=wave_index;j++){
							print(fpR[type],timestamp,1);
						}					
					}
				}
				///如果一开始为0且0的个数小于阈值的情况,把前面的0都输出
				if(pre_index[type] == -1 && wave_index < dist_len){
					for(j = pre_index[type]+1;j<wave_index;j++){
							print(fpR[type],timestamp,0);
					}
				}

			}else{
				//如果现在在波的中央（不是第一个），如果前面还有短波，说明短波并没有和此波合并，需要把短波的总长加1
				if(has_first_short[type]){
					//当小波积攒成大于阈值的大波时，将其全部输出
					if(smallConjunLen[type]+wave_len[type]>= short_len){
						for(j = wave_index-smallConjunLen[type]-wave_len[type]+1;j<=wave_index;j++){
							print(fpR[type],timestamp,1);
						}	
						//将值初始化
						has_first_short[type] = 0;
						smallConjunLen[type] = 0;
						//此波的长度已经大于最短阈值，可以直接输出了
						isCurrentLong[type] = 1;
					}
				}
			}
		}else{
			//if i is the last index,it's possible that i will be unioned to the previous one
			if(wave_index>=0&&previous == 1)
			{			
				if(wave_len[type]<short_len //if the length of the wave is lower than the threshold.
					&& (pre_index[type]==-1 || wave_index-wave_len[type]-pre_index[type]-1 > dist_len || (smallConjunLen[type] < short_len && smallConjunLen[type]!=0))) //µ±´Ë¶Ì²¨ÓëÇ°ÃæµÄ²¨¾àÀëÐ¡ÓÚãÐÖµÊ±£¬Òª¿´ÓëÇ°ÃæµÄ²¨¾ÛºÏºóµÄ³¤¶ÈÊÇ·ñ´óÓÚãÐÖµ
				{
					has_first_short[type] = 1;
					pre_wave_len[type] = wave_len[type];

				}
				//init
				pre_index[type] = wave_index-1;
				wave_len[type] = 0;
				isCurrentLong[type] = 0;
			}
			//如果0的个数大于阈值，直接将0输出
			if(wave_index-pre_index[type] >= dist_len){
				//0的个数大于阈值，并且如果前面有短波，将短波都变成0并输出
				if(has_first_short[type]){
					for(j = pre_index[type]-pre_wave_len[type]+1-smallConjunLen[type];j<=wave_index;j++){
						print(fpR[type],timestamp,0);
					}	
					has_first_short[type] = 0;
					smallConjunLen[type] = 0;
					isIntervalLong[type] = 1;//last short wave was deleted,and the zero was printed too 
				}else{
					//0的个数已经大于阈值，直接输出
					if(isIntervalLong[type]){
						print(fpR[type],timestamp,0);
					}else{
					//0的个数大于阈值，将之前积攒的0一齐输出
					for(j = pre_index[type] + 1;j<=wave_index;j++){
						print(fpR[type],timestamp,0);
					}
					isIntervalLong[type] = 1;				
					}				
				}

			}

		}
	
}
// int main(){
// 	FILE *fp = fopen("baseA.txt","r");
// 	double t,f,tmp;
// 	int current=0,previous=0,timestamp;
// 	fpR = fopen("out_result.txt","w");

// 	while(!feof(fp)){
// 		//fscanf(fp,"%d",&timestamp);
// 		timestamp = wave_index;
// 		fscanf(fp,"%lf",&t);
// 		fscanf(fp,"%lf",&f);

// 		previous = current;

// 		tmp = enlarge_tranform(t,f);
// 		current = binary_transform(tmp);

// 		print_next_filter(timestamp,current,previous);
		
// 		wave_index++;
// 	}
	
	

// 	fclose(fp);
// 	system("pause");
// }