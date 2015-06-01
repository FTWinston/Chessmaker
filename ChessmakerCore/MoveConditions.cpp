#include "MoveConditions.h"
#include "Board.h"
#include "Game.h"
#include "GameState.h"
#include "Move.h"
#include "MoveStep.h"

bool Condition::ResolveComparison(NumericComparison_t type, int val1, int val2)
{
	switch (type) {
	case NumericComparison_t::Equals:
		return val1 == val2;
	case NumericComparison_t::LessThan:
		return val1 < val2;
	case NumericComparison_t::GreaterThan:
		return val1 > val2;
	case NumericComparison_t::LessThanOrEquals:
		return val1 <= val2;
	case NumericComparison_t::GreaterThanOrEquals:
		return val1 >= val2;
	default:
		return false;
	}
}


MoveConditionGroup::MoveConditionGroup(GroupType_t type)
{
	this->type = type;
}


MoveConditionGroup::~MoveConditionGroup()
{
	std::list<MoveCondition*>::iterator it = elements.begin();
	while (it != elements.end())
	{
		delete (*it);
		it++;
	}
}


bool MoveConditionGroup::IsSatisfied(Move *move, MoveStep *lastPerformed)
{
	bool any = false;

	switch (type)
	{
	case GroupType_t::And:
		for (auto it = elements.begin(); it != elements.end(); it++)
			if (!(*it)->IsSatisfied(move, lastPerformed))
                return false;
        return true;
    case GroupType_t::Or:
		for (auto it = elements.begin(); it != elements.end(); it++)
			if ((*it)->IsSatisfied(move, lastPerformed))
                return true;
        return false;
    case GroupType_t::Nand:
		for (auto it = elements.begin(); it != elements.end(); it++)
			if (!(*it)->IsSatisfied(move, lastPerformed))
                return true;
        return false;
    case GroupType_t::Nor:
		for (auto it = elements.begin(); it != elements.end(); it++)
			if ((*it)->IsSatisfied(move, lastPerformed))
                return false;
        return true;
    case GroupType_t::Xor:
		for (auto it = elements.begin(); it != elements.end(); it++)
			if ((*it)->IsSatisfied(move, lastPerformed))
			{
				if (any)
					return false;
				else
					any = true;
			}
        return any;
	default: // there are no other types!
		return false;
    }
}


bool MoveCondition_Type::IsSatisfied(Move *move, MoveStep *lastPerformed)
{
	Piece *other = move->GetPieceByReference(pieceRef);
	if (other == 0)
	{
		ReportError("Referenced piece not found for \"type\" move condition: %s\n", pieceRef);
		return false;
	}
	return other->TypeMatches(type);
}


bool MoveCondition_Owner::IsSatisfied(Move *move, MoveStep *lastPerformed)
{
	Piece *other = move->GetPieceByReference(pieceRef);
	if (other == 0)
	{
		ReportError("Referenced piece not found for \"owner\" move condition: %s\n", pieceRef);
		return false;
	}

	if (relationship == Player::Any)
		return true;

	return relationship == move->GetPlayer()->GetRelationship(other->GetOwner());
}


bool MoveCondition_MoveNumber::IsSatisfied(Move *move, MoveStep *lastPerformed)
{
	Piece *other = move->GetPieceByReference(pieceRef);
	if (other == 0)
	{
		ReportError("Referenced piece not found for \"move number\" move condition: %s\n", pieceRef);
		return false;
	}

    return ResolveComparison(comparison, other->GetMoveNumber(), number);
}


bool MoveCondition_MaxDist::IsSatisfied(Move *move, MoveStep *lastPerformed)
{
	Piece *other = move->GetPieceByReference(pieceRef);
	if (other == 0)
	{
		ReportError("Referenced piece not found for \"max dist\" move condition: %s\n", pieceRef);
		return false;
	}

	auto steps = move->GetSteps();
	MoveStep *previousStep = steps.empty() ? 0 : *steps.rbegin();
	direction_t dirs = move->GetPlayer()->ResolveDirections(dir, previousStep != 0 && previousStep->GetDirection() != 0 ? previousStep->GetDirection() : move->GetPlayer()->GetForwardDirection());

	FOR_EACH_DIR_IN_SET(dirs, dir)
	{
        int maxDist = other->GetPosition()->GetMaxDist(dir);
        if (ResolveComparison(comparison, maxDist, number))
            return true;
    }
    return false;
}


bool MoveCondition_TurnsSinceLastMove::IsSatisfied(Move *move, MoveStep *lastPerformed)
{
	Piece *other = move->GetPieceByReference(pieceRef);
	if (other == 0)
	{
		ReportError("Referenced piece not found for \"turns since last move\" move condition: %s\n", pieceRef);
		return false;
	}

    return ResolveComparison(comparison, move->GetPrevState()->GetTurnNumber() - other->GetLastMoveTurn(), number);
}


