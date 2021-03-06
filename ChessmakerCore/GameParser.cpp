#include "rapidxml\rapidxml.hpp"
#include "rapidxml\rapidxml_print.hpp"

#include <stdio.h>
#include <utility>

#include "GameParser.h"
#include "Board.h"
#include "Cell.h"
#include "Distance.h"
#include "Game.h"
#include "GameState.h"
#include "MoveConditions.h"
#include "MoveDefinition.h"
#include "Piece.h"
#include "PieceType.h"
#include "Player.h"
#include "StateConditions.h"
#include "StateLogic.h"
#include "TurnOrder.h"

using namespace rapidxml;


#ifdef NO_SVG
Game* GameParser::Parse(char *definition)
#else
Game* GameParser::Parse(char *definition, std::string *svgOutput)
#endif
{
	xml_document<> doc;
	doc.parse<0>(definition); // 0 means default parse flags

	game = new Game();
	game->board = new Board(game);
	game->holdCapturedPieces = false; // todo: determine these somehow, rather than hard-coding them
	game->showCapturedPieces = true;  //
	game->showHeldPieces = false;     //

	Player::nextID = 1;

	maxDirection = FIRST_ABSOLUTE_DIRECTION >> 1;
	allDirections = 0;
	maxState = 1;
	directionLookups.clear();

	xml_node<> *node = doc.first_node()->first_node("board");
	if (node == 0)
	{
		ReportError("Can't find \"board\" node in game definition\n");
		delete game;
		return 0;
	}
	
#ifdef NO_SVG
	if (!ParseCells(game->board, node))
#else
	xml_document<char> svgDoc;
	if (!ParseCellsAndGenerateSVG(game->board, node, &svgDoc))
#endif
	{
		delete game;
		return 0;
	}
	
	node = node->next_sibling("dirs");
	if (node == 0)
	{
		ReportError("Can't find \"dirs\" node in game definition\n");
		delete game;
		return 0;
	}
	if (!ParseDirections(game->board, node))
	{
		delete game;
		return 0;
	}
	
	node = node->next_sibling("pieces");
	if (node == 0)
	{
		ReportError("Can't find \"pieces\" node in game definition\n");
		delete game;
		return 0;
	}

#ifdef NO_SVG
	if (!ParsePieceTypes(node))
#else
	xml_node<> *defsNodes = svgDoc.first_node()->first_node("defs");
	if (!ParsePieceTypes(node, defsNodes))
#endif
	{
		delete game;
		return 0;
	}
	
	node = node->next_sibling("setup");
	if (node == 0)
	{
		ReportError("Can't find \"setup\" node in game definition\n");
		delete game;
		return 0;
	}

#ifdef NO_SVG
	if (!ParsePlayers(node))
#else
	if (!ParsePlayers(node, &svgDoc))
#endif
	{
		delete game;
		return 0;
	}
	
	node = node->next_sibling("rules");
	if (node == 0)
	{
		ReportError("Can't find \"rules\" node in game definition\n");
		delete game;
		return 0;
	}
	if (!ParseRules(node))
	{
		delete game;
		return 0;
	}

	// resolve piece type pointers from the various places in move definitions and rules logic
	while (!pieceTypeReferenceQueue.empty())
	{
		auto item = pieceTypeReferenceQueue.begin();
		PieceType **ref = item->first;
		char *typeName = item->second;

		auto it = pieceTypesByName.find(typeName);
		PieceType *type = it == pieceTypesByName.end() ? 0 : it->second;
		if (type == 0)
			ReportError("Got a reference to a piece type that has not been defined: \"%s\"\n", typeName);
		else
			*ref = type;

		delete[] typeName;
		pieceTypeReferenceQueue.erase(item);
	}

#ifndef NO_SVG
	// write svgDoc into svgOutput
	print(std::back_inserter(*svgOutput), svgDoc, print_no_indenting);
#endif

	return game;
}


struct TempCellLink
{
public:
	TempCellLink(Cell *c, direction_t d, char *r)
	{
		fromCell = c;
		direction = d;
		destinationCellRef = r;
	}

	Cell *fromCell;
	direction_t direction;
	char* destinationCellRef;
};


