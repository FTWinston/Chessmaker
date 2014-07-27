﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Xml;

namespace ChessMaker.Models
{
    public abstract class DesignerModel
    {
        public string ThisPage { get; set; }
        public string PrevPage { get; set; }
        public string NextPage { get; set; }

        public string PrevPageTooltip { get; set; }
        public string NextPageTooltip { get; set; }

        public string VariantName { get; set; }
    }

    public class BoardShapeModel : DesignerModel
    {
        public BoardShapeModel(VariantVersion version, XmlDocument boardSvg, string linkData)
        {
            VariantName = version.Variant.Name;
            SvgData = boardSvg.OuterXml;
            LinkData = linkData;

            ThisPage = "Shape";
            NextPage = "Dirs 1";

            NextPageTooltip = "links between cells";
        }

        public string SvgData { get; set; }
        public string LinkData { get; set; }
    }

    public class BoardLinksModel : DesignerModel
    {
        public BoardLinksModel(VariantVersion version, XmlDocument boardSvg, string linkData)
        {
            VariantName = version.Variant.Name;
            SvgData = boardSvg.OuterXml;
            LinkData = linkData;

            PrevPage = "Shape";
            ThisPage = "Dirs 1";
            NextPage = "Dirs 2";

            PrevPageTooltip = "board layout";
            NextPageTooltip = "relative directions";
        }

        public string SvgData { get; set; }
        public string LinkData { get; set; }
    }

    public class RelativeDirectionsModel : DesignerModel
    {
        public RelativeDirectionsModel(VariantVersion version, string globalDirs, string relativeLinks)
        {
            VariantName = version.Variant.Name;
            GlobalDirectionsDiagram = globalDirs;
            RelativeDirections = relativeLinks;

            PrevPage = "Dirs 1";
            ThisPage = "Dirs 2";
            NextPage = "Pieces";

            PrevPageTooltip = "links between cells";
            NextPageTooltip = "piece definitions";
        }

        public string GlobalDirectionsDiagram { get; set; }
        public string RelativeDirections { get; set; }
    }
}