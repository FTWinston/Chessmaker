﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Xml;

namespace ChessMaker.Models
{
    public class BoardShapeModel
    {
        public BoardShapeModel(VariantVersion version, XmlDocument boardSvg, string linkData)
        {
            VariantName = version.Variant.Name;
            SvgData = boardSvg.OuterXml;
            LinkData = linkData;
        }

        public string VariantName { get; set; }
        public string SvgData { get; set; }
        public string LinkData { get; set; }
    }

    public class BoardLinksModel
    {
        public BoardLinksModel(VariantVersion version, XmlDocument boardSvg, string linkData)
        {
            VariantName = version.Variant.Name;
            SvgData = boardSvg.OuterXml;
            LinkData = linkData;
        }

        public string VariantName { get; set; }
        public string SvgData { get; set; }
        public string LinkData { get; set; }
    }
}