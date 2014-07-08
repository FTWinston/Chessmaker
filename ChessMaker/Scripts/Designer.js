﻿function addClass(elems, className) {
    elems.each(function () {
        var classes = this.getAttribute('class');
        var spaced = ' ' + classes + ' ';
        if (spaced.indexOf(className) != -1)
            return;
        this.setAttribute('class', classes + ' ' + className);
    });
}

function remClass(elems, className) {
    var spaced = ' ' + className + ' ';
    elems.each(function () {
        var classes = ' ' + this.getAttribute('class') + ' ';
        if (classes.indexOf(spaced) == -1)
            return;
        this.setAttribute('class', classes.replace(spaced, ' ').trim());
    });
}

function hasClass(elem, className) {
    var classes = ' ' + elem.getAttribute('class') + ' ';
    var spaced = ' ' + className + ' ';
    return classes.indexOf(spaced) != -1;
}

function movePath(dx, dy) {
    $('#render path.selected').each(function () {
        // all our cell paths are designed such that the first segment is the center of the cell, all others are RELATIVELY positioned.
        // so moving the first should be sufficient!
        var seg = this.pathSegList.getItem(0);
        seg.x += dx; seg.y += dy;

        // begin transform rotation code
        // however, as we now use transforms for rotation, we need to update the center of the transform also
        var transform = this.getAttribute('transform');
        if (transform != null) {
            var rotStart = transform.indexOf('rotate(');
            if (rotStart != -1) {
                rotStart += 7;
                var firstSpace = transform.indexOf(' ', rotStart);
                if (firstSpace != -1) {
                    var newTrans = transform.substr(0, firstSpace + 1) + seg.x + ' ' + seg.y + ')';
                    this.setAttribute('transform', newTrans);
                }
            }
        }
        // end transform rotation code
    });
    updateBounds();
}

function rotatePath(deg) {
    var paths = $('#render path.selected');
    if (paths.length == 0)
        return;

    var b = getBounds(paths);
    var cx = (b.maxX + b.minX) / 2; var cy = (b.maxY + b.minY) / 2;
    //var rad = deg * Math.PI / 180;

    paths.each(function () {
        var seg = this.pathSegList.getItem(0);

        // move this element if it should
        if (seg.x != cx || seg.y != cy) {
            var rad = deg * Math.PI / 180;
            rotateAbout(seg, cx, cy, rad);
        }

        var newAngle = deg;
        var transform = this.getAttribute('transform');
        if (transform != null) {
            var rotStart = transform.indexOf('rotate(');
            if (rotStart != -1) {
                rotStart += 7;
                var firstSpace = transform.indexOf(' ', rotStart);
                if (firstSpace != -1) {
                    newAngle = parseInt(transform.substring(rotStart, firstSpace));
                    if (isNaN(newAngle))
                        newAngle = deg;
                    else {
                        newAngle += deg;
                        if (newAngle > 360)
                            newAngle -= 360;
                        else if (newAngle < 0)
                            newAngle += 360;
                    }
                }
            }
        }

        this.setAttribute('transform', 'rotate(' + newAngle + ' ' + seg.x + ' ' + seg.y + ')');
    });
}

function applyOwnTransform(node, transform) {
    var seg = node.pathSegList.getItem(0);
    var transform = $(node).attr('transform');

    var rotStart = transform.indexOf('rotate(');
    if (rotStart == -1)
        return;

    rotStart += 7;
    var rotEnd = transform.indexOf(')', rotStart);
    if (rotEnd == -1)
        return;

    var rotParts = transform.substring(rotStart, rotEnd).split(' ');

    var deg = parseInt(rotParts[0]);
    var rx = parseFloat(rotParts[1]);
    var ry = parseFloat(rotParts[2]);

    if (isNaN(deg) || isNaN(rx) || isNaN(ry))
        return;
    
    while (deg > 360)
        deg -= 360;
    while (deg < 0)
        deg += 360;

    var rad = deg * Math.PI / 180;

    // move this element if it should
    if (seg.x != rx || seg.y != ry)
        rotateAbout(seg, rx, ry, rad);

    var numSegments = node.pathSegList.numberOfItems;
            
    // move each node on the element's path (except the first), assume they are all relative.
    for (var i=1; i<numSegments; i++ )
        rotateAbout(node.pathSegList.getItem(i), 0, 0, rad);
}

