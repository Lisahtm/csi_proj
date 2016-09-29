#include "iwl_connector.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/socket.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include "iwl_nl.h"
#include "iwl_structs.h"
#include "iwl_define.h"
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include "analyze.h"

int iwl_parse_compatibility(struct iwl5000_bfee_notif *bfee, struct iwl5000_bfee_csi *bfeecsi);
void csi_prepare_compatibility(struct iwl5000_bfee_csi *bfeecsi, int parse_result);
void analyzeData(double tmpReceived[][RefreshNum],int hasTimestamp);

int sock_fd = -1;	// the socket to receive csi in netlink
double* csi_amplitudeA;
double* csi_phaseA;
double* csi_amplitudeB;
double* csi_phaseB;
double* csi_amplitudeC;
double* csi_phaseC;

int receive_cycnum = 0;	
int* parse_result_array;
int parse_result;


//analyze variable
int seq = 0;
//amplitude A
double buffer[3][sample_f*sample_T][RefreshNum];
double time_group[3][Group_num],frequency_group[3][Group_num];
//skewness need to be calculated after 900 groups
//to notify wave index(calculated from F and T),wave_index is different from seq;
int wave_index = 0;

int group_index=0;
//socket to display
char paramGraphic[20];
int sockfd,newfd=-999;

char testChannel[4] = "100";
pthread_t pid;
//wave calculate params
int hasManChannelNumber = 1;
//whether write into file
int isWriteFile = 0;
extern double binary_threshold;
extern int short_len;
extern int dist_len;

//file
FILE *basep[3],*skewp[3],*fpR[3];//fpR wave
int timestamp_current = 0;

void startSocket(){
	int ret;
    struct sockaddr_in server_addr;


    server_addr.sin_family=AF_INET;/*设置域为IPV4*/
    server_addr.sin_addr.s_addr=INADDR_ANY;/*绑定到 INADDR_ANY 地址*/
    server_addr.sin_port=htons(5678);/*通信端口号为5678，注意这里必须要用htons函数处理一下，不能直接写5678，否则可能会连不上*/
    sockfd=socket(AF_INET,SOCK_STREAM,0);
    if (sockfd<0)
    {
        printf("调用socket函数建立socket描述符出错！\n");
         exit(1);
    }
    printf("调用socket函数建立socket描述符成功！\n");
    ret=bind(sockfd,(struct sockaddr *)(&server_addr),sizeof(server_addr));
    perror("server");
    if (ret<0)
    {
        printf("调用bind函数绑定套接字与地址出错！\n");
         exit(2);
    }
    printf("调用bind函数绑定套接字与地址成功！\n");
    ret=listen(sockfd,4);
    if (ret<0)
    {
        printf("调用listen函数出错，无法宣告服务器已经可以接受连接！\n");
         exit(3);
    }
    printf("调用listen函数成功，宣告服务器已经可以接受连接请求！\n");
    newfd=accept(sockfd,NULL,NULL);/*newfd连接到调用connect的客户端*/
    if (newfd<0)
    {
        printf("调用accept函数出错，无法接受连接请求，建立连接失败！\n");
         exit(4);
    }
    printf("调用accept函数成功，服务器与客户端建立连接成功！\n");	
}
int previousLisa = 0;

