/*
struct Head {
	unsigned char checknum; //校验和
	unsigned char flag;		//标志位
	uint8_t seq;			//表示数据包的序列号, 一个字节为周期
	uint8_t len;			//表示数据包的长度（包含头）
};
*/

#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996)
#include <WS2tcpip.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <queue>
#include <vector>
#include <mutex>
#include <thread>
using namespace  std;

const unsigned char SYN = 0x01;
const unsigned char SYN_ACK = 0x02;
const unsigned char FIN = 0x03;
const unsigned char FIN_ACK = 0x04;
const unsigned char ACK = 0x05;
const unsigned char END = 0x06;
const unsigned char NOT_END = 0x07;
const unsigned char SEND_OVER = 0x08;

int WIN_SIZE = 10;
const int TIMEOUT = 1000;
const int SEQ_SPACE = 100;
const int MAX_UDP_LEN = 10000;

int SUCCESS_SEND = 0;        //发送的包的数目
int SUCCESS_RECV = 0;		 //成功发送的包的数目

SOCKET m_ClientSocket;		 //客户端套接字
SOCKADDR_IN m_ServerAddress; //远程地址
int len_addr = sizeof(m_ServerAddress);

//计算校验和
unsigned char check_sum(char* package, int len)
{
	if (len == 0) {
		return ~(0);
	}
	unsigned long sum = 0;
	int i = 0;
	while (len--) {
		sum += (unsigned char)package[i++];
		if (sum & 0xFF00) {
			sum &= 0x00FF;
			sum++;
		}
	}
	return ~(sum & 0x00FF);
}

void send_package(char* start, int length, uint8_t req, int is_END = 0) {
	char* package;
	int Len = length + 4;			  //数据包长度（含头4字节）
	package = new char[length + 4];   //数据包

	//组装数据包
	if (is_END) {
		package[1] = END;		      //标志位
	}
	else
	{
		package[1] = NOT_END;
	}
	package[2] = req;				  //序号
	package[3] = length;			  //数据长度（不含头）

	for (int i = 0; i < length; i++) {// 数据
		package[i + 4] = start[i];
	}
	package[0] = check_sum(package + 1, Len - 1);//校验和

	sendto(m_ClientSocket, package, Len, 0, (sockaddr*)&m_ServerAddress, sizeof(m_ServerAddress));
	SUCCESS_SEND++;
	return;
}

