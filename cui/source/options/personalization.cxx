/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <config_folders.h>

#include "personalization.hxx"
#include "personasdochandler.hxx"

#include <comphelper/processfactory.hxx>
#include <officecfg/Office/Common.hxx>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <osl/file.hxx>
#include <rtl/bootstrap.hxx>
#include <rtl/strbuf.hxx>
#include <tools/urlobj.hxx>
#include <vcl/edit.hxx>
#include <vcl/fixed.hxx>
#include <vcl/fixedhyper.hxx>
#include <vcl/weld.hxx>
#include <vcl/lstbox.hxx>
#include <vcl/svapp.hxx>
#include <vcl/settings.hxx>
#include <vcl/graphicfilter.hxx>
#include <vcl/mnemonic.hxx>
#include <dialmgr.hxx>
#include <strings.hrc>
#include <personalization.hrc>

#include <com/sun/star/task/InteractionHandler.hpp>
#include <com/sun/star/ucb/SimpleFileAccess.hpp>
#include <com/sun/star/xml/sax/XParser.hpp>
#include <com/sun/star/xml/sax/Parser.hpp>
#include <ucbhelper/content.hxx>
#include <comphelper/simplefileaccessinteraction.hxx>

using namespace com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::ucb;
using namespace ::com::sun::star::beans;

SelectPersonaDialog::SelectPersonaDialog( vcl::Window *pParent )
    : ModalDialog( pParent, "SelectPersonaDialog", "cui/ui/select_persona_dialog.ui" )
{
    get( m_pSearchButton, "search_personas" );
    m_pSearchButton->SetClickHdl( LINK( this, SelectPersonaDialog, SearchPersonas ) );

    get( m_vSearchSuggestions[0], "suggestion1" );
    get( m_vSearchSuggestions[1], "suggestion2" );
    get( m_vSearchSuggestions[2], "suggestion3" );
    get( m_vSearchSuggestions[3], "suggestion4" );
    get( m_vSearchSuggestions[4], "suggestion5" );
    get( m_vSearchSuggestions[5], "suggestion6" );

    assert(SAL_N_ELEMENTS(RID_SVXSTR_PERSONA_CATEGORIES) >= CATEGORYCOUNT);
    for(sal_uInt32 i = 0; i < CATEGORYCOUNT; ++i)
    {
        m_vSearchSuggestions[i]->SetText(CuiResId(RID_SVXSTR_PERSONA_CATEGORIES[i]));
        m_vSearchSuggestions[i]->SetClickHdl( LINK( this, SelectPersonaDialog, SearchPersonas ) );
    }

    get( m_pEdit, "search_term" );

    get( m_pProgressLabel, "progress_label" );

    get( m_pOkButton, "ok" );
    m_pOkButton->SetClickHdl( LINK( this, SelectPersonaDialog, ActionOK ) );

    get( m_pCancelButton, "cancel" );
    m_pCancelButton->SetClickHdl( LINK( this, SelectPersonaDialog, ActionCancel ) );
    get( m_vResultList[0], "result1" );
    get( m_vResultList[1], "result2" );
    get( m_vResultList[2], "result3" );
    get( m_vResultList[3], "result4" );
    get( m_vResultList[4], "result5" );
    get( m_vResultList[5], "result6" );
    get( m_vResultList[6], "result7" );
    get( m_vResultList[7], "result8" );
    get( m_vResultList[8], "result9" );

    for (VclPtr<PushButton> & nIndex : m_vResultList)
    {
        nIndex->SetClickHdl( LINK( this, SelectPersonaDialog, SelectPersona ) );
        nIndex->Disable();
    }
}

SelectPersonaDialog::~SelectPersonaDialog()
{
    disposeOnce();
}

void SelectPersonaDialog::dispose()
{
    if (m_pSearchThread.is())
    {
        // Release the solar mutex, so the thread is not affected by the race
        // when it's after the m_bExecute check but before taking the solar
        // mutex.
        SolarMutexReleaser aReleaser;
        m_pSearchThread->join();
    }

    m_pEdit.clear();
    m_pSearchButton.clear();
    m_pProgressLabel.clear();
    for (VclPtr<PushButton>& vp : m_vResultList)
        vp.clear();
    for (VclPtr<PushButton>& vp : m_vSearchSuggestions)
        vp.clear();
    m_pOkButton.clear();
    m_pCancelButton.clear();
    ModalDialog::dispose();
}

OUString SelectPersonaDialog::GetSelectedPersona() const
{
    if( !m_aSelectedPersona.isEmpty( ) )
        return m_aSelectedPersona;

    return OUString();
}