void deal(){
	int itmp;
	double dtmp;
	int currentFT;
	FILE *origin = fopen("./origin.txt","r");
	FILE *middle = fopen("./middle.txt","w");
	int seq = 0;
	while(!feof(origin)){
		fscanf(origin,"%d",&itmp);
		fscanf(origin,"%d",&itmp);
		fscanf(origin,"%lf",&dtmp);
		fscanf(origin,"%d",&itmp);
		fprintf(middle,"%d ",itmp);
		fflush(middle);
		currentFT = binary_transform(dtmp);
		previousLisa= currentFT;

		print_next_filter(-1,currentFT,previousLisa,0,seq);

		seq++;
	}
}
//receive packet offline
void file_receive(int hasTimestamp){
	FILE *origin = fopen("./origin.txt","r");
	double tmpReceived[3][RefreshNum];
	int i,j;

	while(!feof(origin)){
		//read data of channel A B C from the file
		for(j =0;j < ChannelNum;j++){
			if(hasTimestamp){
				fscanf(origin,"%d",&timestamp_current);
			}
			for(i = 0;i< RefreshNum;i++){
				fscanf(origin,"%lf",&tmpReceived[j][i]);
				 // printf("%.4lf\n",tmpReceived[j][i]);
			}
		}	
		
		analyzeData(tmpReceived,hasTimestamp);
		seq++;	
	}
}
//After receiving a group of data(Channel A,B,C), analyze it 
void analyzeData(double tmpReceived[][RefreshNum],int hasTimestamp){
	int ABC_index;
	double skew=0,kur =0;
	calculateParam *param;
	pthread_mutex_t *lock;
	pthread_mutex_t *pre_lock = NULL;
	int rc;
	//push the variables
	for(ABC_index = 0;ABC_index<ChannelNum;ABC_index++){
		push_sampling_plus(buffer[ABC_index],seq,tmpReceived[ABC_index]);
	}
	if(isFull(seq)==1){
		//calculate the wave value
		lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(lock,NULL);
		/*****copy the value********/
		param = (calculateParam*)malloc(sizeof(calculateParam));
		memcpy(param->buffer,buffer,sizeof(buffer));
		// param->group_t = time_group[ABC_index];
		// param->group_f = frequency_group[ABC_index];
		param->type = 0;
		param->hasTimestamp = hasTimestamp;
		param->group_index = group_index;
		param->wave_index = wave_index;
		param->own_lock = lock;
		param->pre_lock = pre_lock;
		param->seq = seq;

		pre_lock = lock;
		/*******multiThread*********/
		pthread_mutex_lock(lock);
		if((rc = pthread_create(&pid,NULL,(void *)calculateM,(void*)param))){
			printf("thread 1 create failed: %d\n",rc); 
		}
		pthread_join(pid,NULL); 

		/****single thread***/
		// calculate(buffer[ABC_index],time_group[ABC_index],frequency_group[ABC_index],ABC_index,hasTimestamp,group_index,wave_index);
	
	
		//after calculating,the wave (calculated from f and t) should increase
		wave_index++;
		//when group index is over 90,it will calculate skewness and...
		group_index++;
		if(group_index ==Group_num){
			//reloop ABC
			//calculate skewness of A B C
			if(1<0){
				for(ABC_index = 0;ABC_index<ChannelNum;ABC_index++){
					skew =  getSkewness(time_group[ABC_index]);
					kur =  getKurtosis(time_group[ABC_index]);
					//printf("->[%.2lf,%.2lf]",skew,kur);
					if(hasTimestamp){
						fprintf(skewp[ABC_index],"%d ",timestamp_current);
					}
					fprintf(skewp[ABC_index],"%.2lf %.2lf ",skew,kur);

					skew =  getSkewness(frequency_group[ABC_index]);
					kur =  getKurtosis(frequency_group[ABC_index]);
					//printf("->[%.2lf,%.2lf]",skew,kur);
					fprintf(skewp[ABC_index],"%.2lf %.2lf\n",skew,kur);
					fflush(skewp[ABC_index]);	
				}				
			}
			
			//printf("\n");
			group_index=0;
		}

	}


}


