﻿function AI_RandomCapture() {

}

// Picks from all available CAPTURE moves, completely randomly.
// If there are non, picks from all available moves, completely randomly.
AI_RandomCapture.prototype.selectMove = function () {
    var player = game.currentPlayer;

    var allMoves = [];
    for (var i = 0; i < player.piecesOnBoard.length; i++) {
        var piece = player.piecesOnBoard[i];
        allMoves = allMoves.concat(piece.cachedMoves);
    }

    var captures = [];
    for (var i = 0; i < allMoves.length; i++) {
        var move = allMoves[i];
        if (move.isCapture())
            captures.push(move);
    }

    if (captures.length > 0) {
        var index = Math.floor(Math.random() * captures.length);
        return captures[index];
    }

    var index = Math.floor(Math.random() * allMoves.length);
    return allMoves[index];
};

var AI = new AI_RandomCapture();