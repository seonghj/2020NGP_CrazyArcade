#define _WINSOCK_DEPRECATED_NO_WARNINGS // 최신 VC++ 컴파일 시 경고 방지
#include "stdafx.h"
#include "CClientPacket.h"

int client_ID[4] = { 1, 2, 3, 4 };
BOOL ClientOn[4] = { FALSE, }; // 스레드 생성 확인
BOOL InRobby[4] = { FALSE, }; // 로비 접속 확인
BOOL Ready[4] = { TRUE, TRUE, TRUE, TRUE }; // 게임 시작 준비 확인
BOOL ItemReady = { FALSE };  // 아이템 초기화 확인
BOOL Game_Over[4] = { TRUE, TRUE, TRUE, TRUE }; // 각 스레드 게임 오버 처리 확인

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
    SOCKADDR_IN clientaddr;
    char packet_buf[BUFSIZE];
    int prev_size;
};

SOCKETINFO clients[MAX_CLIENT];

HANDLE hcp;

int Player_Count = -1;

//HANDLE hReadEvent, hWriteEvent;


InputPacket Send_P;
InputPacket Recv_P;
InputPacket Player_P[4]; // 플레이어 초기 정보
InputPacket Item_P;

CMap m_Map;

CRITICAL_SECTION cs;
PacketFunc m_PF;

int GameState = Robby; // 게임 흐름
int Death_count = 0; // 죽은 유저 수
int Accept_count = 0; // 클라이언트가 서버에 접속한 횟수
int ItemValue;

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

void Disconnected(int id)
{
    printf("client_end: %d", id);
    closesocket(clients[id].sock);
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
    /*printf("Recv %d번 <- type: %d, x: %d, y: %d\n", id,
        Packet->type, Packet->x, Packet->y);*/
    if (retval == SOCKET_ERROR)
    {
        int err_no = WSAGetLastError();
        if (err_no != WSA_IO_PENDING)
        {
            err_display("WSARecv()");
        }
    }
    memcpy(clients[id].packet_buf, reinterpret_cast<char*>(Packet), sizeof(InputPacket));
}

void do_send(int to, char* packet)
{
    SOCKET client_s = clients[to].sock;

    int retval = 0;

    OVER_EX* over = reinterpret_cast<OVER_EX*>(malloc(sizeof(OVER_EX)));
    ZeroMemory(&(over->overlapped), sizeof(WSAOVERLAPPED));
    over->dataBuffer.len = sizeof(InputPacket);
    over->dataBuffer.buf = packet;
    over->is_recv = false;

    InputPacket* Packet = reinterpret_cast<InputPacket*>(over->dataBuffer.buf);
    //printf("Send -> type: %d, x: %d, y: %d\n", Packet->type, Packet->x, Packet->y);
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
            Ready[id] = TRUE;
            GameState = Gameready;
        }
        else
        {
            if (Packet->status == Status::DEAD)
            {
                Death_count++;
                if (Player_Count - Death_count <= 0)
                    Packet->type = end;
            }
        }

        // 게임 시작 판단
        if (GameState != InGame)
        {
            if (Ready[0] && Ready[1] && Ready[2] && Ready[3])
            {
                m_Map.Init_Map();
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
                                if (clients[a].connected == true) {
                                    do_send(a, (char*)&Item_P);
                                }
                        }
                    }
                }
                ItemReady = TRUE;
                if (ItemReady)
                {
                    m_PF.InitPacket(&Send_P);
                    Send_P.type = start;
                    for (int i = 0; i < MAX_CLIENT; ++i)
                        if (clients[i].connected)
                            do_send(i, (char*)&Send_P);
                    GameState = InGame;
                }
            }
        }
    }

    if (GameState == InGame)
    {
        switch (Packet->type) {
        case PacketType::end: {
            Game_Over[id] = TRUE;
            if (Game_Over[0] && Game_Over[1] && Game_Over[2] && Game_Over[3])
            {
                GameState = GameOver;
            }
            break;
        }
        case PacketType::player: {
            m_PF.User[id].x = Packet->x;
            m_PF.User[id].y = Packet->y;
            break;
        }
        }
        for (int i = 0; i < MAX_CLIENT; ++i)
            if (clients[i].connected)
                do_send(i, buf);
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


        SOCKADDR_IN clientaddr;
        int addrlen = sizeof(clientaddr);
        getpeername(clients[id].sock, (SOCKADDR*)&clientaddr, &addrlen);

        if (FALSE == retval)
            err_display("WSAGetOverlappedResult()");
        if (0 == cbTransferred) {
            closesocket(clients[id].sock);
            clients[id].connected = false;
        }

        if (lpover_ex->is_recv) {
            do_recv(id);
            process_packet(id, clients[id].packet_buf);
        }
        else {
            delete lpover_ex;
        }
    }
}

