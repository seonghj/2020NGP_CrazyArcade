#define _WINSOCK_DEPRECATED_NO_WARNINGS // �ֽ� VC++ ������ �� ��� ����
#include "stdafx.h"
#include "ServerFunc.h"
#include "CClientPacket.h"

int client_ID[4] = { 1, 2, 3, 4 };
BOOL ClientOn[4] = { FALSE, }; // ������ ���� Ȯ��
BOOL InRobby[4] = { FALSE, }; // �κ� ���� Ȯ��
BOOL Ready[4] = { TRUE, TRUE, TRUE, TRUE }; // ���� ���� �غ� Ȯ��
BOOL ItemReady = { FALSE };  // ������ �ʱ�ȭ Ȯ��
BOOL Game_Over[4] = { TRUE, TRUE, TRUE, TRUE }; // �� ������ ���� ���� ó�� Ȯ��

std::thread accept_thread;

struct OVER_EX
{
    WSAOVERLAPPED	overlapped;
    WSABUF			dataBuffer;
    char			messageBuffer[BUFSIZE];
    bool			is_recv;
};

struct SOCKETINFO
{
    bool	connected;
    OVER_EX over;
    SOCKET sock;
    char packet_buf[BUFSIZE];
    int prev_size;
};

SOCKETINFO clients[MAX_CLIENT];

HANDLE hcp;

int Thread_Count = -1; // send+recv������ �� ����

//HANDLE hReadEvent, hWriteEvent;


InputPacket Send_P;
InputPacket Recv_P;
InputPacket Player_P[4]; // �÷��̾� �ʱ� ����
InputPacket Item_P;

CMap m_Map;

CRITICAL_SECTION cs;
PacketFunc m_PF;

int GameState = Robby; // ���� �帧
int Death_count = 0; // ���� ���� ��
int Accept_count = 0; // Ŭ���̾�Ʈ�� ������ ������ Ƚ��
int ItemValue;

// ���� �Լ� ���� ��� �� ����
void err_quit(char* msg)
{
    LPVOID lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
    LocalFree(lpMsgBuf);
    exit(1);
}

// ���� �Լ� ���� ���
void err_display(char* msg)
{
    LPVOID lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    printf("[%s] %s", msg, (char*)lpMsgBuf);
    LocalFree(lpMsgBuf);
}

void InitGame()
{
    for (int i = 0; i < MAX_CLIENT; i++)
    {
        if (clients[i].connected)
        {
            Ready[i] = FALSE;
            Game_Over[i] = FALSE;
        }
    }
    ItemReady = FALSE;
    Death_count = 0;
    GameState = Robby;

    for (int i = 0; i < MAX_CLIENT; ++i)
        clients[i].connected = false;
}

void do_recv(char id)
{
    DWORD flags = 0;

    SOCKET client_s = clients[id].sock;
    OVER_EX* over = &clients[id].over;

    int retval = 0;

    over->dataBuffer.len = BUFSIZE;
    over->dataBuffer.buf = over->messageBuffer;
    ZeroMemory(&(over->overlapped), sizeof(WSAOVERLAPPED));

    InputPacket* Packet = reinterpret_cast<InputPacket*>(over->dataBuffer.buf);

    retval = WSARecv(client_s, &over->dataBuffer, 1, (LPDWORD)clients[id].prev_size,
        &flags, &(over->overlapped), NULL);
    printf("Recv %d�� <- type: %d, x: %d, y: %d\n", id,
        Packet->type, Packet->x, Packet->y);
    if (retval == SOCKET_ERROR)
    {
        int err_no = WSAGetLastError();
        if (err_no != WSA_IO_PENDING)
        {
            err_display("WSARecv()");
        }
    }
    memcpy(clients[id].packet_buf, reinterpret_cast<char*>(Packet), sizeof(InputPacket));
    //clients[id].prev_size = retval;
}

void do_send(int to, char* packet)
{
    SOCKET client_s = clients[to].sock;

    int retval = 0;

    OVER_EX* over = reinterpret_cast<OVER_EX*>(malloc(sizeof(OVER_EX)));

    over->dataBuffer.len = sizeof(InputPacket);
    over->dataBuffer.buf = packet;

    ZeroMemory(&(over->overlapped), sizeof(WSAOVERLAPPED));
    over->is_recv = false;

    InputPacket* Packet = reinterpret_cast<InputPacket*>(over->dataBuffer.buf);
    printf("Send -> type: %d, x: %d, y: %d\n", Packet->type, Packet->x, Packet->y);
    retval = WSASend(client_s, &over->dataBuffer, 1, NULL,
        0, &(over->overlapped), NULL);

    if (retval == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            std::cout << "Error - Fail WSASend(error_code : ";
            std::cout << WSAGetLastError() << ")\n";
        }
    }
}