function rotateAbout(seg, cx, cy, angle) {
    // all path segment types have x and y properties except horizontal/vertical lines, which only have x or y. These are IDs 12 - 15.
    if (seg.pathSegType >= 12 && seg.pathSegType <= 15)
        return;

    var x2 = Math.cos(angle) * (seg.x - cx) - Math.sin(angle) * (seg.y - cy) + cx;
    var y2 = Math.sin(angle) * (seg.x - cx) + Math.cos(angle) * (seg.y - cy) + cy;
    seg.x = Math.round(x2 * 100) / 100;
    seg.y = Math.round(y2 * 100) / 100;
}

function getBounds(paths) {
    var bounds = { minX: 99999, maxX: -99999, minY: 99999, maxY: -99999 };

    paths.each(function () {
        var seg = this.pathSegList.getItem(0);
        if (seg.x < bounds.minX)
            bounds.minX = seg.x;
        if (seg.x > bounds.maxX)
            bounds.maxX = seg.x;
        if (seg.y < bounds.minY)
            bounds.minY = seg.y;
        if (seg.y > bounds.maxY)
            bounds.maxY = seg.y;
    });
    return bounds;
}

function updateBounds() {
    var paths = $('#render path');

    if (paths.length == 0)
        return;

    var b = getBounds(paths);

    // min & max currently based on object CENTERS. So add half width to get full size
    b.minX -= 40; b.minY -= 40;
    b.maxX += 40; b.maxY += 40;

    var width = b.maxX - b.minX, height = b.maxY - b.minY;

    if (width < 120) {
        b.minX = (b.maxX + b.minX) / 2 - 60;
        width = 120;
    }
    if (height < 120) {
        b.minY = (b.maxY + b.minY) / 2 - 60;
        height = 120;
    }

    // jquery's .attr sets everything lowercase. Won't work for this.
    $('#render')[0].setAttribute('viewBox', b.minX + ' ' + b.minY + ' ' + width + ' ' + height);
}

function resizeBoard() {
    var svg = $('#render');
    var parent = svg.parent();
    var size = Math.floor(Math.min(parent.outerWidth(false), parent.outerHeight(false)));

    svg.css('width', size + "px").css('height', size + "px");
}

function SVG(tag) {
    return document.createElementNS('http://www.w3.org/2000/svg', tag);
}

function cellClicked(e) {
    if (!e.shiftKey && !e.ctrlKey) {
        clearSelection();
        addClass($(this), 'selected');
    }
    else if (hasClass(this, 'selected'))
        remClass($(this), 'selected');
    else
        addClass($(this), 'selected');

    selectedChanged();
    return false;
}

function clearSelection() {
    var paths = $('#render path.selected');
    if (paths.length == 0)
        return;

    remClass(paths, 'selected');
    selectedChanged();
}

function selectedChanged() {
    var paths = $('#render path.selected');
    var noneSelected = paths.length == 0;

    $('.itemProperties .button').button('option', 'disabled', noneSelected);

    var colorOptions = $('.itemProperties .ui-buttonset');
    colorOptions.buttonset('option', 'disabled', noneSelected);

    if (noneSelected)
        return;

    // if selected cells of multiple colors, clear the options
    var last = null;
    paths.each(function () {
        var color;
        if (hasClass(this, 'light'))
            color = 'light';
        else if (hasClass(this, 'mid'))
            color = 'mid';
        else if (hasClass(this, 'dark'))
            color = 'dark';
        else
            return;
        if (color != last && last != null) { // color has changed
            colorOptions.find('input').prop('checked', false);
            last = null;
            return false;
        }
        last = color;
    });

    if (last != null) {
        // otherwise, all selected items have same color. Update the color selector to match.
        $('#' + last).prop('checked', true);
    }
    colorOptions.buttonset('refresh');
}

function addSingleElement(pathData) {
    clearSelection();
    var color = $('.itemProperties :radio:checked').attr('id');

    addElement(pathData, color);

    updateBounds();
    selectedChanged();
}

var nextElem = 1;
function addElement(pathData, color) {
    var elem = $(SVG('path'))
                .attr('d', pathData)
			    .attr('class', 'cell ' + color + ' selected')
                .attr('id', 'cell' + nextElem)
                .appendTo($('#render'))
                .click(cellClicked);

    nextElem++;
}