void iwl_receive()
{
	/* Local variables */
	u_char *buf;
	int len;
	int ret;
	int i;
	struct iwl5000_bfee_notif *bfee;
    struct iwl5000_bfee_csi *bfee_csi;

    int tmpIndex = 0;
	double tmpReceived[3][RefreshNum];
	//memset(tmpReceived,0,sizeof(double)*RefreshNum);
	time_t timeT;

	csi_amplitudeA = (double*)malloc(CSINum * sizeof(double));
	csi_phaseA = (double*)malloc(CSINum * sizeof(double));
	csi_amplitudeB = (double*)malloc(CSINum * sizeof(double));
	csi_phaseB = (double*)malloc(CSINum * sizeof(double));
	csi_amplitudeC = (double*)malloc(CSINum * sizeof(double));
	csi_phaseC = (double*)malloc(CSINum * sizeof(double));

	parse_result_array = (int*)malloc(ReserveNum * sizeof(int));
	for(i = 0; i < CSINum; i++)
	{
		csi_amplitudeA[i] = csi_phaseA[i] =  0.0;
		csi_amplitudeB[i] = csi_phaseB[i] = 0.0;
		csi_amplitudeC[i] = csi_phaseC[i] = 0.0;
	}
	for(i = 0; i < ReserveNum; i++) 
	{
        parse_result_array[i] = 0;
    }
    printf("starting setting up socket\n");
	/* Setup the socket */
	sock_fd = open_iwl_netlink_socket();
	if (sock_fd == -1)
	{
		printf("fail to open netlink socket!\n");
		exit(-1);
	}

	/* Set up the "caught_signal" function as this program's sig handler */
	//signal(SIGINT, caught_signal);

    bfee_csi = (struct iwl5000_bfee_csi*)malloc(sizeof(struct iwl5000_bfee_csi));
	if(bfee_csi == NULL){
		printf("bfee_csi NULL value!\n");
		exit(-1);
	}
	
	int csi_len = sizeof(struct iwl5000_bfee_csi );
	/* Poll socket forever waiting for a message */

	while (1) 
	{

		usleep(1000);
	    memset(bfee_csi,0,csi_len);
		/* Receive from socket with infinite timeout */
		ret = iwl_netlink_recv(sock_fd, &buf, &len);
	
		if (ret == -1)
		{
			printf("fail to receive data from netlink!\n");
            exit(-1);
		}
		else if (ret == 100)
		{
			continue;
		}
		if (buf == NULL || buf[0] != IWL_CONN_BFEE_NOTIF) 
		{
			continue;
		}

		/* Log the data to file */
		/* Calculate effective SNRs */
		bfee = (struct iwl5000_bfee_notif *)&buf[1];
		//iw_bfee(&buf[1],bfee_csi);
		// old_version
		//解析网卡驱动提交上来的数据，按照约定的格式存储数据
		parse_result = iwl_parse_compatibility(bfee, bfee_csi);
		if (parse_result != 11 && parse_result != 31 && parse_result != 32)
			continue;
		//解析出csi的模长和相位信息
		csi_prepare_compatibility(bfee_csi, parse_result);

		//printf("\ncsi_amplitudeA:\n");
		//assign value
		for(i = (receive_cycnum - 1) * RefreshNum; i < receive_cycnum * RefreshNum; i++)
		{
			
			tmpReceived[0][tmpIndex] = csi_amplitudeA[i];
			tmpReceived[1][tmpIndex] = csi_amplitudeB[i];
			tmpReceived[2][tmpIndex] = csi_amplitudeC[i];

			tmpIndex++;
		}	
		tmpIndex = 0;
		//printf("begin sampling\n");
		timestamp_current = time(&timeT);
		analyzeData(tmpReceived,0);
	

		seq++;
		// printf("seq:%d:\n",seq);

	}
	if(bfee_csi != NULL)
	{
		free(bfee_csi);
	}
}

int no_noise_value = 0;

