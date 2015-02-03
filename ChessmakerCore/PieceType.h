#pragma once
#include <list>
#include <map>

class MoveDefinition;

#define TYPE_NAME_LENGTH 32
#define TYPE_NOTATION_LENGTH 3

class PieceType
{
public:
	PieceType();
	~PieceType();
private:
	int value = 1;
	char name[TYPE_NAME_LENGTH];
	char notation[TYPE_NOTATION_LENGTH];
	std::list<MoveDefinition*> moves;
	std::map<int, char*> appearances;
	//std::list<PromotionOpportunity*> promotionOpportunities;
	PieceType *capturedAs;

	friend class GameParser;
};

