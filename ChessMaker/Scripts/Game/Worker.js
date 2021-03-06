﻿"use strict";

importScripts('Chessmaker.js');

var performMove, performAIMove;

onmessage = function (event) {
    var command = event.data[0];
    switch (command) {
        case 'load':
            loadDefinition(event.data[1], event.data[2]);
            break;
        case 'move':
            console.time('move');
            var retVal = performMove(event.data[1]);
            console.timeEnd('move');
            checkRunAI(retVal);
            break;
        default:
            error("Worker received an expected message from UI thread: " + event.data[0]);
            break;
    }
};

function error(msg) {
    postMessage(['error', msg]);
}

function loadDefinition(defUrl, players) {
    var request = new XMLHttpRequest();
    request.open('GET', defUrl, true);

    request.onload = function () {
        if (request.status >= 200 && request.status < 400)
            parseDefinition(request.responseText, players);
        else
            error('Failed to load game definition, server returned code ' + request.status);
    };

    request.onerror = function () {
        error('Failed to load game definition, encountered an error retrieving definition');
    };

    request.send();
}

function parseDefinition(xml, players) {
    console.time('parse');
    if (Module.ccall('Parse', 'number', ['string'], [xml]) == 0) {
        error('An error occurred initializing the game');
        return;
    }
    console.timeEnd('parse');

    // set players to the correct types
    var setPlayerLocal = Module.cwrap('SetPlayerLocal', 'number', ['number']);
    var setPlayerRemote = Module.cwrap('SetPlayerRemote', 'number', ['number']);
    var setPlayerAI = Module.cwrap('SetPlayerAI', 'number', ['number', 'string']);
    for (var i = 1; i <= players.length; i++) {
        var player = players[i - 1];
        if (player === true) {
            if (setPlayerLocal(i) == 0)
                console.log('Unable to set player #' + i + ' to play locally');
        }
        else if (player === false) {
            if (setPlayerRemote(i) == 0)
                console.log('Unable to set player #' + i + ' to play remotely');
        }
        else {
            if (setPlayerAI(i, player) == 0)
                console.log('Unable to set player #' + i + ' to use "' + player + '" AI');
        }
    }

    performMove = Module.cwrap('PerformMove', 'number', ['string']);
    performAIMove = Module.cwrap('PerformAIMove', 'number');

    var boardSVG = Module.ccall('GetBoardSVG', 'string');
    var showCaptured = Module.ccall('ShouldShowCapturedPieces', 'number') != 0;
    var showHeld = Module.ccall('ShouldShowHeldPieces', 'number') != 0;
    postMessage(['init', boardSVG, showCaptured, showHeld]);

    var retVal = Module.ccall('Start', 'number');
    checkRunAI(retVal);
}

function checkRunAI(retVal) {
    while (retVal == 2) {
        console.time('ai');
        retVal = performAIMove();
        console.timeEnd('ai');
    }
}

function addPossibleMove(notation, fromCell, toCell, disambiguation) {
    postMessage(['poss', Pointer_stringify(notation), Pointer_stringify(fromCell), Pointer_stringify(toCell), Pointer_stringify(disambiguation)]);
}

function startTurn(msg, isLocal) {
    postMessage(['player', Pointer_stringify(msg), isLocal]);
}

function showGameEnd(message) {
    postMessage(['end', Pointer_stringify(message)]);
}

function logMove(player, number, notation, from, to) {
    postMessage(['log', Pointer_stringify(player), number, Pointer_stringify(notation), Pointer_stringify(from), Pointer_stringify(to)]);
}

function movePiece(pieceID, state, location, owner, type) {
    if (state == 1)
        state = 'captured';
    else if (state == 2)
        state = 'held';
    else {
        state = 'board';
        location = Pointer_stringify(location);
    }
    postMessage(['move', pieceID, state, location, Pointer_stringify(owner), Pointer_stringify(type)]);
}

function showMessage(msg) {
    postMessage(['msg', Pointer_stringify(msg)]);
}