void compute_11(uint8_t *payload, struct iwl5000_bfee_csi *bfeecsi)
{
	const uint32_t size = 1*1*2*8+3; /* 1 byte + 3 bits for index */
	const uint32_t size_byte = size / 8;
	const uint32_t size_rem = size % 8;
	uint32_t index_byte = 0;
	uint32_t index_rem = 3;
	uint32_t cur;
	int8_t *curi = (int8_t *)&cur;
	uint32_t i, j;
	int32_t t;

	int32_t csi[RefreshNum][1*1*2];
	/**
	 * Loop through subcarriers, computing ber-subcarrier SNRs
	 * and accumulating total sum of matrix values
	 */
	for (i = 0; i < RefreshNum; ++i) 
	{
		/* Get 4 bytes (Need 2) */
		cur = *((uint32_t *)&payload[index_byte]);
		/* Shift down by remainder */
		if (index_rem != 0)
			cur >>= index_rem;

		/* Convert int8_t to int32_t and update SNR accumulator */
		for (j = 0; j < 1*1*2; ++j) 
		{
			t = curi[j];
			csi[i][j] = t;
		}

		/* Update index_byte and index_rem */
		index_byte += size_byte;
		index_rem += size_rem;
		if (index_rem > 7) 
		{
			index_byte++;
			index_rem -= 8;
		}
	}
	int k = 0;
	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < RefreshNum; j++)
		{
	      	bfeecsi->csi[k++] = csi[j][i];
        }
	}
}

void compute_31(uint8_t *payload, struct iwl5000_bfee_csi *bfeecsi)
{
	const uint32_t size = 3*1*2*8+3; /* 1 byte + 3 bits for index */
	const uint32_t size_byte = size / 8;
	const uint32_t size_rem = size % 8;
	uint32_t index_byte = 0;
	uint32_t index_rem = 3;
	uint64_t cur;
	int8_t *curi = (int8_t *)&cur;
	uint32_t i, j;
	int32_t t;

	int32_t csi[RefreshNum][3*1*2];
	/**
	 * Loop through subcarriers, computing ber-subcarrier SNRs
	 * and accumulating total sum of matrix values
	 */
	for (i = 0; i < RefreshNum; ++i) 
	{
		/* Get 4 bytes (Need 2) */
		cur = *((uint64_t *)&payload[index_byte]);
		/* Shift down by remainder */
		if (index_rem != 0)
			cur >>= index_rem;

		/* Convert int8_t to int32_t and update SNR accumulator */
		for (j = 0; j < 3*1*2; ++j) 
		{
			t = curi[j];
			csi[i][j] = t;
		}

		/* Update index_byte and index_rem */
		index_byte += size_byte;
		index_rem += size_rem;
		if (index_rem > 7) 
		{
			index_byte++;
			index_rem -= 8;
		}
	}
	int k = 0;
	for(i = 0; i < 6; i++)
	{
		for(j = 0; j < RefreshNum; j++)
		{
	      	bfeecsi->csi[k++] = csi[j][i];
        }
	}
}

void compute_32(uint8_t *payload, struct iwl5000_bfee_csi *bfeecsi)
{
	const uint32_t size = 3*2*2*8+3;
	const uint32_t size_byte = size / 8;
	const uint32_t size_rem = size % 8;
	uint32_t index_byte = 0;
	uint32_t index_rem = 3;
	uint64_t cur[2];
	int8_t *curi = (int8_t *)cur;
	uint32_t i, j;
	int32_t t;

	int32_t csi[RefreshNum][3*2*2];
	/**
	 * Loop through subcarriers, computing ber-subcarrier SNRs
	 * and accumulating total sum of matrix values
	 */
	for (i = 0; i < RefreshNum; ++i) 
	{
		/* Get 4 bytes (Need 2) */
		cur[0] = *((uint64_t *)&payload[index_byte]);
		cur[1] = *((uint64_t *)&payload[index_byte] + 1);
		/* Shift down by remainder */
		if (index_rem != 0) 
		{
			cur[0] >>= index_rem;
			cur[0] |= (cur[1] << (64-index_rem));
			cur[1] >>= index_rem;
		}

		/* Convert int8_t to int32_t and update SNR accumulator */
		for (j = 0; j < 3*2*2; ++j) 
		{
			t = curi[j];
			csi[i][j] = t;
		}

		/* Update index_byte and index_rem */
		index_byte += size_byte;
		index_rem += size_rem;
		if (index_rem > 7) 
		{
			index_byte++;
			index_rem -= 8;
		}
	}
	// we just get the three in the front
	int k = 0;
	for(i = 0; i < 6; i++)
	{
		for(j = 0; j < RefreshNum; j++)
		{
	      	bfeecsi->csi[k++] = csi[j][i];
        }
	}
}

