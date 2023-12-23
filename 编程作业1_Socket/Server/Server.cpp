#define _CRT_SECURE_NO_WARNINGS //禁止使用不安全的函数报错
#define _WINSOCK_DEPRECATED_NO_WARNINGS //禁止使用旧版本的函数报错
#include <iostream>
#include <thread>
#include <WinSock2.h>            
#pragma comment(lib,"ws2_32.lib") 
using namespace std;

#define PORT 6000           //端口号
#define BufSize 1024        //缓冲区大小
#define MaxClient 5         //最大连接数

SOCKET clientSockets[MaxClient];    //存储客户端socket
SOCKADDR_IN clientAddrs[MaxClient]; //存储客户端地址

int total_connect = 0;              //当前连接的客户总数
int is_online[MaxClient] = {0};      //客户端连接与否

//获取当前时间，格式化后保存为字符串
void get_current_time(char* cur_time) 
{
    auto currentTime = chrono::system_clock::now();                      //获取当前时间
    time_t timestamp = chrono::system_clock::to_time_t(currentTime);     //time_t 以秒为单位表示自时间戳以来的时间
    tm localTime;                                                        //localTime存储时间的年、月、日、时、分和秒
    localtime_s(&localTime, &timestamp);
    char temp[50];
    memset(temp, 0, sizeof(temp));
    strftime(temp, sizeof(temp), "(%Y/%m/%d %H:%M:%S)", &localTime);     /* 年 / 月 / 日  时：分：秒 */
    strncpy(cur_time, temp, strlen(temp));
    return;
}

//转发线程函数
DWORD WINAPI recvThread(LPVOID lpParameter)     
{
    int get_rec = 0;
    char RecvBuf[BufSize]; //接收缓冲区
    char SendBuf[BufSize]; //发送缓冲区

    //接收信息
    while (true)
    {
        int num = (int)lpParameter;     //当前连接的索引，在调用线程函数时传入
        Sleep(100); 

        get_rec = recv(clientSockets[num], RecvBuf, sizeof(RecvBuf), 0); //接收信息
        if (get_rec > 0)                //接收成功
        {
            char ctime[50];             //获取时间
            memset(ctime, 0, sizeof(ctime));
            get_current_time(ctime);

            //打印消息在服务器窗口
            cout << "Client " << num << ": " << RecvBuf << "   " << ctime << endl;
            // 格式化要转发的信息
            sprintf_s(SendBuf, sizeof(SendBuf), "Client %d: %s %s", num, RecvBuf, ctime);

            for (int i = 0; (i < MaxClient); i++)         //将消息转发到所有聊天窗口（但不包括自己）
            {
                if ((is_online[i] == 1)&&i!=num)          //在线        
                {
                    send(clientSockets[i], SendBuf, sizeof(SendBuf), 0);
                }
            }
        }
        else //接收失败
        {
                 char ctime[50];
                 memset(ctime, 0, sizeof(ctime));
                 get_current_time(ctime);
                 cout << "———Client " << num << " 离线啦... " << ctime << endl;
 
                 closesocket(clientSockets[num]);     //释放此套接字
                 total_connect--;                     //在线总数减一
                 is_online[num] = 0;
                 cout << "——在线人数： " << total_connect << endl;
                 return 0;
        }
    }
}
//发送消息线程
DWORD WINAPI sendThread(LPVOID lpParameter)
{
    char msg[BufSize] = {};
    char SendBuf[BufSize] = {};
    while (1)
    {
        cin.getline(msg, sizeof(msg));
        if (strcmp(msg, "exit") == 0) //输入exit退出
        {
            break;
        }

        char ctime[50];           
        memset(ctime, 0, sizeof(ctime));
        get_current_time(ctime);

        sprintf_s(SendBuf, sizeof(SendBuf), "Server:  %s %s", msg, ctime);  //格式化服务器消息
        for (int i = 0; i < MaxClient; i++)
        {
            if (is_online[i])
            {
                send(clientSockets[i], SendBuf, sizeof(SendBuf), 0);//发送消息
            }
        }
    }
    return 0;
}

int main()
{
    //套接字初始化
    WSADATA wsaData;
    //WSADATA被用来存储被WSAStartup函数调用后返回的 Windows Sockets 数据。
    WORD sockVersion = MAKEWORD(2, 2);
    //声明采用2.2版本
    if (WSAStartup(sockVersion, &wsaData) != 0) 
    {
        cout << "——套接字初始化失败!" << endl;
        return 0;
    }

    //创建服务器端套接字
    SOCKET serverSocket;
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//IPv4地址族，流式套接字，TCP协议
    if (serverSocket == INVALID_SOCKET)
    {
        cout << "创建服务器套接字失败" << endl;
        return 0;
    }
    cout << "已创建服务器端套接字" << endl;

    //绑定服务器地址
    SOCKADDR_IN serverAddr;               //服务器地址
    serverAddr.sin_family = AF_INET;      //地址类型为IPv4
    serverAddr.sin_port = htons(PORT);    //端口号
    serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//本机IP地址 ,INADDR_ANY表示服务器将接受来自任何源地址的连接请求
    //htonl将主机字节序转换为网络字节序
    
    if (bind(serverSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) //将服务器套接字与服务器地址和端口绑定
    {
        cout << "绑定失败" << endl;
        return 0;
    }
    else
    {
        cout << "已绑定服务器端套接字 " << endl;
    }

    //设置监听
    if (listen(serverSocket, SOMAXCONN) != 0)
    {
        cout << "监听失败" << endl;
        return 0;
    }
    else
    {
        cout << "正在监听..." << endl;
    }

    //接收客户端请求
    while (1)
    {
        if (total_connect >= MaxClient)
        {
            cout << "—-已达到最大连接数..." << endl;
            continue;
        }

        int num;                //找到可以放入的数组索引
        for (int i = 0; i < MaxClient; i++)
        {
            if (is_online[i] == 0)
            {
                num = i;
                break;
            }
        }
        int address_len = sizeof(SOCKADDR);                      //接收客户端请求
        SOCKET revClientSocket = accept(serverSocket, (sockaddr*)&clientAddrs[num], &address_len);
        if (revClientSocket == SOCKET_ERROR)
        {
            cout << "——客户端连接失败" << endl;
            return 0;
        }
        else
        {
            clientSockets[num] = revClientSocket;   //客户端Socket加入数组
            is_online[num] = 1;                     //在线转态
            total_connect++;                        //当前连接总数加1
        }

        char ctime[50];
        memset(ctime, 0, sizeof(ctime));  
        get_current_time(ctime);
    
        cout << "——Client " << num << " 上线啦！   " << ctime << endl;
        cout << "——目前在线人数：" << total_connect << endl;

        HANDLE recv_Thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)recvThread, (LPVOID)num, 0, NULL);//接收、转发线程
        if (recv_Thread == NULL)//线程创建失败
        {
            cout << "——创建recv线程失败" << endl;
        }
        HANDLE send_Thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)sendThread, NULL, 0, NULL);//server发送消息线程
        if (send_Thread == NULL)
        {
            cout << "——创建send线程失败" << endl;
        }

    }

    closesocket(serverSocket);
    WSACleanup();
    cout << "——已结束服务器" << endl;

    return 0;
}

