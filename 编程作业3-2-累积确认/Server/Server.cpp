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

//����У���
//�������ݰ��е�ÿ���ֽڣ�����ӵ� sum ��
//��� sum �ĸ�λ��Ϊ 0������λ���㣬Ȼ�� sum �� 1��
//��󣬷��� sum �Ĳ���ȡ����
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

	uint8_t recv_req = 0;  //���ݰ����
	len_recv = 0;		   //�������ֽ����
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
			//����ACKȷ�ϵ����к�
			if (check_sum(recv, MAX_UDP_LEN + 4) == 0 && (uint8_t)recv[2] == recv_req) {
				recv_req++;
				flag = 1;
			}

			send[1] = ACK;				//��־
			if (recv_req == 0)
				{
				send[2] = 99;
				cout << "���ն�ȷ�����к�: " << 99 << endl;
				}		//���
			else
				{
				send[2] = recv_req - 1;
				cout << "���ն�ȷ�����к�: " << recv_req - 1 << endl;
				}

			send[0] = check_sum(send + 1, 2); //У���
			
			sendto(m_ServerSocket, send, 3, 0, (sockaddr*)&m_ClientAdress, sizeof(m_ClientAdress));

			if (flag)
				break;
		}

		// �����ݷ��뻺������
		for (int i = 4; i < MAX_UDP_LEN + 4; i++) {
			message[len_recv++] = recv[i];
		}
		if (recv[1] == END) {
			cout << "�ѽ������һ�����ݰ�" << endl;
			for (int i = 4; i < recv[3] + 4; i++)
				message[len_recv++] = recv[i];
			break;
		}
		//if (recv[1] == SEND_OVER) {
		//	cout << "���յ�SEND_OVER" << endl;
		//	break;
		//}
	}
}

void recv_file()
{
	printf("��ʼ�����ļ�\n");
	recv_packet(buffer, len);
	buffer[len] = 0;
	cout <<"�ļ�����"<< buffer << endl;//����ļ���
	string file_name(buffer);

	//Ȼ������ļ�����
	recv_packet(buffer, len);
	printf("��Ϣ���ճɹ�\n");
	ofstream fout(file_name.c_str(), ofstream::binary);//�����ļ�����/�����ļ�
	for (int i = 0; i < len; i++)
		fout << buffer[i];
	fout.close();

	printf("�ļ����ճɹ�\n");
	cout << "-----------------------------------------------------------------" << endl;
}

void three_way_handshake()
{
	while (true)
	{
		char recv[2];
		while (recvfrom(m_ServerSocket, recv, 2, 0, (sockaddr*)&m_ClientAdress, &len_addr) == SOCKET_ERROR);
		//����SYN������������
		if (check_sum(recv, 2) != 0 || recv[1] != SYN) {
			continue;
		}
		cout << "���յ��ͻ��˵�һ��SYN" << endl;
		while (1)
		{
			//��������
			recv[1] = SYN_ACK;
			recv[0] = check_sum(recv + 1, 1);
			sendto(m_ServerSocket, recv, 2, 0, (sockaddr*)&m_ClientAdress, sizeof(m_ClientAdress));
			cout << "�ѷ���SYN_ACK" << endl;

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

			//û�г�ʱ���������ȷ����������
			if (is_timeout == 0 && check_sum(recv, 2) == 0 && recv[1] == ACK) {
				cout << "���յ��ͻ���ACK" << endl;
				break;
			}
		}
		break;
	}
	cout << "�������������" << endl;
}

void double_wave()
{
	printf("��ʼ�Ͽ�����\n");
	while (1)
	{
		char recv[2];
		while (recvfrom(m_ServerSocket, recv, 2, 0, (sockaddr*)&m_ClientAdress, &len_addr) == SOCKET_ERROR);
		if (check_sum(recv, 2) != 0 || recv[1] != (char)FIN)
			continue;
		printf("%s\n", "�ɹ����յ�һ�λ�����Ϣ");
		recv[1] = FIN_ACK;
		recv[0] = check_sum(recv + 1, 1);
		sendto(m_ServerSocket, recv, 2, 0, (sockaddr*)&m_ClientAdress, sizeof(m_ClientAdress));
		printf("%s\n", "�ɹ����͵ڶ��λ���");
		break;
	}
	printf("���ֳɹ�, �Ͽ�����\n");
}

int main() {
	WSADATA  wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup error:" << GetLastError() << endl;
		return false;
	}

	// socket����
	m_ServerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_ServerSocket == INVALID_SOCKET)
	{
		closesocket(m_ServerSocket);
		m_ServerSocket = INVALID_SOCKET;
		return false;
	}

	// ��
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
	cout << "�������������------";
	printf("ip: %s, �˿�: %d\n", inet_ntoa(m_BindAddress.sin_addr), ntohs(m_BindAddress.sin_port));

	//��������
	three_way_handshake();

	//��������
	recv_file();
	cout << "���յ��İ�������" << SUCC_RECV << endl;

	//���λ���
	double_wave();

	//�ر��׽���
	closesocket(m_ServerSocket);
	//������Դ
	WSACleanup();

	system("pause");
	return 0;
}