#ifdef NO_SVG
bool GameParser::ParseCells(Board *board, xml_node<> *boardNode)
#else
bool GameParser::ParseCellsAndGenerateSVG(Board *board, xml_node<> *boardNode, xml_document<> *svgDoc)
#endif
{
#ifndef NO_SVG
	// create SVG root node
	xml_node<> *svgRoot = svgDoc->allocate_node(node_element, "svg");
	svgDoc->append_node(svgRoot);
	
	xml_attribute<> *svgAttr = svgDoc->allocate_attribute("xmlns", "http://www.w3.org/2000/svg");
	svgRoot->append_attribute(svgAttr);

	svgAttr = svgDoc->allocate_attribute("id", "render");
	svgRoot->append_attribute(svgAttr);

	svgAttr = boardNode->first_attribute("viewBox");
	char *value = svgDoc->allocate_string(svgAttr->value());

	svgAttr = svgDoc->allocate_attribute("viewBox", value);
	svgRoot->append_attribute(svgAttr);

	xml_node<> *svgNode = svgDoc->allocate_node(node_element, "defs");
	svgRoot->append_node(svgNode);
#endif
	// loop through all the cell nodes
	int iCell = 0;
	xml_node<> *node = boardNode->first_node();
	while (node != 0)
	{
		if (strcmp(node->name(), "cell") == 0)
			iCell++;
		node = node->next_sibling();
	}

	std::vector<Cell*> cells(iCell, NULL);
	iCell = 0;

	cellsByRef.clear();
	std::list<TempCellLink> links;

	node = boardNode->first_node();
	xml_attribute<> *attr;
	while (node != 0)
	{
		if (strcmp(node->name(), "cell") == 0)
		{
			attr = node->first_attribute("id");
			Cell *cell = cells[iCell++] = new Cell();

			char *ref = attr->value();
			strcpy(cell->reference, ref);
			
			// parse cell links, and store them until all cells have been loaded, so we can resolve the names
			xml_node<> *link = node->first_node("link");
			while (link != 0)
			{
				attr = link->first_attribute("dir");
				char *dir = attr->value();

				attr = link->first_attribute("to");
				ref = attr->value();

				links.push_back(TempCellLink(cell, LookupDirection(dir), ref));

				link = link->next_sibling("link");
			}

			cellInfo info;
#ifndef NO_SVG
			// load cell into SVG
			xml_node<> *cellSVG = svgDoc->allocate_node(node_element, "path");
			svgRoot->append_node(cellSVG);

			ref = svgDoc->allocate_string(cell->reference);
			attr = svgDoc->allocate_attribute("id", ref);
			cellSVG->append_attribute(attr);

			// determine css class
			char *val = svgDoc->allocate_string("cell ", 28);
			attr = node->first_attribute("fill");
			strcat(val, attr->value());

			attr = node->first_attribute("border");
			if (attr != 0)
			{
				strcat(val, " ");
				strcat(val, attr->value());
				strcat(val, "Stroke");
			}
			attr = svgDoc->allocate_attribute("class", val);
			cellSVG->append_attribute(attr); // !

			// copy svg path as-is
			attr = node->first_attribute("path");
			val = svgDoc->allocate_string(attr->value());
			attr = svgDoc->allocate_attribute("d", val);
			cellSVG->append_attribute(attr); // !

			// save off each cell's position. Get the first two numbers from the path.
			// "M100 60 m-20 -20" should become "100" and "60"
			char *firstSpace = strchr(val, ' ');
			info.x = atoi(val + 1);
			info.y = atoi(firstSpace + 1);
#endif
			info.cell = cell;
			cellsByRef.insert(std::pair<char*, cellInfo>(ref, info));
		}
#ifndef NO_SVG
		else if (strcmp(node->name(), "line") == 0)
		{
			xml_node<> *lineSVG = svgDoc->allocate_node(node_element, "line");
			svgRoot->append_node(lineSVG);

			attr = node->first_attribute("x1");
			char *val = svgDoc->allocate_string(attr->value());
			attr = svgDoc->allocate_attribute("x1", val);
			lineSVG->append_attribute(attr);

			attr = node->first_attribute("x2");
			val = svgDoc->allocate_string(attr->value());
			attr = svgDoc->allocate_attribute("x2", val);
			lineSVG->append_attribute(attr);

			attr = node->first_attribute("y1");
			val = svgDoc->allocate_string(attr->value());
			attr = svgDoc->allocate_attribute("y1", val);
			lineSVG->append_attribute(attr);

			attr = node->first_attribute("y2");
			val = svgDoc->allocate_string(attr->value());
			attr = svgDoc->allocate_attribute("y2", val);
			lineSVG->append_attribute(attr);

			val = svgDoc->allocate_string("detail ", 20);
			attr = node->first_attribute("color");
			strcat(val, attr->value());
			strcat(val, "Stroke");
			attr = svgDoc->allocate_attribute("class", val);
			lineSVG->append_attribute(attr);
        }
#endif
		node = node->next_sibling();
	}
	board->cells = cells;

	// look through links, resolve the named cell for each, and then call cell.AddLink() for each.
	while (!links.empty())
	{
		TempCellLink link = links.front();
		links.pop_front();
		
		auto it = cellsByRef.find(link.destinationCellRef);
		if (it == cellsByRef.end())
		{
			ReportError("Cell %s has a link to an invalid destination cell: %s\n", link.fromCell->GetName(), link.destinationCellRef);
			continue;
		}

		Cell *dest = it->second.cell;
		link.fromCell->AddLink(link.direction, dest);
	}
	
	board->allAbsoluteDirections = allDirections;
	board->PrecomputeDistances();
	return true;
}


bool GameParser::ParseDirections(Board *board, xml_node<> *dirsNode)
{
	board->firstRelativeDirection = maxDirection << 1;

	xml_node<> *node = dirsNode->first_node();
	while (node != 0)
	{
		if (strcmp(node->name(), "relative") == 0)
		{
			char *name = node->first_attribute("name")->value();
			direction_t id = LookupDirection(name); // this should always be new
			relativeDir_t values;

			xml_node<> *child = node->first_node();
			while (child != 0)
			{
				char *from = child->first_attribute("from")->value();
				char *to = child->first_attribute("to")->value();
				
				direction_t fromID = LookupDirection(from); // this should never be new, and also < firstRelativeDirection
				direction_t toID = LookupDirection(to); // this should never be new, and also < firstRelativeDirection

				values.insert(relativeDirValue_t(fromID, toID));

				child = child->next_sibling();
			}

			board->relativeDirections.insert(std::pair<direction_t, relativeDir_t>(id, values));
		}
		else if (strcmp(node->name(), "group") == 0)
		{
			char *name = node->first_attribute("name")->value();
			int dirMask = 0;

			xml_node<> *child = node->first_node();
			while (child != 0)
			{
				char *dir = child->first_attribute("dir")->value();
				dirMask |= LookupDirection(dir); // this should never be new

				child = child->next_sibling();
			}

			directionLookups.insert(dirLookupEntry_t(name, dirMask));
		}

		node = node->next_sibling();
	}

	board->lastRelativeDirection = maxDirection;

	if (board->firstRelativeDirection == board->lastRelativeDirection)
		board->lastRelativeDirection = board->lastRelativeDirection >> 1; // there are none, so ensure they don't get looped over

	return true;
}

#ifdef NO_SVG
bool GameParser::ParsePieceTypes(xml_node<> *piecesNode)
#else
bool GameParser::ParsePieceTypes(xml_node<> *piecesNode, xml_node<> *svgDefsNode)
#endif
{
	pieceTypesByName.clear();

	// first parse any "named" moves used by multiple pieces
	xml_node<> *node = piecesNode->first_node("namedMoves");
	if (node != 0)
	{
		node = node->first_node("move");
		while (node != 0)
		{
			char *name = node->first_attribute("name")->value();
			auto it = namedMoves.find(name);
			
			if (it == namedMoves.end())
				namedMoves.insert(std::make_pair(name, node));
			else
				ReportError("Got multiple named moves with the same name: %s\n", name);

			node = node->next_sibling("move");
		}
	}

	// parse each piece type
	node = piecesNode->first_node("piece");
	while (node != 0)
	{
		PieceType *type = new PieceType();
#ifdef NO_SVG
		char *capturedAs = ParsePieceType(node, type);
#else
		char *capturedAs = ParsePieceType(node, svgDefsNode, type);
#endif

		pieceTypesByName.insert(std::make_pair(type->name, type));
		game->allPieceTypes.push_back(type);
		node = node->next_sibling("piece");

		if (capturedAs != 0)
		{
			char *typeNameCopy = new char[TYPE_NAME_LENGTH];
			strcpy(typeNameCopy, capturedAs);
			pieceTypeReferenceQueue.insert(std::make_pair(&type->capturedAs, typeNameCopy));
		}
	}

	while (!promotionTypeQueue.empty())
	{
		auto item = promotionTypeQueue.begin();
		Promotion *moveDef = item->first;
		char *typeName = item->second;

		auto it = pieceTypesByName.find(typeName);
		if (it == pieceTypesByName.end())
			ReportError("promotion move refers to a piece type that has not been defined: \"%s\"\n", typeName);
		else
			moveDef->options.push_back(it->second);

		delete[] typeName;
		promotionTypeQueue.erase(item);
	}
	
	return true;
}