IMPL_LINK( SelectPersonaDialog, SearchPersonas, Button*, pButton, void )
{
    /*
     * English category names should be used for search.
     * These strings should be in sync with the strings of
     * RID_SVXSTR_PERSONA_CATEGORIES in personalization.hrc
     */
    static const OUStringLiteral vSuggestionCategories[] =
        {"LibreOffice", "Abstract", "Color", "Music", "Nature", "Solid"};

    OUString searchTerm;
    if( m_pSearchThread.is() )
        m_pSearchThread->StopExecution();

    if( pButton ==  m_pSearchButton )
        searchTerm = m_pEdit->GetText();
    else
    {
        for ( sal_uInt32 i = 0; i < CATEGORYCOUNT; ++i)
        {
            if( pButton == m_vSearchSuggestions[i] )
            {
                // Use the category name in English as search term
                searchTerm = vSuggestionCategories[i];
                break;
            }
        }
    }

    if( searchTerm.isEmpty( ) )
        return;

    // 15 results so that invalid and duplicate search results whose names can't be retrieved can be skipped
    OUString rSearchURL = "https://services.addons.mozilla.org/en-US/firefox/api/1.5/search/" + searchTerm + "/9/15";

    if ( searchTerm.startsWith( "https://addons.mozilla.org/" ) )
    {
        searchTerm = "https://addons.mozilla.org/en-US/" + searchTerm.copy( searchTerm.indexOf( "firefox" ) );
        m_pSearchThread = new SearchAndParseThread( this, searchTerm, true );
    }
    else
        m_pSearchThread = new SearchAndParseThread( this, rSearchURL, false );

    m_pSearchThread->launch();
}

IMPL_LINK_NOARG( SelectPersonaDialog, ActionOK, Button*, void )
{
    OUString aSelectedPersona = GetSelectedPersona();

    if( !aSelectedPersona.isEmpty() )
    {
        m_pGetPersonaThread = new GetPersonaThread( this, aSelectedPersona );
        m_pGetPersonaThread->launch();
    }

    else
    {
        if ( m_pSearchThread.is() )
            m_pSearchThread->StopExecution();

        EndDialog( RET_OK );
    }
}

IMPL_LINK_NOARG( SelectPersonaDialog, ActionCancel, Button*, void )
{
    if( m_pSearchThread.is() )
        m_pSearchThread->StopExecution();
    if( m_pGetPersonaThread.is() )
        m_pGetPersonaThread->StopExecution();

    EndDialog();
}

IMPL_LINK( SelectPersonaDialog, SelectPersona, Button*, pButton, void )
{
    if( m_pSearchThread.is() )
        m_pSearchThread->StopExecution();
    if ( m_pGetPersonaThread.is() )
        return;

    for( sal_Int32 index = 0; index < MAX_RESULTS; index++ )
    {
        if( pButton == m_vResultList[index] )
        {
            if( !m_vPersonaSettings[index].isEmpty() )
            {
                m_aSelectedPersona = m_vPersonaSettings[index];
                // get the persona name from the setting variable to show in the progress.
                sal_Int32 nSlugIndex, nNameIndex;
                OUString aName, aProgress;

                // Skip the slug
                nSlugIndex = m_aSelectedPersona.indexOf( ';' );
                nNameIndex = m_aSelectedPersona.indexOf( ';', nSlugIndex );
                aName = m_aSelectedPersona.copy( nSlugIndex + 1, nNameIndex );
                aProgress = CuiResId(RID_SVXSTR_SELECTEDPERSONA) + aName;
                SetProgress( aProgress );
            }
            break;
        }
    }
}

void SelectPersonaDialog::SetAppliedPersonaSetting( OUString const & rPersonaSetting )
{
    m_aAppliedPersona = rPersonaSetting;
}

const OUString& SelectPersonaDialog::GetAppliedPersonaSetting() const
{
    return m_aAppliedPersona;
}

void SelectPersonaDialog::SetProgress( const OUString& rProgress )
{
    if(rProgress.isEmpty())
        m_pProgressLabel->Hide();
    else
    {
        SolarMutexGuard aGuard;
        m_pProgressLabel->Show();
        m_pProgressLabel->SetText( rProgress );
        setOptimalLayoutSize();
    }
}

void SelectPersonaDialog::SetImages( const Image& aImage, sal_Int32 nIndex )
{
    m_vResultList[nIndex]->Enable();
    m_vResultList[nIndex]->SetModeImage( aImage );
}

void SelectPersonaDialog::AddPersonaSetting( OUString const & rPersonaSetting )
{
    m_vPersonaSettings.push_back( rPersonaSetting );
}

