#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>        
#include "font.h"

char *p=NULL;

struct LcdDevice *init_lcd(const char *device)
{
	//申请空间
	struct LcdDevice* lcd = malloc(sizeof(struct LcdDevice));
	if(lcd == NULL)
	{
		return NULL;
	} 

	//1打开设备
	lcd->fd = open(device, O_RDWR);
	if(lcd->fd < 0)
	{
		perror("open lcd fail");
		free(lcd);
		return NULL;
	}
	
	//映射
	lcd->mp = mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcd->fd,0);

	return lcd;
}

void showbmp(char *tmp)
{
    int bmpfd;
    int lcdfd;
    int i;
    int x,y;
    //定义数组存放读取到的RGB数值
    char bmpbuf[800*480*3];  //char每个数据占1个字节
    //定义int数组存放转换得到的ARGB数据
    int lcdbuf[800*480]; //int每个数据占4个字节
    //定义中间变量存放数据
    int tempbuf[800*480];

    //打开800*480大小的bmp图片
    bmpfd=open(tmp,O_RDWR);
    if(bmpfd==-1)
    {
        printf("打开bmp失败!\n");
        //return -1;
    }
    //打开lcd的驱动
    lcdfd=open("/dev/fb0",O_RDWR);
    if(lcdfd==-1)
    {
        printf("打开lcd失败!\n");
        //return -1;
    }

    //跳过前面没有用的54个字节，从55字节开始读取真实的RGB
    lseek(bmpfd,54,SEEK_SET);

    //读取bmp的rgb数据，从图片的第55个字节开始读取
    read(bmpfd,bmpbuf,800*480*3);
    //bmpbuf[0] --》R
    //bmpbuf[1] --》G
    //bmpbuf[2] --》B
    //把三个字节的RGB--》转换成四个字节的ARGB
    //思路：用位或运算和左移实现数据拼接
    for(i=0; i<800*480; i++)
        lcdbuf[i]=bmpbuf[3*i]|bmpbuf[3*i+1]<<8|bmpbuf[3*i+2]<<16|0x00<<24;
                  // 00[2][1][0]  ABGR
                  // 00[5][4][3]

    //将颠倒的图片翻转过来(x,y)跟(x,479-y)交换即可
    for(x=0; x<800; x++)
    {
        for(y=0; y<480; y++)
            //不应该自己赋值给自己(会覆盖掉一部分像素点)
            //lcdbuf[(479-y)*800+x]=lcdbuf[y*800+x]; //  0-799  800-1599
            tempbuf[(479-y)*800+x]=lcdbuf[y*800+x];
    }
    //把转换得到的ARGB写入到lcd
	p  = mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcdfd,0); 
    memcpy(p,tempbuf,800*480*4);
	//write(lcdfd,tempbuf,800*480*4);

    //关闭
    close(bmpfd);
    close(lcdfd);
	munmap(p, 800*480*4);
}



