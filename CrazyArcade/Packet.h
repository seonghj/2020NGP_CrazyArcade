#pragma once


// �̵� = 1, ��ǳ�� = 2
struct Packet
{
	Packet(int type) { this->type = type; }
	int type;
};

enum PacketType
{
	index,
	start,
	player,
	item,
	bubble,
	ready,
	end,
};

struct InputPacket : public Packet
{
	InputPacket(int idx, int x, int y, u_short stat) :Packet(player) { idx_player = idx; this->x = x; this->y = y; status = stat; }
	InputPacket(int idx, int x, int y) :Packet(bubble) { this->x = x; this->y = y; this->idx_player = idx; }

	int idx_player;
	int x, y;
	u_short status = 0;
};

struct PlayerPacket : public Packet
{
	PlayerPacket(int idx, int x, int y, u_short stat) :Packet(player) { idx_player = idx; this->x = x; this->y = y; status = stat; }
	int idx_player;
	int x, y;
	u_short status;
};

struct BubblePacket : public Packet
{
	int power;
	int x, y;
};


struct ItemPacket : public Packet
{
	int x, y;
	int value;
};

enum Status {
	MOVE_LEFT = 3, MOVE_RIGHT = 2, MOVE_UP = 0, MOVE_DOWN = 1, STOP = 4, IN_BUBBLE = 5, Dead = 6,
};

enum ClientPacket {
	input_left = 1,
	input_right = 2,
	input_top = 4,
	input_bottom = 8,
	input_space = 16,
	input_ctrl = 32,
	state_dead = 64,
	state_trapped = 128,
};