int do_accept()
{

    // 윈속 초기화
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

    SOCKET client_sock;
    SOCKADDR_IN clientaddr;
    int addrlen = sizeof(SOCKADDR_IN);
    DWORD recvbytes, flags;

    int startsign = 0;

    int recv_ClientID = 0;

    int count = 0;


    for (int i = 0; i < MAX_CLIENT; i++)
        m_PF.InitPacket(&Player_P[i]);

    while (1) {
        client_sock = accept(listen_sock, (struct sockaddr*)&clientaddr, &addrlen);
        if (client_sock == INVALID_SOCKET) {
            err_display("accept()");
            break;
        }
        getpeername(client_sock, (SOCKADDR*)&clientaddr, &addrlen);

        Player_Count++;

        int new_id = get_new_id();

        memset(&clients[new_id], 0x00, sizeof(struct SOCKETINFO));
        clients[new_id].sock = client_sock;
        clients[new_id].over.dataBuffer.len = BUFSIZE;
        clients[new_id].over.dataBuffer.buf =
            clients[client_sock].over.messageBuffer;
        clients[new_id].over.is_recv = true;
        flags = 0;

        CreateIoCompletionPort((HANDLE)client_sock, hcp, new_id, 0);
        clients[new_id].connected = true;

        Ready[new_id] = FALSE;
        ItemReady = FALSE;
        Game_Over[new_id] = FALSE;


        if (GameState == Robby)
        {
            if (!InRobby[new_id])
            {
                Accept_count++;
                m_PF.InitPlayer(m_Map, &Player_P[new_id], new_id);

                do_send(new_id, (char*)&Player_P[new_id]);

                for (int i = 0; i < MAX_CLIENT; ++i)
                    if (clients[i].connected)
                    {
                        for (int j = 0; j < new_id + 1; ++j)
                        {
                            do_send(i, (char*)&Player_P[j]);
                            printf("[TCP 서버] %d로 %d의 초기 데이터 전송: %d, %d\n"
                                , i, j, Player_P[j].x, Player_P[j].y);
                        }
                    }

                if (retval == SOCKET_ERROR) {
                    err_display("send()");
                    break;
                }
                InRobby[new_id] = TRUE;

            }
            do_recv(new_id);
        }
    }

    // closesocket()
    closesocket(listen_sock);

    // 윈속 종료
    WSACleanup();
    return 0;
}


int main(int argc, char* argv[])
{
    std::vector <std::thread> working_threads;

    for (int i = 0; i < MAX_CLIENT; ++i)
        clients[i].connected = false;

    hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
    if (hcp == NULL) return 1;

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    for (int i = 0; i < (int)si.dwNumberOfProcessors * 2; i++) {
        working_threads.emplace_back(std::thread{ &WorkerFunc });
    }
    accept_thread = std::thread(&do_accept);

    accept_thread.join();
    for (auto& t : working_threads)
        t.join();

    CloseHandle(hcp);

    return 0;
}