void SelectPersonaDialog::ClearSearchResults()
{
    // for VCL to be able to destroy bitmaps
    SolarMutexGuard aGuard;
    m_vPersonaSettings.clear();
    m_aSelectedPersona.clear();
    for(VclPtr<PushButton> & nIndex : m_vResultList)
    {
        nIndex->Disable();
        nIndex->SetModeImage(Image());
    }
}

SvxPersonalizationTabPage::SvxPersonalizationTabPage( vcl::Window *pParent, const SfxItemSet &rSet )
    : SfxTabPage( pParent, "PersonalizationTabPage", "cui/ui/personalization_tab.ui", &rSet )
{
    // persona
    get( m_pNoPersona, "no_persona" );
    get( m_pDefaultPersona, "default_persona" );
    get( m_pAppliedThemeLabel, "applied_theme_link" );

    get( m_pOwnPersona, "own_persona" );
    m_pOwnPersona->SetClickHdl( LINK( this, SvxPersonalizationTabPage, ForceSelect ) );

    get( m_pSelectPersona, "select_persona" );
    m_pSelectPersona->SetClickHdl( LINK( this, SvxPersonalizationTabPage, SelectPersona ) );

    get( m_vDefaultPersonaImages[0], "default1" );
    m_vDefaultPersonaImages[0]->SetClickHdl( LINK( this, SvxPersonalizationTabPage, DefaultPersona ) );

    get( m_vDefaultPersonaImages[1], "default2" );
    m_vDefaultPersonaImages[1]->SetClickHdl( LINK( this, SvxPersonalizationTabPage, DefaultPersona ) );

    get( m_vDefaultPersonaImages[2], "default3" );
    m_vDefaultPersonaImages[2]->SetClickHdl( LINK( this, SvxPersonalizationTabPage, DefaultPersona ) );

    get( m_pPersonaList, "installed_personas" );
    m_pPersonaList->SetSelectHdl( LINK( this, SvxPersonalizationTabPage, SelectInstalledPersona ) );

    get( m_pExtensionPersonaPreview, "persona_preview" );

    get ( m_pExtensionLabel, "extensions_label" );

    CheckAppliedTheme();
    LoadDefaultImages();
    LoadExtensionThemes();
}

SvxPersonalizationTabPage::~SvxPersonalizationTabPage()
{
    disposeOnce();
}

void SvxPersonalizationTabPage::dispose()
{
    m_pNoPersona.clear();
    m_pDefaultPersona.clear();
    m_pOwnPersona.clear();
    m_pSelectPersona.clear();
    for (VclPtr<PushButton> & i : m_vDefaultPersonaImages)
        i.clear();
    m_pExtensionPersonaPreview.clear();
    m_pPersonaList.clear();
    m_pExtensionLabel.clear();
    m_pAppliedThemeLabel.clear();
    SfxTabPage::dispose();
}


VclPtr<SfxTabPage> SvxPersonalizationTabPage::Create( TabPageParent pParent, const SfxItemSet *rSet )
{
    return VclPtr<SvxPersonalizationTabPage>::Create( pParent.pParent, *rSet );
}

bool SvxPersonalizationTabPage::FillItemSet( SfxItemSet * )
{
    // persona
    OUString aPersona( "default" );
    if ( m_pNoPersona->IsChecked() )
        aPersona = "no";
    else if ( m_pOwnPersona->IsChecked() )
        aPersona = "own";

    bool bModified = false;
    uno::Reference< uno::XComponentContext > xContext( comphelper::getProcessComponentContext() );
    if ( xContext.is() &&
            ( aPersona != officecfg::Office::Common::Misc::Persona::get( xContext ) ||
              m_aPersonaSettings != officecfg::Office::Common::Misc::PersonaSettings::get( xContext ) ) )
    {
        bModified = true;
    }

    // write
    std::shared_ptr< comphelper::ConfigurationChanges > batch( comphelper::ConfigurationChanges::create() );
    if( aPersona == "no" )
        m_aPersonaSettings.clear();
    officecfg::Office::Common::Misc::Persona::set( aPersona, batch );
    officecfg::Office::Common::Misc::PersonaSettings::set( m_aPersonaSettings, batch );
    batch->commit();

    if ( bModified )
    {
        // broadcast the change
        DataChangedEvent aDataChanged( DataChangedEventType::SETTINGS, nullptr, AllSettingsFlags::STYLE );
        Application::NotifyAllWindows( aDataChanged );
    }

    return bModified;
}