//发送数据函数
//len:字节数
void send_buffer(char* buffer, int len) {

	queue<int> win;							//窗口队列
	int timer[SEQ_SPACE + 1] = { 0 };		//计时器，记录包发送出去的时间

	int package_num = len / MAX_UDP_LEN + (len % MAX_UDP_LEN != 0);//数据包总数
	int base = 0;			//基序号(0到package_num-1)
	int has_send = 0;		//已发送未确认的数量(0到package_num-1)
	int next_seq = base;	//标识未确认的数据包
	int has_ack = 0;		//已确认的数量

	while (true) {
		if (has_ack == package_num) {
			cout << "接收到所有ACK，发送完毕" << endl;
			break;
		}
		if (next_seq >= SEQ_SPACE) { //在序号空间内
			next_seq %= SEQ_SPACE;
		}
		//窗口还有空余并且还要数据包需要发送
		if (win.size() < WIN_SIZE && has_send != package_num) {
			send_package(buffer + has_send * MAX_UDP_LEN,
				has_send == package_num - 1 ? len - (package_num - 1) * MAX_UDP_LEN : MAX_UDP_LEN,
				next_seq,
				has_send == package_num - 1);
			//cout << "发包总次数" << SUCCESS_SEND << endl;
			timer[next_seq] = clock();
			cout << "已发送数据包：" << (next_seq) << endl << endl;
			win.push(next_seq);
			//cout << "窗口队列win的大小：" << win.size() << endl;
			next_seq++;
			has_send++;
		}

		char recv[3];
		if (recvfrom(m_ClientSocket, recv, 3, 0, (sockaddr*)&m_ServerAddress, &len_addr) != SOCKET_ERROR && check_sum(recv, 3) == 0 && recv[1] == ACK)
		{
			//( (win.front() > 89) && ((int)recv[2] < 11))条件是应对序号空间的问题
			//如果Server接收了99,0,1等等，但ACK_99丢失，ACK_0传达，会出现 win.front()=99 > ACK=1 的情况，从而一直重传99，但窗口无法前进
			while ((win.front() > SEQ_SPACE - WIN_SIZE) && ((int)recv[2] < WIN_SIZE)) {
				SUCCESS_RECV++;
				has_ack++;							//确认发送成功的包（收到有效ACK）++
				base++;								//窗口前进
				win.pop();
			}

			//根据接收到的ACK来前进窗口
			//为什么不在while循环里写 <= 呢，因为最后一次判定会触发win.front为空的情况导致中断。。。

			while ((win.front() < (int)recv[2])) {
				if ((win.front() < WIN_SIZE) && ((int)recv[2] > SEQ_SPACE - WIN_SIZE)) //和上面类似，这种情况的ack其实小于front，不用管
				{
					SUCCESS_RECV++;
					break;
				}
				//cout << "接收到ACK：" << (int)recv[2] << endl;
				SUCCESS_RECV++;
				has_ack++;							//确认发送成功的包（收到有效ACK）++
				base++;								//窗口前进
				win.pop();
			}

			cout << "接收到ACK：" << (int)recv[2] << endl;
			cout << "窗口队列头的序号：" << win.front() << endl;
			cout << "窗口队列的大小：" << win.size() << endl << endl;
			//cout << "窗口队列头_and_ACK：" << win.front() << " " << (int)recv[2] << endl;

			SUCCESS_RECV++;
			if (win.front() == (int)recv[2]) {
				//cout << "窗口前进" << endl << endl;
				SUCCESS_RECV++;
				has_ack++;
				base++;
				win.pop();
			}
			//对于ACK<win.front()的情况不用理会
		}
		else {
			//最远的已发送未确认的包就是win.front()，检测其是否超时
			if (clock() - timer[win.front()] > TIMEOUT) {
				//清理窗口
				next_seq = base % SEQ_SPACE;
				has_send -= win.size();
				while (!win.empty()) win.pop();
				cout << "超时重传，从数据包" << next_seq << "开始重传" << endl;
			}
		}
	}

	//char package[2];
	//package[1] = SEND_OVER;
	//package[2] = 11;
	//package[0] = check_sum(package + 1, 1);
	//sendto(m_ClientSocket, package, 3, 0, (sockaddr*)&m_ServerAddress, sizeof(m_ServerAddress));
	//cout << "已发送SEND_OVER" << endl;
	return;
}
void send_file()
{
	// 发送文件
	int len = 0;
	char* buffer = new char[100000000];
	string filename;

	cout << "请输入窗口大小（默认10）： ";
	cin >> WIN_SIZE;

	cout << endl  << "请输入发送的文件名" << endl;
	while (true)
	{
		cin >> filename;
		ifstream fin(filename.c_str(), ifstream::binary);//打开输入的文件，以二进制模式读取
		if (!fin) {
			cout << "文件名有误,请重新输入" << endl;
			continue;
		}
		unsigned char t = fin.get();//按字节读取数据
		while (fin) {
			buffer[len++] = t;
			t = fin.get();
		}
		fin.close();
		break;
	}

	int time_begin = clock();
	send_buffer((char*)(filename.c_str()), filename.length());	//文件名
	cout << "文件名传输完毕" << endl << endl;
	send_buffer(buffer, len);									//文件内容
	int time_end = clock();
	cout << filename << "文件传输完毕" << endl;

	int time = (time_end - time_begin) / CLOCKS_PER_SEC;
	cout << "传输时间：" << time << endl;
	if (time != 0)
	{
		double kbps = ((len * 8.0) / 1000) / time;
		cout << "吞吐量：" << kbps << " kbps" << endl;
	}
	cout << "丢包率：" << (SUCCESS_SEND - SUCCESS_RECV) * 100 / SUCCESS_SEND << "%" << endl;
	cout << endl << endl;
}