void compute_33(uint8_t *payload, struct iwl5000_bfee_csi *bfeecsi)
{
	printf("enter in 3, 3 branch\n\n");
}

int iwl_parse_compatibility(struct iwl5000_bfee_notif *bfee, struct iwl5000_bfee_csi *bfeecsi)
{
	uint8_t Nrx = bfee->Nrx;
	uint8_t Ntx = bfee->Ntx;
	uint16_t rate_n_flags = bfee->fake_rate_n_flags;

	/* If calculated length != actual length, exit early */
	uint32_t calc_len = (RefreshNum * (Nrx * Ntx * 2 * 8 + 3) + 7) / 8;

	if (bfeecsi == NULL)
		return 0;

	if (calc_len != bfee->len)
		return 0;

	/* If not 3 RX antennas, exit */
	if (!((Nrx == 3) || ((Nrx == 1) && (Ntx == 1))))
		return 0;

	/* Now compute values needed for SNR calculation */
	uint8_t rssi[3] = {bfee->rssiA, bfee->rssiB, bfee->rssiC};
	int16_t noise = bfee->noise;
	if (noise == -127) 
	{
		if (!no_noise_value) 
		{
			fprintf(stderr, "warning: packet has no noise value\n");
			no_noise_value = 1;
		}
		noise = -95;
	}

	bfeecsi->Ntx = (uint32_t)Ntx;
	bfeecsi->Nrx = (uint32_t)Nrx;
	bfeecsi->rssiA = rssi[0];
	bfeecsi->rssiB = rssi[1];
	bfeecsi->rssiC = rssi[2];
	bfeecsi->noise = noise;
	bfeecsi->fake_rate_n_flags = rate_n_flags;
	bfeecsi->len = calc_len;

    /*int ptrR[3];
    ptrR[0] = ((bfee->antenna_sel) & 0x3) + 1;
    ptrR[1] = ((bfee->antenna_sel >> 2) & 0x3) + 1;
    ptrR[2] = ((bfee->antenna_sel >> 4) & 0x3) + 1;
    printf("hello: %d %d %d!&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n", ptrR[0], ptrR[1], ptrR[2]);*/

	/* Depending on the numbers of streams, compute effective SNRs */
	if ((Nrx == 1) && (Ntx == 1)) 
	{
		compute_11(bfee->payload, bfeecsi);
	}
	else if (Ntx == 1) 
	{
		compute_31(bfee->payload, bfeecsi);
	}
	else if (Ntx == 2) 
	{
		compute_32(bfee->payload, bfeecsi);
	}
	else if (Ntx == 3) 
	{
		compute_33(bfee->payload, bfeecsi);
	}

	return (Nrx * 10 + Ntx);
}

