/*
FileName    :HandleMsg.c
Description :Handle message
Author      :WRTnode machine team
Email       :summer@wrtnode.com
Version     :V01
Data        :2015.06.03
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "serial.h"
#include "serial_posix.h"
#include "spi.h"
#include "spi_mt7688.h"
#include "uixo_console.h"

#define OTHERDEL  1

enum uixo_rx_status {
	UIXO_RX_STATUS_IDLE = 0,
	UIXO_RX_STATUS_GOT_INVALID,
	UIXO_RX_STATUS_GOT_HEAD,
	UIXO_RX_STATUS_GOT_MSG
};
enum uixo_cmd_status {
	UIXO_CMD_STATUS_IDLE = 0,
	UIXO_CMD_STATUS_GOT_LEN,
	UIXO_CMD_STATUS_GOT_CMD
};


int uixo_receive_data(uixo_port_t* p, uixo_message_list_t** msgp)
{
	int ret = 0;
	int chk = 0;
	uixo_message_list_t* msg;
	if((NULL == p)||(NULL == p->port)) {
			return -UIXO_CONSOLE_ERR_INPUT_NULL;
	}

	/* got posix serial port */
	struct posix_serial* ps = (struct posix_serial*)p->port;

	/* read port one by one. maybe can be improve */
	static char rx_buf[MAX_UIXO_MSG_LEN];
	static int rx_pos;
	char* ptr = NULL;
	ptr = rx_buf + rx_pos;
	int readn = ps->read(ps, ptr, 1);
	if(1!=readn) {
		return -UIXO_CONSOLE_ERR_READ_PORT;
	}
	if(*ptr == '0'){
		struct timeval tv;
		uixo_message_list_t* msg = NULL;
		msg = (uixo_message_list_t*)malloc(1*sizeof(uixo_message_list_t));
		if(NULL==msg) {
			free(msg);
			return -UIXO_CONSOLE_ERR_MEM;
		}
		msg->len = rx_pos;
		msg->data = rx_buf;
		gettimeofday(&tv, NULL);
		msg->time = tv.tv_sec*1000 + (unsigned long)(tv.tv_usec/1000);//out
		*msgp = msg;
		rx_pos = 0;
	}
	else{
		rx_buf[rx_pos] = *ptr;
		rx_pos += readn;
	}
	return ret;
}
int uixo_save_cmd(uixo_port_t* p,const uixo_message_list_t* msg,char* Callback)
{
	int socketfd;
	int fds;
	char cmd;
	char port_name[20];
	int flag = 0;
	struct list_head* pos;
	struct list_head* n;
	int ret = 0;
	int size = 0;
	char buff[1024];
	uixo_message_list_t* listmsg;
	char* Command = NULL;

	if((NULL == msg)||
			(NULL == msg->data)) {
		return -UIXO_CONSOLE_ERR_INPUT_NULL;
	}
	/* translate message to string */
	char cmd_data_buf[MAX_UIXO_MSG_LEN];
	char* ptr = cmd_data_buf;
	int offset = 0;
	offset = sprintf(ptr,"[%lu:%u:",msg->time,(unsigned int)msg->len);
	ptr += offset;
	memcpy(ptr, msg->data, (int)msg->len);
	offset += (int)msg->len;
	ptr += (int)msg->len;
	offset += sprintf(ptr,"]\n");
	PR_DEBUG("cmd_data_buf = %s\n",cmd_data_buf);
	/* write string to fd */
	if(Callback == NULL){
		list_for_each_entry(listmsg,p->msghead,list){
			socketfd = listmsg->socketfd;
			size = write(socketfd, cmd_data_buf, offset);
			printf("write size = %d\n",size);
			listmsg->rttimes --;
			PR_DEBUG("listmsg->rttimes = %d\n",listmsg->rttimes);
			if(listmsg->rttimes == 0){
				/*delete the msg from msg list */
				flag = 1;
				fds = socketfd;
				strcpy(port_name,listmsg->port_name);
			}
			else{
				//每次默认从串口读一次数据，如果是一直需要数据的rttimes不可能等于0
				uixo_transmit_data(p,listmsg);
			}
			//一条消息只发给一个客户端
			break;
		}
	}
	else {
		Command =(char *)malloc((size_t)strlen(Callback)+offset+8);
		sprintf(Command,"/root/%s %s", Callback, cmd_data_buf);
		PR_DEBUG("Callback Command = %s",Command);
		system(Command);
		free(Command);
		return 1;
	}
	if(flag == 1){
		del_msg(p,pos,n,port_name,fds,OTHERDEL);
	}
	return 0;
}

int uixo_invalid_receive_data_process(void* port, char* str, int size)
{
	int ret = 0;
	if((NULL == port)||
			(NULL == str)) {
		return -UIXO_CONSOLE_ERR_INPUT_NULL;
	}
	/* write string to port */
	struct posix_serial* ps = (struct posix_serial*)port;
	ps->write(ps,str,size);
	return 0;
}
int uixo_transmit_data(uixo_port_t* p, const uixo_message_list_t* msg)
{
	if((NULL == p)||
			(NULL == p->port)||
			(NULL == msg)) {
		return -1;
	}

    if(strncmp(p->name, "/dev/spiS", strlen("/dev/spiS")) == 0) {
        struct spi_mt7688* sm = (struct spi_mt7688*)(p->port);
	    PR_DEBUG("send to port data = %s and len = %d",msg->data,msg->len);
        int writen = 0;
        writen = sm->write(sm, msg->data, msg->len);
        printf("write to SPI = %s\n", msg->data);
	    if(writen < 0) {
		    printf("make message failed\n");
		    return -1;
	    }
    }
    else {
    	/* got posix serial port */
    	struct posix_serial* ps = (struct posix_serial*)p->port;
    	/* call transmit data process */
    	PR_DEBUG("send to port data = %s and len = %d",msg->data,msg->len);
    	int writen = 0;
    	writen = ps->write(ps, msg->data,msg->len);
    	if(writen < 0) {
    		printf("make message failed\n");
    		return -1;
    	}
    }
	return 0;
}

/*Traverse the list*/
int TraverseMsg (struct list_head* list)
{
	uixo_port_t* port;
	uixo_message_list_t* msg;
	int nummsg = 0;
	list_for_each_entry(port,list,list){
		list_for_each_entry(msg,port->msghead,list){
			nummsg ++;
			PR_DEBUG("msg->port_name=%s;msgnum = %d\n",msg->port_name,nummsg);
		}
	}
}
int TraversePort (struct list_head* list)
{
	uixo_port_t* port;
	uixo_message_list_t* msg;
	int numport = 0;
	list_for_each_entry(port,list,list){
		numport ++;
		PR_DEBUG("port = %s,portnum = %d\n",port->name,numport);
	}
}