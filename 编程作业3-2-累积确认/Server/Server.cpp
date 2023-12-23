#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996)
#include <WS2tcpip.h>
#include <iostream>
#include <queue>
#include <string>
#include <fstream>
#include <time.h>
using namespace std;

const unsigned char SYN = 0x01;
const unsigned char SYN_ACK = 0x02;
const unsigned char FIN = 0x03;
const unsigned char FIN_ACK = 0x04;
const unsigned char ACK = 0x05;
const unsigned char END = 0x06;
const unsigned char NOT_END = 0x07;
const unsigned char SEND_OVER = 0x08;

const int TIMEOUT = 1000;
const int MAX_UDP_LEN = 10000;
int SUCC_RECV = 0;

char buffer[100000000];
int len;

SOCKET m_ServerSocket;
SOCKADDR_IN m_ClientAdress;
int len_addr = sizeof(m_ClientAdress);

//检验校验和
//遍历数据包中的每个字节，将其加到 sum 中
//如果 sum 的高位不为 0，将高位清零，然后将 sum 加 1。
//最后，返回 sum 的补码取反。
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

void recv_packet(char* message, int& len_recv) {
	char recv[MAX_UDP_LEN + 4];

	uint8_t recv_req = 0;  //数据包序号
	len_recv = 0;		   //缓存区字节序号
	while (1) {
		while (1) {
			if (recv_req >= 100)
			{
				recv_req %= 100;
			}
			memset(recv, 0, sizeof(recv));
			while (recvfrom(m_ServerSocket, recv, MAX_UDP_LEN + 4, 0, (sockaddr*)&m_ClientAdress, &len_addr) == SOCKET_ERROR);

			char send[3];
			int flag = 0;

			SUCC_RECV++;
			//发送ACK确认的序列号
			if (check_sum(recv, MAX_UDP_LEN + 4) == 0 && (uint8_t)recv[2] == recv_req) {
				recv_req++;
				flag = 1;
			}

			send[1] = ACK;				//标志
			if (recv_req == 0)
				{
				send[2] = 99;
				cout << "接收端确认序列号: " << 99 << endl;
				}		//序号
			else
				{
				send[2] = recv_req - 1;
				cout << "接收端确认序列号: " << recv_req - 1 << endl;
				}

			send[0] = check_sum(send + 1, 2); //校验和
			
			sendto(m_ServerSocket, send, 3, 0, (sockaddr*)&m_ClientAdress, sizeof(m_ClientAdress));

			if (flag)
				break;
		}

		// 将数据放入缓存区中
		for (int i = 4; i < MAX_UDP_LEN + 4; i++) {
			message[len_recv++] = recv[i];
		}
		if (recv[1] == END) {
			cout << "已接收最后一个数据包" << endl;
			for (int i = 4; i < recv[3] + 4; i++)
				message[len_recv++] = recv[i];
			break;
		}
		//if (recv[1] == SEND_OVER) {
		//	cout << "接收到SEND_OVER" << endl;
		//	break;
		//}
	}
}

void recv_file()
{
	printf("开始接收文件\n");
	recv_packet(buffer, len);
	buffer[len] = 0;
	cout <<"文件名："<< buffer << endl;//输出文件名
	string file_name(buffer);

	//然后接收文件内容
	recv_packet(buffer, len);
	printf("信息接收成功\n");
	ofstream fout(file_name.c_str(), ofstream::binary);//根据文件名打开/创建文件
	for (int i = 0; i < len; i++)
		fout << buffer[i];
	fout.close();

	printf("文件接收成功\n");
	cout << "-----------------------------------------------------------------" << endl;
}

void three_way_handshake()
{
	while (true)
	{
		char recv[2];
		while (recvfrom(m_ServerSocket, recv, 2, 0, (sockaddr*)&m_ClientAdress, &len_addr) == SOCKET_ERROR);
		//不是SYN或者数据有损
		if (check_sum(recv, 2) != 0 || recv[1] != SYN) {
			continue;
		}
		cout << "接收到客户端第一次SYN" << endl;
		while (1)
		{
			//二次握手
			recv[1] = SYN_ACK;
			recv[0] = check_sum(recv + 1, 1);
			sendto(m_ServerSocket, recv, 2, 0, (sockaddr*)&m_ClientAdress, sizeof(m_ClientAdress));
			cout << "已发送SYN_ACK" << endl;

			int begin = clock();
			int is_timeout = 0;
			while (recvfrom(m_ServerSocket, recv, 2, 0, (sockaddr*)&m_ClientAdress, &len_addr) == SOCKET_ERROR)
			{
				if (clock() - begin > TIMEOUT)
				{
					is_timeout = 1;
					break;
				}
			}
			if (is_timeout == 0 && check_sum(recv, 2) == 0 && recv[1] == SYN)
				continue;

			//没有超时，检验和正确，三次握手
			if (is_timeout == 0 && check_sum(recv, 2) == 0 && recv[1] == ACK) {
				cout << "接收到客户端ACK" << endl;
				break;
			}
		}
		break;
	}
	cout << "已完成三次握手" << endl;
}

void double_wave()
{
	printf("开始断开连接\n");
	while (1)
	{
		char recv[2];
		while (recvfrom(m_ServerSocket, recv, 2, 0, (sockaddr*)&m_ClientAdress, &len_addr) == SOCKET_ERROR);
		if (check_sum(recv, 2) != 0 || recv[1] != (char)FIN)
			continue;
		printf("%s\n", "成功接收第一次挥手消息");
		recv[1] = FIN_ACK;
		recv[0] = check_sum(recv + 1, 1);
		sendto(m_ServerSocket, recv, 2, 0, (sockaddr*)&m_ClientAdress, sizeof(m_ClientAdress));
		printf("%s\n", "成功发送第二次挥手");
		break;
	}
	printf("挥手成功, 断开连接\n");
}

int main() {
	WSADATA  wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup error:" << GetLastError() << endl;
		return false;
	}

	// socket对象
	m_ServerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_ServerSocket == INVALID_SOCKET)
	{
		closesocket(m_ServerSocket);
		m_ServerSocket = INVALID_SOCKET;
		return false;
	}

	// 绑定
	const char* ip = "127.0.0.1";
	int port = 4002;
	SOCKADDR_IN m_BindAddress;
	m_BindAddress.sin_family = AF_INET;
	m_BindAddress.sin_addr.S_un.S_addr = inet_addr(ip);
	m_BindAddress.sin_port = htons(port);
	auto ret = bind(m_ServerSocket, (sockaddr*)&m_BindAddress, sizeof(SOCKADDR));
	if (ret == SOCKET_ERROR)
	{
		closesocket(m_ServerSocket);
		m_ServerSocket = INVALID_SOCKET;
		return false;
	}
	cout << "服务端正在运行------";
	printf("ip: %s, 端口: %d\n", inet_ntoa(m_BindAddress.sin_addr), ntohs(m_BindAddress.sin_port));

	//三次握手
	three_way_handshake();

	//接收数据
	recv_file();
	cout << "接收到的包总数：" << SUCC_RECV << endl;

	//两次挥手
	double_wave();

	//关闭套接字
	closesocket(m_ServerSocket);
	//清理资源
	WSACleanup();

	system("pause");
	return 0;
}