void SvxPersonalizationTabPage::Reset( const SfxItemSet * )
{
    uno::Reference< uno::XComponentContext > xContext( comphelper::getProcessComponentContext() );

    // persona
    OUString aPersona( "default" );
    if ( xContext.is() )
    {
        aPersona = officecfg::Office::Common::Misc::Persona::get( xContext );
        m_aPersonaSettings = officecfg::Office::Common::Misc::PersonaSettings::get( xContext );
    }

    if ( aPersona == "no" )
        m_pNoPersona->Check();
    else if ( aPersona == "own" )
        m_pOwnPersona->Check();
    else
        m_pDefaultPersona->Check();
}

void SvxPersonalizationTabPage::SetPersonaSettings( const OUString& aPersonaSettings )
{
    m_aPersonaSettings = aPersonaSettings;
    ShowAppliedThemeLabel( m_aPersonaSettings );
    m_pOwnPersona->Check();
}

void SvxPersonalizationTabPage::CheckAppliedTheme()
{
    uno::Reference< uno::XComponentContext > xContext( comphelper::getProcessComponentContext() );
    OUString aPersona( "default" ), aPersonaSetting;
    if ( xContext.is())
    {
        aPersona = officecfg::Office::Common::Misc::Persona::get( xContext );
        aPersonaSetting = officecfg::Office::Common::Misc::PersonaSettings::get( xContext );
    }
    if(aPersona == "own")
        ShowAppliedThemeLabel(aPersonaSetting);
}

void SvxPersonalizationTabPage::ShowAppliedThemeLabel(const OUString& aPersonaSetting)
{
    OUString aSlug, aName;
    sal_Int32 nIndex = 0;

    aSlug = aPersonaSetting.getToken( 0, ';', nIndex );

    if ( nIndex > 0 )
        aName = "(" + aPersonaSetting.getToken( 0, ';', nIndex ) + ")";

    if ( !aName.isEmpty() )
    {
        m_pAppliedThemeLabel->SetText( aName );
        m_pAppliedThemeLabel->SetURL( "https://addons.mozilla.org/en-US/firefox/addon/" + aSlug + "/" );
        m_pAppliedThemeLabel->Show();
    }
    else
    {
        SAL_WARN("cui.options", "Applied persona doesn't have a name!");
    }
}

void SvxPersonalizationTabPage::LoadDefaultImages()
{
    // Load the pre saved personas

    OUString gallery
        = "$BRAND_BASE_DIR/" LIBO_SHARE_FOLDER "/gallery/personas/";
    rtl::Bootstrap::expandMacros( gallery );
    OUString aPersonasList = gallery + "personas_list.txt";
    SvFileStream aStream( aPersonasList, StreamMode::READ );
    GraphicFilter aFilter;
    Graphic aGraphic;
    sal_Int32 nIndex = 0;
    bool foundOne = false;

    while( aStream.IsOpen() && !aStream.eof() && nIndex < MAX_DEFAULT_PERSONAS )
    {
        OString aLine;
        OUString aPersonaSetting, aPreviewFile;
        sal_Int32 nPreviewIndex = 0;

        aStream.ReadLine( aLine );
        aPersonaSetting = OStringToOUString( aLine, RTL_TEXTENCODING_UTF8 );
        aPreviewFile = aPersonaSetting.getToken( 2, ';', nPreviewIndex );

        if (aPreviewFile.isEmpty())
            break;

        // There is no room for the preview file in the PersonaSettings currently
        aPersonaSetting = aPersonaSetting.replaceFirst( aPreviewFile + ";", "" );
        m_vDefaultPersonaSettings.push_back( aPersonaSetting );

        INetURLObject aURLObj( gallery + aPreviewFile );
        aFilter.ImportGraphic( aGraphic, aURLObj );
        BitmapEx aBmp = aGraphic.GetBitmapEx();
        m_vDefaultPersonaImages[nIndex]->SetModeImage( Image( aBmp ) );
        m_vDefaultPersonaImages[nIndex++]->Show();
        foundOne = true;
    }

    m_pDefaultPersona->Enable(foundOne);
}

