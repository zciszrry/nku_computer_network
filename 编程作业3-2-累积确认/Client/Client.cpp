/*
struct Head {
	unsigned char checknum; //У���
	unsigned char flag;		//��־λ
	uint8_t seq;			//��ʾ���ݰ������к�, һ���ֽ�Ϊ����
	uint8_t len;			//��ʾ���ݰ��ĳ��ȣ�����ͷ��
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

int SUCCESS_SEND = 0;        //���͵İ�����Ŀ
int SUCCESS_RECV = 0;		 //�ɹ����͵İ�����Ŀ

SOCKET m_ClientSocket;		 //�ͻ����׽���
SOCKADDR_IN m_ServerAddress; //Զ�̵�ַ
int len_addr = sizeof(m_ServerAddress);

//����У���
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
	int Len = length + 4;			  //���ݰ����ȣ���ͷ4�ֽڣ�
	package = new char[length + 4];   //���ݰ�

	//��װ���ݰ�
	if (is_END) {
		package[1] = END;		      //��־λ
	}
	else
	{
		package[1] = NOT_END;
	}
	package[2] = req;				  //���
	package[3] = length;			  //���ݳ��ȣ�����ͷ��

	for (int i = 0; i < length; i++) {// ����
		package[i + 4] = start[i];
	}
	package[0] = check_sum(package + 1, Len - 1);//У���

	sendto(m_ClientSocket, package, Len, 0, (sockaddr*)&m_ServerAddress, sizeof(m_ServerAddress));
	SUCCESS_SEND++;
	return;
}

//�������ݺ���
//len:�ֽ���
void send_buffer(char* buffer, int len) {

	queue<int> win;							//���ڶ���
	int timer[SEQ_SPACE + 1] = { 0 };		//��ʱ������¼�����ͳ�ȥ��ʱ��

	int package_num = len / MAX_UDP_LEN + (len % MAX_UDP_LEN != 0);//���ݰ�����
	int base = 0;			//�����(0��package_num-1)
	int has_send = 0;		//�ѷ���δȷ�ϵ�����(0��package_num-1)
	int next_seq = base;	//��ʶδȷ�ϵ����ݰ�
	int has_ack = 0;		//��ȷ�ϵ�����

	while (true) {
		if (has_ack == package_num) {
			cout << "���յ�����ACK���������" << endl;
			break;
		}
		if (next_seq >= SEQ_SPACE) { //����ſռ���
			next_seq %= SEQ_SPACE;
		}
		//���ڻ��п��ಢ�һ�Ҫ���ݰ���Ҫ����
		if (win.size() < WIN_SIZE && has_send != package_num) {
			send_package(buffer + has_send * MAX_UDP_LEN,
				has_send == package_num - 1 ? len - (package_num - 1) * MAX_UDP_LEN : MAX_UDP_LEN,
				next_seq,
				has_send == package_num - 1);
			//cout << "�����ܴ���" << SUCCESS_SEND << endl;
			timer[next_seq] = clock();
			cout << "�ѷ������ݰ���" << (next_seq) << endl << endl;
			win.push(next_seq);
			//cout << "���ڶ���win�Ĵ�С��" << win.size() << endl;
			next_seq++;
			has_send++;
		}

		char recv[3];
		if (recvfrom(m_ClientSocket, recv, 3, 0, (sockaddr*)&m_ServerAddress, &len_addr) != SOCKET_ERROR && check_sum(recv, 3) == 0 && recv[1] == ACK)
		{
			//( (win.front() > 89) && ((int)recv[2] < 11))������Ӧ����ſռ������
			//���Server������99,0,1�ȵȣ���ACK_99��ʧ��ACK_0�������� win.front()=99 > ACK=1 ��������Ӷ�һֱ�ش�99���������޷�ǰ��
			while ((win.front() > SEQ_SPACE - WIN_SIZE) && ((int)recv[2] < WIN_SIZE)) {
				SUCCESS_RECV++;
				has_ack++;							//ȷ�Ϸ��ͳɹ��İ����յ���ЧACK��++
				base++;								//����ǰ��
				win.pop();
			}

			//���ݽ��յ���ACK��ǰ������
			//Ϊʲô����whileѭ����д <= �أ���Ϊ���һ���ж��ᴥ��win.frontΪ�յ���������жϡ�����

			while ((win.front() < (int)recv[2])) {
				if ((win.front() < WIN_SIZE) && ((int)recv[2] > SEQ_SPACE - WIN_SIZE)) //���������ƣ����������ack��ʵС��front�����ù�
				{
					SUCCESS_RECV++;
					break;
				}
				//cout << "���յ�ACK��" << (int)recv[2] << endl;
				SUCCESS_RECV++;
				has_ack++;							//ȷ�Ϸ��ͳɹ��İ����յ���ЧACK��++
				base++;								//����ǰ��
				win.pop();
			}

			cout << "���յ�ACK��" << (int)recv[2] << endl;
			cout << "���ڶ���ͷ����ţ�" << win.front() << endl;
			cout << "���ڶ��еĴ�С��" << win.size() << endl << endl;
			//cout << "���ڶ���ͷ_and_ACK��" << win.front() << " " << (int)recv[2] << endl;

			SUCCESS_RECV++;
			if (win.front() == (int)recv[2]) {
				//cout << "����ǰ��" << endl << endl;
				SUCCESS_RECV++;
				has_ack++;
				base++;
				win.pop();
			}
			//����ACK<win.front()������������
		}
		else {
			//��Զ���ѷ���δȷ�ϵİ�����win.front()��������Ƿ�ʱ
			if (clock() - timer[win.front()] > TIMEOUT) {
				//������
				next_seq = base % SEQ_SPACE;
				has_send -= win.size();
				while (!win.empty()) win.pop();
				cout << "��ʱ�ش��������ݰ�" << next_seq << "��ʼ�ش�" << endl;
			}
		}
	}

	//char package[2];
	//package[1] = SEND_OVER;
	//package[2] = 11;
	//package[0] = check_sum(package + 1, 1);
	//sendto(m_ClientSocket, package, 3, 0, (sockaddr*)&m_ServerAddress, sizeof(m_ServerAddress));
	//cout << "�ѷ���SEND_OVER" << endl;
	return;
}
void send_file()
{
	// �����ļ�
	int len = 0;
	char* buffer = new char[100000000];
	string filename;

	cout << "�����봰�ڴ�С��Ĭ��10���� ";
	cin >> WIN_SIZE;

	cout << endl  << "�����뷢�͵��ļ���" << endl;
	while (true)
	{
		cin >> filename;
		ifstream fin(filename.c_str(), ifstream::binary);//��������ļ����Զ�����ģʽ��ȡ
		if (!fin) {
			cout << "�ļ�������,����������" << endl;
			continue;
		}
		unsigned char t = fin.get();//���ֽڶ�ȡ����
		while (fin) {
			buffer[len++] = t;
			t = fin.get();
		}
		fin.close();
		break;
	}

	int time_begin = clock();
	send_buffer((char*)(filename.c_str()), filename.length());	//�ļ���
	cout << "�ļ����������" << endl << endl;
	send_buffer(buffer, len);									//�ļ�����
	int time_end = clock();
	cout << filename << "�ļ��������" << endl;

	int time = (time_end - time_begin) / CLOCKS_PER_SEC;
	cout << "����ʱ�䣺" << time << endl;
	if (time != 0)
	{
		double kbps = ((len * 8.0) / 1000) / time;
		cout << "��������" << kbps << " kbps" << endl;
	}
	cout << "�����ʣ�" << (SUCCESS_SEND - SUCCESS_RECV) * 100 / SUCCESS_SEND << "%" << endl;
	cout << endl << endl;
}

void three_way_handshake()
{
	//�������ֽ�������
	while (true)
	{
		char sendBuf[2];
		sendBuf[1] = SYN;
		sendBuf[0] = check_sum(sendBuf + 1, 1);

		int ret = sendto(m_ClientSocket, sendBuf, 2, 0, (sockaddr*)&m_ServerAddress, sizeof(m_ServerAddress));
		if (ret != SOCKET_ERROR)
			cout << "�ѷ��͵�һ����������SYN" << endl;
		else {
			cout << "������������ʧ�ܡ���" << endl;
			return;
		}
		//��ʼ��ʱ
		int begin = clock();

		char recv[2];  //���ջ�����
		int is_timeout = 0;

		u_long mode = 1;  // 1 ��ʾ��������0 ��ʾ����
		ioctlsocket(m_ClientSocket, FIONBIO, &mode);
		while (recvfrom(m_ClientSocket, recv, 2, 0, (sockaddr*)&m_ServerAddress, &len_addr) == SOCKET_ERROR)
		{
			if (clock() - begin > TIMEOUT)
			{
				is_timeout = 1;
				break;
			}
		}

		//û�г�ʱ�����յ������ݰ�������ȷ���Ƕ������� 
		if (is_timeout == 0 && check_sum(recv, 2) == 0 && recv[1] == SYN_ACK)
		{
			cout << "���յ�����˶�������SYN_ACK" << endl;
			sendBuf[1] = ACK;
			sendBuf[0] = check_sum(sendBuf + 1, 1);
			sendto(m_ClientSocket, sendBuf, 2, 0, (sockaddr*)&m_ServerAddress, sizeof(m_ServerAddress));
			cout << "�ѷ��͵���������ACK" << endl;
			break;
		}
	}
	cout << "���ӵ������" << endl << endl;
	return;
}


void double_wave()
{
	cout << "���ж��λ���ing" << endl;
	int timeout_num = 0;
	while (true)
	{
		char package[2];
		package[1] = FIN;
		package[0] = check_sum(package + 1, 1);
		sendto(m_ClientSocket, package, 2, 0, (sockaddr*)&m_ServerAddress, sizeof(m_ServerAddress));
		cout << "�ѷ��͵�һ�λ���" << endl;

		int begin = clock();
		char recv[2];
		int fail = 0;
		//��ʱ�ش�
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
			cout << "���յڶ��λ���" << endl;
			break;
		}
		if (timeout_num >= 3) {
			cout << "���յڶ��λ���" << endl;
			break;
		}
	}
	cout << "���ֳɹ����Ͽ�����" << endl;
}



int main() {
	WSADATA  wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup error:" << GetLastError() << endl;
		return false;
	}

	//����SOCKET
	m_ClientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_ClientSocket == INVALID_SOCKET)
	{
		closesocket(m_ClientSocket);
		m_ClientSocket = INVALID_SOCKET;
		return false;
	}

	// Զ�˵�ַ(·����:4003,Server:4002��
	const char* ip = "127.0.0.1";
	int	port = 4003;
	//int	port = 4002;
	m_ServerAddress.sin_family = AF_INET;				//ipv4
	m_ServerAddress.sin_port = htons(port);				//ת��Ϊ�����ֽ��򣨴����
	inet_pton(AF_INET, ip, &m_ServerAddress.sin_addr);  // ���ַ�����ʽ��Զ��IP��ַת��Ϊ�����Ƹ�ʽ������ֵ�� m_ServerAddress.sin_addr

	// �󶨱��ص�ַ
	sockaddr_in localAddress;
	localAddress.sin_family = AF_INET;
	localAddress.sin_port = htons(4001);
	inet_pton(AF_INET, "127.0.0.1", &localAddress.sin_addr);

	// ���׽��ֵ�ָ���ı��ص�ַ
	if (bind(m_ClientSocket, (sockaddr*)&localAddress, sizeof(localAddress)) == SOCKET_ERROR) {
		// �����ʧ�ܵ����
		cout << "Bind failed" << endl;
		return 0;
	}

	//��������
	three_way_handshake();

	//��������
	send_file();
	cout << "�����ܴ�����" << SUCCESS_SEND << endl;

	//���λ���
	double_wave();

	//�ر��׽���
	closesocket(m_ClientSocket);
	//�����Դ
	WSACleanup();

	system("pause");
	return 0;
}
