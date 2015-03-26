#pragma once

#include "Definitions.h"

class Game;
class Piece;

#define PLAYER_NAME_LENGTH 16

class Player
{
public:
	typedef enum { Local, AI, Remote } Type_t;
	typedef enum { Any, Self, Enemy, Ally } Relationship_t;

	Player(Game *game, char *name, direction_t forwardDir);
	~Player();

	char *GetName() { return name; }
	Game *GetGame() { return game; }
	direction_t ResolveDirections(direction_t dir, direction_t prevDir);
	Relationship_t GetRelationship(Player *other);
	direction_t GetForwardDirection() { return forwardDir; }
	
	std::set<Piece*> GetPiecesOnBoard() { return piecesOnBoard; }

private:
	Game *game;
	char name[PLAYER_NAME_LENGTH];
	int id;
	direction_t forwardDir;
	Type_t type;

	std::set<Piece*> piecesOnBoard, piecesHeld, piecesCaptured;

	static int nextID;

	friend class EndOfGame;
	friend class Game;
	friend class MoveStep;
	friend class GameParser;
	friend class GameState;
};