#ifdef NO_SVG
char *GameParser::ParsePieceType(xml_node<> *pieceNode, PieceType *type)
#else
char *GameParser::ParsePieceType(xml_node<> *pieceNode, xml_node<> *svgDefsNode, PieceType *type)
#endif
{
	xml_attribute<> *attr = pieceNode->first_attribute("name");
	strcpy(type->name, attr->value());

	attr = pieceNode->first_attribute("notation");
	if (attr != 0)
		strcpy(type->notation, attr->value());

	attr = pieceNode->first_attribute("value");
	if (attr != 0)
		type->value = atoi(attr->value());

#ifndef NO_SVG
	xml_document<> *svgDoc = svgDefsNode->document();
#endif
	char *capturedAs = 0;

	xml_node<> *node = pieceNode->first_node();
	while (node != 0)
	{
		if (strcmp(node->name(), "capturedAs") == 0)
		{
			capturedAs = node->value();
		}
		else if (strcmp(node->name(), "moves") == 0)
		{
			xml_node<> *moveNode = node->first_node();
			while (moveNode != 0)
			{
				MoveDefinition *move = ParseMove(moveNode, true);
				if (move != 0)
					type->moves.push_back(move);

				moveNode = moveNode->next_sibling();
			}
		}/*
		else if (strcmp(node->name(), "special") == 0)
		{
			var specials = childNode.childNodes;
			for (var j = 0; j<specials.length; j++)
				var special = specials[j];

			if (special.tagName == "royal") // consider: while these properties should remain on pieces IN CODE (for game logic's sake) - shouldn't royalty in the DEFINITION be handled via victory conditions? lose when any/all pieces of given type are checkmated/captured/are in check/aren't in check? loading code could then apply royal / antiroyal values
				type.royalty = PieceType.RoyalState.Royal;
			else if (special.tagName == "anti_royal")
				type.royalty = PieceType.RoyalState.AntiRoyal;
			else if (special.tagName == "immobilize")
				type.immobilizations.push(Immobilization.parse(this));
			else // consider: other special types: blocks (as per immobilize, but instead prevents pieces entering a square), kills (kills pieces in target squares without expending a move)
				throw "Unexpected node name in piece's \"special\" tag: " + this.tagName;;
		}*/
#ifndef NO_SVG
		else if (strcmp(node->name(), "appearance") == 0)
		{
			char *playerName = node->first_attribute("player")->value();

			char *defID = svgDoc->allocate_string(type->name, TYPE_NAME_LENGTH + PLAYER_NAME_LENGTH + 1);
			strcat(defID, "_");
			strcat(defID, playerName);

			xml_node<> *def = svgDoc->allocate_node(node_element, "g");
			svgDefsNode->append_node(def);

			attr = svgDoc->allocate_attribute("class", "piece");
			def->append_attribute(attr);
			
			attr = svgDoc->allocate_attribute("id", defID);
			def->append_attribute(attr);

			attr = node->first_attribute("transform");
			if (attr != 0)
			{
				char *trans = attr->value();
				attr = svgDoc->allocate_attribute("transform", trans);
				def->append_attribute(attr);
			}
			
			// move the piece appearance SVG into the def node
			xml_node<> *appNode = node->first_node();
			while (appNode != 0)
			{
				xml_node<> *moveNode = appNode;
				appNode = appNode->next_sibling();
				node->remove_node(moveNode);

				def->append_node(moveNode);
			}
		}
#endif

		node = node->next_sibling();
	}

	return capturedAs;
}


MoveDefinition *GameParser::ParseMove(xml_node<char> *moveNode, bool isTopLevel)
{
	MoveDefinition *retVal = 0;
	if (strcmp(moveNode->name(), "slide") == 0)
		retVal = ParseMove_Slide(moveNode);
	else if (strcmp(moveNode->name(), "leap") == 0)
		retVal = ParseMove_Leap(moveNode);
	else if (strcmp(moveNode->name(), "hop") == 0)
		retVal = ParseMove_Hop(moveNode);
	else if (strcmp(moveNode->name(), "shoot") == 0)
		retVal = ParseMove_Shoot(moveNode);
	else if (strcmp(moveNode->name(), "moveLike") == 0)
		retVal = ParseMove_MoveLike(moveNode);
	else if (strcmp(moveNode->name(), "promotion") == 0)
		retVal = ParseMove_Promotion(moveNode);
	else if (isTopLevel)
	{
		if (strcmp(moveNode->name(), "sequence") == 0)
			retVal = ParseMove_Sequence(moveNode);
	}
	else
	{
		if (strcmp(moveNode->name(), "repeat") == 0)
			retVal = ParseMove_Repeat(moveNode);
		else if (strcmp(moveNode->name(), "whenPossible") == 0)
			retVal = ParseMove_WhenPossible(moveNode);
		else if (strcmp(moveNode->name(), "referencePiece") == 0)
			retVal = ParseMove_ReferencePiece(moveNode);
		else if (strcmp(moveNode->name(), "setState") == 0)
			retVal = ParseMove_State(moveNode);
		else if (strcmp(moveNode->name(), "forEachPiece") == 0)
			retVal = ParseMove_ForEachPiece(moveNode);
	}

	if (retVal == 0)
		ReportError("Got a move with an unexpected type: %s\n", moveNode->name());
	else if (isTopLevel)
	{
		xml_attribute<> *attr = moveNode->first_attribute("customNotation");
		if (attr != 0)
		{
			std::string notation = attr->value();
			retVal->SetCustomNotation(notation);
		}
	}
	
	return retVal;
}

MoveDefinition *GameParser::ParseMove_Slide(xml_node<char> *moveNode)
{
	MoveConditionGroup *conditions = ParseMoveConditions(moveNode->first_node("conditions"), Condition::And);

	xml_attribute<> *attr = moveNode->first_attribute("piece");
	const char *pieceRef = attr == 0 ? "self" : attr->value();

	direction_t dir = LookupDirection(moveNode->first_attribute("dir")->value());

	Distance *dist = ParseDistance(moveNode->first_attribute("dist")->value());

	attr = moveNode->first_attribute("distMax");
	Distance *distMax = attr == 0 ? 0 : ParseDistance(attr->value());

	attr = moveNode->first_attribute("when");
	MoveDefinition::When_t when = attr == 0 ? MoveDefinition::Any : ParseWhen(attr->value());

	return new Slide(pieceRef, conditions, when, dir, dist, distMax);
}