void SvxPersonalizationTabPage::LoadExtensionThemes()
{
    // See if any extensions are used to install personas. If yes, load them.

    css::uno::Sequence<OUString> installedPersonas( officecfg::Office::Common::Misc::PersonasList::get()->getElementNames() );
    sal_Int32 nLength = installedPersonas.getLength();

    if( nLength == 0 )
        return;

    m_pPersonaList->Show();
    m_pExtensionLabel->Show();

    for( sal_Int32 nIndex = 0; nIndex < nLength; nIndex++ )
    {
        Reference< XPropertySet > xPropertySet( officecfg::Office::Common::Misc::PersonasList::get()->getByName( installedPersonas[nIndex] ), UNO_QUERY_THROW );
        OUString aPersonaSlug, aPersonaName, aPreviewFile, aHeaderFile, aFooterFile, aTextColor, aAccentColor, aPersonaSettings;

        Any aValue = xPropertySet->getPropertyValue( "Slug" );
        aValue >>= aPersonaSlug;

        aValue = xPropertySet->getPropertyValue( "Name" );
        aValue >>= aPersonaName;
        m_pPersonaList->InsertEntry( aPersonaName );

        aValue = xPropertySet->getPropertyValue( "Preview" );
        aValue >>= aPreviewFile;

        aValue = xPropertySet->getPropertyValue( "Header" );
        aValue >>= aHeaderFile;

        aValue = xPropertySet->getPropertyValue( "Footer" );
        aValue >>= aFooterFile;

        aValue = xPropertySet->getPropertyValue( "TextColor" );
        aValue >>= aTextColor;

        aValue = xPropertySet->getPropertyValue( "AccentColor" );
        aValue >>= aAccentColor;

        aPersonaSettings = aPersonaSlug + ";" + aPersonaName + ";" + aPreviewFile
                + ";" + aHeaderFile + ";" + aFooterFile + ";" + aTextColor + ";" + aAccentColor;
        rtl::Bootstrap::expandMacros( aPersonaSettings );
        m_vExtensionPersonaSettings.push_back( aPersonaSettings );
    }
}

IMPL_LINK_NOARG( SvxPersonalizationTabPage, SelectPersona, Button*, void )
{
    m_pOwnPersona->Check();
    ScopedVclPtrInstance< SelectPersonaDialog > aDialog(nullptr);

    if ( aDialog->Execute() == RET_OK )
    {
        OUString aPersonaSetting( aDialog->GetAppliedPersonaSetting() );
        if ( !aPersonaSetting.isEmpty() )
        {
            SetPersonaSettings( aPersonaSetting );
        }
    }
}

IMPL_LINK( SvxPersonalizationTabPage, ForceSelect, Button*, pButton, void )
{
    if ( pButton == m_pOwnPersona && m_aPersonaSettings.isEmpty() )
        SelectPersona( m_pSelectPersona );
}

IMPL_LINK( SvxPersonalizationTabPage, DefaultPersona, Button*, pButton, void )
{
    m_pDefaultPersona->Check();
    for( sal_Int32 nIndex = 0; nIndex < MAX_DEFAULT_PERSONAS; nIndex++ )
    {
        if( pButton == m_vDefaultPersonaImages[nIndex] )
            m_aPersonaSettings = m_vDefaultPersonaSettings[nIndex];
    }
}

IMPL_LINK_NOARG( SvxPersonalizationTabPage, SelectInstalledPersona, ListBox&, void)
{
    m_pOwnPersona->Check();

    // Get the details of the selected theme.
    m_pExtensionPersonaPreview->Show();
    sal_Int32 nSelectedPos = m_pPersonaList->GetSelectedEntryPos();
    OUString aSettings = m_vExtensionPersonaSettings[nSelectedPos];
    sal_Int32 nIndex = aSettings.indexOf( ';' );
    OUString aPreviewFile = aSettings.copy( 0, nIndex );
    m_aPersonaSettings = aSettings.copy( nIndex + 1 );

    // Show the preview file in the button.
    GraphicFilter aFilter;
    Graphic aGraphic;
    INetURLObject aURLObj( aPreviewFile );
    aFilter.ImportGraphic( aGraphic, aURLObj );
    BitmapEx aBmp = aGraphic.GetBitmapEx();
    m_pExtensionPersonaPreview->SetModeImage( Image( aBmp ) );
}

/// Find the value on the Persona page, and convert it to a usable form.
static OUString searchValue( const OString &rBuffer, sal_Int32 from, const OString &rIdentifier )
{
    sal_Int32 where = rBuffer.indexOf( rIdentifier, from );
    if ( where < 0 )
        return OUString();

    where += rIdentifier.getLength();

    sal_Int32 end = rBuffer.indexOf( "\"", where );
    if ( end < 0 )
        return OUString();

    OString aOString( rBuffer.copy( where, end - where ) );
    OUString aString( aOString.getStr(),  aOString.getLength(), RTL_TEXTENCODING_UTF8, OSTRING_TO_OUSTRING_CVTFLAGS );

    return aString.replaceAll( "\\u002F", "/" );
}

