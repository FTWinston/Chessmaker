#include "PieceType.h"
#include "MoveDefinition.h"

PieceType::PieceType()
{
	capturedAs = this;
}


PieceType::~PieceType()
{
	while (!moves.empty())
		delete moves.front(), moves.pop_front();
}