MoveDefinition *GameParser::ParseMove_Leap(xml_node<char> *moveNode)
{
	MoveConditionGroup *conditions = ParseMoveConditions(moveNode->first_node("conditions"), Condition::And);

	xml_attribute<> *attr = moveNode->first_attribute("piece");
	const char *pieceRef = attr == 0 ? "self" : attr->value();

	direction_t dir = LookupDirection(moveNode->first_attribute("dir")->value());

	Distance *dist = ParseDistance(moveNode->first_attribute("dist")->value());

	attr = moveNode->first_attribute("distMax");
	Distance *distMax = attr == 0 ? 0 : ParseDistance(attr->value());

	attr = moveNode->first_attribute("secondDist");
	Distance *secondDist = attr == 0 ? &Distance::Zero : ParseDistance(attr->value());

	attr = moveNode->first_attribute("secondDir");
	direction_t secondDir = attr == 0 ? 0 : LookupDirection(attr->value());

	attr = moveNode->first_attribute("when");
	MoveDefinition::When_t when = attr == 0 ? MoveDefinition::Any : ParseWhen(attr->value());

	return new Leap(pieceRef, conditions, when, dir, dist, distMax, secondDir, secondDist);
}


MoveDefinition *GameParser::ParseMove_Hop(xml_node<char> *moveNode)
{
	MoveConditionGroup *conditions = ParseMoveConditions(moveNode->first_node("conditions"), Condition::And);

	xml_attribute<> *attr = moveNode->first_attribute("piece");
	const char *pieceRef = attr == 0 ? "self" : attr->value();

	direction_t dir = LookupDirection(moveNode->first_attribute("dir")->value());

	Distance *distToHurdle = ParseDistance(moveNode->first_attribute("distToHurdle")->value());

	attr = moveNode->first_attribute("distToHurdleMax");
	Distance *distToHurdleMax = attr == 0 ? 0 : ParseDistance(attr->value());

	Distance *distAfterHurdle = ParseDistance(moveNode->first_attribute("distAfterHurdle")->value());

	attr = moveNode->first_attribute("distAfterHurdleMax");
	Distance *distAfterHurdleMax = attr == 0 ? 0 : ParseDistance(attr->value());

	attr = moveNode->first_attribute("when");
	MoveDefinition::When_t when = attr == 0 ? MoveDefinition::Any : ParseWhen(attr->value());

	attr = moveNode->first_attribute("captureHurdle");
	bool captureHurdle = attr != 0 && strcmp(attr->value(), "true") == 0;

	return new Hop(pieceRef, conditions, when, dir, distToHurdle, distToHurdleMax, distAfterHurdle, distAfterHurdleMax, captureHurdle);
}


MoveDefinition *GameParser::ParseMove_Shoot(xml_node<char> *moveNode)
{
	MoveConditionGroup *conditions = ParseMoveConditions(moveNode->first_node("conditions"), Condition::And);

	xml_attribute<> *attr = moveNode->first_attribute("piece");
	const char *pieceRef = attr == 0 ? "self" : attr->value();

	direction_t dir = LookupDirection(moveNode->first_attribute("dir")->value());

	Distance *dist = ParseDistance(moveNode->first_attribute("dist")->value());

	attr = moveNode->first_attribute("distMax");
	Distance *distMax = attr == 0 ? 0 : ParseDistance(attr->value());

	attr = moveNode->first_attribute("secondDist");
	Distance *secondDist = attr == 0 ? &Distance::Zero : ParseDistance(attr->value());

	attr = moveNode->first_attribute("secondDir");
	direction_t secondDir = attr == 0 ? 0 : LookupDirection(attr->value());

	return new Shoot(pieceRef, conditions, MoveDefinition::Capturing, dir, dist, distMax, secondDir, secondDist);
}


MoveDefinition *GameParser::ParseMove_MoveLike(xml_node<char> *moveNode)
{
	MoveConditionGroup *conditions = ParseMoveConditions(moveNode->first_node("conditions"), Condition::And);

	const char *pieceRef = moveNode->first_attribute("other")->value();
	
	xml_attribute<> *attr = moveNode->first_attribute("when");
	MoveDefinition::When_t when = attr == 0 ? MoveDefinition::Any : ParseWhen(attr->value());
	
	return new MoveLike(pieceRef, conditions, when);
}


MoveDefinition *GameParser::ParseMove_Promotion(xml_node<char> *moveNode)
{
	MoveConditionGroup *conditions = ParseMoveConditions(moveNode->first_node("conditions"), Condition::And);

	xml_attribute<> *attr = moveNode->first_attribute("piece");
	const char *pieceRef = attr == 0 ? "self" : attr->value();

	attr = moveNode->first_attribute("appendNotation");
	bool appendNot = attr == 0 || strcmp(attr->value(), "true") == 0;

	Promotion *moveDef = new Promotion(pieceRef, conditions, appendNot);

	xml_node<> *optionNode = moveNode->first_node("option");
	do
	{
		// queue up setting the piece type potions of this promotion once all the piece types have been loaded. Set it by typeName.
		char *typeNameCopy = new char[TYPE_NAME_LENGTH];
		strcpy(typeNameCopy, optionNode->value());
		promotionTypeQueue.insert(std::make_pair(moveDef, typeNameCopy));

		optionNode = optionNode->next_sibling("option");
	} while (optionNode != 0);

	return moveDef;
}


MoveDefinition *GameParser::ParseMove_Sequence(xml_node<char> *moveNode)
{
	Sequence *move = new Sequence();

	xml_node<> *node = moveNode->first_node();
	while (node != 0)
	{
		MoveDefinition *child = ParseMove(node, false);
		move->contents.push_back(child);

		node = node->next_sibling();
	}

	xml_attribute<> *attr = moveNode->first_attribute("append");
	if (attr != 0)
	{
		auto it = namedMoves.find(attr->value());
		if (it == namedMoves.end())
			ReportError("Got a reference to a non-existing named move: %s\n", attr->value());
		else
			move->contents.push_back(ParseMove_Sequence(it->second));
	}

	return move;
}


MoveDefinition *GameParser::ParseMove_Repeat(xml_node<char> *moveNode)
{
	int min = atoi(moveNode->first_attribute("min")->value());
	int max = atoi(moveNode->first_attribute("max")->value());

	Repeat *move = new Repeat(min, max);

	xml_node<> *node = moveNode->first_node();
	while (node != 0)
	{
		MoveDefinition *child = ParseMove(node, false);
		move->contents.push_back(child);

		node = node->next_sibling();
	}

	return move;
}


MoveDefinition *GameParser::ParseMove_WhenPossible(xml_node<char> *moveNode)
{
	WhenPossible *move = new WhenPossible();

	xml_node<> *node = moveNode->first_node();
	while (node != 0)
	{
		MoveDefinition *child = ParseMove(node, false);
		move->contents.push_back(child);

		node = node->next_sibling();
	}

	return move;
}