void process_packet(char id, char* buf)
{
    InputPacket* Packet = reinterpret_cast<InputPacket*>(buf);

    if (GameState == Robby || GameState == InGame || GameState == Gameready)
    {
        if (Packet->type == ready) {
            printf("Recv %d��: S<-C: %d = ���� ��ȣ\n", id, Packet->type);
            Ready[id] = TRUE;
            GameState = Gameready;
        }
        else
        {
            if (Packet->status == Status::DEAD)
            {
                Death_count++;
                if (Thread_Count - Death_count <= 0)
                    Packet->type = end;
            }
        }

        // ���� ���� �Ǵ�
        if (GameState != InGame)
        {
            if (Ready[0] && Ready[1] && Ready[2] && Ready[3])
            {
                srand((unsigned)time(NULL));
                for (int i = 0; i < m_Map.Tile_CountY; i++)
                {
                    for (int j = 0; j < m_Map.Tile_CountX; j++) {
                        ItemValue = rand() % 30;
                        if (ItemValue != 0 && ItemValue != 7 && m_Map.isBox[0][i][j]) {
                            Item_P.x = i;
                            Item_P.y = j;
                            Item_P.idx_player = ItemValue;
                            Item_P.type = PacketType::item;
                            for (int a = 0; a < MAX_CLIENT; ++a)
                                if (clients[a].connected == true)
                                    do_send(a, (char*)&Item_P);
                        }
                    }
                }
                printf("Send ������ ��ġ ����\n");
                ItemReady = TRUE;
                if (ItemReady)
                {
                    m_PF.InitPacket(&Send_P);
                    Send_P.type = start;
                    for (int i = 0; i < MAX_CLIENT; ++i)
                        if (clients[i].connected)
                            do_send(i, (char*)&Send_P);
                    printf("Send %d�� ���� ���� ��ȣ ����\n", id);
                    GameState = InGame;
                }
            }
        }
    }

    if (GameState == InGame)
    {
        for (int i = 0; i < MAX_CLIENT; ++i)
            if (clients[i].connected)
                do_send(i, buf);
        if (Packet->type == end)
        {
            Game_Over[id] = TRUE;
            if (Game_Over[0] && Game_Over[1] && Game_Over[2] && Game_Over[3])
            {
                GameState = GameOver;
            }
        }
    }
    if (GameState == GameOver)
    {
        InitGame();
    }
}

int get_new_id()
{
    while (true)
        for (int i = 0; i < MAX_CLIENT; ++i)
            if (clients[i].connected == false) {
                clients[i].connected = true;
                return i;
            }
}

int count = 0;

void WorkerFunc()
{
    int retval = 0;

    while (1) {
        DWORD cbTransferred;
        SOCKET client_sock;
        ULONG id;
        SOCKETINFO* ptr;

        OVER_EX* lpover_ex;

        retval = GetQueuedCompletionStatus(hcp, &cbTransferred,
            (PULONG_PTR)&id, (LPOVERLAPPED*)&lpover_ex, INFINITE);

        std::thread::id Thread_id = std::this_thread::get_id();

        printf("thread id: %d\n", Thread_id);

        // Ŭ���̾�Ʈ ���� ���
        SOCKADDR_IN clientaddr;
        int addrlen = sizeof(clientaddr);
        getpeername(clients[id].sock, (SOCKADDR*)&clientaddr, &addrlen);

        // �񵿱� ����� ��� Ȯ��

        if (FALSE == retval)
            err_display("WSAGetOverlappedResult()");
        if (0 == cbTransferred) {
            closesocket(clients[id].sock);
            clients[id].connected = false;
            printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
                inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
        }

        if (lpover_ex->is_recv) {
            do_recv(id);
            int rest_size = cbTransferred;
            char* buf_ptr = lpover_ex->messageBuffer;
            char packet_size = 0;
            if (0 < clients[id].prev_size)
                packet_size = sizeof(clients[id].packet_buf);
            /*while (rest_size > 0) {
                if (0 == packet_size) packet_size = sizeof(buf_ptr);
                int required = packet_size - clients[id].prev_size;
                printf("required: %d\n", required);
                if (rest_size >= required) {
                    memcpy(clients[id].packet_buf + clients[id].
                        prev_size, buf_ptr, required);
                    process_packet(id, clients[id].packet_buf);
                    rest_size -= required;
                    buf_ptr += required;
                    packet_size = 0;
                }
                else {
                    memcpy(clients[id].packet_buf + clients[id].prev_size,
                        buf_ptr, rest_size);
                    rest_size = 0;
                }
            }*/
            process_packet(id, clients[id].packet_buf);
        }
        else {
            delete lpover_ex;
        }
    }
}