bool MoveCondition_Threatened::checkingThreat = false; // when performing moves to check this, don't go performing other moves for other "threatened" checks, or things get messy
bool MoveCondition_Threatened::IsSatisfied(Move *move, MoveStep *lastPerformed)
{
	if (checkingThreat)
		return true;

	checkingThreat = true;
	bool retVal = CheckSatisfied(move, lastPerformed);
	checkingThreat = false;

	return retVal;
}


bool MoveCondition_Threatened::CheckSatisfied(Move *move, MoveStep *lastPerformed)
{
	if (start && move->GetPiece()->GetState() == Piece::OnBoard)
	{
		bool threatened = IsThreatened(move->GetPrevState(), move->GetPiece()->GetPosition());
		if (threatened != value)
			return false;
	}

	if (!end)
		return true;

	bool passedLast = lastPerformed == 0;
	auto steps = move->GetSteps();
	for (auto it = steps.begin(); it != steps.end(); it++)
	{
		MoveStep *step = *it;
		if (passedLast)
		{
			if (!step->Perform(false))
				return false;
		}
		else if (step == lastPerformed)
			passedLast = true;
	}

	bool returnVal = true;
	if (move->GetPiece()->GetState() == Piece::OnBoard)
	{
		bool threatened = IsThreatened(move->GetPrevState(), move->GetPiece()->GetPosition());
		returnVal = threatened == value;
	}

	for (auto it = steps.rbegin(); it != steps.rend() && *it != lastPerformed; it++)
		(*it)->Reverse(false);

	return returnVal;
}


bool MoveCondition_Threatened::IsThreatened(GameState *state, Cell *position)
{
	auto moves = state->DetermineThreatMoves();
	bool retVal = false;
	
	for (auto it = moves->begin(); it != moves->end(); it++)
	{
		Move *move = *it;
		if (move->WouldCapture(position))
		{
			retVal = true;

			for (; it != moves->end(); it++)
				delete *it;
			break;
		}

		delete *it;
	}
	
	delete moves;
	return retVal;
}


bool MoveCondition_Count::IsSatisfied(Move *move, MoveStep *lastPerformed)
{
	return ResolveComparison(comparison, GetCount(move, lastPerformed), number);
}


int MoveCondition_Count::GetCount(Move *move, MoveStep *lastPerformed)
{
	int count = 0;

	Piece *from = move->GetPieceByReference(pieceRef);
	if (from == 0)
	{
		ReportError("Referenced piece not found for \"count\" move condition: %s\n", pieceRef);
		return count;
	}

	// check the piece we're counting from, also, i guess
	if (from->TypeMatches(type) && (relationship == Player::Any || move->GetPlayer()->GetRelationship(from->GetOwner()) == relationship))
		count++;

	direction_t dirs = move->GetPlayer()->ResolveDirections(dir, lastPerformed != 0 && lastPerformed->GetDirection() != 0 ? lastPerformed->GetDirection() : move->GetPlayer()->GetForwardDirection());
	FOR_EACH_DIR_IN_SET(dirs, dir)
	{
		Cell *pos = from->GetPosition();
		int boardMaxDist = pos->GetMaxDist(dir);
		int distLimit = distance->GetValue(lastPerformed, boardMaxDist);

		for (int dist = 1; dist <= distLimit; dist++)
		{
			pos = pos->FollowLink(dir);
			if (pos == 0)
				break;

			Piece *other = pos->GetPiece();
			if (other != 0 && other->TypeMatches(type) && (relationship == Player::Any || move->GetPlayer()->GetRelationship(other->GetOwner()) == relationship))
				count++;
		}
	}

	return count;
}


bool MoveCondition_CountMultiple::IsSatisfied(Move *move, MoveStep *lastPerformed)
{
	auto it = items.begin();
	MoveCondition_Count *count = *it;
	int total = count->GetCount(move, lastPerformed);

	for (it++; it != items.end(); it++)
	{
		count = *it;
		int num = count->GetCount(move, lastPerformed);

		switch (operation)
		{
		case Add:
			total += num; break;
		case Subtract:
			total -= num; break;
		}
	}

	return ResolveComparison(comparison, total, number);
}


bool MoveCondition_State::IsSatisfied(Move *move, MoveStep *lastPerformed)
{
	Piece *other = move->GetPieceByReference(pieceRef);
	if (other == 0)
	{
		ReportError("Referenced piece not found for \"state\" move condition: %s\n", pieceRef);
		return false;
	}

	return other->HasState(state);
}


bool MoveCondition_ReferencePiece::IsSatisfied(Move *move, MoveStep *lastPerformed)
{
	Piece *other = ReferencePiece::FindReferencedPiece(move, lastPerformed, relationship, otherPieceType, distance, dir);

	if (other != 0)
	{
		move->AddPieceReference(other, otherPieceRef);
		return true;
	}

	return false;
}