MoveDefinition *GameParser::ParseMove_ReferencePiece(xml_node<char> *moveNode)
{
	xml_attribute<> *attr = moveNode->first_attribute("name");
	char *name = attr == 0 ? 0 : attr->value();

	attr = moveNode->first_attribute("type");
	char *typeName = attr == 0 ? 0 : attr->value();

	attr = moveNode->first_attribute("owner");
	Player::Relationship_t relat = ParseRelationship(attr == 0 ? 0 : attr->value());

	attr = moveNode->first_attribute("dir");
	direction_t dir = attr == 0 ? 0 : LookupDirection(attr->value());

	attr = moveNode->first_attribute("dist");
	Distance *dist = attr == 0 ? 0 : ParseDistance(attr->value());

	ReferencePiece *moveDef = new ReferencePiece(name, relat, dir, dist);

	if (typeName != 0)
	{
		// queue up setting the OtherPieceType of this referencePiece once all the piece types have been loaded. Set it by typeName.
		char *typeNameCopy = new char[TYPE_NAME_LENGTH];
		strcpy(typeNameCopy, typeName);
		pieceTypeReferenceQueue.insert(std::make_pair(&moveDef->otherPieceType, typeNameCopy));
	}

	return moveDef;
}


MoveDefinition *GameParser::ParseMove_State(xml_node<char> *moveNode)
{
	MoveConditionGroup *conditions = ParseMoveConditions(moveNode->first_node("conditions"), Condition::And);

	xml_attribute<> *attr = moveNode->first_attribute("piece");
	const char *piece = attr == 0 ? "self" : attr->value();

	customstate_t state = LookupState(moveNode->first_attribute("name")->value());

	SetState::Mode_t mode;
	const char *strMode = moveNode->first_attribute("mode")->value();
	if (strcmp(strMode, "set") == 0)
		mode = SetState::Set;
	else if (strcmp(strMode, "clear") == 0)
		mode = SetState::Clear;
	else if (strcmp(strMode, "set and clear") == 0)
		mode = SetState::SetAndClear;

	return new SetState(piece, state, mode, conditions);
}


MoveDefinition *GameParser::ParseMove_ForEachPiece(xml_node<char> *moveNode)
{
	xml_attribute<> *attr = moveNode->first_attribute("owner");
	Player::Relationship_t relat = ParseRelationship(attr == 0 ? 0 : attr->value());

	Sequence *content = new Sequence();

	xml_node<> *childNode = moveNode->first_node();
	while (childNode != 0)
	{
		MoveDefinition *move = ParseMove(childNode, false);
		if (move != 0)
			content->contents.push_back(move);

		childNode = childNode->next_sibling();
	}

	return new ForEachPiece(relat, content);
}


MoveConditionGroup *GameParser::ParseMoveConditions(xml_node<char> *node, Condition::GroupType_t type)
{
	if (node == 0)
		return 0;

	MoveConditionGroup *conditions = new MoveConditionGroup(type);
	
	xml_node<> *child = node->first_node();

	while (child != 0)
	{
		if (strcmp(child->name(), "and") == 0)
			conditions->elements.push_back(ParseMoveConditions(child, Condition::And));
		else if (strcmp(child->name(), "or") == 0)
			conditions->elements.push_back(ParseMoveConditions(child, Condition::Or));
		else if (strcmp(child->name(), "nand") == 0)
			conditions->elements.push_back(ParseMoveConditions(child, Condition::Nand));
		else if (strcmp(child->name(), "nor") == 0 || strcmp(child->name(), "not") == 0)
			conditions->elements.push_back(ParseMoveConditions(child, Condition::Nor));
		else if (strcmp(child->name(), "xor") == 0)
			conditions->elements.push_back(ParseMoveConditions(child, Condition::Xor));
		else if (strcmp(child->name(), "type") == 0)
		{
			const char *of = child->first_attribute("of")->value();
			MoveCondition_Type *condition = new MoveCondition_Type(of);

			char *typeNameCopy = new char[TYPE_NAME_LENGTH];
			strcpy(typeNameCopy, child->value());
			pieceTypeReferenceQueue.insert(std::make_pair(&condition->type, typeNameCopy));

			conditions->elements.push_back(condition);
		}
		else if (strcmp(child->name(), "owner") == 0)
		{
			const char *of = child->first_attribute("of")->value();
			Player::Relationship_t relationship = ParseRelationship(child->value());
			conditions->elements.push_back(new MoveCondition_Owner(of, relationship));
		}
		else if (strcmp(child->name(), "moveNumber") == 0)
		{
			xml_attribute<> *attr = child->first_attribute("of");
			const char *of = attr == 0 ? "self" : attr->value();
			int number = atoi(child->value());
			Condition::NumericComparison_t comparison = ParseNumericComparison(child->first_attribute("comparison")->value());
			conditions->elements.push_back(new MoveCondition_MoveNumber(of, number, comparison));
		}
		else if (strcmp(child->name(), "maxDist") == 0)
		{
			xml_attribute<> *attr = child->first_attribute("from"); // not present in the schema
			const char *from = attr == 0 ? "self" : attr->value();
			direction_t dir = LookupDirection(child->first_attribute("dir")->value());
			int number = atoi(child->value());
			Condition::NumericComparison_t comparison = ParseNumericComparison(child->first_attribute("comparison")->value());
			conditions->elements.push_back(new MoveCondition_MaxDist(from, dir, number, comparison));
		}
		else if (strcmp(child->name(), "turnsSinceLastMove") == 0)
		{
			xml_attribute<> *attr = child->first_attribute("of");
			const char *of = attr == 0 ? "self" : attr->value();
			int number = atoi(child->value());
			Condition::NumericComparison_t comparison = ParseNumericComparison(child->first_attribute("comparison")->value());
			conditions->elements.push_back(new MoveCondition_TurnsSinceLastMove(of, number, comparison));
		}
		else if (strcmp(child->name(), "threatened") == 0)
		{
			bool value = strcmp(child->value(), "true") == 0;
			const char *partOfMove = child->first_attribute("where")->value();
			bool start = strcmp(partOfMove, "end") != 0;
			bool end = strcmp(partOfMove, "start") != 0;
			conditions->elements.push_back(new MoveCondition_Threatened(start, end, value));
		}
		else if (strcmp(child->name(), "count") == 0)
		{
			xml_attribute<> *attr = child->first_attribute("from");
			const char *from = attr == 0 ? "self" : attr->value();

			direction_t dir = LookupDirection(child->first_attribute("dir")->value());
			Distance *dist = ParseDistance(child->first_attribute("dist")->value());

			attr = child->first_attribute("owner");
			Player::Relationship_t relationship = attr == 0 ? Player::Any : ParseRelationship(attr->value());

			Condition::NumericComparison_t comparison = ParseNumericComparison(child->first_attribute("comparison")->value());
			int number = atoi(child->value());

			attr = child->first_attribute("exclude");
			const char *exclude = attr == 0 ? 0 : attr->value();

			MoveCondition_Count *condition = new MoveCondition_Count(from, dir, dist, relationship, exclude, comparison, number);

			attr = child->first_attribute("type");
			if (attr != 0)
			{
				char *typeNameCopy = new char[TYPE_NAME_LENGTH];
				strcpy(typeNameCopy, attr->value());
				pieceTypeReferenceQueue.insert(std::make_pair(&condition->type, typeNameCopy));
			}
			conditions->elements.push_back(condition);
		}
		else if (strcmp(child->name(), "countMultiple") == 0)
		{
			xml_attribute<> *attr = child->first_attribute("operation");
			MoveCondition_CountMultiple::Operation_t operation;
			if (strcmp(attr->value(), "add") == 0)
				operation = MoveCondition_CountMultiple::Add;
			else if (strcmp(attr->value(), "subtract") == 0)
				operation = MoveCondition_CountMultiple::Subtract;

			Condition::NumericComparison_t comparison = ParseNumericComparison(child->first_attribute("comparison")->value());
			int number = atoi(child->first_attribute("value")->value());

			MoveCondition_CountMultiple *condition = new MoveCondition_CountMultiple(operation, comparison, number);

			xml_node<> *count = child->first_node();
			while (count != 0)
			{
				attr = count->first_attribute("from");
				const char *from = attr == 0 ? "self" : attr->value();

				direction_t dir = LookupDirection(count->first_attribute("dir")->value());
				Distance *dist = ParseDistance(count->first_attribute("dist")->value());

				attr = count->first_attribute("owner");
				Player::Relationship_t relationship = attr == 0 ? Player::Any : ParseRelationship(attr->value());

				xml_attribute<> *attr = count->first_attribute("exclude");
				const char *exclude = attr == 0 ? 0 : attr->value();

				MoveCondition_Count *countCondition = new MoveCondition_Count(from, dir, dist, relationship, exclude, Condition::Equals, 0);

				attr = count->first_attribute("type");
				if (attr != 0)
				{
					char *typeNameCopy = new char[TYPE_NAME_LENGTH];
					strcpy(typeNameCopy, attr->value());
					pieceTypeReferenceQueue.insert(std::make_pair(&countCondition->type, typeNameCopy));
				}

				condition->AddItem(countCondition);
				count = count->next_sibling();
			}

			conditions->elements.push_back(condition);
		}
		else if (strcmp(child->name(), "hasState") == 0)
		{
			xml_attribute<> *attr = child->first_attribute("piece");
			const char *piece = attr == 0 ? "self" : attr->value();

			customstate_t state = LookupState(child->first_attribute("state")->value());
			bool value = strcmp(child->value(), "true") == 0;

			conditions->elements.push_back(new MoveCondition_State(piece, state, value));
		}
		else if (strcmp(child->name(), "referencePiece") == 0)
		{
			xml_attribute<> *attr = child->first_attribute("name");
			char *name = attr == 0 ? 0 : attr->value();

			attr = child->first_attribute("type");
			char *typeName = attr == 0 ? 0 : attr->value();

			attr = child->first_attribute("owner");
			Player::Relationship_t relat = ParseRelationship(attr == 0 ? 0 : attr->value());

			attr = child->first_attribute("dir");
			direction_t dir = attr == 0 ? 0 : LookupDirection(attr->value());

			attr = child->first_attribute("dist");
			Distance *dist = attr == 0 ? 0 : ParseDistance(attr->value());

			MoveCondition_ReferencePiece *condition = new MoveCondition_ReferencePiece(name, relat, dir, dist);

			if (typeName != 0)
			{
				// queue up setting the OtherPieceType of this referencePiece once all the piece types have been loaded. Set it by typeName.
				char *typeNameCopy = new char[TYPE_NAME_LENGTH];
				strcpy(typeNameCopy, typeName);
				pieceTypeReferenceQueue.insert(std::make_pair(&condition->otherPieceType, typeNameCopy));
			}

			conditions->elements.push_back(condition);
		}
		else
			ReportError("Unexpected move condition type: %s\n", child->name());

		child = child->next_sibling();
	}

	return conditions;
}