int do_accept()
{

    // ���� �ʱ�ȭ
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    // socket()
    SOCKET listen_sock = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (listen_sock == INVALID_SOCKET) err_quit("socket()");

    // bind()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);
    int retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR) err_quit("bind()");

    // listen()
    retval = listen(listen_sock, MAX_CLIENT);
    if (retval == SOCKET_ERROR) err_quit("listen()");

    // ������ ��ſ� ����� ����
    SOCKET client_sock;
    SOCKADDR_IN clientaddr;
    int addrlen = sizeof(SOCKADDR_IN);
    DWORD recvbytes, flags;

    int startsign = 0;

    int recv_ClientID = 0;

    int count = 0;

    //InitializeCriticalSection(&cs);

    for (int i = 0; i < MAX_CLIENT; i++)
        m_PF.InitPacket(&Player_P[i]);

    while (1) {
        client_sock = accept(listen_sock, (struct sockaddr*)&clientaddr, &addrlen);
        if (client_sock == INVALID_SOCKET) {
            err_display("accept()");
            break;
        }
        getpeername(client_sock, (SOCKADDR*)&clientaddr, &addrlen);

        Thread_Count++;

        int new_id = get_new_id();

        printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d, key=%d\n",
            inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), new_id);

        //// DB ������ �Է�
        //sprintf(query, "INSERT into player (idplayer) VALUES"
        //    "('%d')", new_id);
        //int state = mysql_query(connection, query);
        //if (state != 0)
        //{
        //    printf("MySQL query error : %s\n", mysql_error(&conn));
        //    //return 1;
        //}

        //m_Map.Init_Map();
        //m_PF.InitPacket(&Recv_P);
        //m_PF.InitPacket(&Send_P);
        ////ClientOn[Thread_Count] = TRUE;

        memset(&clients[new_id], 0x00, sizeof(struct SOCKETINFO));
        clients[new_id].sock = client_sock;
        clients[new_id].over.dataBuffer.len = BUFSIZE;
        clients[new_id].over.dataBuffer.buf =
            clients[client_sock].over.messageBuffer;
        clients[new_id].over.is_recv = true;
        flags = 0;

        // ���ϰ� ����� �Ϸ� ��Ʈ ����
        CreateIoCompletionPort((HANDLE)client_sock, hcp, new_id, 0);
        clients[new_id].connected = true;

        Ready[new_id] = FALSE;
        ItemReady = FALSE;
        Game_Over[new_id] = FALSE;


        if (GameState == Robby)
        {
            // ���� �� �ʱ� ������ ����
            if (!InRobby[new_id])
            {
                Accept_count++;
                //EnterCriticalSection(&cs);
                m_PF.InitPlayer(m_Map, &Player_P[new_id], new_id);

                do_send(new_id, (char*)&Player_P[new_id]);

                for (int i = 0; i < MAX_CLIENT; ++i)
                    if (clients[i].connected)
                    {
                        for (int j = 0; j < new_id + 1; ++j)
                        {
                            do_send(i, (char*)&Player_P[j]);
                            printf("[TCP ����] %d�� %d�� �ʱ� ������ ����: %d, %d\n"
                                , i, j, Player_P[j].x, Player_P[j].y);
                        }
                    }

                if (retval == SOCKET_ERROR) {
                    err_display("send()");
                    break;
                }
                InRobby[new_id] = TRUE;

                //LeaveCriticalSection(&cs);
            }
            do_recv(new_id);
        }
    }
    //DeleteCriticalSection(&cs);

    // closesocket()
    closesocket(listen_sock);

    // ���� ����
    WSACleanup();
    return 0;
}


int main(int argc, char* argv[])
{
    //printf("MySQL Ver. %s\n", mysql_get_client_info());

    //if (mysql_init(&conn) == NULL)
    //    printf("mysql_init() error\n");

    //connection = mysql_real_connect(&conn, DB_HOST, DB_USER
    //    , DB_PW, DB_NAME, 3306, (const char*)NULL, 0);

    //mysql_query(connection, "set session character_set_connection=euckr;");
    //mysql_query(connection, "set session character_set_results=euckr;");
    //mysql_query(connection, "set session character_set_client=euckr;");

    //if (connection == NULL)
    //{
    //    printf("%d: %s\n", mysql_errno(&conn), mysql_error(&conn));
    //    //return 1;
    //}
    //else
    //{
    //    printf("DB connected\n");
    //}

    std::vector <std::thread> working_threads;

    for (int i = 0; i < MAX_CLIENT; ++i)
        clients[i].connected = false;

    // ����� �Ϸ� ��Ʈ ����
    hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
    if (hcp == NULL) return 1;

    // CPU ���� Ȯ��
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    // (CPU ���� * 2)���� �۾��� ������ ����
    for (int i = 0; i < (int)si.dwNumberOfProcessors * 2; i++) {
        working_threads.emplace_back(std::thread{ &WorkerFunc });
    }
    accept_thread = std::thread(&do_accept);

    // ������ ���� ���
    accept_thread.join();
    for (auto& t : working_threads)
        t.join();

    CloseHandle(hcp);
    //mysql_close(connection);

    return 0;
}