void three_way_handshake()
{
	//三次握手建立连接
	while (true)
	{
		char sendBuf[2];
		sendBuf[1] = SYN;
		sendBuf[0] = check_sum(sendBuf + 1, 1);

		int ret = sendto(m_ClientSocket, sendBuf, 2, 0, (sockaddr*)&m_ServerAddress, sizeof(m_ServerAddress));
		if (ret != SOCKET_ERROR)
			cout << "已发送第一次握手请求SYN" << endl;
		else {
			cout << "发送握手请求失败……" << endl;
			return;
		}
		//开始计时
		int begin = clock();

		char recv[2];  //接收缓存区
		int is_timeout = 0;

		u_long mode = 1;  // 1 表示非阻塞，0 表示阻塞
		ioctlsocket(m_ClientSocket, FIONBIO, &mode);
		while (recvfrom(m_ClientSocket, recv, 2, 0, (sockaddr*)&m_ServerAddress, &len_addr) == SOCKET_ERROR)
		{
			if (clock() - begin > TIMEOUT)
			{
				is_timeout = 1;
				break;
			}
		}

		//没有超时，接收到的数据包检验正确，是二次握手 
		if (is_timeout == 0 && check_sum(recv, 2) == 0 && recv[1] == SYN_ACK)
		{
			cout << "接收到服务端二次握手SYN_ACK" << endl;
			sendBuf[1] = ACK;
			sendBuf[0] = check_sum(sendBuf + 1, 1);
			sendto(m_ClientSocket, sendBuf, 2, 0, (sockaddr*)&m_ServerAddress, sizeof(m_ServerAddress));
			cout << "已发送第三次握手ACK" << endl;
			break;
		}
	}
	cout << "连接到服务端" << endl << endl;
	return;
}


void double_wave()
{
	cout << "进行二次挥手ing" << endl;
	int timeout_num = 0;
	while (true)
	{
		char package[2];
		package[1] = FIN;
		package[0] = check_sum(package + 1, 1);
		sendto(m_ClientSocket, package, 2, 0, (sockaddr*)&m_ServerAddress, sizeof(m_ServerAddress));
		cout << "已发送第一次挥手" << endl;

		int begin = clock();
		char recv[2];
		int fail = 0;
		//超时重传
		while (recvfrom(m_ClientSocket, recv, 2, 0, (sockaddr*)&m_ServerAddress, &len_addr) == SOCKET_ERROR)
		{
			if (clock() - begin > TIMEOUT)
			{
				fail = 1;
				timeout_num++;
				break;
			}
		};
		if (fail == 0 && check_sum(recv, 2) == 0 && recv[1] == FIN_ACK)
		{
			cout << "接收第二次挥手" << endl;
			break;
		}
		if (timeout_num >= 3) {
			cout << "接收第二次挥手" << endl;
			break;
		}
	}
	cout << "挥手成功，断开连接" << endl;
}



int main() {
	WSADATA  wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup error:" << GetLastError() << endl;
		return false;
	}

	//创建SOCKET
	m_ClientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_ClientSocket == INVALID_SOCKET)
	{
		closesocket(m_ClientSocket);
		m_ClientSocket = INVALID_SOCKET;
		return false;
	}

	// 远端地址(路由器:4003,Server:4002）
	const char* ip = "127.0.0.1";
	int	port = 4003;
	//int	port = 4002;
	m_ServerAddress.sin_family = AF_INET;				//ipv4
	m_ServerAddress.sin_port = htons(port);				//转换为网络字节序（大端序）
	inet_pton(AF_INET, ip, &m_ServerAddress.sin_addr);  // 将字符串格式的远程IP地址转换为二进制格式，并赋值给 m_ServerAddress.sin_addr

	// 绑定本地地址
	sockaddr_in localAddress;
	localAddress.sin_family = AF_INET;
	localAddress.sin_port = htons(4001);
	inet_pton(AF_INET, "127.0.0.1", &localAddress.sin_addr);

	// 绑定套接字到指定的本地地址
	if (bind(m_ClientSocket, (sockaddr*)&localAddress, sizeof(localAddress)) == SOCKET_ERROR) {
		// 处理绑定失败的情况
		cout << "Bind failed" << endl;
		return 0;
	}

	//三次握手
	three_way_handshake();

	//发送数据
	send_file();
	cout << "发包总次数：" << SUCCESS_SEND << endl;

	//两次挥手
	double_wave();

	//关闭套接字
	closesocket(m_ClientSocket);
	//清除资源
	WSACleanup();

	system("pause");
	return 0;
}
