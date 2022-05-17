#include "Socket_Programming.h"
#include "Packet.h"
#include "SoundMgr.h"

// �̺�Ʈ
extern HANDLE hRecvEvent, hSendEvent, hConnectEvent, hPlayerEvent, hBubbleEvent, hInputEvent;
// ����
extern SOCKET sock;
// ��Ŷ
extern InputPacket* Recv_Player_Packet;
extern InputPacket* Send_Client_Packet;

//�ڵ鰪
extern HWND hwnd;
// �÷��̾�
extern RECT Player1, Player2;
extern RECT Player[MAX_PLAYER];
// ���� ������Ʈ enum GAME_BG { MENU = 1, ROBBY = 2, INGAME = 3};
extern int GameState;
extern const int Player_CX = 34;
extern const int Player_CY = 34;
extern int Client_Idx;
extern int nPlayer;
extern int xPos_Player[4];
extern int yPos_Player[4];
extern BOOL Player_Bubble[4][7]; 
extern BOOL Player_Bubble_Flow[4][7];
extern RECT Tile_Bubble[4][7];
extern int Power[4];
extern int Itemset[2][13][15];
extern BOOL bInBubble[4];
extern BOOL bDie[4];
extern int Player_bCount[4]; 
extern int Player_Speed[4];
extern BOOL Ending;
extern int BubbleResource[4];
extern int BubbleCount[4];
extern BOOL Player_Bubble_Boom[4][7];
extern int MoveResource[4][7];
extern int Player_Dying[4];
extern BOOL Player_Remove[4];
extern int Bubble_cnt[4][7];
extern BOOL Box_Break[13][15];



extern enum Player_Position { LEFT = 3, RIGHT = 2, UP = 0, DOWN = 1 };
extern enum GAME_BG { MENU = 1, ROBBY, INGAME };
extern enum Item { Ball = 1, OnePower = 6, Speed = 11, MaxPower = 16, RedDevil = 21 };
extern enum Timer { P1 = 1, P2 = 2, P3 = 3, P4 = 4, Bubble_BfBoom, Bubble_Flow, In_Bubble, Die, Monster_Move, Recv_Bubble };

extern int Sel_Map;

extern BOOL TextOn; 
extern bool bSceneChange;
extern BOOL SelectMap1, SelectMap2;					//�ʼ��� 

extern BOOL Player_Live[MAX_PLAYER];
extern BOOL Player_Move[MAX_PLAYER];

extern void CALLBACK TimeProc_Text(HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime);
extern void CALLBACK TimeProc_P1_Move(HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime);
extern void CALLBACK TimeProc_InBubble(HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime);
extern void CALLBACK TimeProc_Die(HWND hWnd, UINT uMsg, UINT ideEvent, DWORD dwTime);
extern void SetPos();

int recvn(SOCKET s, char* buf, int len, int flags)
{
    int received;
    char* ptr = buf;
    int left = len;

    while (left > 0) {
        received = recv(s, ptr, left, flags);
        if (received == SOCKET_ERROR)
            return SOCKET_ERROR;
        else if (received == 0)
            break;
        left -= received;
        ptr += received;
    }

    return (len - left);
}

