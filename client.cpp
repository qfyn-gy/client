#include<stdio.h>
#include<string>
#include<conio.h>
using namespace std;

#include<winsock2.h>
#include<stdlib.h>
#pragma comment(lib,"WS2_32.lib")

#define SERVER_IP "118.126.117.125"
#define QUN_LIAO_PORT 2022

SOCKET serverSocket;  //网络套接字
sockaddr_in sockAddr;  //服务器地址
char nickname[32];     //昵称
char line1[111];       //一行分割线
char line2[111];       //一行空白字符串

HANDLE hMutex;         //互斥锁

std::string UTF8ToGBK(const char* strUTF8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, strUTF8, -1, NULL, 0);
    wchar_t* wszGBK = new wchar_t[len + 1];
    memset(wszGBK, 0, len * 2 + 2);
    MultiByteToWideChar(CP_UTF8, 0, strUTF8, -1, wszGBK, len);
    len = WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, NULL, 0, NULL, NULL);
    char* szGBK = new char[len + 1];
    memset(szGBK, 0, len + 1);
    WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, szGBK, len, NULL, NULL);
    std::string strTemp(szGBK);
    if (wszGBK) delete[] wszGBK;
    if (szGBK)  delete[] szGBK;
    return strTemp;
}

void GBKToUTF8(string& strGBK) {
    int len = MultiByteToWideChar(CP_ACP, 0, strGBK.c_str(), -1., NULL, 0);
    wchar_t* wszUtf8 = new wchar_t[len];
    memset(wszUtf8, 0, len);
    MultiByteToWideChar(CP_ACP, 0, strGBK.c_str(), -1, wszUtf8, len);
    len = WideCharToMultiByte(CP_UTF8, 0, wszUtf8, -1, NULL, 0, NULL, NULL);
    char* szUtf8 = new char[len + 1];
    memset(szUtf8, 0, len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wszUtf8, -1, szUtf8, len, NULL, NULL);
    strGBK = szUtf8;
    delete[] szUtf8;
    delete[] wszUtf8;
}

bool init() {
    // 1.网络服务的初始化
    WSADATA data;
    int ret = WSAStartup(MAKEWORD(1, 1), &data);
    if (ret != 0) {
        return false;
    }

    // 2.网络套接字
    serverSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    // 3.物理地址
    sockAddr.sin_family = PF_INET;
    sockAddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
    sockAddr.sin_port = htons(QUN_LIAO_PORT);

    hMutex = CreateMutex(0, 0, "console");

    return true;

}

void login() {
    system("mode con lines=5 cols=30\n");
    printf("    欢迎进入奇牛QQ聊天室\n\n");
    printf("    昵称: ");
    scanf_s("%s", nickname, sizeof(nickname));

    while (getchar() != '\n'); //清空输入缓冲区

    string name = nickname;
    GBKToUTF8(name);
    send(serverSocket, name.c_str(), strlen(name.c_str()) + 1, 0);

}

void gotoxy(int x, int y) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = { x ,y };
    SetConsoleCursorPosition(hOut, pos);
}

void uiInit() {
    system("mode con lines=36 cols=110");
    system("cls");
    gotoxy(0, 33);

    for (int i = 0;i < 110;i++) {
        line1[i] = '-';
        line2[i] = ' ';
    }
    line1[110] = 0; //'\0'
    line2[110] = 0;
    printf("%s\n", line1);
}

void printMsg(const char* msg) {
    //上锁（申请互斥锁）
    //INFINITE,表示如果没有申请到资源，就一直等待，直到等到为止!
    WaitForSingleObject(hMutex, INFINITE);

    static POINT pos={0,0};
    gotoxy(pos.x, pos.y);
    //printf("%s\n", msg);
    static int color = 31;
    printf("\033[0;%d;40m%s\033[0m\n", color++, msg);
    if (color > 36) {
        color = 31;
    }

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(hOut, &info);
    pos.x= info.dwCursorPosition.X;
    pos.y = info.dwCursorPosition.Y;

    if (pos.y >= 33) {
        printf("%s\n", line2);
        printf("\n\n");
        gotoxy(0, 33);
        printf("%s\n", line1);
        pos.y -= 1;
    }

    gotoxy(1, 34);

    //释放锁
    ReleaseMutex(hMutex);
}