void csi_prepare_compatibility(struct iwl5000_bfee_csi *bfeecsi, int parse_result)
{
	int i, n;
	if (parse_result == 31 || parse_result == 32)
	{
		if(receive_cycnum < ReserveNum)
		{
			for(i = 0; i < RefreshNum; i++)
			{
				csi_amplitudeA[receive_cycnum*RefreshNum+i] = sqrt((double)(bfeecsi->csi[i]*bfeecsi->csi[i]+bfeecsi->csi[RefreshNum+i]*bfeecsi->csi[RefreshNum+i]));
				// csi_phaseA[receive_cycnum*RefreshNum+i] = atan(((double)bfeecsi->csi[RefreshNum+i])/((double)bfeecsi->csi[i]));
				csi_amplitudeB[receive_cycnum*RefreshNum+i] = sqrt((double)(bfeecsi->csi[i+2*RefreshNum]*bfeecsi->csi[i+2*RefreshNum]+bfeecsi->csi[3*RefreshNum+i]*bfeecsi->csi[3*RefreshNum+i]));
				// csi_phaseB[receive_cycnum*RefreshNum+i] = atan(((double)bfeecsi->csi[3*RefreshNum+i])/((double)bfeecsi->csi[i+2*RefreshNum]));
				csi_amplitudeC[receive_cycnum*RefreshNum+i] = sqrt((double)(bfeecsi->csi[i+4*RefreshNum]*bfeecsi->csi[i+4*RefreshNum]+bfeecsi->csi[5*RefreshNum+i]*bfeecsi->csi[5*RefreshNum+i]));
				// csi_phaseC[receive_cycnum*RefreshNum+i] = atan(((double)bfeecsi->csi[5*RefreshNum+i])/((double)bfeecsi->csi[i+4*RefreshNum]));
			}
			parse_result_array[receive_cycnum] = parse_result;
			receive_cycnum++;
		}
		else
		{
			n = CSINum - RefreshNum;
			for(i = 0; i < n; i++)
			{
				csi_amplitudeA[i] = csi_amplitudeA[i+RefreshNum];
				csi_amplitudeB[i] = csi_amplitudeB[i+RefreshNum];
				csi_amplitudeC[i] = csi_amplitudeC[i+RefreshNum];
				// csi_phaseA[i] = csi_phaseA[i+RefreshNum];
				// csi_phaseB[i] = csi_phaseB[i+RefreshNum];
				// csi_phaseC[i] = csi_phaseC[i+RefreshNum];
			}
			for(i = 0; i < RefreshNum; i++)
			{
				csi_amplitudeA[n+i] = sqrt((double)(bfeecsi->csi[i]*bfeecsi->csi[i]+bfeecsi->csi[RefreshNum+i]*bfeecsi->csi[RefreshNum+i]));
				// csi_phaseA[n+i] = atan(((double)bfeecsi->csi[RefreshNum+i])/((double)bfeecsi->csi[i]));
				csi_amplitudeB[n+i] = sqrt((double)(bfeecsi->csi[i+2*RefreshNum]*bfeecsi->csi[i+2*RefreshNum]+bfeecsi->csi[3*RefreshNum+i]*bfeecsi->csi[3*RefreshNum+i]));
				// csi_phaseB[n+i] = atan(((double)bfeecsi->csi[3*RefreshNum+i])/((double)bfeecsi->csi[i+2*RefreshNum]));
				csi_amplitudeC[n+i] = sqrt((double)(bfeecsi->csi[i+4*RefreshNum]*bfeecsi->csi[i+4*RefreshNum]+bfeecsi->csi[5*RefreshNum+i]*bfeecsi->csi[5*RefreshNum+i]));
				// csi_phaseC[n+i] = atan(((double)bfeecsi->csi[5*RefreshNum+i])/((double)bfeecsi->csi[i+4*RefreshNum]));
			}
			for (i = 0; i < ReserveNum - 1; i++) 
			{
                parse_result_array[i] = parse_result_array[i+1];
            }
            parse_result_array[i] = parse_result;
		}
	}
	else if (parse_result == 11)
	{
		if(receive_cycnum < ReserveNum)
		{
			for(i = 0; i < RefreshNum; i++)
			{
				csi_amplitudeA[receive_cycnum*RefreshNum+i] = sqrt((double)(bfeecsi->csi[i]*bfeecsi->csi[i]+bfeecsi->csi[RefreshNum+i]*bfeecsi->csi[RefreshNum+i]));
				// csi_phaseA[receive_cycnum*RefreshNum+i] = atan(((double)bfeecsi->csi[RefreshNum+i])/((double)bfeecsi->csi[i]));
			}
			parse_result_array[receive_cycnum] = parse_result;
			receive_cycnum++;
		}
		else
		{
			n = CSINum - RefreshNum;
			for(i = 0; i < n; i++)
			{
				csi_amplitudeA[i] = csi_amplitudeA[i+RefreshNum];
				// csi_phaseA[i] = csi_phaseA[i+RefreshNum];
			}
			for(i = 0; i < RefreshNum; i++)
			{
				csi_amplitudeA[n+i] = sqrt((double)(bfeecsi->csi[i]*bfeecsi->csi[i]+bfeecsi->csi[RefreshNum+i]*bfeecsi->csi[RefreshNum+i]));
				// csi_phaseA[n+i] = atan(((double)bfeecsi->csi[RefreshNum+i])/((double)bfeecsi->csi[i]));
			}
			for (i = 0; i < ReserveNum - 1; i++) 
			{
                parse_result_array[i] = parse_result_array[i+1];
            }
            parse_result_array[i] = parse_result;
		}
	}
}