// Receive�� ������ ������. 
DWORD WINAPI RecvClient(LPVOID arg)
{
    // �޴� -> �κ�� ���� ������ ��ٸ���.
    WaitForSingleObject(hRecvEvent, INFINITE);
    ResetEvent(hRecvEvent);
    int retval;

    char buf[BUFSIZE];

    // ���� �ʱ�ȭ
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    // socket()
    sock = socket(AF_INET, SOCK_STREAM, 0);

    // ���� �ɼǿ��� recv �Լ��� 0.5�� �̻� ����ϰ� ������ return �ϵ��� ����
    DWORD recvTimeout = 500; //0.5��
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(recvTimeout));

    // connect()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = connect(sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
        MessageBox(hwnd, L"���� �� ��", L"�����", MB_OK);
    else
        MessageBox(hwnd, L"�����", L"�����?", MB_OK);


    // Ŀ��Ʈ ���� �ڽ��� �÷��̾� ��Ŷ ����
    retval = recvn(sock, buf, sizeof(InputPacket), 0);
    buf[retval] = '\0';
    Recv_Player_Packet = (InputPacket*)buf;
    SetEvent(hConnectEvent);    //connect�� ������ �۽ſ����� ���� ��밡���ϴٴ� �̺�Ʈ �߻���Ų��.
    printf("Packet ID : %d\nPacket x : %d\nPacket y : %d\nPacket type : %d\n", Recv_Player_Packet->idx_player, Recv_Player_Packet->x, Recv_Player_Packet->y, Recv_Player_Packet->type);
    Client_Idx = Recv_Player_Packet->idx_player;
    nPlayer = Client_Idx + 1;
    for (int i = 0; i < nPlayer; ++i)
        Player_Live[i] = true;

    // ������ ��ſ� ���� while, �� ���� ó�� ������ �������� ���� ��Ŷ�� �޾ƿ��� �۾� �ʿ�
    while (1) {
        if (GameState == ROBBY) // ���� ������Ʈ�� �޴��� ��
        {
            retval = recvn(sock, buf, sizeof(InputPacket), 0);
            if (retval == SOCKET_ERROR)
                continue;
            buf[retval] = '\0';
            Recv_Player_Packet = (InputPacket*)buf;
            printf("Packet ID : %d\nPacket x : %d\nPacket y : %d\nPacket type : %d\n", Recv_Player_Packet->idx_player, Recv_Player_Packet->x, Recv_Player_Packet->y, Recv_Player_Packet->type);
            if (Recv_Player_Packet->type == PacketType::start)
            {
                printf("����\n");
                // ���� ��ŸƮ (GameState 3���� �ٲٰ� ����� �Ұ� ü���� �ʿ�
               /* CSoundMgr::GetInstance()->PlayEffectSound(L"SFX_Button_Off.ogg");
                CSoundMgr::GetInstance()->PlayEffectSound2(L"SFX_Word_Start.ogg");*/
                TextOn = TRUE;
                SetTimer(hwnd, 8, 750, (TIMERPROC)TimeProc_Text);
                GameState = INGAME;
                bSceneChange = true;
                if (SelectMap1)
                    Sel_Map = 0;
                else
                    Sel_Map = 1;
            }
            else if (Recv_Player_Packet->type == PacketType::player)
            {
                if (Recv_Player_Packet->idx_player + 1 > nPlayer) {
                    Player_Live[nPlayer++] = TRUE;
                }
            }
            else if (Recv_Player_Packet->type == PacketType::item)
            {
                Itemset[0][Recv_Player_Packet->x][Recv_Player_Packet->y] = Recv_Player_Packet->idx_player;
            }
        }
        else if (GameState == INGAME)
        {
            retval = recvn(sock, buf, sizeof(InputPacket), 0);
            if (retval == SOCKET_ERROR)
                continue;
            buf[retval] = '\0';
            Recv_Player_Packet = (InputPacket*)buf;
            printf("%d Ÿ�� ��Ŷ ����\n", Recv_Player_Packet->type);
            if (Recv_Player_Packet->type == PacketType:: player)
            {
                int idx = Recv_Player_Packet->idx_player;
                //printf("�÷��̾� ��Ŷ ���� -> type : %d, idx : %d, x : %d, y : %d, status : %d\n\n", Recv_Player_Packet->type, Recv_Player_Packet->idx_player, Recv_Player_Packet->x, Recv_Player_Packet->y, Recv_Player_Packet->status);
                if (Recv_Player_Packet->status ==Status::IN_BUBBLE) {
                    Player_Speed[Recv_Player_Packet->idx_player] = 100;
                    SetTimer(hwnd, Timer::In_Bubble, Player_Speed[Recv_Player_Packet->idx_player], (TIMERPROC)TimeProc_InBubble);
                    bInBubble[Recv_Player_Packet->idx_player] = TRUE;
                }
                else if (Recv_Player_Packet->status == Status::Dead)
                {
                    bInBubble[Recv_Player_Packet->idx_player] = FALSE;
                    bDie[Recv_Player_Packet->idx_player] = TRUE;
                    BubbleResource[Recv_Player_Packet->idx_player] = 3;
                    BubbleCount[Recv_Player_Packet->idx_player] = 0;
                    CSoundMgr::GetInstance()->PlayEffectSound(L"SFX_Character_Die.ogg");
                    SetTimer(hwnd, Die, 150, (TIMERPROC)TimeProc_Die);
                }
                else if (Recv_Player_Packet->status == Status::MOVE_DOWN)
                {
                    printf("�̵� ��Ŷ ���� -> type : %d x : %d y : %d\n\n", Recv_Player_Packet->type, Recv_Player_Packet->x, Recv_Player_Packet->y);
                    Player[Recv_Player_Packet->idx_player].left = Recv_Player_Packet->x;
                    Player[Recv_Player_Packet->idx_player].right = Recv_Player_Packet->x + Player_CX;
                    Player[Recv_Player_Packet->idx_player].top = Recv_Player_Packet->y;
                    Player[Recv_Player_Packet->idx_player].bottom = Recv_Player_Packet->y + Player_CY;

                    yPos_Player[idx] = DOWN;
                    SetTimer(hwnd, idx, Player_Speed[idx], (TIMERPROC)TimeProc_P1_Move);
                    Player_Move[idx] = TRUE;
                }
                else if (Recv_Player_Packet->status == Status::MOVE_RIGHT)
                {
                    printf("�̵� ��Ŷ ���� -> type : %d x : %d y : %d\n\n", Recv_Player_Packet->type, Recv_Player_Packet->x, Recv_Player_Packet->y);
                    Player[Recv_Player_Packet->idx_player].left = Recv_Player_Packet->x;
                    Player[Recv_Player_Packet->idx_player].right = Recv_Player_Packet->x + Player_CX;
                    Player[Recv_Player_Packet->idx_player].top = Recv_Player_Packet->y;
                    Player[Recv_Player_Packet->idx_player].bottom = Recv_Player_Packet->y + Player_CY;

                    yPos_Player[idx] = RIGHT;
                    SetTimer(hwnd, idx, Player_Speed[idx], (TIMERPROC)TimeProc_P1_Move);
                    Player_Move[idx] = TRUE;
                }
                else if (Recv_Player_Packet->status == Status::MOVE_UP)
                {
                    printf("�̵� ��Ŷ ���� -> type : %d x : %d y : %d\n\n", Recv_Player_Packet->type, Recv_Player_Packet->x, Recv_Player_Packet->y);
                    Player[Recv_Player_Packet->idx_player].left = Recv_Player_Packet->x;
                    Player[Recv_Player_Packet->idx_player].right = Recv_Player_Packet->x + Player_CX;
                    Player[Recv_Player_Packet->idx_player].top = Recv_Player_Packet->y;
                    Player[Recv_Player_Packet->idx_player].bottom = Recv_Player_Packet->y + Player_CY;

                    yPos_Player[idx] = UP;
                    SetTimer(hwnd, idx, Player_Speed[idx], (TIMERPROC)TimeProc_P1_Move);
                    Player_Move[idx] = TRUE;
                }
                else if (Recv_Player_Packet->status == Status::MOVE_LEFT)
                {
                    printf("�̵� ��Ŷ ���� -> type : %d x : %d y : %d\n\n", Recv_Player_Packet->type, Recv_Player_Packet->x, Recv_Player_Packet->y);
                    Player[Recv_Player_Packet->idx_player].left = Recv_Player_Packet->x;
                    Player[Recv_Player_Packet->idx_player].right = Recv_Player_Packet->x + Player_CX;
                    Player[Recv_Player_Packet->idx_player].top = Recv_Player_Packet->y;
                    Player[Recv_Player_Packet->idx_player].bottom = Recv_Player_Packet->y + Player_CY;

                    yPos_Player[idx] = LEFT;
                    SetTimer(hwnd, idx, Player_Speed[idx], (TIMERPROC)TimeProc_P1_Move);
                    Player_Move[idx] = TRUE;
                }
                else {
       
                    if (Recv_Player_Packet->status == Status::STOP)
                    {
                        KillTimer(hwnd, idx);
                        Player[Recv_Player_Packet->idx_player].left = Recv_Player_Packet->x;
                        Player[Recv_Player_Packet->idx_player].right = Recv_Player_Packet->x + Player_CX;
                        Player[Recv_Player_Packet->idx_player].top = Recv_Player_Packet->y;
                        Player[Recv_Player_Packet->idx_player].bottom = Recv_Player_Packet->y + Player_CY;
                        printf("�÷��̾� ���� ��Ŷ ���� -> type : %d, idx : %d\n\n"
                            , Recv_Player_Packet->type, Recv_Player_Packet->idx_player);
                        xPos_Player[Recv_Player_Packet->idx_player] = 0;
                        Player_Move[Recv_Player_Packet->idx_player] = FALSE;
                    }
                    else if (yPos_Player[Recv_Player_Packet->idx_player] != Recv_Player_Packet->status)
                    {
                        yPos_Player[Recv_Player_Packet->idx_player] = Recv_Player_Packet->status;
                        xPos_Player[Recv_Player_Packet->idx_player] = 0;
                        Player_Move[Recv_Player_Packet->idx_player] = TRUE;
                    }                    
                }
            }
            else if (Recv_Player_Packet->type == PacketType::bubble)
            {
                printf("���� ��Ŷ ���� -> type : %d x : %d y : %d\n\n", Recv_Player_Packet->type, Recv_Player_Packet->x, Recv_Player_Packet->y);
                for (int i = 0; i < Player_bCount[Recv_Player_Packet->idx_player]; i++)
                {
                    if (Player_Bubble[Recv_Player_Packet->idx_player][i] == TRUE || Player_Bubble_Flow[Recv_Player_Packet->idx_player][i] == TRUE)
                    {
                        continue;
                    }
                    else
                    {
                        if (Tile_Bubble[Recv_Player_Packet->idx_player][i - 1].left != Recv_Player_Packet->x || Tile_Bubble[Recv_Player_Packet->idx_player][i - 1].top != Recv_Player_Packet->y)
                        {
                            Tile_Bubble[Recv_Player_Packet->idx_player][i].left = Recv_Player_Packet->x;
                            Tile_Bubble[Recv_Player_Packet->idx_player][i].right = Tile_Bubble[Recv_Player_Packet->idx_player][i].left + 40;
                            Tile_Bubble[Recv_Player_Packet->idx_player][i].top = Recv_Player_Packet->y;
                            Tile_Bubble[Recv_Player_Packet->idx_player][i].bottom = Tile_Bubble[Recv_Player_Packet->idx_player][i].top + 40;
                            Player_Bubble[Recv_Player_Packet->idx_player][i] = TRUE;
                            CSoundMgr::GetInstance()->PlayEffectSound(L"SFX_Bubble_On.ogg");
                        }
                        SetEvent(hBubbleEvent);
                        break;
                    }
                }
                // ������ ������ ó�� �κ� ���� �ʿ�
                // ������ ���� �����ϰ� ��Ŷ ������ �������� ����Ǵµ� �̰� ���� �ڿ� ���� ��Ŷ �ް� �����ϴ°ɷ� ���� �ʿ�
            }
            else if (Recv_Player_Packet->type == PacketType::item)
            {
                if (Itemset[Sel_Map][Recv_Player_Packet->x][Recv_Player_Packet->y] == Ball) {
                    CSoundMgr::GetInstance()->PlayEffectSound(L"SFX_Item_Off.ogg");
                    if (Player_bCount[Recv_Player_Packet->idx_player] < 7)
                        Player_bCount[Recv_Player_Packet->idx_player]++;
                }
                if (Itemset[Sel_Map][Recv_Player_Packet->x][Recv_Player_Packet->y] == OnePower) {
                    CSoundMgr::GetInstance()->PlayEffectSound(L"SFX_Item_Off.ogg");
                    if (Power[Recv_Player_Packet->idx_player] < 7)
                        Power[Recv_Player_Packet->idx_player]++;
                }
                if (Itemset[Sel_Map][Recv_Player_Packet->x][Recv_Player_Packet->y] == Speed) {
                    CSoundMgr::GetInstance()->PlayEffectSound(L"SFX_Item_Off.ogg");
                    if (Player_Speed[Recv_Player_Packet->idx_player] >= 20)
                        Player_Speed[Recv_Player_Packet->idx_player] -= 5;
                    if (Recv_Player_Packet->idx_player == Client_Idx) {
                        KillTimer(hwnd, P1);
                        SetTimer(hwnd, P1, Player_Speed[Client_Idx], (TIMERPROC)TimeProc_P1_Move);
                    }
                }
                if (Itemset[Sel_Map][Recv_Player_Packet->x][Recv_Player_Packet->y] == MaxPower) {
                    CSoundMgr::GetInstance()->PlayEffectSound(L"SFX_Item_Off.ogg");
                    Power[Recv_Player_Packet->idx_player] = 7;
                }
                if (Itemset[Sel_Map][Recv_Player_Packet->x][Recv_Player_Packet->y] == RedDevil) {
                    CSoundMgr::GetInstance()->PlayEffectSound(L"SFX_Item_Off.ogg");
                    Player_Speed[Recv_Player_Packet->idx_player] = 15;
                    if (Recv_Player_Packet->idx_player == Client_Idx) {
                        KillTimer(hwnd, P1);
                        SetTimer(hwnd, P1, Player_Speed[Client_Idx], (TIMERPROC)TimeProc_P1_Move);
                    }
                    Player_bCount[Recv_Player_Packet->idx_player] = 7;
                    Power[Recv_Player_Packet->idx_player] = 7;
                }
                Itemset[Sel_Map][Recv_Player_Packet->x][Recv_Player_Packet->y] = 0;
            }
            else if (Recv_Player_Packet->type == PacketType::end)
            {
               /* if(bDie[Client_Idx])
                    CSoundMgr::GetInstance()->PlayEffectSound2(L"SFX_Word_Lose.ogg");
                else
                    CSoundMgr::GetInstance()->PlayEffectSound2(L"SFX_Word_Win.ogg");*/
                Ending = true;
                GameState = ROBBY;
                TextOn = FALSE;
                bSceneChange = true;
                for (int i = 0; i < nPlayer; ++i)
                    Player_Live[i] = true;
                for (int i = 0; i < MAX_PLAYER; ++i) {
                    Player_Speed[i] = 40;
                    Player_bCount[i] = 1;
                    Power[i] = 1;
                    bDie[i] = FALSE;
                    bInBubble[i] = FALSE;
                    BubbleCount[i] = 0;
                    BubbleResource[i] = 0;
                    xPos_Player[i] = 0;
                    yPos_Player[i] = 1;
                    Player_Dying[i] = 0;
                    Player_Remove[i] = FALSE;
                    Player_Move[i] = FALSE;
                    for (int j = 0; j < 7; j++)
                    {
                        MoveResource[i][j] = 0;
                        Player_Bubble[i][j] = FALSE;
                        Player_Bubble_Flow[i][j] = FALSE;
                        Player_Bubble_Boom[i][j] = FALSE;
                        Bubble_cnt[i][j] = 0;
                        Tile_Bubble[i][j] = { 0,0,0,0 };
                    }
                }
                for (int i = 0; i < 13; i++)
                {
                    for (int j = 0; j < 15; j++)
                    {
                        Box_Break[i][j] = FALSE;
                        Itemset[0][i][j] = 0;
                    }
                }
                KillTimer(hwnd, In_Bubble);
                KillTimer(hwnd, Die);
                KillTimer(hwnd, Bubble_Flow);
                KillTimer(hwnd, P1);
                KillTimer(hwnd, P2);
                KillTimer(hwnd, P3);
                KillTimer(hwnd, P4);
                SetPos();
            }
        }
    }
    // closesocket()
    closesocket(sock);

    // ���� ����
    WSACleanup();
    return 0;
}


