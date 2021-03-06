/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <oox/ppt/dgmimport.hxx>
#include <oox/drawingml/theme.hxx>
#include <drawingml/diagram/diagram.hxx>
#include <oox/dump/pptxdumper.hxx>

#include <com/sun/star/drawing/XShape.hpp>
#include <com/sun/star/drawing/XShapes.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::xml::sax;
using namespace oox::core;

namespace oox { namespace ppt {

QuickDiagrammingImport::QuickDiagrammingImport( const css::uno::Reference< css::uno::XComponentContext >& rxContext )
    : XmlFilterBase( rxContext )
{}

bool QuickDiagrammingImport::importDocument()
{
    /*  to activate the PPTX dumper, define the environment variable
        OOO_PPTXDUMPER and insert the full path to the file
        file:///<path-to-oox-module>/source/dump/pptxdumper.ini. */
    OOX_DUMP_FILE( ::oox::dump::pptx::Dumper );

    OUString aFragmentPath = getFragmentPathFromFirstTypeFromOfficeDoc( "diagramLayout" );

    Reference<drawing::XShapes> xParentShape(getParentShape(),
                                             UNO_QUERY_THROW);
    oox::drawingml::ShapePtr pShape(
        new oox::drawingml::Shape( "com.sun.star.drawing.DiagramShape" ) );
    drawingml::loadDiagram(pShape,
                           *this,
                           "",
                           aFragmentPath,
                           "",
                           "");
    oox::drawingml::ThemePtr pTheme(
        new oox::drawingml::Theme());
    basegfx::B2DHomMatrix aMatrix;
    pShape->addShape( *this,
                      pTheme.get(),
                      xParentShape,
                      aMatrix, pShape->getFillProperties() );

    return true;
}

bool QuickDiagrammingImport::exportDocument() throw()
{
    return false;
}

const ::oox::drawingml::Theme* QuickDiagrammingImport::getCurrentTheme() const
{
    // TODO
    return nullptr;
}

const oox::drawingml::table::TableStyleListPtr QuickDiagrammingImport::getTableStyles()
{
    return oox::drawingml::table::TableStyleListPtr();
}

oox::vml::Drawing* QuickDiagrammingImport::getVmlDrawing()
{
    return nullptr;
}

oox::drawingml::chart::ChartConverter* QuickDiagrammingImport::getChartConverter()
{
    return nullptr;
}

OUString QuickDiagrammingImport::getImplementationName()
{
    return OUString( "com.sun.star.comp.Impress.oox.QuickDiagrammingImport" );
}

::oox::ole::VbaProject* QuickDiagrammingImport::implCreateVbaProject() const
{
    return nullptr;
}

}}

extern "C" SAL_DLLPUBLIC_EXPORT uno::XInterface*
com_sun_star_comp_oox_ppt_QuickDiagrammingImport_get_implementation(
    uno::XComponentContext* pCtx, uno::Sequence<uno::Any> const& /*rSeq*/)
{
    return cppu::acquire(new oox::ppt::QuickDiagrammingImport(pCtx));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