int main(int argc ,char *argv[])
{
	if(argc != 2)
	{
		printf("请输入你的名字\n");
		return 0;
	}	
	
	showbmp("hh.bmp");
	//初始化Lcd
	struct LcdDevice* lcd = init_lcd("/dev/fb0");			
	//打开字体	
	font *f = fontLoad("/usr/share/fonts/DroidSansFallback.ttf");  
	while(1)
	{
		printf("请输入: 1.聊天室 2.人工智障 3.心灵鸡汤 4.退出\n");
		int num=0;
		scanf("%d",&num);
		if(num == 1)
		{
			//1.创建 TCP  通信协议
			int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
			if(tcp_socket < 0)
			{
				perror("");
				return 0; 
			}
			else
			{
				printf("创建成功\n");
			}
			
			int on=1;
			setsockopt(tcp_socket,SOL_SOCKET,SO_REUSEADDR,&on,4);//设置IP复用
			//设置链接的服务器地址信息 
			struct sockaddr_in  addr;  
			addr.sin_family   = AF_INET; //IPV4 协议  
			addr.sin_port     = htons(9301); //端口
			addr.sin_addr.s_addr = inet_addr("192.168.22.173");//服务器的IP地址
			//2.链接服务器 
			int ret=connect(tcp_socket,(struct sockaddr *)&addr,sizeof(addr));
			if(ret < 0)
			{
				perror("");
				return 0;
			}
			else
			{
				printf("链接服务器成功\n");
			}
			char buf[100]={0};
			strcpy(buf,argv[1]);
			write(tcp_socket,buf,sizeof(buf));
			
			printf("输入1聊天，输入2接发文件\n");
			int tmp=0;
			scanf("%d",&tmp);
			if(tmp==1)
			{	
				int pid = fork();
				if(pid == -1)
				{
					perror("fork");
					return -1;
				}
				else if(pid == 0)
				{	
					while(1)
					{
						char buf[1024]={0}; 
						int size=read(tcp_socket,buf,1024);
						if(size<=0)
						{
							printf("服务器关闭\n");
							close(tcp_socket);
							break;
						}
						if(strstr(buf,"公告"))
						{
							//字体大小的设置
							fontSetSize(f,36);
							//创建一个画板（点阵图）
							bitmap *bm = createBitmapWithInit(700,85,4,getColor(0,255,255,255)); //也可使用createBitmapWithInit函数，改变画板颜色
							//将字体写到点阵图上
							fontPrint(f,bm,0,0,buf,getColor(0,255,0,0),800);
							//把字体框输出到LCD屏幕上
							show_font_to_lcd(lcd->mp,100,75,bm);
							destroyBitmap(bm);//画板需要每次都销毁 
						}
						
						printf("服务器说:%s\n",buf);
						//字体大小的设置
						fontSetSize(f,36);
						//创建一个画板（点阵图）
						bitmap *bm = createBitmapWithInit(720,75,4,getColor(0,255,255,255)); //也可使用createBitmapWithInit函数，改变画板颜色
						//将字体写到点阵图上
						fontPrint(f,bm,0,0,buf,getColor(0,255,0,0),800);
						//把字体框输出到LCD屏幕上
						show_font_to_lcd(lcd->mp,0,0,bm);
						destroyBitmap(bm);//画板需要每次都销毁 
					}
				}
				else if(pid > 0)
				{
					//进行通信 
					printf("请输入你要发送的消息,输入 chat=名字 开始聊天,输入 exit 返回,输入 end 可重新选择聊天的人\n");
					while(1)
					{
						char buf[1024]={0};
						scanf("%s",buf);
						int tmp=strcmp(buf,"exit");
						if(tmp==0)
						{
							munmap(lcd, 800*480*4);
							close(tcp_socket);
							showbmp("hh.bmp");
							break;			
						}
						write(tcp_socket,buf,sizeof(buf));
					}
				}
			}
			if(tmp==2)
			{
				int pid=fork();
				if(pid == -1)
				{
					perror("fork");
					return -1;
				}
				else if(pid==0)
				{
					while(1)
					{
		
						char buf[1024]={0};
						read(tcp_socket,buf,1024);  
						if(strstr(buf,"send_file"))
						{
							int file_size=0;  
							sscanf(buf,"send_file %d",&file_size);
							printf("对方发送文件是否接收 大小为 %d  1 YES  2 NO\n",file_size);
						
							int a=0;
							scanf("%d",&a);
							
							char file_name[1024]={0};
							printf("请输入另存为名\n");
							scanf("%s",file_name);
							
							
							//应答对方已经接收到文件的大小 
							write(tcp_socket,"OK",strlen("OK"));
							
							if(a == 1)  //接收文件 
							{
								int fd=open(file_name,O_RDWR|O_CREAT|O_TRUNC,0777);
									if(fd < 0)
									{
										perror("接收失败");
										return 0;
									}

								//接收网络数据写入到本地文件中 
								int  recv_size=0;
								while(1)
								{
									char   recv[4096]={0}; 
									int size = read(tcp_socket,recv,4096); //读取网络数据 
									recv_size += size;
									printf("当前接收的大小 %d ,总大小 %d\n",size,recv_size);
									write(fd,recv,size); //写入到本地文件中  
									
									if(recv_size == file_size)
									{
										printf("接收完毕\n");
											//应答对方 
										write(tcp_socket,"END",strlen("END"));
										close(fd); 
										break;
									}						
								}
							}
							else
							{
								continue;
							}
						}
					}	
				}
				else if(pid >0)
				{
					while(1)
					{
						printf("输入1发送文件\n");
						int a=0; 
						scanf("%d",&a);		
						if(a == 1)  //文件的发送 
						{
							
							
							printf("请输入需要发送的文件名\n");
							char file_name[1024];
							scanf("%s",file_name);
						
							
							int fd = open(file_name,O_RDWR);
								if(fd < 0)
								{
									perror("文件不存在");
									break;
								}
							
								
							//获取发送文件的大小 
							int  file_size=lseek(fd,0,SEEK_END);  
								 lseek(fd,0,SEEK_SET);

							char  head[54]={0}; 
							sprintf(head,"send_file %d",file_size);
							write(tcp_socket,head,strlen(head));
							
							//等待对方回应  
							bzero(head,54);
							read(tcp_socket,head,54);
							
							
							if(strstr(head,"OK"))  //对方成功接收到 文件大小
							{
								int  send_size=0;
								while(1)
								{
									//读取文件中的数据  
									char send[4096]={0};
									int size=read(fd,send,4096);
							
									
									printf("发送的大小 %d 总大小 %d\n",size,send_size);
									
									//发送网络中 
									int  s_size=write(tcp_socket,send,size);
										send_size+=s_size;
										if(send_size == file_size)
										{
											printf("发送完毕等待对方接收完毕\n");
											bzero(head,54);
											read(tcp_socket,head,54);
											if(strstr(head,"END"))
											{
												printf("文件传输完毕\n");
												break;
											}
										}							
								}
							}
						
						}
						
					}
				}
			}
		}
		else if(num == 2)
		{
			while(1)
			{
				//1.新建TCP 通信对象 
				int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
				//2.链接服务器  
				//设置服务器的IP地址信息  
				struct sockaddr_in  addr;  
				addr.sin_family   = AF_INET; //IPV4 协议  
				addr.sin_port     = htons(80); //端口 80  ,所有的HTTP 服务器端口都是  80  
				addr.sin_addr.s_addr = inet_addr("47.107.155.132"); //服务器的IP 地址信息
				int ret=connect(tcp_socket,(struct sockaddr *)&addr,sizeof(addr));
				if(ret < 0)
				{
					perror("connect");
					return 0; 
				}
				else
				{
					printf("链接网络服务器成功\n");
				}
				printf("请输入你想知道的消息，有没有就不知道了,输入“退出”就返回\n");
				char name[1024]={0};
				memset(name,0,1024);
				scanf("%s",name);
				int hh=strcmp(name,"退出");
				if(hh == 0)
				{
					munmap(lcd, 800*480*4);
					showbmp("hh.bmp");	
					break;
				}
				char http[4096]={0};
				//重点！！定制HTTP 请求协议  
				//https://    api.qingyunke.com    /api.php?key=free&appid=0&msg=天气广州			
				sprintf(http,"GET /api.php?key=free&appid=0&msg=%s HTTP/1.1\r\nHost:api.qingyunke.com\r\n\r\n",name);
				//发送数据给服务器 
				write(tcp_socket,http,strlen(http));
				//获取服务器的信息 
				char  buf[4096]={0}; 
				char  *buf1=(char *)malloc(sizeof(buf)); 
				int size=read(tcp_socket,buf,4096);
				buf1 = strstr(buf,"{");
				printf("%s\n",buf1);
				//字体大小的设置
				fontSetSize(f,24);
				//创建一个画板（点阵图）
				bitmap *bm = createBitmapWithInit(800,310,4,getColor(0,255,255,255)); //也可使用createBitmapWithInit函数，改变画板颜色
				//将字体写到点阵图上
				fontPrint(f,bm,0,0,buf1,getColor(0,255,0,0),800);
				//把字体框输出到LCD屏幕上
				show_font_to_lcd(lcd->mp,0,160,bm);
				destroyBitmap(bm);//画板需要每次都销毁 
				close(tcp_socket);
			}
			
		}
		else if(num == 3)
		{
			while(1)
			{
				//1.新建TCP 通信对象 
				int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);

				//2.链接服务器  
				//设置服务器的IP地址信息  
				struct sockaddr_in  addr;  
				addr.sin_family   = AF_INET; //IPV4 协议  
				addr.sin_port     = htons(80); //端口 80  ,所有的HTTP 服务器端口都是  80  
				addr.sin_addr.s_addr = inet_addr("121.32.228.42"); //服务器的IP 地址信息
				int ret=connect(tcp_socket,(struct sockaddr *)&addr,sizeof(addr));
				if(ret < 0)
				{
					perror("connect");
					return 0; 
				}
				else
				{
					printf("链接网络服务器成功\n");
				}
				printf("输入其它信息就刷新信息，输入“退出”就返回\n");
				char name[1024]={0};
				memset(name,0,1024);
				scanf("%s",name);
				int hh=strcmp(name,"退出");
				if(hh == 0)
				{
					munmap(lcd, 800*480*4);
					showbmp("hh.bmp");	
					break;
				}
				else
				{
					//重点！！定制HTTP 请求协议  
					//https:// v1.alapi.cn /api/hitokoto		
					char *http="GET /api/hitokoto HTTP/1.1\r\nHost:v1.alapi.cn\r\n\r\n";
					//发送数据给服务器 
					write(tcp_socket,http,strlen(http));
					//获取服务器的信息 
					char  buf[4096]={0}; 
					char  *buf1=(char *)malloc(sizeof(buf)); 
					int size=read(tcp_socket,buf,4096);
					buf1 = strstr(buf,"hitokoto");
					printf("%s\n",buf1);
					//字体大小的设置
					fontSetSize(f,24);
					//创建一个画板（点阵图）
					bitmap *bm = createBitmapWithInit(800,310,4,getColor(0,255,255,255)); //也可使用createBitmapWithInit函数，改变画板颜色
					//将字体写到点阵图上
					fontPrint(f,bm,0,0,buf1,getColor(0,255,0,0),800);
					//把字体框输出到LCD屏幕上
					show_font_to_lcd(lcd->mp,0,160,bm);
					destroyBitmap(bm);//画板需要每次都销毁 
					close(tcp_socket);
				}
			}
		}
		else if(num==4)
		{
			break;
		}
		else
		{
			printf("无该功能\n");
			return 0;
		}
	}
	fontUnload(f);  //字库不需要每次都关闭 
	return 0;
}