function submitPopup(popup) {
    var id = popup.attr('id');
    switch (id) {
        case 'addSquares':
            var width = parseInt($('#sqWidth').val()), height = parseInt($('#sqHeight').val());
            if (isNaN(width) || isNaN(height) || width < 1 || height < 1 || width > 64 || height > 64)
                return false;
            addSquares(width, height, $('#sqPattern').val());
            return true;
        case 'addTriangles':
            var length = parseInt($('#triLength').val());
            if (isNaN(length) || length < 1 || length > 64)
                return false;
            addTriangles(length, $('#triPattern').val());
            return true;
        case 'addHexes':
            var length = parseInt($('#hexLength').val());
            if (isNaN(length) || length < 1 || length > 64)
                return false;
            addHexes(length, $('#hexPattern').val());
            return true;
        case 'addCircle':
            var radiusOuter = parseInt($('#cirRadius').val()), radiusInner = parseInt($('#cirRadiusInner').val());
            var slicesTot = parseInt($('#cirSlicesTot').val()), slicesAct = parseInt($('#cirSlicesAct').val());
            if (isNaN(radiusOuter) || isNaN(radiusInner) || isNaN(slicesTot) || isNaN(slicesAct) || radiusOuter < 1 || radiusOuter > 64 || radiusInner > radiusOuter || radiusInner < 0 || slicesTot < 1 || slicesAct < 1 || slicesTot > 64 || slicesAct > slicesTot)
                return false;
            addCircle(radiusOuter, radiusInner, slicesTot, slicesAct, $('#cirPattern').val());
            return true;
        default:
            return false;
    }
}

function resolvePattern(step, pattern) {
    switch (pattern) {
        case '1':
            return 'light';
        case '2':
            return 'mid';
        case '3':
            return 'dark';
        case '4':
            return step % 2 == 0 ? 'light' : 'dark';
        case '5':
            return step % 2 == 0 ? 'light' : 'mid';
        case '6':
            return step % 2 == 0 ? 'mid' : 'dark';
        case '7':
            switch (step % 3) {
                case 0:
                    return 'light';
                case 1:
                    return 'mid';
                case 2:
                    return 'dark';
            }
        default:
            console.log('pattern = ' + pattern);
            return 'light';
    }
}

function addSquares(width, height, pattern) {
    var squarePath = ' m-20 -20 l40 0 l0 40 l-40 0 Z';

    clearSelection();

    for (var iy = 0; iy < height; iy++) {
        var ystep = width * iy;
        if (width % 2 == 0 && iy % 2 == 1)
            ystep++;
        for (var ix = 0; ix < width; ix++)
            addElement('M' + (60 + ix * 40) + ' ' + (60 + iy * 40) + squarePath, resolvePattern(ix + ystep, pattern));
    }

    updateBounds();
    selectedChanged();
}

function addTriangles(size, pattern) {
    var triPath = ' m0 -20 l20 35 l-40 0 Z', triPathInverted = ' m0 20 l20 -35 l-40 0 Z';

    clearSelection();

    for (var iy = 0; iy < size; iy++) {
        var ystep = (iy + 1) * iy;
        for (var ix = 0; ix <= iy; ix++) {
            addElement('M' + (60 + ix * 40 - iy * 20) + ' ' + (60 + iy * 35) + triPath, resolvePattern(ystep, pattern));
            if (ix != 0)
                addElement('M' + (40 + ix * 40 - iy * 20) + ' ' + (55 + iy * 35) + triPathInverted, resolvePattern(ystep + 1, pattern));
        }
    }

    updateBounds();
    selectedChanged();
}

function addHexes(size, pattern) {
    var hexPath = ' m0 -40 l35 20 l0 40 l-35 20 l-35 -20 l0 -40 Z';

    var maxRowSize = size * 2 - 1, iy = 0;
    for (var rowSize = size; rowSize <= maxRowSize; rowSize++) {
        var rowOffset = (maxRowSize - rowSize) * 2;

        for (var ix = 0; ix < rowSize; ix++)
            addElement('M' + (60 + ix * 70 - iy * 35) + ' ' + (60 + iy * 60) + hexPath, resolvePattern(ix + rowOffset, pattern));

        iy++;
    }
    for (var rowSize = maxRowSize - 1; rowSize >= size; rowSize--) {
        var rowOffset = (maxRowSize - rowSize) * 2;

        for (var ix = 0; ix < rowSize; ix++)
            addElement('M' + (60 + ix * 70 - (maxRowSize - iy - 1) * 35) + ' ' + (60 + iy * 60) + hexPath, resolvePattern(ix + rowOffset, pattern));

        iy++;
    }

    updateBounds();
    selectedChanged();
}

