/*
	文件：main.c
	说明：uARM驱动主函数
	作者：SchumyHao
	版本：V02
	日期：2013.03.18
*/
/* 调试定义 */
#define DEBUG

/* 头文件 */
#include "UART.h"
#include "uARM_driver.h"

/* 参数定义 */
#define HOST_UART_PORT 	(0)
#define ARGC_MUN	(4)

/* 函数 */
int main(int argc, char* argv[]){
	/* 局部变量声明 */
	int tmp;
	int BuffDeep = DEFAULT_BUFF_DEEP;
	t_coordinate CooSys;
	t_uart UartConfig;
	struct termios OldCfg;
	char* pBuff = NULL;
	/* 坐标系初始化 */
	InitCoordinateSystem(&CooSys);
	/* 串口结构体初始化 */
	InitUartStruct(&UartConfig);
	/* 输入参数检查 */
	#ifdef DEBUG
	int i;
	for(i = 0; i < argc; i++){
		printf("argv[%d]: %s\n",i,argv[i]);
	}
	#endif
	if(argc == ARGC_MUN){
		tmp = atoi(argv[1]);
		if(IS_X_LOCATION(tmp)){
			CooSys.X = tmp;
		}
		else{
			perror("1st argument 'x' wrong!\n");
			return -1;
		}
		tmp = atoi(argv[2]);
		if(IS_Y_LOCATION(tmp)){
			CooSys.Y = tmp;
		}
		else{
			perror("2nd argument 'y' wrong!\n");
			return -1;
		}
		tmp = atoi(argv[3]);
		if(IS_DESTINATION(tmp)){
			switch(tmp){
			case DEST_ONE:
				CooSys.DestAngle = DEST_ONE_A;
				CooSys.DestRadius = DEST_ONE_R;
				break;
			case DEST_FIVE:
				CooSys.DestAngle = DEST_FIVE_A;
				CooSys.DestRadius = DEST_FIVE_R;
				break;
			case DEFAULT_DEST:
			default:
				CooSys.DestAngle = DEFAULT_DEST_A;
				CooSys.DestRadius = DEFAULT_DEST_R;
			}
		}
		else{
			perror("3rd argument 'destination' wrong!\n");
			return -1;
		}
	}
	else{
		perror("Argument number is wrong!\n");
		return -1;
	}

	/* 坐标变换 */
	ShiftCoordinate(&CooSys);
	#ifdef DEBUG
	printf("X location is %d.\n",CooSys.X);
	printf("Y location is %d.\n",CooSys.Y);
	printf("Angle degree is %d.\n",CooSys.Angle);
	printf("Radius length is %d.\n",CooSys.Radius);
	#endif
	/* 动作生成函数 */
	pBuff = (char*)malloc(sizeof(char)*BUFFER_SIZE*BUFFER_DEEP);
	if(pBuff == NULL){
		perror("Memory allocation wrong.\n");
		return -1;
	}
	BuffDeep = GenerateMotion(&CooSys, pBuff);
	if(BuffDeep == DEFAULT_BUFF_DEEP){
		perror("Motion generation wrong.\n");
		return -1;
	}
	#ifdef DEBUG
	printf("BuffDeep is %d.\n",BuffDeep);
	#endif
	/* 串口配置 */
	//打开串口设备文件
	UartConfig.Fd = OpenPort(HOST_UART_PORT);
	if(UartConfig.Fd < 0){
		perror("Can not open UART port.\n");	
		return -1;
	}
	//在设备文件上打开标准IO流
	UartConfig.pFp=fdopen(UartConfig.Fd,"w");
	if(UartConfig.pFp == NULL){
		perror("Can not open UART stream!\n");
		return -1;
	}
	//Change the buffer type to none.
	setbuf(UartConfig.pFp, NULL);

	#ifdef DEBUG
	printf("UART device fd is %d.\n",UartConfig.Fd);
	#endif
	//配置串口参数
	UartConfig.BaudRate = BAUD_RATE_9600;
	UartConfig.DataBits = DATA_BITS_8BITS;
	UartConfig.Parity = PARITY_NONE;
	UartConfig.StopBits = STOP_BITS_1BIT;		
	if(ConfigUart(&UartConfig, &OldCfg) < 0){
		perror("Config UART error.\n");
		return -1;
	}
	/* 发送数据 */
	if(SendData(UartConfig.pFp, BuffDeep, pBuff) < 0){
		perror("Send data error.\n");
		return -1;
	}
	/* 释放内存，还原缓冲深度变量 */
	free(pBuff);
	BuffDeep = DEFAULT_BUFF_DEEP;
	/* 还原终端之前的配置 */
	if((tcsetattr(UartConfig.Fd, TCSANOW, &OldCfg)) != 0){
		perror("Revert tty error.\n");
		return -1;
	}
	close(UartConfig.Fd);
	fclose(UartConfig.pFp);
	return 0;
}
