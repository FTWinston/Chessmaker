﻿function populate() {
    var relations = $('#relData').val().split(';');
    for (var i = 0; i < relations.length; i++) {
        var parts = relations[i].split(':');
        if (parts.length != 3)
            continue;

        var dir = parts[1];

        if ($('#main .groupbox[dir="' + dir.replace('"', '""') + '"]').length == 0)
            addRelativeDir(dir);
        populateLink(dir, parts[0], parts[2]);
    }
}

var absDirTable = null;
var absDirOptions = null;
function addRelativeDir(dir) {
    // add a group box for it, with a row for every absolute dir, and a dropdown-list for where this relative dir leads to from each abs dir

    if (absDirTable == null) {
        absDirTable = '<table><tr><th>from</th><th>to</th></tr>';
        
        absDirOptions = '<option value="">(not applicable)</option>';
        $('#indicator .marker').each(function () {
            var absdir = $(this).attr('dir');
            absDirOptions += '<option value="' + absdir + '">' + absdir + '</option>';
            absDirTable += '<tr class="row" dir="' + absdir + '"><td>' + absdir + '</td><td><select class="toAbsDirs"></select></td></tr>';
        });
        absDirTable += '</table>';
    }

    var groupbox = $('<div/>')
        .attr('class', 'groupbox ui-widget ui-corner-all')
        .attr('dir', dir)
        .html('<h4>' + dir + '</h4>' + absDirTable)
        .click(groupBoxClicked);

    groupbox.find('select.toAbsDirs').html(absDirOptions).change(function () {
        remClass($('#indicator .marker'), 'to');
        var dir = $(this).val();
        var marker = $('#indicator .marker[dir="' + dir.replace('"', '""') + '"]');
        addClass(marker, 'to');
    }).click(function (e) {
        e.stopPropagation();
    });

    groupbox.find('tr.row').hover(groupBoxRowHoverOver, groupBoxRowHoverOut);
    groupbox.appendTo($('#main'));
}

function groupBoxClicked() {
    var box = $(this);
    var noneSelected;

    if (box.hasClass('selected')) {
        box.removeClass('selected');
        noneSelected = true;
    }
    else {
        $('.groupbox').removeClass('selected');
        box.addClass('selected');
        noneSelected = false;
    }

    $('#renameDir, #deleteDir').button('option', 'disabled', noneSelected);
}

function groupBoxRowHoverOver() {
    $('#indicator .marker').attr('marker-end', 'url(#arrowhead)');

    var dir = $(this).attr('dir');
    var marker = $('#indicator .marker[dir="' + dir.replace('"', '""') + '"]');
    addClass(marker, 'from');
    marker.attr('marker-end', 'url(#arrowhead_from)');

    dir = $(this).find('select.toAbsDirs');
    dir.addClass('highlightTo');
    if (dir.length == 0)
        return;
    marker = $('#indicator .marker[dir="' + dir.val().replace('"', '""') + '"]');
    addClass(marker, 'to');
    marker.attr('marker-end', 'url(#arrowhead_to)');
}

function groupBoxRowHoverOut() {
    $('#indicator .marker').attr('marker-end', 'url(#arrowhead)');

    var dir = $(this).attr('dir');
    var marker = $('#indicator .marker[dir="' + dir.replace('"', '""') + '"]');
    remClass(marker, 'from');

    dir = $(this).find('select.toAbsDirs');
    dir.removeClass('highlightTo');
    if (dir.length == 0)
        return;
    marker = $('#indicator .marker[dir="' + dir.val().replace('"', '""') + '"]');
    remClass(marker, 'to');
}

function populateLink(dir, from, to) {
    // in the group box for "dir", find the "from" row and set the destination dropdown-list to the "to" value
    $('#main .groupbox[dir="' + dir.replace('"', '""') + '"] tr[dir="' + from.replace('"', '""') + '"] select.toAbsDirs').val(to);
}

$(function () {
    populate();

    $('#new').dialog({
        modal: true,
        autoOpen: false,
        buttons: {
            "OK": function () {
                var dir = $('#txtNew').val();
                if ($('#main .groupbox[dir="' + dir.replace('"', '""') + '"]').length != 0)
                    return; // one already exists with this name
                addRelativeDir(dir);
                $('#txtNew').val('');
                $(this).dialog("close");
            },
            Cancel: function () {
                $(this).dialog("close");
            }
        }
    });

    $('#rename').dialog({
        modal: true,
        autoOpen: false,
        buttons: {
            "OK": function () {
                var dir = $('#txtRename').val();

                var selected = $('#main .groupbox.selected');
                if (selected.attr('dir') != dir){ // only make changes if the name has changed

                    if ($('#main .groupbox[dir="' + dir.replace('"', '""') + '"]').length != 0)
                        return; // one already exists with this name

                    selected.attr('dir', dir).find('h4').html(dir);
                }

                $('#txtRename').val('');
                $(this).dialog("close");
            },
            Cancel: function () {
                $(this).dialog("close");
            }
        }
    });

    $('#addDir').button().click(function () {
        $('#new').dialog('open');
    });

    $('#renameDir').button().click(function () {
        $('#txtRename').val($('#main .groupbox.selected').attr('dir'));
        $('#rename').dialog('open');
    }).button('option', 'disabled', true);

    $('#deleteDir').button().click(function () {
        $('#main .groupbox.selected').remove();
        $('#renameDir, #deleteDir').button('option', 'disabled', true);
    }).button('option', 'disabled', true);

    $('.popup').keyup(function (e) {
        if (e.keyCode != 13)
            return;
        $(this).parent().find('button:nth-child(1)').click();
    });

    $('#relForm').submit(function () {
        var data = '';
        $('#main .groupbox').each(function () {
            var dir = $(this).attr('dir');

            $(this).find('table tr.row').each(function () {
                var fromDir = $(this).attr('dir');
                var toDir = $(this).find('select.toAbsDirs').val();
                if (toDir != '')
                    data += ';' + fromDir + ':' + dir + ':' + toDir;
            });
        });
        $('#relData').val(data);
    });
});