/// Parse the Persona web page, and find where to get the bitmaps + the color values.
static bool parsePersonaInfo( const OString &rBufferArg, OUString *pHeaderURL, OUString *pFooterURL,
                              OUString *pTextColor, OUString *pAccentColor, OUString *pPreviewURL,
                              OUString *pName, OUString *pSlug )
{
    // tdf#115417: buffer retrieved from html response can contain &quot; or &#34;
    // let's replace the whole buffer with last one so we can treat it easily
    OString rBuffer = rBufferArg.replaceAll(OString("&quot;"), OString("&#34;"));
    // it is the first attribute that contains "persona="
    sal_Int32 persona = rBuffer.indexOf( "\"type\":\"persona\"" );
    if ( persona < 0 )
        return false;

    // now search inside
    *pHeaderURL = searchValue( rBuffer, persona, "\"headerURL\":\"" );
    if ( pHeaderURL->isEmpty() )
        return false;

    *pFooterURL = searchValue( rBuffer, persona, "\"footerURL\":\"" );
    if ( pFooterURL->isEmpty() )
        return false;

    *pTextColor = searchValue( rBuffer, persona, "\"textcolor\":\"" );
    if ( pTextColor->isEmpty() )
        return false;

    *pAccentColor = searchValue( rBuffer, persona, "\"accentcolor\":\"" );
    if ( pAccentColor->isEmpty() )
        return false;

    *pPreviewURL = searchValue( rBuffer, persona, "\"previewURL\":\"" );
    if ( pPreviewURL->isEmpty() )
        return false;

    *pName = searchValue( rBuffer, persona, "\"name\":\"" );
    if ( pName->isEmpty() )
        return false;

    *pSlug = searchValue( rBuffer, persona, "\"bySlug\":{\"" );

    return !pSlug->isEmpty();
}

SearchAndParseThread::SearchAndParseThread( SelectPersonaDialog* pDialog,
                          const OUString& rURL, bool bDirectURL ) :
            Thread( "cuiPersonasSearchThread" ),
            m_pPersonaDialog( pDialog ),
            m_aURL( rURL ),
            m_bExecute( true ),
            m_bDirectURL( bDirectURL )
{
}

SearchAndParseThread::~SearchAndParseThread()
{
}

namespace {

bool getPreviewFile( const OUString& rURL, OUString *pPreviewFile, OUString *pPersonaSetting )
{
    uno::Reference< ucb::XSimpleFileAccess3 > xFileAccess( ucb::SimpleFileAccess::create( comphelper::getProcessComponentContext() ), uno::UNO_QUERY );
    if ( !xFileAccess.is() )
        return false;

    Reference<XComponentContext> xContext( ::comphelper::getProcessComponentContext() );
    uno::Reference< io::XInputStream > xStream;
    try {
        css:: uno::Reference< task::XInteractionHandler > xIH(
            css::task::InteractionHandler::createWithParent( xContext, nullptr ) );

        xFileAccess->setInteractionHandler( new comphelper::SimpleFileAccessInteraction( xIH ) );
        xStream = xFileAccess->openFileRead( rURL );

        if( !xStream.is() )
            return false;
    }
    catch (...)
    {
        return false;
    }

    // read the persona specification
    // NOTE: Parsing for real is an overkill here; and worse - I tried, and
    // the HTML the site provides is not 100% valid ;-)
    const sal_Int32 BUF_LEN = 8000;
    uno::Sequence< sal_Int8 > buffer( BUF_LEN );
    OStringBuffer aBuffer( 64000 );

    sal_Int32 nRead = 0;
    while ( ( nRead = xStream->readBytes( buffer, BUF_LEN ) ) == BUF_LEN )
        aBuffer.append( reinterpret_cast< const char* >( buffer.getConstArray() ), nRead );

    if ( nRead > 0 )
        aBuffer.append( reinterpret_cast< const char* >( buffer.getConstArray() ), nRead );

    xStream->closeInput();

    // get the important bits of info
    OUString aHeaderURL, aFooterURL, aTextColor, aAccentColor, aPreviewURL, aName, aSlug;

    if ( !parsePersonaInfo( aBuffer.makeStringAndClear(), &aHeaderURL, &aFooterURL, &aTextColor, &aAccentColor, &aPreviewURL, &aName, &aSlug ) )
        return false;

    // copy the images to the user's gallery
    OUString gallery = "${$BRAND_BASE_DIR/" LIBO_ETC_FOLDER "/" SAL_CONFIGFILE( "bootstrap") "::UserInstallation}";
    rtl::Bootstrap::expandMacros( gallery );
    gallery += "/user/gallery/personas/" + aSlug + "/";

    OUString aPreviewFile( INetURLObject( aPreviewURL ).getName() );

    try {
        osl::Directory::createPath( gallery );

        if ( !xFileAccess->exists( gallery + aPreviewFile ) )
            xFileAccess->copy( aPreviewURL, gallery + aPreviewFile );
    }
    catch ( const uno::Exception & )
    {
        return false;
    }
    *pPreviewFile = gallery + aPreviewFile;
    *pPersonaSetting = aSlug + ";" + aName + ";" + aHeaderURL + ";" + aFooterURL + ";" + aTextColor + ";" + aAccentColor;
    return true;
}

}

