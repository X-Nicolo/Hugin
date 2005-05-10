// -*- c-basic-offset: 4 -*-

/** @file OptimizePanel.cpp
 *
 *  @brief implementation of OptimizePanel
 *
 *  @author Pablo d'Angelo <pablo.dangelo@web.de>
 *
 *  $Id$
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>
#include "panoinc_WX.h"

#include "panoinc.h"

#include "PT/PTOptimise.h"
#include "common/stl_utils.h"

#include "hugin/OptimizePanel.h"
#include "hugin/CommandHistory.h"
#include "hugin/RunOptimizerFrame.h"
#include "hugin/MainFrame.h"
#include "hugin/MyProgressDialog.h"
#include "hugin/config_defaults.h"

using namespace std;
using namespace PT;
using namespace utils;

//============================================================================
//============================================================================
//============================================================================

BEGIN_EVENT_TABLE(OptimizePanel, wxPanel)
    EVT_CLOSE(OptimizePanel::OnClose)
    EVT_BUTTON(XRCID("optimize_frame_optimize"), OptimizePanel::OnOptimizeButton)
    EVT_BUTTON(XRCID("opt_yaw_select"), OptimizePanel::OnSelYaw)
    EVT_BUTTON(XRCID("opt_yaw_clear"), OptimizePanel::OnDelYaw)
    EVT_BUTTON(XRCID("opt_pitch_select"), OptimizePanel::OnSelPitch)
    EVT_BUTTON(XRCID("opt_pitch_clear"), OptimizePanel::OnDelPitch)
//    EVT_BUTTON(XRCID("opt_pitch_equalize"), OptimizePanel::OnEqPitch)
    EVT_BUTTON(XRCID("opt_roll_select"), OptimizePanel::OnSelRoll)
    EVT_BUTTON(XRCID("opt_roll_clear"), OptimizePanel::OnDelRoll)
    EVT_CHOICE(XRCID("optimize_panel_mode"), OptimizePanel::OnChangeMode)
//    EVT_BUTTON(XRCID("opt_roll_equalize"), OptimizePanel::OnEqRoll)
END_EVENT_TABLE()

// local optimize definition. need to be in sync with the xrc file
enum OptimizeMode { OPT_PAIRWISE=0, OPT_YRP, OPT_YRP_V,
                    OPT_YRP_B, OPT_YRP_BV, OPT_ALL, OPT_CUSTOM,
                    OPT_END_MARKER};


OptimizePanel::OptimizePanel(wxWindow * parent, PT::Panorama * pano)
    : m_pano(pano)
{
    DEBUG_TRACE("");
    wxXmlResource::Get()->LoadPanel(this, parent, wxT("optimize_panel"));

    m_yaw_list = XRCCTRL(*this, "optimizer_yaw_list", wxCheckListBox);
    m_pitch_list = XRCCTRL(*this, "optimizer_pitch_list", wxCheckListBox);
    m_roll_list = XRCCTRL(*this, "optimizer_roll_list", wxCheckListBox);

    m_v_list = XRCCTRL(*this, "optimizer_v_list", wxCheckListBox);
    m_a_list = XRCCTRL(*this, "optimizer_a_list", wxCheckListBox);
    m_b_list = XRCCTRL(*this, "optimizer_b_list", wxCheckListBox);
    m_c_list = XRCCTRL(*this, "optimizer_c_list", wxCheckListBox);
    m_d_list = XRCCTRL(*this, "optimizer_d_list", wxCheckListBox);
    m_e_list = XRCCTRL(*this, "optimizer_e_list", wxCheckListBox);

    m_edit_cb = XRCCTRL(*this, "optimizer_edit_script", wxCheckBox);
    DEBUG_ASSERT(m_edit_cb);
    m_mode_cb = XRCCTRL(*this, "optimize_panel_mode", wxChoice);
    DEBUG_ASSERT(m_mode_cb);

#ifdef USE_WX26x
    m_opt_ctrls = XRCCTRL(*this, "optimize_controls_panel", wxScrolledWindow);
    DEBUG_ASSERT(m_opt_ctrls);
    m_opt_ctrls->SetSizeHints(20, 20);
    m_opt_ctrls->FitInside();
    m_opt_ctrls->SetScrollRate(10, 10);
#endif

//    wxConfigBase * config = wxConfigBase::Get();
//    long w = config->Read(wxT("/OptimizerPanel/width"),-1);
//    long h = config->Read(wxT("/OptimizerPanel/height"),-1);
//    if (w != -1) {
//        SetClientSize(w,h);
//    }

    wxCommandEvent dummy;
    dummy.m_commandInt = m_mode_cb->GetSelection();
    OnChangeMode(dummy);

    // observe the panorama
    m_pano->addObserver(this);
}

OptimizePanel::~OptimizePanel()
{
    DEBUG_TRACE("dtor, writing config");
    wxSize sz = GetClientSize();
    wxConfigBase * config = wxConfigBase::Get();
    config->Write(wxT("/OptimizerPanel/width"),sz.GetWidth());
    config->Write(wxT("/OptimizerPanel/height"),sz.GetHeight());
    m_pano->removeObserver(this);
    DEBUG_TRACE("dtor end");
}

void OptimizePanel::OnOptimizeButton(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    // run optimizer
    // take the OptimizeVector from somewhere...

    OptimizeVector optvars = getOptimizeVector();
    PanoramaOptions opts = m_pano->getOptions();

	UIntSet imgs;
	if (wxConfigBase::Get()->Read(wxT("/General/UseOnlySelectedImages"),
		                          HUGIN_USE_SELECTED_IMAGES))
	{
		// use only selected images.
		imgs = m_pano->getActiveImages();
	} else {
		for (unsigned int i = 0 ; i < m_pano->getNrOfImages(); i++) {
			imgs.insert(i);
		}
	}

    runOptimizer(optvars, opts, imgs);
}


void OptimizePanel::SetCheckMark(wxCheckListBox * l, int check)
{
    int n = l->GetCount();
    for (int i=0; i < n; i++) {
        l->Check(i, check != 0);
    }
}


OptimizeVector OptimizePanel::getOptimizeVector()
{

    int nrLI = m_yaw_list->GetCount();
    assert(nrLI >=0);
    unsigned int nr = (unsigned int) nrLI;
    unsigned int nImages = m_pano->getNrOfImages();
    assert(nr == nImages);

    OptimizeVector optvars;

    for (unsigned int i=0; i < nImages; i++) {

        set<string> imgopt;
        // lens variables
        unsigned int lensNr = m_pano->getImage(i).getLensNr();

        if (m_v_list->IsChecked(lensNr)) {
            imgopt.insert("v");
        }
        if (m_a_list->IsChecked(lensNr)) {
            imgopt.insert("a");
        }
        if (m_b_list->IsChecked(lensNr)) {
            imgopt.insert("b");
        }
        if (m_c_list->IsChecked(lensNr)) {
            imgopt.insert("c");
        }
        if (m_d_list->IsChecked(lensNr)) {
            imgopt.insert("d");
        }
        if (m_e_list->IsChecked(lensNr)) {
            imgopt.insert("e");
        }

        // image variables
        if (m_roll_list->IsChecked(i)) {
            imgopt.insert("r");
        }
        if (m_pitch_list->IsChecked(i)) {
            imgopt.insert("p");
        }
        if (m_yaw_list->IsChecked(i)) {
            imgopt.insert("y");
        }

        optvars.push_back(imgopt);
    }

    return optvars;
}

void OptimizePanel::panoramaChanged(PT::Panorama & pano)
{

    // update accordingly to the choosen mode
//    wxCommandEvent dummy;
//    OnChangeMode(dummy);
}

void OptimizePanel::panoramaImagesChanged(PT::Panorama &pano,
                                          const PT::UIntSet & imgNr)
{
    DEBUG_TRACE("nr of changed images: " << imgNr.size());

    // update lens values
    int nrLensList = m_v_list->GetCount();
    assert(nrLensList >=0);
    unsigned int nr = (unsigned int) nrLensList;
    unsigned int nLens = m_pano->getNrOfLenses();
    while (nr < nLens) {
        // add checkboxes.
        m_v_list->Append(wxString::Format(wxT("%d"),nr));
        m_a_list->Append(wxString::Format(wxT("%d"),nr));
        m_b_list->Append(wxString::Format(wxT("%d"),nr));
        m_c_list->Append(wxString::Format(wxT("%d"),nr));
        m_d_list->Append(wxString::Format(wxT("%d"),nr));
        m_e_list->Append(wxString::Format(wxT("%d"),nr));
        nr++;
    }

    while (nr > nLens) {
        if (nr == 0)
            break;
        m_v_list->Delete(nr-1);
        m_a_list->Delete(nr-1);
        m_b_list->Delete(nr-1);
        m_c_list->Delete(nr-1);
        m_d_list->Delete(nr-1);
        m_e_list->Delete(nr-1);
        nr--;
    }

    // add/remove items
    int nrLI = m_yaw_list->GetCount();
    assert(nrLI >=0);
    nr = (unsigned int) nrLI;
    unsigned int nImages = m_pano->getNrOfImages();
    while (nr < nImages) {
        // add checkboxes.
        m_yaw_list->Append(wxString::Format(wxT("%d"),nr));
        m_pitch_list->Append(wxString::Format(wxT("%d"),nr));
        m_roll_list->Append(wxString::Format(wxT("%d"),nr));
        nr++;
    }

    while (nr > nImages) {
        if (nr == 0)
            break;
        m_yaw_list->Delete(nr-1);
        m_pitch_list->Delete(nr-1);
        m_roll_list->Delete(nr-1);
        nr--;
    }

    // display values of the variables.
    UIntSet::const_iterator it;
    for (it = imgNr.begin(); it != imgNr.end(); it++) {
        DEBUG_DEBUG("setting checkmarks for image " << *it)
        const VariableMap & vars = pano.getImageVariables(*it);
        // keep selections
        bool sel = m_yaw_list->IsChecked(*it);
        m_yaw_list->SetString(*it, wxString::Format(wxT("%d (%.3f)"),
                                *it, const_map_get(vars,"y").getValue()));
        m_yaw_list->Check(*it,sel);

        sel = m_pitch_list->IsChecked(*it);
        m_pitch_list->SetString(*it, wxString::Format(wxT("%d (%.3f)"),
                                *it, const_map_get(vars,"p").getValue()));
        m_pitch_list->Check(*it,sel);

        sel = m_roll_list->IsChecked(*it);
        m_roll_list->SetString(*it, wxString::Format(wxT("%d (%.3f)"),
                                *it, const_map_get(vars,"r").getValue()));
        m_roll_list->Check(*it,sel);
    }

    // display lens values if they are linked
    for (unsigned int i=0; i < nLens; i++) {
        const Lens & lens = pano.getLens(i);
        const LensVariable & v = const_map_get(lens.variables,"v");
        bool sel = m_v_list->IsChecked(i);
        if (v.isLinked()) {
            m_v_list->SetString(i,wxString::Format(wxT("%d (%.2f)"),i, v.getValue()));
        } else {
            m_v_list->SetString(i,wxString::Format(wxT("%d"),nr));
        }
        m_v_list->Check(i,sel);

        sel = m_a_list->IsChecked(i);
        const LensVariable & a = const_map_get(lens.variables,"a");
        if (a.isLinked()) {
            m_a_list->SetString(i,wxString::Format(wxT("%d (%.3f)"),i, a.getValue()));
        } else {
            m_a_list->SetString(i,wxString::Format(wxT("%d"),nr));
        }
        m_a_list->Check(i,sel);

        sel = m_b_list->IsChecked(i);
        const LensVariable & b = const_map_get(lens.variables,"b");
        if (b.isLinked()) {
            m_b_list->SetString(i,wxString::Format(wxT("%d (%.3f)"),i, b.getValue()));
        } else {
            m_b_list->SetString(i,wxString::Format(wxT("%d"),nr));
        }
        m_b_list->Check(i,sel);

        sel = m_c_list->IsChecked(i);
        const LensVariable & c = const_map_get(lens.variables,"c");
        if (c.isLinked()) {
            m_c_list->SetString(i,wxString::Format(wxT("%d (%.3f)"),i, c.getValue()));
        } else {
            m_c_list->SetString(i,wxString::Format(wxT("%d"),nr));
        }
        m_c_list->Check(i,sel);

        sel = m_d_list->IsChecked(i);
        const LensVariable & d = const_map_get(lens.variables,"d");
        if (d.isLinked()) {
            m_d_list->SetString(i,wxString::Format(wxT("%d (%.1f)"),i, d.getValue()));
        } else {
            m_d_list->SetString(i,wxString::Format(wxT("%d"),nr));
        }
        m_d_list->Check(i,sel);

        sel = m_e_list->IsChecked(i);
        const LensVariable & e = const_map_get(lens.variables,"e");
        if (e.isLinked()) {
            m_e_list->SetString(i,wxString::Format(wxT("%d (%.1f)"),i, e.getValue()));
        } else {
            m_e_list->SetString(i,wxString::Format(wxT("%d"),nr));
        }
        m_e_list->Check(i,sel);
    }

    // update automatic checkmarks
    wxCommandEvent dummy;
    dummy.m_commandInt = m_mode_cb->GetSelection();
    OnChangeMode(dummy);

}

void OptimizePanel::setOptimizeVector(const OptimizeVector & optvec)
{
    DEBUG_ASSERT((int)optvec.size() == m_yaw_list->GetCount());

    for (int i=0; i < (int) m_pano->getNrOfLenses(); i++) {
	m_v_list->Check(i,false);
	m_a_list->Check(i,false);
	m_b_list->Check(i,false);
	m_c_list->Check(i,false);
	m_d_list->Check(i,false);
	m_e_list->Check(i,false);
    }

    unsigned int nImages = optvec.size();
    for (unsigned int i=0; i < nImages; i++) {
	m_yaw_list->Check(i,false);
	m_pitch_list->Check(i,false);
	m_roll_list->Check(i,false);
        unsigned int lensNr = m_pano->getImage(i).getLensNr();

        for(set<string>::const_iterator it = optvec[i].begin();
	    it != optvec[i].end(); ++it)
	{
	    if (*it == "y") {
	        m_yaw_list->Check(i);
	    }
	    if (*it == "p") {
	        m_pitch_list->Check(i);
	    }
	    if (*it == "r") {
	        m_roll_list->Check(i);
	    }

	    if (*it == "v") {
	        m_v_list->Check(lensNr);
	    }
	    if (*it == "a") {
	        m_a_list->Check(lensNr);
	    }
	    if (*it == "b") {
	        m_b_list->Check(lensNr);
	    }
	    if (*it == "c") {
	        m_c_list->Check(lensNr);
	    }
	    if (*it == "d") {
	        m_d_list->Check(lensNr);
	    }
	    if (*it == "e") {
	        m_e_list->Check(lensNr);
	    }
	}
    }
}

void OptimizePanel::runOptimizer(const OptimizeVector & optvars, 
								 const PanoramaOptions & options,
								 const UIntSet & imgs)
{
    DEBUG_TRACE("");
    if (m_pano->getNrOfImages() == 0) {
        // nothing to optimize
		wxMessageBox(_("No images - Nothing to do"),_("Warning"), wxOK | wxICON_EXCLAMATION);
        return;
    }

    // open window that shows a status dialog, and allows to
    // apply the results
    int mode = m_mode_cb->GetSelection();
    if (mode == OPT_PAIRWISE) {
        OptProgressDialog prog(this);
        std::set<std::string> optvars2;
        optvars2.insert("y");
        optvars2.insert("p");
        optvars2.insert("r");
        CPVector cps = m_pano->getCtrlPoints();
        VariableMapVector vars = PTools::autoOptimise(*m_pano, optvars2, cps, prog);
        GlobalCmdHist::getInstance().addCommand(
            new PT::UpdateVariablesCPCmd(*m_pano, vars, cps)
            );
        // run the usual optimizer afterwards.
        bool edit = m_edit_cb->IsChecked();
        PT::UIntSet all;
        fill_set(all, 0, m_pano->getNrOfImages()-1);
        new RunOptimizerFrame(this, m_pano, options, optvars, all, edit);
    } else {
        bool edit = m_edit_cb->IsChecked();
        new RunOptimizerFrame(this, m_pano, options, optvars, imgs, edit);
    }
}


void OptimizePanel::OnClose(wxCloseEvent& event)
{
    DEBUG_TRACE("OnClose");
    // do not close, just hide if we're not forced
    if (event.CanVeto()) {
        event.Veto();
        Hide();
        DEBUG_DEBUG("Hiding");
    } else {
        DEBUG_DEBUG("Closing");
        Destroy();
    }
}

void OptimizePanel::OnChangeMode(wxCommandEvent & e)
{
    DEBUG_TRACE("");
    int mode = m_mode_cb->GetSelection();
    DEBUG_ASSERT(mode >= 0 && mode < OPT_END_MARKER);
    switch (mode) {
    case OPT_PAIRWISE:
        // smart auto optimize
        SetCheckMark(m_yaw_list,true);
        SetCheckMark(m_roll_list,true);
        SetCheckMark(m_pitch_list,true);
        SetCheckMark(m_v_list,false);
        SetCheckMark(m_a_list,false);
        SetCheckMark(m_b_list,false);
        SetCheckMark(m_c_list,false);
        SetCheckMark(m_d_list,false);
        SetCheckMark(m_e_list,false);
    case OPT_YRP:
        // simple position
        SetCheckMark(m_yaw_list,true);
        SetCheckMark(m_roll_list,true);
        SetCheckMark(m_pitch_list,true);
        SetCheckMark(m_v_list,false);
        SetCheckMark(m_a_list,false);
        SetCheckMark(m_b_list,false);
        SetCheckMark(m_c_list,false);
        SetCheckMark(m_d_list,false);
        SetCheckMark(m_e_list,false);
        break;
    case OPT_YRP_V:
        // v + position
        SetCheckMark(m_yaw_list,true);
        SetCheckMark(m_roll_list,true);
        SetCheckMark(m_pitch_list,true);
        SetCheckMark(m_v_list,true);
        SetCheckMark(m_a_list,false);
        SetCheckMark(m_b_list,false);
        SetCheckMark(m_c_list,false);
        SetCheckMark(m_d_list,false);
        SetCheckMark(m_e_list,false);
        break;
    case OPT_YRP_B:
        // important lens distortion + position
        SetCheckMark(m_yaw_list,true);
        SetCheckMark(m_roll_list,true);
        SetCheckMark(m_pitch_list,true);
        SetCheckMark(m_v_list,false);
        SetCheckMark(m_a_list,false);
        SetCheckMark(m_b_list,true);
        SetCheckMark(m_c_list,false);
        SetCheckMark(m_d_list,false);
        SetCheckMark(m_e_list,false);
        break;
    case OPT_YRP_BV:
        // important lens distortion + v + position
        SetCheckMark(m_yaw_list,true);
        SetCheckMark(m_roll_list,true);
        SetCheckMark(m_pitch_list,true);
        SetCheckMark(m_v_list,true);
        SetCheckMark(m_a_list,false);
        SetCheckMark(m_b_list,true);
        SetCheckMark(m_c_list,false);
        SetCheckMark(m_d_list,false);
        SetCheckMark(m_e_list,false);
        break;
    case OPT_ALL:
        // everything
        SetCheckMark(m_yaw_list,true);
        SetCheckMark(m_roll_list,true);
        SetCheckMark(m_pitch_list,true);
        SetCheckMark(m_v_list,true);
        SetCheckMark(m_a_list,true);
        SetCheckMark(m_b_list,true);
        SetCheckMark(m_c_list,true);
        SetCheckMark(m_d_list,true);
        SetCheckMark(m_e_list,true);
        break;
    case OPT_CUSTOM:
        break;
    }
    // do not try to do anything on our own
    // if the user selected custom
    if (mode != OPT_CUSTOM && m_pano->getNrOfImages() > 0) {
        // get anchor image
        unsigned int refImg = m_pano->getOptions().optimizeReferenceImage;

        // count number of vertical/horizontal control points
        int nHCP = 0;
        int nVCP = 0;
        const CPVector & cps = m_pano->getCtrlPoints();
        for (CPVector::const_iterator it = cps.begin(); it != cps.end(); it++) {
            // control points
            if (it->mode == ControlPoint::X) {
                nVCP++;
            } else if (it->mode == ControlPoint::Y) {
                nHCP++;
            }
        }

        // try to select sensible position optimisation parameters,
        // dependent on output projection
        switch (m_pano->getOptions().projectionFormat) {
        case PT::PanoramaOptions::RECTILINEAR:
            m_roll_list->Check(refImg, (nHCP > 0 || nVCP > 0));
            m_yaw_list->Check(refImg, (nHCP > 0));
            m_pitch_list->Check(refImg, (nVCP > 0));
	    break;
        case PT::PanoramaOptions::CYLINDRICAL:
        case PT::PanoramaOptions::EQUIRECTANGULAR:
            m_yaw_list->Check(refImg,false);
            m_pitch_list->Check(refImg, (nHCP+nVCP > 1));
            m_roll_list->Check(refImg, (nHCP+nVCP >= 1));
			break;
        }

	// disable all manual settings
	m_yaw_list->Disable();
	m_pitch_list->Disable();
	m_roll_list->Disable();
	m_v_list->Disable();
	m_a_list->Disable();
	m_b_list->Disable();
	m_c_list->Disable();
	m_d_list->Disable();
	m_e_list->Disable();
        XRCCTRL(*this, "opt_yaw_select", wxButton)->Disable();
        XRCCTRL(*this, "opt_roll_select", wxButton)->Disable();
        XRCCTRL(*this, "opt_pitch_select", wxButton)->Disable();
        XRCCTRL(*this, "opt_yaw_clear", wxButton)->Disable();
        XRCCTRL(*this, "opt_roll_clear", wxButton)->Disable();
        XRCCTRL(*this, "opt_pitch_clear", wxButton)->Disable();
    } else {
	m_yaw_list->Enable();
	m_pitch_list->Enable();
	m_roll_list->Enable();
	m_v_list->Enable();
	m_a_list->Enable();
	m_b_list->Enable();
	m_c_list->Enable();
	m_d_list->Enable();
	m_e_list->Enable();
        XRCCTRL(*this, "opt_yaw_select", wxButton)->Enable();
        XRCCTRL(*this, "opt_roll_select", wxButton)->Enable();
        XRCCTRL(*this, "opt_pitch_select", wxButton)->Enable();
        XRCCTRL(*this, "opt_yaw_clear", wxButton)->Enable();
        XRCCTRL(*this, "opt_roll_clear", wxButton)->Enable();
        XRCCTRL(*this, "opt_pitch_clear", wxButton)->Enable();
    }
}

							