function addCircle(radiusOuter, radiusInner, slicesTot, slicesAct, pattern) {
    var start = { x: 60, y: 60 };
    var sliceAngle = Math.PI * 2 / slicesTot;
    for (var ring = radiusInner + 1; ring <= radiusOuter; ring++) {
        var startAngle = 0;

        for (var slice = 0; slice < slicesTot; slice++) {
            var endAngle = startAngle + sliceAngle;
            var outerStart = projectAngle(start, startAngle, ring * 40);
            var outerEnd = projectAngle(start, endAngle, ring * 40);

            var centerX; var centerY;
            if (ring == 1) {
                centerX = (outerStart.x + outerEnd.x + start.x) / 3;
                centerY = (outerStart.y + outerEnd.y + start.y) / 3;

                outerEnd.x -= outerStart.x; outerEnd.y -= outerStart.y;
                outerStart.x -= start.x; outerStart.y -= start.y;
                var midPoint = { x: start.x - centerX, y: start.y - centerY };

                addElement('M' + centerX + ' ' + centerY + ' m' + midPoint.x + ' ' + midPoint.y +
                            ' l' + outerStart.x + ' ' + outerStart.y +
                            ' a' + (ring * 40) + ',' + (ring * 40) + ' 0 0,1 ' + outerEnd.x + ',' + outerEnd.y + ' z',
                            resolvePattern(slice + ring, pattern));
            }
            else {
                var innerEnd = projectAngle(start, startAngle, (ring - 1) * 40);
                var innerStart = projectAngle(start, endAngle, (ring - 1) * 40);

                centerX = (outerStart.x + outerEnd.x + innerStart.x + innerEnd.x) / 4;
                centerY = (outerStart.y + outerEnd.y + innerStart.y + innerEnd.y) / 4;

                outerEnd.x -= outerStart.x; outerEnd.y -= outerStart.y;
                outerStart.x -= innerEnd.x; outerStart.y -= innerEnd.y;
                innerEnd.x -= innerStart.x; innerEnd.y -= innerStart.y;
                innerStart.x -= centerX; innerStart.y -= centerY;

                addElement('M' + centerX + ' ' + centerY + ' m' + innerStart.x + ' ' + innerStart.y +
                            ' a' + ((ring - 1) * 40) + ',' + ((ring - 1) * 40) + ' 0 0,0 ' + innerEnd.x + ',' + innerEnd.y +
                            ' l' + outerStart.x + ' ' + outerStart.y +
                            ' a' + (ring * 40) + ',' + (ring * 40) + ' 0 0,1 ' + outerEnd.x + ',' + outerEnd.y + ' z',
                            resolvePattern(slice + ring, pattern));
            }

            startAngle = endAngle;
        }
    }

    // The "a" sgment's parameters are: rx,ry / ? / largeArc,sweep / dx,dy);
    updateBounds();
    selectedChanged();
}

function projectAngle(start, angle, dist) {
    return { x: start.x + dist * Math.sin(angle), y: start.y - dist * Math.cos(angle) };
}

$('#shapeForm').submit(function () {
    remClass($('#render path.selected'), 'selected');

    $('#render path.cell[transform]').each(function () {
        applyOwnTransform(this);
    });

    $('#data').val($('#render').prop('outerHTML'));
});

