#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <windows.h>
#pragma comment(lib,"ws2_32.lib")   
using namespace std;

#define PORT 6000		 //端口号
#define BufSize 1024	 //缓冲区大小
SOCKET clientSocket;	 //定义客户端socket

DWORD WINAPI recvThread() //接收消息线程
{
	while (1)
	{
		char buffer[BufSize] = {};//接收数据缓冲区
		if (recv(clientSocket, buffer, sizeof(buffer), 0) > 0) //接收
		{
			cout << buffer << endl;
		}
		else if (recv(clientSocket, buffer, sizeof(buffer), 0) < 0)
		{
			cout << "——连接失败..." << endl;
			break;
		}
	}
	Sleep(100);
	return 0;
}

int main()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	//绑定服务器地址
	SOCKADDR_IN servAddr;				//服务器地址
	servAddr.sin_family = AF_INET;		//地址类型
	servAddr.sin_port = htons(PORT);	//端口号
	if (inet_pton(AF_INET, "127.0.0.1", &(servAddr.sin_addr)) != 1) //将字符串 "127.0.0.1" 转换为二进制形式的 IPv4 地址并存储在 servAddr.sin_addr 中
	{
		cout << "——无法绑定地址" << endl;
		exit(EXIT_FAILURE);
	}

	//向服务器发起请求
	if (connect(clientSocket, (SOCKADDR*)&servAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << "——无法连接到服务器：" << WSAGetLastError() << endl;
		exit(EXIT_FAILURE);
	}

	//创建消息线程
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)recvThread, NULL, 0, 0);


	char msg[BufSize] = {};
	cout << "——欢迎加入聊天！" << endl;

	//发送消息
	while (1)
	{
		cin.getline(msg, sizeof(msg));
		if (strcmp(msg, "exit") == 0) //输入exit退出
		{
			break;
		}
		send(clientSocket, msg, sizeof(msg), 0);//发送消息
	}

	closesocket(clientSocket);
	WSACleanup();

	return 0;
}