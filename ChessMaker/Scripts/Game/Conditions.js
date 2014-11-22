﻿function Conditions(type) {
    this.type = type;
    this.elements = [];
}

Conditions.GroupType = {
    And: 0,
    Or: 1,
    Nand: 2,
    Nor: 3,
    Xor: 4
};

Conditions.findNode = function (node) {
    var nodes = node.childNodes;
    for (var i = 0; i < nodes.length; i++) {
        var node = nodes[i];
        if (node.tagName == 'conditions') 
            return node;
    }
    return null;
}

Conditions.parse = function (node, type) {
    if (type === undefined)
        type = Conditions.GroupType.And;

    var group = new Conditions(type);
    if (node == null)
        return group;

    var children = node.childNodes;
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        var name = child.nodeName.toLowerCase();

        switch (name) {
            case 'and':
                group.elements.push(Conditions.parse(child, Conditions.GroupType.And));
                break;
            case 'or':
                group.elements.push(Conditions.parse(child, Conditions.GroupType.Or));
                break;
            case 'nand':
                group.elements.push(Conditions.parse(child, Conditions.GroupType.Nand));
                break;
            case 'nor':
            case 'not':
                group.elements.push(Conditions.parse(child, Conditions.GroupType.Nor));
                break;
            case 'xor':
                group.elements.push(Conditions.parse(child, Conditions.GroupType.Xor));
                break;

            case 'type':
                var of = child.getAttribute('of');
                var type = child.textContent;
                group.elements.push(new Conditions_Type(of, type));
                break;
            case 'owner':
                var of = child.getAttribute('of');
                var relationship = Player.Relationship.parse(child.textContent);
                group.elements.push(new Conditions_Owner(of, relationship));
                break;

            case 'threatened':
            case 'compare':
            case 'num_pieces_in_range':
            case 'move_causes_check':
            case 'move_causes_checkmate':
            case 'checkmate':
            case 'pieces_threatened':
            case 'repeated_check':
            case 'no_moves_possible':
            case 'repetition_of_position':
            case 'turns_since_last_capture':
            case 'turns_since_last_move':
            default:
                console.log("Unrecognised condition type: " + name);
                break;
        }
    }

    return group;
};

Conditions.prototype.isSatisfied = function (move, game) {
    switch (this.type) {
        case Conditions.GroupType.And:
            for (var i = 0; i < this.elements.length; i++)
                if (!this.elements[i].isSatisfied(move, game))
                    return false;
            return true;
        case Conditions.GroupType.Or:
            for (var i = 0; i < this.elements.length; i++)
                if (this.elements[i].isSatisfied(move, game))
                    return true;
            return false;
        case Conditions.GroupType.Nand:
            for (var i = 0; i < this.elements.length; i++)
                if (!this.elements[i].isSatisfied(move, game))
                    return true;
            return false;
        case Conditions.GroupType.Nor:
            for (var i = 0; i < this.elements.length; i++)
                if (this.elements[i].isSatisfied(move, game))
                    return false;
            return true;
        case Conditions.GroupType.Xor:
            var any = false;
            for (var i = 0; i < this.elements.length; i++)
                if (this.elements[i].isSatisfied(move, game))
                    if (any)
                        return false;
                    else
                        any = true;
            return any;
        default: // there are no other types!
            return false;
    }
};

function Conditions_Type(of, type) {
    this.of = of;
    this.type = type;
}

Conditions_Type.prototype.isSatisfied = function(move, game) {
    var other = move.getPieceByRef(this.of);
    if (other == null)
        throw "Piece reference not found: " + this.of;

    return other.pieceType.name == this.type;
};

function Conditions_Owner(of, relationship) {
    this.of = of;
    this.relationship = relationship;
}

Conditions_Owner.prototype.isSatisfied = function (move, game) {
    var other = move.getPieceByRef(this.of);
    if (other == null)
        throw "Piece reference not found: " + this.of;

    if (this.relationship == Player.Relationship.Any)
        return true;

    return this.relationship == move.player.getRelationship(other.ownerPlayer);
};