StateConditionGroup *GameParser::ParseStateConditions(xml_node<char> *node, Condition::GroupType_t type)
{
	if (node == 0)
		return 0;

	StateConditionGroup *conditions = new StateConditionGroup(type);

	xml_node<> *child = node->first_node();

	while (child != 0)
	{
		if (strcmp(child->name(), "and") == 0)
			conditions->elements.push_back(ParseStateConditions(child, Condition::And));
		else if (strcmp(child->name(), "or") == 0)
			conditions->elements.push_back(ParseStateConditions(child, Condition::Or));
		else if (strcmp(child->name(), "nand") == 0)
			conditions->elements.push_back(ParseStateConditions(child, Condition::Nand));
		else if (strcmp(child->name(), "nor") == 0 || strcmp(child->name(), "not") == 0)
			conditions->elements.push_back(ParseStateConditions(child, Condition::Nor));
		else if (strcmp(child->name(), "xor") == 0)
			conditions->elements.push_back(ParseStateConditions(child, Condition::Xor));
		else if (strcmp(child->name(), "cannotMove") == 0)
		{
			conditions->elements.push_back(new StateCondition_CannotMove());
		}
		else if (strcmp(child->name(), "threatened") == 0)
		{
			char *typeName = child->first_attribute("type")->value();
			StateCondition_Threatened *condition = new StateCondition_Threatened();

			char *typeNameCopy = new char[TYPE_NAME_LENGTH];
			strcpy(typeNameCopy, typeName);
			pieceTypeReferenceQueue.insert(std::make_pair(&condition->type, typeNameCopy));

			conditions->elements.push_back(condition);
		}
		else if (strcmp(child->name(), "turnsSinceLastMove") == 0)
		{
			char *typeName = child->first_attribute("type")->value();
			
			xml_attribute<> *attr = child->first_attribute("owner");
			Player::Relationship_t relat = ParseRelationship(attr == 0 ? 0 : attr->value());

			int number = atoi(child->value());
			Condition::NumericComparison_t comparison = ParseNumericComparison(child->first_attribute("comparison")->value());

			StateCondition_TurnsSinceLastMove *condition = new StateCondition_TurnsSinceLastMove(relat, comparison, number);

			char *typeNameCopy = new char[TYPE_NAME_LENGTH];
			strcpy(typeNameCopy, typeName);
			pieceTypeReferenceQueue.insert(std::make_pair(&condition->type, typeNameCopy));

			conditions->elements.push_back(condition);
		}
		else if (strcmp(child->name(), "turnsSinceLastCapture") == 0)
		{
			xml_attribute<> *attr = child->first_attribute("owner");
			Player::Relationship_t relat = ParseRelationship(attr == 0 ? 0 : attr->value());

			int number = atoi(child->value());

			Condition::NumericComparison_t comparison = ParseNumericComparison(child->first_attribute("comparison")->value());
			StateCondition_TurnsSinceLastCapture *condition = new StateCondition_TurnsSinceLastCapture(relat, comparison, number);

			attr = child->first_attribute("type");
			if (attr != 0)
			{
				char *typeNameCopy = new char[TYPE_NAME_LENGTH];
				strcpy(typeNameCopy, attr->value());
				pieceTypeReferenceQueue.insert(std::make_pair(&condition->type, typeNameCopy));
			}

			conditions->elements.push_back(condition);
		}
		else if (strcmp(child->name(), "repetitionOfPosition") == 0)
		{
			int number = atoi(child->value());
			Condition::NumericComparison_t comparison = ParseNumericComparison(child->first_attribute("comparison")->value());

			conditions->elements.push_back(new StateCondition_RepetitionOfPosition(comparison, number));
		}
		else
			ReportError("Unexpected state condition type: %s\n", child->name());

		child = child->next_sibling();
	}

	return conditions;
}