$(function () {
    var data = $('#data').val();
    if (data.length > 0) {
        $('#render')
            .prop('outerHTML', data);

        $('#render path.cell')
            .click(cellClicked);
    }

    $('.popup').dialog({
        //height: 140,
        modal: true,
        autoOpen: false,
        buttons: {
            "Add": function () {
                if (submitPopup($(this)))
                    $(this).dialog("close");
            },
            Cancel: function () {
                $(this).dialog("close");
            }
        }
    });

    $('.toolbox .button').button();
    $('.toolbox .button.square').click(function () {
        addSingleElement('M60 60 m-20 -20 l40 0 l0 40 l-40 0 Z');
    });
    $('.toolbox .button.triangle').click(function () {
        addSingleElement('M60 60 m0 -20 l20 35 l-40 0 Z');
    });
    $('.toolbox .button.hex').click(function () {
        addSingleElement('M60 60 m0 -40 l35 20 l0 40 l-35 20 l-35 -20 l0 -40 Z');
    });
    $('.toolbox .button.squares').click(function () {
        $("#addSquares").dialog("open");
    });
    $('.toolbox .button.triangles').click(function () {
        $("#addTriangles").dialog("open");
    });
    $('.toolbox .button.hexes').click(function () {
        $("#addHexes").dialog("open");
    });
    $('.toolbox .button.circle').click(function () {
        $("#addCircle").dialog("open");
        // addSingleElement('M60 60 m-20 -0 a20,20 0 0,0 40,0 Z'); // a 75,75 0 0,0 75,75 means rx,ry / ? / largeArc,sweep / dx,dy);
    });

    /*
    $('.toolbox .button[quantity="set"]').click(function () {
    //showAddSetPopup($(this).attr('addtype'));
    });
    */
    $('#moveUp').button({
        icons: {
            primary: "ui-icon-arrowthick-1-n"
        },
        text: false
    }).click(function () {
        movePath(0, -2.5);
    });

    $('#moveDown').button({
        icons: {
            primary: "ui-icon-arrowthick-1-s"
        },
        text: false
    }).click(function () {
        movePath(0, 2.5);
    });

    $('#moveLeft').button({
        icons: {
            primary: "ui-icon-arrowthick-1-w"
        },
        text: false
    }).click(function () {
        movePath(-2.5, 0);
    });

    $('#moveRight').button({
        icons: {
            primary: "ui-icon-arrowthick-1-e"
        },
        text: false
    }).click(function () {
        movePath(2.5, 0);
    });

    $('#moveUpFast').button({
        icons: {
            primary: "ui-icon-arrowthickstop-1-n"
        },
        text: false
    }).click(function () {
        movePath(0, -40);
    });

    $('#moveDownFast').button({
        icons: {
            primary: "ui-icon-arrowthickstop-1-s"
        },
        text: false
    }).click(function () {
        movePath(0, 40);
    });

    $('#moveLeftFast').button({
        icons: {
            primary: "ui-icon-arrowthickstop-1-w"
        },
        text: false
    }).click(function () {
        movePath(-40, 0);
    });

    $('#moveRightFast').button({
        icons: {
            primary: "ui-icon-arrowthickstop-1-e"
        },
        text: false
    }).click(function () {
        movePath(40, 0);
    });

    $('#rotateCW').button({
        icons: {
            primary: "ui-icon-arrowrefresh-1-w"
        },
        text: false
    }).click(function () {
        rotatePath(15);
    });

    $('#rotateCCW').button({
        icons: {
            primary: "ui-icon ui-icon-arrowreturn-1-s"
        },
        text: false
    }).click(function () {
        rotatePath(-15);
    });

    $('#deleteCell').button({
        icons: {
            primary: "ui-icon-trash"
        },
        text: false
    }).click(function () {
        $('#render path.selected').remove();
        selectedChanged();
        updateBounds();
    });

    $('#copy').button({
        icons: {
            primary: "ui-icon-copy"
        },
        text: false
    }).click(function () {
        var source = $('#render path.selected');

        source.clone(true).each(function () {
            // allocate new IDs!
            $(this).attr('id', 'cell' + nextElem);
            nextElem++;
        }).appendTo('#render');

        remClass(source, 'selected');

        selectedChanged();
    });

    $('.itemProperties .color').buttonset().click(function () {
        var color = $('.itemProperties :radio:checked').attr('id');
        $('#render path.selected').attr('class', 'cell ' + color + ' selected');
    });

    $('#board').click(function (e) {
        if (!e.shiftKey && !e.ctrlKey)
            clearSelection();
    });

    $(document).keydown(function (e) {
        switch (e.which) {
            case 37:
                if (e.shiftKey)
                    $('#moveLeftFast').click();
                else if (e.ctrlKey)
                    $('#rotateCCW').click();
                else
                    $('#moveLeft').click();
                break;
            case 38:
                if (e.shiftKey)
                    $('#moveUpFast').click();
                else
                    $('#moveUp').click();
                break;
            case 39:
                if (e.shiftKey)
                    $('#moveRightFast').click();
                else if (e.ctrlKey)
                    $('#rotateCW').click();
                else
                    $('#moveRight').click();
                break;
            case 40:
                if (e.shiftKey)
                    $('#moveDownFast').click();
                else
                    $('#moveDown').click();
                break;
            case 46:
                $('#deleteCell').click();
                break;
            default:
                return;
        }
        e.preventDefault();
    });

    resizeBoard();
    $(window).resize(function () { resizeBoard(); });

    selectedChanged();
});