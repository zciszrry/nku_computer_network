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
using namespace  std;

const unsigned char SYN = 0x01;
const unsigned char SYN_ACK = 0x02;
const unsigned char FIN = 0x03;
const unsigned char FIN_ACK = 0x04;
const unsigned char ACK = 0x05;
const unsigned char END = 0x06;
const unsigned char NOT_END = 0x07;

const int TIMEOUT = 1000;
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

bool send_package(char* start, int length, uint8_t req, int is_END = 0) {
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
	package[0] = check_sum(package + 1, Len-1);//校验和

	while (1) {
		//套接字，数据包，长度，0，目的地址，地址大小
		sendto(m_ClientSocket, package, Len, 0, (sockaddr*)&m_ServerAddress, sizeof(m_ServerAddress));
		SUCCESS_SEND++;

		int begintime = clock();
		char recv[3];
		int is_send = 0;

		//超时重传
		//套接字，数据包，长度，0，地址，地址长度
		while (recvfrom(m_ClientSocket, recv, 3, 0, (sockaddr*)&m_ServerAddress, &len_addr) == SOCKET_ERROR) {
			if (clock() - begintime > TIMEOUT) {
				cout << "超时，进行重传" << endl;
				is_send = 1;
				break;
			}
		}
		if (is_send == 0 && check_sum(recv, 3) == 0 && recv[1] == ACK && recv[2] == (uint8_t)req)
		{
			cout << "接收到ACK： " << int(recv[2])<< endl;
			SUCCESS_RECV++;
			return true;
		}
	}
}
//发送数据函数
//len:字节数
void send_buffer(char* buffer, int len) {
	int package_num = len / MAX_UDP_LEN + (len % MAX_UDP_LEN != 0);  //数据包的数目

	int flag_last = 0;

	for (int i = 0; i < package_num; i++) {	
		if (i == package_num -1 ) {
			flag_last = 1;
		}
		uint8_t seq = i % 100;
		cout << "发送数据包序号: " << (int)seq << endl;
		send_package(buffer + i * MAX_UDP_LEN, i == package_num-1 ? len - (package_num - 1) * MAX_UDP_LEN : MAX_UDP_LEN, seq,
			flag_last);
	}
}

void send_file()
{
	// 发送文件
	int len = 0;
	char* buffer = new char[100000000];
	string filename;

	cout << "请输入发送的文件名" << endl;
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
		//cout << "测试点2" << endl;

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
	cout << "连接到服务端" << endl;
	return;
}


void double_wave()
{
	cout << "进行二次挥手ing" << endl;
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
				break;
			}
		};
		if (fail == 0 && check_sum(recv, 2) == 0 && recv[1] == FIN_ACK)
		{
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

	// 远端地址(路由器）
	const char* ip = "127.0.0.1";
	int	port = 4003;
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

	//两次挥手
	double_wave();

	//关闭套接字
	closesocket(m_ClientSocket);
	//清除资源
	WSACleanup();

	system("pause");
	return 0;
}