Condition::NumericComparison_t GameParser::ParseNumericComparison(char *value)
{
	if (strcmp(value, "equals") == 0)
		return Condition::Equals;
	if (strcmp(value, "less than") == 0)
		return Condition::LessThan;
	if (strcmp(value, "less than or equals") == 0)
		return Condition::LessThanOrEquals;
	if (strcmp(value, "greater than") == 0)
		return Condition::GreaterThan;
	if (strcmp(value, "greater than or equals") == 0)
		return Condition::GreaterThanOrEquals;
	
	ReportError("Unexpected numeric comparison type: %s\n", value);
	return Condition::Equals;
}


Distance *GameParser::ParseDistance(char *val)
{
	if (val == 0)
		return 0;

	if (strcmp(val, "any") == 0)
		return &Distance::Any;

	int len = strlen(val);
	if (len >= 3 && val[0] == 'm' && val[1] == 'a' && val[2] == 'x')
		return new Distance(Distance::Max, len == 3 ? 0 : atoi(val+3));
	else if (len >= 4 && val[0] == 'p' && val[1] == 'r' && val[2] == 'e' && val[3] == 'v')
		return new Distance(Distance::Prev, len == 4 ? 0 : atoi(val + 4));
	else
		return new Distance(Distance::None, atoi(val));
}


MoveDefinition::When_t GameParser::ParseWhen(char *val)
{
	if (val == 0)
		return MoveDefinition::Any;

	if (strcmp(val, "any") == 0)
		return MoveDefinition::Any;
	if (strcmp(val, "move") == 0)
		return MoveDefinition::Moving;
	if (strcmp(val, "capture") == 0)
		return MoveDefinition::Capturing;

	ReportError("Unexpected 'when' value: %s\n", val);
	return MoveDefinition::Any;
}


Player::Relationship_t GameParser::ParseRelationship(char *val)
{
	if (val == 0)
		return Player::Any;

	if (strcmp(val, "self") == 0)
		return Player::Self;
	if (strcmp(val, "enemy") == 0)
		return Player::Enemy;
	if (strcmp(val, "ally") == 0)
		return Player::Ally;

	ReportError("Unexpected relationship type: %s\n", val);
	return Player::Any;
}

#ifndef NO_SVG
void tidyCssClass(char *val) {
	for (int i=0; val[i]!= '\0'; i++)
		if (val[i] == ' ')
			val[i] = '_';
}
#endif

#ifdef NO_SVG
bool GameParser::ParsePlayers(xml_node<> *setupNode)
#else
bool GameParser::ParsePlayers(xml_node<> *setupNode, xml_document<> *svgDoc)
#endif
{
#ifndef NO_SVG
	xml_node<> *svgRoot = svgDoc->first_node();
#endif

	xml_node<> *playerNode = setupNode->first_node();
	while (playerNode != 0)
	{
		char *playerName = playerNode->first_attribute("name")->value();
		char *forwardDir = playerNode->first_attribute("forwardDirection")->value();

		Player *player = new Player(game, playerName, LookupDirection(forwardDir));
		game->players.push_back(player);

		xml_node<> *pieceNode = playerNode->first_node();
		while (pieceNode != 0)
		{
			char *position = pieceNode->first_attribute("location")->value();

			char *typeName = pieceNode->first_attribute("type")->value();
			auto it = pieceTypesByName.find(typeName);
			if (it == pieceTypesByName.end())
			{
				ReportError("Unrecognized piece type: %s\n", typeName);
				return false;
			}
			PieceType *type = it->second;

			if (strcmp(position, "held") == 0)
			{
				Piece *piece = new Piece(player, type, 0, Piece::Held);
				player->piecesHeld.insert(piece);
			}
			else
			{
				auto it = cellsByRef.find(position);
				if (it == cellsByRef.end())
				{
					ReportError("Piece has unrecognised position: %s\n", position);
					return false;
				}
				cellInfo info = it->second;
				Cell *cell = info.cell;

				Piece *piece = new Piece(player, type, cell, Piece::OnBoard);
				player->piecesOnBoard.insert(piece);

				if (cell->piece == 0)
					cell->piece = piece;
				else
					ReportError("Definition specified multiple pieces in cell %s - this is not allowed\n", cell->GetName());

#ifndef NO_SVG
				// generate piece image
				xml_node<> *image = svgDoc->allocate_node(node_element, "use");

				char *val = svgDoc->allocate_string(0, 8);
				sprintf(val, "p%d", piece->uniqueID);
				image->append_attribute(svgDoc->allocate_attribute("id", val));

				val = svgDoc->allocate_string(player->GetName(), 14);
				tidyCssClass(val);
				strcat(val, " piece");
				image->append_attribute(svgDoc->allocate_attribute("class", val));

				val = svgDoc->allocate_string(0, 8);
				sprintf(val, "%d", info.x);
				image->append_attribute(svgDoc->allocate_attribute("x", val));

				val = svgDoc->allocate_string(0, 8);
				sprintf(val, "%d", info.y);
				image->append_attribute(svgDoc->allocate_attribute("y", val));

				// assign correct piece appearance to this element
				val = svgDoc->allocate_string("#", TYPE_NAME_LENGTH + PLAYER_NAME_LENGTH + 2);
				strcat(val, piece->GetType()->GetName());
				strcat(val, "_");
				strcat(val, player->GetName());
				tidyCssClass(val);
				image->append_attribute(svgDoc->allocate_attribute("xmlns:xlink", "http://www.w3.org/1999/xlink"));
				image->append_attribute(svgDoc->allocate_attribute("xlink:href", val));

				val = svgDoc->allocate_string(player->GetName(), TYPE_NAME_LENGTH + PLAYER_NAME_LENGTH + 1);
				strcat(val, " ");
				strcat(val, piece->GetType()->GetName());
				image->append_attribute(svgDoc->allocate_attribute("name", val));

				val = svgDoc->allocate_string(0, CELL_REF_LENGTH);
				sprintf(val, cell->GetName());
				image->append_attribute(svgDoc->allocate_attribute("cell", val));

				svgRoot->append_node(image);
#endif
			}

			pieceNode = pieceNode->next_sibling();
		}

		playerNode = playerNode->next_sibling();
	}
	return true;
}