void SearchAndParseThread::execute()
{
    m_pPersonaDialog->ClearSearchResults();
    OUString sProgress( CuiResId( RID_SVXSTR_SEARCHING ) ), sError;
    m_pPersonaDialog->SetProgress( sProgress );

    PersonasDocHandler* pHandler = new PersonasDocHandler();
    Reference<XComponentContext> xContext( ::comphelper::getProcessComponentContext() );
    Reference< xml::sax::XParser > xParser = xml::sax::Parser::create(xContext);
    Reference< xml::sax::XDocumentHandler > xDocHandler = pHandler;
    uno::Reference< ucb::XSimpleFileAccess3 > xFileAccess( ucb::SimpleFileAccess::create( comphelper::getProcessComponentContext() ), uno::UNO_QUERY );
    uno::Reference< io::XInputStream > xStream;
    xParser->setDocumentHandler( xDocHandler );

    if( !m_bDirectURL )
    {
        if ( !xFileAccess.is() )
            return;

        try {
            css:: uno::Reference< task::XInteractionHandler > xIH(
                        css::task::InteractionHandler::createWithParent( xContext, nullptr ) );

            xFileAccess->setInteractionHandler( new comphelper::SimpleFileAccessInteraction( xIH ) );

            xStream = xFileAccess->openFileRead( m_aURL );
            if( !xStream.is() )
            {
                // in case of a returned CommandFailedException
                // SimpleFileAccess serves it, returning an empty stream
                SolarMutexGuard aGuard;
                sError = CuiResId(RID_SVXSTR_SEARCHERROR);
                sError = sError.replaceAll("%1", m_aURL);
                m_pPersonaDialog->SetProgress( OUString() );

                std::unique_ptr<weld::MessageDialog> xBox(Application::CreateMessageDialog(nullptr,
                                                                                           VclMessageType::Error, VclButtonsType::Ok,
                                                                                           sError));
                xBox->run();
                return;
            }
        }
        catch (...)
        {
            // a catch all clause, in case the exception is not
            // served elsewhere
            SolarMutexGuard aGuard;
            sError = CuiResId(RID_SVXSTR_SEARCHERROR);
            sError = sError.replaceAll("%1", m_aURL);
            m_pPersonaDialog->SetProgress( OUString() );
            std::unique_ptr<weld::MessageDialog> xBox(Application::CreateMessageDialog(nullptr,
                                                                                       VclMessageType::Error, VclButtonsType::Ok,
                                                                                       sError));
            xBox->run();
            return;
        }

        xml::sax::InputSource aParserInput;
        aParserInput.aInputStream = xStream;
        xParser->parseStream( aParserInput );

        if( !pHandler->hasResults() )
        {
            sProgress = CuiResId( RID_SVXSTR_NORESULTS );
            m_pPersonaDialog->SetProgress( sProgress );
            return;
        }
    }

    std::vector<OUString> vLearnmoreURLs;
    sal_Int32 nIndex = 0;
    GraphicFilter aFilter;
    Graphic aGraphic;

    if( !m_bDirectURL )
        vLearnmoreURLs = pHandler->getLearnmoreURLs();
    else
        vLearnmoreURLs.push_back( m_aURL );

    for (auto const& learnMoreUrl : vLearnmoreURLs)
    {
        OUString sPreviewFile, aPersonaSetting;
        bool bResult = getPreviewFile( learnMoreUrl, &sPreviewFile, &aPersonaSetting );
        // parsing is buggy at times, as HTML is not proper. Skip it.
        if(aPersonaSetting.isEmpty() || !bResult)
        {
            if( m_bDirectURL )
            {
                SolarMutexGuard aGuard;
                sError = CuiResId(RID_SVXSTR_SEARCHERROR);
                sError = sError.replaceAll("%1", m_aURL);
                m_pPersonaDialog->SetProgress( OUString() );
                std::unique_ptr<weld::MessageDialog> xBox(Application::CreateMessageDialog(nullptr,
                                                                                           VclMessageType::Error, VclButtonsType::Ok,
                                                                                           sError));
                xBox->run();
                return;
            }
            continue;
        }
        INetURLObject aURLObj( sPreviewFile );

        // Stop the thread if requested -- before taking the solar mutex.
        if( !m_bExecute )
            return;

        // for VCL to be able to create bitmaps / do visual changes in the thread
        SolarMutexGuard aGuard;
        aFilter.ImportGraphic( aGraphic, aURLObj );
        BitmapEx aBmp = aGraphic.GetBitmapEx();

        m_pPersonaDialog->SetImages( Image( aBmp ), nIndex++ );
        m_pPersonaDialog->setOptimalLayoutSize();
        m_pPersonaDialog->AddPersonaSetting( aPersonaSetting );
        if (nIndex >= MAX_RESULTS)
            break;
    }

    if( !m_bExecute )
        return;

    SolarMutexGuard aGuard;
    sProgress.clear();
    m_pPersonaDialog->SetProgress( sProgress );
    m_pPersonaDialog->setOptimalLayoutSize();
}

