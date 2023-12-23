#define _CRT_SECURE_NO_WARNINGS //��ֹʹ�ò���ȫ�ĺ�������
#define _WINSOCK_DEPRECATED_NO_WARNINGS //��ֹʹ�þɰ汾�ĺ�������
#include <iostream>
#include <thread>
#include <WinSock2.h>            
#pragma comment(lib,"ws2_32.lib") 
using namespace std;

#define PORT 6000           //�˿ں�
#define BufSize 1024        //��������С
#define MaxClient 5         //���������

SOCKET clientSockets[MaxClient];    //�洢�ͻ���socket
SOCKADDR_IN clientAddrs[MaxClient]; //�洢�ͻ��˵�ַ

int total_connect = 0;              //��ǰ���ӵĿͻ�����
int is_online[MaxClient] = {0};      //�ͻ����������

//��ȡ��ǰʱ�䣬��ʽ���󱣴�Ϊ�ַ���
void get_current_time(char* cur_time) 
{
    auto currentTime = chrono::system_clock::now();                      //��ȡ��ǰʱ��
    time_t timestamp = chrono::system_clock::to_time_t(currentTime);     //time_t ����Ϊ��λ��ʾ��ʱ���������ʱ��
    tm localTime;                                                        //localTime�洢ʱ����ꡢ�¡��ա�ʱ���ֺ���
    localtime_s(&localTime, &timestamp);
    char temp[50];
    memset(temp, 0, sizeof(temp));
    strftime(temp, sizeof(temp), "(%Y/%m/%d %H:%M:%S)", &localTime);     /* �� / �� / ��  ʱ���֣��� */
    strncpy(cur_time, temp, strlen(temp));
    return;
}

//ת���̺߳���
DWORD WINAPI recvThread(LPVOID lpParameter)     
{
    int get_rec = 0;
    char RecvBuf[BufSize]; //���ջ�����
    char SendBuf[BufSize]; //���ͻ�����

    //������Ϣ
    while (true)
    {
        int num = (int)lpParameter;     //��ǰ���ӵ��������ڵ����̺߳���ʱ����
        Sleep(100); 

        get_rec = recv(clientSockets[num], RecvBuf, sizeof(RecvBuf), 0); //������Ϣ
        if (get_rec > 0)                //���ճɹ�
        {
            char ctime[50];             //��ȡʱ��
            memset(ctime, 0, sizeof(ctime));
            get_current_time(ctime);

            //��ӡ��Ϣ�ڷ���������
            cout << "Client " << num << ": " << RecvBuf << "   " << ctime << endl;
            // ��ʽ��Ҫת������Ϣ
            sprintf_s(SendBuf, sizeof(SendBuf), "Client %d: %s %s", num, RecvBuf, ctime);

            for (int i = 0; (i < MaxClient); i++)         //����Ϣת�����������촰�ڣ����������Լ���
            {
                if ((is_online[i] == 1)&&i!=num)          //����        
                {
                    send(clientSockets[i], SendBuf, sizeof(SendBuf), 0);
                }
            }
        }
        else //����ʧ��
        {
                 char ctime[50];
                 memset(ctime, 0, sizeof(ctime));
                 get_current_time(ctime);
                 cout << "������Client " << num << " ������... " << ctime << endl;
 
                 closesocket(clientSockets[num]);     //�ͷŴ��׽���
                 total_connect--;                     //����������һ
                 is_online[num] = 0;
                 cout << "�������������� " << total_connect << endl;
                 return 0;
        }
    }
}
//������Ϣ�߳�
DWORD WINAPI sendThread(LPVOID lpParameter)
{
    char msg[BufSize] = {};
    char SendBuf[BufSize] = {};
    while (1)
    {
        cin.getline(msg, sizeof(msg));
        if (strcmp(msg, "exit") == 0) //����exit�˳�
        {
            break;
        }

        char ctime[50];           
        memset(ctime, 0, sizeof(ctime));
        get_current_time(ctime);

        sprintf_s(SendBuf, sizeof(SendBuf), "Server:  %s %s", msg, ctime);  //��ʽ����������Ϣ
        for (int i = 0; i < MaxClient; i++)
        {
            if (is_online[i])
            {
                send(clientSockets[i], SendBuf, sizeof(SendBuf), 0);//������Ϣ
            }
        }
    }
    return 0;
}

