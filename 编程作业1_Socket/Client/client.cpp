#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <windows.h>
#pragma comment(lib,"ws2_32.lib")   
using namespace std;

#define PORT 6000		 //�˿ں�
#define BufSize 1024	 //��������С
SOCKET clientSocket;	 //����ͻ���socket

DWORD WINAPI recvThread() //������Ϣ�߳�
{
	while (1)
	{
		char buffer[BufSize] = {};//�������ݻ�����
		if (recv(clientSocket, buffer, sizeof(buffer), 0) > 0) //����
		{
			cout << buffer << endl;
		}
		else if (recv(clientSocket, buffer, sizeof(buffer), 0) < 0)
		{
			cout << "��������ʧ��..." << endl;
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

	//�󶨷�������ַ
	SOCKADDR_IN servAddr;				//��������ַ
	servAddr.sin_family = AF_INET;		//��ַ����
	servAddr.sin_port = htons(PORT);	//�˿ں�
	if (inet_pton(AF_INET, "127.0.0.1", &(servAddr.sin_addr)) != 1) //���ַ��� "127.0.0.1" ת��Ϊ��������ʽ�� IPv4 ��ַ���洢�� servAddr.sin_addr ��
	{
		cout << "�����޷��󶨵�ַ" << endl;
		exit(EXIT_FAILURE);
	}

	//���������������
	if (connect(clientSocket, (SOCKADDR*)&servAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << "�����޷����ӵ���������" << WSAGetLastError() << endl;
		exit(EXIT_FAILURE);
	}

	//������Ϣ�߳�
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)recvThread, NULL, 0, 0);


	char msg[BufSize] = {};
	cout << "������ӭ�������죡" << endl;

	//������Ϣ
	while (1)
	{
		cin.getline(msg, sizeof(msg));
		if (strcmp(msg, "exit") == 0) //����exit�˳�
		{
			break;
		}
		send(clientSocket, msg, sizeof(msg), 0);//������Ϣ
	}

	closesocket(clientSocket);
	WSACleanup();

	return 0;
}