GetPersonaThread::GetPersonaThread( SelectPersonaDialog* pDialog,
                          const OUString& rSelectedPersona ) :
            Thread( "cuiPersonasGetPersonaThread" ),
            m_pPersonaDialog( pDialog ),
            m_aSelectedPersona( rSelectedPersona ),
            m_bExecute( true )
{
}

GetPersonaThread::~GetPersonaThread()
{
    //TODO: Clean-up
}

void GetPersonaThread::execute()
{
    OUString sProgress( CuiResId( RID_SVXSTR_APPLYPERSONA ) ), sError;
    m_pPersonaDialog->SetProgress( sProgress );

    uno::Reference< ucb::XSimpleFileAccess3 > xFileAccess( ucb::SimpleFileAccess::create( comphelper::getProcessComponentContext() ), uno::UNO_QUERY );
    if ( !xFileAccess.is() )
        return;

    OUString aSlug, aName, aHeaderURL, aFooterURL, aTextColor, aAccentColor;
    OUString aPersonaSetting;

    // get the required fields from m_aSelectedPersona
    sal_Int32 nIndex = 0;

    aSlug = m_aSelectedPersona.getToken(0, ';', nIndex);
    aName = m_aSelectedPersona.getToken(0, ';', nIndex);
    aHeaderURL = m_aSelectedPersona.getToken(0, ';', nIndex);
    aFooterURL = m_aSelectedPersona.getToken(0, ';', nIndex);
    aTextColor = m_aSelectedPersona.getToken(0, ';', nIndex);
    aAccentColor = m_aSelectedPersona.getToken(0, ';', nIndex);

    // copy the images to the user's gallery
    OUString gallery = "${$BRAND_BASE_DIR/" LIBO_ETC_FOLDER "/" SAL_CONFIGFILE( "bootstrap") "::UserInstallation}";
    rtl::Bootstrap::expandMacros( gallery );
    gallery += "/user/gallery/personas/";

    OUString aHeaderFile( aSlug + "/" + INetURLObject( aHeaderURL ).getName() );
    OUString aFooterFile( aSlug + "/" + INetURLObject( aFooterURL ).getName() );

    try {
        osl::Directory::createPath( gallery );

        if ( !xFileAccess->exists(gallery + aHeaderFile) )
            xFileAccess->copy( aHeaderURL, gallery + aHeaderFile );

        if ( !xFileAccess->exists(gallery + aFooterFile) )
            xFileAccess->copy( aFooterURL, gallery + aFooterFile );
    }
    catch ( const uno::Exception & )
    {
        SolarMutexGuard aGuard;
        sError = CuiResId( RID_SVXSTR_SEARCHERROR );
        sError = sError.replaceAll("%1", m_aSelectedPersona);
        m_pPersonaDialog->SetProgress( OUString() );
        std::unique_ptr<weld::MessageDialog> xBox(Application::CreateMessageDialog(nullptr,
                                                                                   VclMessageType::Error, VclButtonsType::Ok,
                                                                                   sError));
        xBox->run();
        return;
    }

    if( !m_bExecute )
        return;

    SolarMutexGuard aGuard;

    aPersonaSetting = aSlug + ";" + aName + ";" + aHeaderFile + ";" + aFooterFile
            + ";" + aTextColor + ";" + aAccentColor;

    m_pPersonaDialog->SetAppliedPersonaSetting( aPersonaSetting );
    m_pPersonaDialog->EndDialog( RET_OK );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