int main()
{
    //�׽��ֳ�ʼ��
    WSADATA wsaData;
    //WSADATA�������洢��WSAStartup�������ú󷵻ص� Windows Sockets ���ݡ�
    WORD sockVersion = MAKEWORD(2, 2);
    //��������2.2�汾
    if (WSAStartup(sockVersion, &wsaData) != 0) 
    {
        cout << "�����׽��ֳ�ʼ��ʧ��!" << endl;
        return 0;
    }

    //�������������׽���
    SOCKET serverSocket;
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//IPv4��ַ�壬��ʽ�׽��֣�TCPЭ��
    if (serverSocket == INVALID_SOCKET)
    {
        cout << "�����������׽���ʧ��" << endl;
        return 0;
    }
    cout << "�Ѵ������������׽���" << endl;

    //�󶨷�������ַ
    SOCKADDR_IN serverAddr;               //��������ַ
    serverAddr.sin_family = AF_INET;      //��ַ����ΪIPv4
    serverAddr.sin_port = htons(PORT);    //�˿ں�
    serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//����IP��ַ ,INADDR_ANY��ʾ�����������������κ�Դ��ַ����������
    //htonl�������ֽ���ת��Ϊ�����ֽ���
    
    if (bind(serverSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) //���������׽������������ַ�Ͷ˿ڰ�
    {
        cout << "��ʧ��" << endl;
        return 0;
    }
    else
    {
        cout << "�Ѱ󶨷��������׽��� " << endl;
    }

    //���ü���
    if (listen(serverSocket, SOMAXCONN) != 0)
    {
        cout << "����ʧ��" << endl;
        return 0;
    }
    else
    {
        cout << "���ڼ���..." << endl;
    }

    //���տͻ�������
    while (1)
    {
        if (total_connect >= MaxClient)
        {
            cout << "��-�Ѵﵽ���������..." << endl;
            continue;
        }

        int num;                //�ҵ����Է������������
        for (int i = 0; i < MaxClient; i++)
        {
            if (is_online[i] == 0)
            {
                num = i;
                break;
            }
        }
        int address_len = sizeof(SOCKADDR);                      //���տͻ�������
        SOCKET revClientSocket = accept(serverSocket, (sockaddr*)&clientAddrs[num], &address_len);
        if (revClientSocket == SOCKET_ERROR)
        {
            cout << "�����ͻ�������ʧ��" << endl;
            return 0;
        }
        else
        {
            clientSockets[num] = revClientSocket;   //�ͻ���Socket��������
            is_online[num] = 1;                     //����ת̬
            total_connect++;                        //��ǰ����������1
        }

        char ctime[50];
        memset(ctime, 0, sizeof(ctime));  
        get_current_time(ctime);
    
        cout << "����Client " << num << " ��������   " << ctime << endl;
        cout << "����Ŀǰ����������" << total_connect << endl;

        HANDLE recv_Thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)recvThread, (LPVOID)num, 0, NULL);//���ա�ת���߳�
        if (recv_Thread == NULL)//�̴߳���ʧ��
        {
            cout << "��������recv�߳�ʧ��" << endl;
        }
        HANDLE send_Thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)sendThread, NULL, 0, NULL);//server������Ϣ�߳�
        if (send_Thread == NULL)
        {
            cout << "��������send�߳�ʧ��" << endl;
        }

    }

    closesocket(serverSocket);
    WSACleanup();
    cout << "�����ѽ���������" << endl;

    return 0;
}