// ���带 ������ ������
DWORD WINAPI SendClient(LPVOID arg)
{
    WaitForSingleObject(hConnectEvent, INFINITE);   // RecvClient���� Connect �� ������ wait

    if (GameState == 2)
    {
        while (1)
        {
            // ó���� �κ� ȭ�鿡�� Ŭ���� ���� ����, game state �� ingame�̸� break�ϴ� ����
            WaitForSingleObject(hInputEvent, INFINITE);
            printf("�÷��̾� ��Ŷ �۽� -> type : %d idx : %d, x : %d, y : %d, status : %d\n", Send_Client_Packet->type, Send_Client_Packet->idx_player, Send_Client_Packet->x, Send_Client_Packet->y, Send_Client_Packet->status);
            send(sock, (char*)Send_Client_Packet, sizeof(InputPacket), 0);
            delete Send_Client_Packet;
            Send_Client_Packet = NULL;
            SetEvent(hSendEvent);
            if (GameState == 3)
            {
                break;
            }
        }
    }
    // ingame �����ϰ� �� ���Ŀ��� �Է¿� ���� ������ �۽�
    if (GameState == 3)
    {
        while (1)
        {
            WaitForSingleObject(hInputEvent, INFINITE);
            printf("�÷��̾� ��Ŷ �۽� -> type : %d idx : %d, x : %d, y : %d, status : %d\n", Send_Client_Packet->type, Send_Client_Packet->idx_player, Send_Client_Packet->x, Send_Client_Packet->y, Send_Client_Packet->status);
            send(sock, (char*)Send_Client_Packet, sizeof(InputPacket), 0);
            delete Send_Client_Packet;
            Send_Client_Packet = NULL;
            SetEvent(hSendEvent);
        }
    }
    return 0;
}