﻿function EndOfGame(game) {
    this.game = game;
    this.startOfTurnChecks = [];
    this.endOfTurnChecks = [];
}

EndOfGame.parse = function (xmlNode, game) {
    var endOfGame = new EndOfGame(game);
    
    var childNodes = xmlNode.childNodes;
    for (var i = 0; i < childNodes.length; i++) {
        var childNode = childNodes[i];

        var checkType;
        switch (childNode.nodeName) {
            case 'win':
                checkType = EndOfGame.Type.Win; break;
            case 'lose':
                checkType = EndOfGame.Type.Lose; break;
            case 'draw':
                checkType = EndOfGame.Type.Draw; break;
            default:
                throw 'Unexpected endOfGame node type: ' + childNode.name;
        }

        var conditions = EndOfGameConditions.parse(childNode);

        var stage = childNode.getAttribute('when');
        var checkList = stage == 'startOfTurn' ? endOfGame.startOfTurnChecks : endOfGame.endOfTurnChecks;
        checkList.push(new EndOfGameCheck(checkType, conditions));
    }

    return endOfGame;
};

EndOfGame.createDefault = function (game) {
    var endOfGame = new EndOfGame(game);

    var condition = new EndOfGameConditions();
    // add a <pieceCount owner="self" check="equals">0</pieceCount> condition here
    var outOfPieces = new EndOfGameCheck(EndOfGame.Type.Lose, condition);
    endOfGame.startOfTurnChecks.push(outOfPieces);

    condition = new Conditions();
    // add a <cannotMove /> condition here
    var outOfMoves = new EndOfGameCheck(EndOfGame.Type.Draw, condition);
    endOfGame.startOfTurnChecks.push(outOfMoves);

    return endOfGame;
};

EndOfGame.prototype.checkStartOfTurn = function (anyPossibleMoves) {
    // return Win/Lose/Draw, or undefined if the game isn't over yet

    for (var i = 0; i < this.startOfTurnChecks.length; i++) {
        var check = this.startOfTurnChecks[i];
        if (check.conditions.isSatisfied(this.game, anyPossibleMoves))
            return check.type;
    }

    if (anyPossibleMoves)
        return undefined;

    if (this.game.currentPlayer.piecesOnBoard.length == 0)
        return EndOfGame.Type.Lose; // can't move and have no pieces. lose.

    return EndOfGame.Type.Draw; // can't move, but have pieces. draw.
};

EndOfGame.prototype.checkEndOfTurn = function () {
    // return Win/Lose/Draw, or undefined if the game isn't over yet

    var noNextPlayer = this.game.turnOrder.getNextPlayer() == null;
    this.game.turnOrder.stepBackward();

    for (var i = 0; i < this.endOfTurnChecks.length; i++) {
        var check = this.endOfTurnChecks[i];
        if (check.conditions.isSatisfied(this.game, true))
            return check.type;
    }

    // if the next player is null, we've reached the end of the turn order
    if (noNextPlayer)
        return EndOfGame.Type.Draw;

    return undefined;
};

EndOfGame.Type = {
    Win: 0,
    Lose: 1,
    Draw: 2
};

function EndOfGameCheck(type, conditions) {
    this.type = type;
    this.conditions = conditions;
}

function EndOfGameConditions() {
    this.elements = [];
}

EndOfGameConditions.parse = function (xmlNode) {
    var conditions = new EndOfGameConditions();
    xmlNode = xmlNode.firstChild;

    while (xmlNode != null) {
        switch(xmlNode.nodeName)
        {
            case 'cannotMove':
                conditions.elements.push(EndOfGameConditions_cannotMove.parse(xmlNode));
                break;
            case 'threatened':
                conditions.elements.push(EndOfGameConditions_threatened.parse(xmlNode));
                break;

            default:
                console.log("Unrecognised end-of-game condition type: " + xmlNode.nodeName);
                break;
        }

        xmlNode = xmlNode.nextSibling;
    }
};

function EndOfGameConditions_cannotMove() {
    
}

EndOfGameConditions_cannotMove.parse = function (xmlNode) {
    return new EndOfGameConditions_cannotMove();
};

EndOfGameConditions_cannotMove.prototype.isSatisfied = function (game, canMove) {
    return !canMove;
};

function EndOfGameConditions_threatened(pieceType) {
    this.pieceType = pieceType;
}

EndOfGameConditions_threatened.parse = function (xmlNode) {
    var type = xmlNode.getAttribute('type');
    return new EndOfGameConditions_threatened(type);
};

EndOfGameConditions_threatened.prototype.isSatisfied = function (game, canMove) {
    var pieces = game.currentPlayer.piecesOnBoard;
    for (var i = 0; i < pieces.length; i++) {
        var piece = pieces[i];
        if (piece.pieceType.name != this.pieceType)
            continue;

        if (piece.isThreatenedAt(piece.position))
            return true;
    }

    return false;
};