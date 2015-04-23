#include "Move.h"
#include "MoveStep.h"
#include "Game.h"
#include "GameState.h"
#include "TurnOrder.h"

Move::Move(Player *player, GameState *prevState, Piece *piece, Cell *startPos)
{
	this->player = player;
	this->prevState = prevState;
	this->piece = piece;
	this->startPos = startPos;
}


Move::~Move()
{
	while (!steps.empty())
		delete steps.front(), steps.pop_front();
}


Move *Move::Clone()
{
	Move *move = new Move(player, prevState, piece, startPos);
	for (auto it = steps.begin(); it != steps.end(); it++)
	{
		MoveStep *step = *it;
		move->AddStep(new MoveStep(*step));
	}

	move->references = references;

	return move;
}


GameState *Move::Perform(bool updateDisplay)
{
	for (auto it = steps.begin(); it != steps.end(); it++)
	{
		MoveStep *step = *it;

		if (!step->Perform(updateDisplay)) // move failed, roll back
		{
			if (it == steps.begin())
				return 0;

			bool reversing = true;
			do
			{
				it--;
				reversing = it != steps.begin();

				step = *it;
				if (!step->Reverse(updateDisplay))
				{
					ReportError("Encountered an error rolling back a failed move\n");
					return 0;
				}
			} while (reversing);
			
			return 0;
		}
	}

	prevPieceMoveTurn = piece->lastMoveTurn;
	piece->lastMoveTurn = prevState->turnNumber;
	piece->moveNumber++;

	return new GameState(prevState->game, prevState->game->GetTurnOrder()->GetNextPlayer(), prevState->turnNumber + 1);
}


bool Move::Reverse(bool updateDisplay)
{
	for (auto it = steps.rbegin(); it != steps.rend(); it++)
	{
		MoveStep *step = *it;

		if (!step->Reverse(updateDisplay)) // move failed, roll back
		{
			if (it == steps.rbegin())
				return false;

			bool reversing = true;
			do
			{
				it--;
				reversing = it != steps.rbegin();

				step = *it;
				if (!step->Perform(updateDisplay))
				{
					ReportError("Encountered an error rolling back a failed move reversal\n");
					return false;
				}
			} while (reversing);

			return false;
		}
	}

	piece->lastMoveTurn = prevPieceMoveTurn;
    piece->moveNumber--;
	prevState->game->GetTurnOrder()->StepBackward();
	return true;
}


char *Move::DetermineNotation(int detailLevel)
{
    switch (detailLevel)
	{
        case 1:
			strcpy(notation, piece->pieceType->notation);
			if (IsCapture())
				strcat(notation, "x");
			strcat(notation, GetEndPos()->GetName());
            break;
        case 2:
			strcpy(notation, piece->pieceType->notation);
			strcat(notation, startPos->GetName());
			if (IsCapture())
				strcat(notation, "x");
			strcat(notation, GetEndPos()->GetName());
            break;
        default:
			sprintf(notation, "%s(%i)", piece->pieceType->notation, piece->GetID());

			strcat(notation, startPos->GetName());
			if (IsCapture())
				strcat(notation, "x");
			strcat(notation, GetEndPos()->GetName());
            break;
    }
	
    // add promotion letter onto the end
    /*var promotion = GetPromotionType();
    if (promotion != 0)
        strcat(notation, promotion->notation;*/

    // add a plus to the end if its check. add a hash if its checkmate.

    // how to account for castling? some special moves simply need to specify their own special notation, i guess...

	return notation;
}


void Move::AddPieceReference(Piece *piece, const char *ref)
{
	auto it = references.find(ref);
	if (it != references.end())
		it->second = piece;
	else
		references.insert(std::pair<const char*, Piece*>(ref, piece));
}


Piece *Move::GetPieceByReference(const char *ref)
{
	if (strcmp(ref, "self") == 0)
		return piece; // this is needed only by conditions code, it looks like. If they could be made to handle "self" separately, this wouldn't be needed.

	auto it = references.find(ref);
	if (it == references.end())
		return 0;
	return it->second;
}


Cell *Move::GetEndPos()
{
	for (auto it = steps.rbegin(); it != steps.rend(); it++)
	{
		MoveStep *step = *it;
		if (step->GetPiece() == piece && step->toState == Piece::OnBoard)
			return step->toPos;
	}

	// if this piece doesn't move, consider other pieces that it might move or capture
	for (auto it = steps.rbegin(); it != steps.rend(); it++)
	{
		MoveStep *step = *it;
		if (step->toState == Piece::OnBoard)
			return step->toPos;
		else if (step->fromState == Piece::OnBoard)
			return step->fromPos;
	}

    return startPos;
}


std::list<Cell*> Move::GetAllPositions()
{
	std::list<Cell*> allPositions;

	for (auto it = steps.begin(); it != steps.end(); it++)
	{
		MoveStep *step = *it;
		if (step->GetPiece() == piece && step->toState == Piece::OnBoard)
			allPositions.push_back(step->toPos);
	}

	return allPositions;
}


bool Move::IsCapture()
{
	for (auto it = steps.begin(); it != steps.end(); it++)
	{
		MoveStep *step = *it;
		if (step->GetPiece() != piece && step->toState != Piece::OnBoard)
			return true;
	}
    return false;
}


int Move::GetCaptureValue()
{
	int sum = 0;
	for (auto it = steps.begin(); it != steps.end(); it++)
	{
		MoveStep *step = *it;
		int val = step->GetPiece()->GetType()->GetValue();

		if (step->toState != Piece::OnBoard)
		{
			if (player->GetRelationship(step->GetPiece()->GetOwner()) == Player::Enemy)
				sum += val;
			else
				sum -= val;
		}
		else
		{
			if (player->GetRelationship(step->GetPiece()->GetOwner()) == Player::Enemy)
				sum -= val;
			else
				sum += val;
		}
	}
	return sum;
}


bool Move::WouldCapture(Cell* target)
{
	for (auto it = steps.begin(); it != steps.end(); it++)
	{
		MoveStep *step = *it;
		if (step->GetPiece() != piece && step->toState != Piece::OnBoard && step->fromState == Piece::OnBoard && step->fromPos == target)
			return true;
	}
	return false;
}


PieceType *Move::GetPromotionType()
{
	for (auto it = steps.rbegin(); it != steps.rend(); it++)
	{
		MoveStep *step = *it;
		if (step->GetPiece() == piece && step->toType != step->fromType)
			return step->toType;
	}
	return 0;
}