int main(int argc,char *argv[])
{
	pthread_t pid;
	int i;
	int offlineFlag = 0;
	int ishelp = 0;

	initCalculate();
	// startSocket();//for display
	pthread_create(&pid,NULL,(void *)beepManager,NULL);

	if(argc>=2){
		for(i = 1;i < argc;i++){
			if(strcmp(argv[i],"offline")==0){
				offlineFlag = 1;
				printf("\noffline\n");
				break;
			}else if(strcmp(argv[i],"-channel")==0){
				strcpy(testChannel,argv[i+1]);
				i++;
			}else if(strcmp(argv[i],"-threshold")==0){
				sscanf(argv[i+1],"%lf",&binary_threshold);
				i++;
			}else if(strcmp(argv[i],"-waveLen")==0){
				sscanf(argv[i+1],"%d",&short_len);
				i++;
			}else if(strcmp(argv[i],"-intervalLen")==0){
				sscanf(argv[i+1],"%d",&dist_len);
				i++;
			}else if(strcmp(argv[i],"-channelNumber")==0){
				sscanf(argv[i+1],"%d",&hasManChannelNumber);
				i++;
			}else if(strcmp(argv[i],"-w")==0){
				isWriteFile = 1;
			}else if(strcmp(argv[i],"-help")==0){
				printf("\t************************************************************************\n");
				printf("\t* Params introduction:\n\n");
				printf("\t* offline: read files from origin.txt in the base path directly\n\n");
				printf("\t* -channel[default is 100]: format is 000,if you want to open channel A,\n");
				printf("\t   format is 100, if you want to open ABC,format is 111\n\n");
				printf("\t* -channelNumber[int,default is 1]: Every channel alive will ouput 1 \n");
				printf("\t   or 0,if the number of 1 is over channelNumber, then it will finally\n");
				printf("\t   output 1,or it will output 0.\n\n");
				printf("\t* -threshold[format:double,default is 0.9]: the threshold to judge \n");
				printf("\t   whether it has someone or not.\n\n");
				printf("\t* -waveLen[format:int,default is 10]: the mimum wave length to judge\n");
				printf("\t   there is someone.\n\n");
				printf("\t* -intervalLen[format:int,default is 50]: the mimum interval length\n");
				printf("\t   to judge there isnot someone\n\n");
				printf("\t* -w[default is no]: order the program to write the data to files\n\n");
				printf("\t************************************************************************\n");
				ishelp = 1;
				break;
			}
		}	

	}
	if(!ishelp){
		if(!offlineFlag){
			iwl_receive();	
		}else{
			file_receive(0);
		}		
	}

		

	return 0;
}