bool GameParser::ParseRules(xml_node<> *rulesNode)
{
	xml_node<> *node = rulesNode->first_node("turnOrder");
	TurnOrder *order = node == 0 ? TurnOrder::CreateDefault(game->players) : ParseTurnOrder(node);

	if (order == 0)
		return false;
	else
		game->turnOrder = order;

	node = rulesNode->first_node("startOfTurn");
	StateLogic *logic = node == 0 ? new StateLogic(true, true) : ParseStateLogic(node, true, true);

	if (logic == 0)
	{
		ReportError("An error occurred parsing the startOfTurn rules\n");
		return false;
	}
	else
		game->startOfTurnLogic = logic;

	node = rulesNode->first_node("endOfTurn");
	logic = node == 0 ? new StateLogic(false, true) : ParseStateLogic(node, false, true);

	if (logic == 0)
	{
		ReportError("An error occurred parsing the endOfTurn rules\n");
		return false;
	}
	else
		game->endOfTurnLogic = logic;

	return true;
}

TurnOrder *GameParser::ParseTurnOrder(xml_node<> *node)
{
	TurnOrder *turnOrder = new TurnOrder();

	if (!ParseTurnRepeat(turnOrder, node))
	{
		delete turnOrder;
		return 0;
	}

	return turnOrder;
}

bool GameParser::ParseTurnRepeat(TurnRepeat *repeat, xml_node<> *repeatNode)
{
	xml_node<> *node = repeatNode->first_node();
	while (node != 0)
	{
		if (strcmp(node->name(), "turn") == 0)
		{
			char *playerName = node->first_attribute("player")->value();
			Player *player = GetPlayerByName(playerName);
			if (player != 0)
			{
				TurnStep *step = new TurnStep(player);
				repeat->steps.push_back(step);
			}
		}
		else if (strcmp(node->name(), "repeat") == 0)
		{
			xml_attribute<> *attr = node->first_attribute("count");
			int count = attr == 0 ? 0 : atoi(attr->value());
			TurnRepeat *child = new TurnRepeat(count);
			repeat->steps.push_back(child);
			if (!ParseTurnRepeat(child, node))
				return false;
		}
		else
			ReportError("Unexpected move node: %s\n", node->name());

		node = node->next_sibling();
	}

	return true;
}

Player *GameParser::GetPlayerByName(char *name)
{
	for (auto it = game->players.begin(); it != game->players.end(); it++)
	{
		Player *player = *it;
		if (strcmp(player->GetName(), name) == 0)
			return player;
	}

	ReportError("Attempted to look up invalid player name: %s\n", name);
	return 0;
}

StateLogic *GameParser::ParseStateLogic(xml_node<> *rootNode, bool startOfTurn, bool root)
{
	StateLogic *stateLogic = new StateLogic(startOfTurn, root);

	// essentially gonna parse any number of if/then/else, endGame, notify & disallow, to any depth
	xml_node<> *node = rootNode->first_node();
	StateLogicBlock *lastIf = 0;
	StateLogicElement *logicElement;

	while (node != 0)
	{
		if (strcmp(node->name(), "if") == 0)
		{
			lastIf = new StateLogicBlock(ParseStateConditions(node, Condition::And));
			logicElement = lastIf;
		}
		else if (strcmp(node->name(), "then") == 0)
		{
			lastIf->logicIfTrue = ParseStateLogic(node, startOfTurn, false);
			logicElement = 0;
		}
		else if (strcmp(node->name(), "else") == 0)
		{
			lastIf->logicIfFalse = ParseStateLogic(node, startOfTurn, false);
			logicElement = 0;
		}
		else if (strcmp(node->name(), "endGame") == 0)
		{
			xml_attribute<> *appendNot = node->first_attribute("appendNotation");
			const char *appendNotation = appendNot == 0 ? "" : appendNot->value();
			const char *message = node->first_attribute("message")->value();
			const char *type = node->first_attribute("type")->value();

			if (strcmp(type, "win") == 0)
				logicElement = new GameEnd(StateLogic::Win, message, appendNotation);
			else if (strcmp(type, "lose") == 0)
				logicElement = new GameEnd(StateLogic::Lose, message, appendNotation);
			else if (strcmp(type, "draw") == 0)
				logicElement = new GameEnd(StateLogic::Draw, message, appendNotation);
			else
			{
				ReportError("Unexpected GameEnd type: %s\n", type);
				logicElement = 0;
			}
		}
		else if (strcmp(node->name(), "notify") == 0)
		{
			xml_attribute<> *appendNot = node->first_attribute("appendNotation");
			const char *appendNotation = appendNot == 0 ? "" : appendNot->value();
			const char *message = node->first_attribute("message")->value();

			logicElement = new GameEnd(StateLogic::None, message, appendNotation);
		}
		else if (strcmp(node->name(), "disallow") == 0)
		{
			game->illegalMovesSpecified = true;
			logicElement = new GameEnd(StateLogic::IllegalMove, "");
		}
		else
		{
			ReportError("Unexpected end-of-game check type: %s\n", node->name());
			logicElement = 0;
		}

		if (logicElement != 0)
			stateLogic->elements.push_back(logicElement);

		node = node->next_sibling();
	}

	return stateLogic;
}

direction_t GameParser::LookupDirection(char *dirName)
{
	if (strcmp(dirName, "forward") == 0)
		return DIRECTION_FORWARD;
	else if (strcmp(dirName, "same") == 0)
		return DIRECTION_SAME;

	dirLookup_t::iterator it = directionLookups.find(dirName);

	if (it == directionLookups.end())
	{// a new direction, add it
		maxDirection = maxDirection << 1;
		allDirections |= maxDirection;
		directionLookups.insert(dirLookupEntry_t(dirName, maxDirection));

		return maxDirection;
	}

	return it->second;
}

customstate_t GameParser::LookupState(char *stateName)
{
	stateLookup_t::iterator it = stateLookups.find(stateName);

	if (it == stateLookups.end())
	{// a new state, add it
		customstate_t state = maxState;
		maxState = maxState << 1;
		stateLookups.insert(dirLookupEntry_t(stateName, state));

		return state;
	}

	return it->second;
}