void editPrintf(int col, char ch) {
    WaitForSingleObject(hMutex, INFINITE);

    gotoxy(col, 34);
    printf("%c", ch);

    ReleaseMutex(hMutex);
}

void editPrintf(int col, const char*str) {
    WaitForSingleObject(hMutex, INFINITE);

    gotoxy(col, 34);
    printf("%s", str);

    ReleaseMutex(hMutex);
}

DWORD WINAPI threadFuncRecv(LPVOID pram) {
    char buff[4096];
    while (1) {
        int ret=recv(serverSocket, buff, sizeof(buff), 0);
        if (ret <= 0) {
            printf("服务器关闭或故障!\n");
            break;
        }

        // 打印接收到的信息
        printMsg(UTF8ToGBK(buff).c_str());

    }

    return NULL;
}

bool isHZ(char str[], int index) {
    //一个汉字两个字节 第一个字节 < 0 第二个字节 各种可能
    //一个英文字符，只有一个字节, >0
    //-50 48 -56 -70 83
    int i = 0;
    while (i < index) {
        if (str[i] > 0) {
            i++;
        }
        else {
            i += 2;
        }
    }
    //return i>index
    if (i == index) {
        return false;
    }
    else {
        return true;
    }
}

int main(void) {
    if (init() == false) {
        printf("初始化失败!\n");
        return -1;
    }

    int ret = connect(serverSocket,
        (SOCKADDR*)&sockAddr, sizeof(sockAddr));
    if (ret != 0) {
        printf("连接服务器失败，请检查网络连接!\n");
        return -2;
    }

    //登录聊天室
    login();

    uiInit();   //初始化聊天界面

   HANDLE hThread =CreateThread(0, 0, threadFuncRecv, 0, 0, 0);
   CloseHandle(hThread);

   //编辑信息
   while (1) {
       char buff[1024]; //保存用户输入的字符串
       memset(buff, 0, sizeof(buff));
       editPrintf(0, '>');

       int len = 0;
       while (1) {
           if (_kbhit()) {
               char c = getch();
               if (c == '\r') { //按下回车按键
                   break;
               }
               else if(c==8){ //退格键的返回值是8
                   //i love you
                   if (len == 0) {
                       continue;
                   }
                   if (isHZ(buff, len - 1)) {
                       //printf("\b\b  \b\b");
                       editPrintf(len + 1, "\b\b  \b\b");
                       buff[len - 1] = 0;
                       buff[len - 2] = 0;
                       len -= 2;
                   }
                   else {
                       editPrintf(len + 1, "\b \b");
                       buff[len - 1] = 0;
                       len -= 1;

                   }

                   continue;
               }

               WaitForSingleObject(hMutex, INFINITE);

               do {
                   printf("%c", c);
                   buff[len++] = c;
               } while (_kbhit()&& (c=getch()));

               ReleaseMutex(hMutex);

              /* editPrintf(len + 1, c);
               buff[len++] = c;*/
               
           }
       }

       if (len == 0) {
           continue;
       }

       //清除编辑区的信息
       char buff2[1024];
       sprintf_s(buff2, sizeof(buff2), "%s\n", line2);
       editPrintf(0, buff2);
       
       //把用户自己说的话，输出到聊天室
       sprintf_s(buff2, sizeof(buff2), "【LocalHoset@%s】%s", nickname, buff);
       printMsg(buff2);

       //发送编辑好的字符串
       send(serverSocket, buff, strlen(buff) + 1,0);
   }

   getchar();
   return 0;
}