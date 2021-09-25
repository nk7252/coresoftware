/* vim: set sw=2 ft=cpp: */

#include "PHG4EPDSteppingAction.h"

#include "PHG4EPDDetector.h"

#include <phool/getClass.h>

#include <g4main/PHG4Hit.h>
#include <g4main/PHG4HitContainer.h>
#include <g4main/PHG4Hitv1.h>
#include <g4main/PHG4Shower.h>
#include <g4main/PHG4SteppingAction.h>
#include <g4main/PHG4TrackUserInfoV1.h>

#include <TSystem.h>

#include <Geant4/G4ParticleDefinition.hh>
#include <Geant4/G4ReferenceCountedHandle.hh>
#include <Geant4/G4Step.hh>
#include <Geant4/G4StepPoint.hh>
#include <Geant4/G4StepStatus.hh>
#include <Geant4/G4SystemOfUnits.hh>
#include <Geant4/G4ThreeVector.hh>
#include <Geant4/G4TouchableHandle.hh>
#include <Geant4/G4Track.hh>
#include <Geant4/G4TrackStatus.hh>
#include <Geant4/G4Types.hh>
#include <Geant4/G4VTouchable.hh>
#include <Geant4/G4VUserTrackInformation.hh>

#include <iostream>
#include <string>

class G4VPhysicalVolume;

PHG4EPDSteppingAction::PHG4EPDSteppingAction(PHG4EPDDetector* detector,
                                           const PHParameters*)
  : PHG4SteppingAction(detector->GetName())
  , m_detector(detector)
  , poststatus(-1)
{
}

PHG4EPDSteppingAction::~PHG4EPDSteppingAction()
{
  delete m_hit;
}

bool PHG4EPDSteppingAction::UserSteppingAction(const G4Step* step, bool)
{
  G4StepPoint* prestep = step->GetPreStepPoint();
  G4TouchableHandle prehandle = prestep->GetTouchableHandle();

  G4VPhysicalVolume* volume = prehandle->GetVolume();

//  if (!m_detector->contains(volume))
  if (!m_detector->IsInDetector(volume))
    return false;

  G4double deposit = step->GetTotalEnergyDeposit() / GeV;
  G4double ionising = deposit - step->GetNonIonizingEnergyDeposit() / GeV;
  G4double light_yield = GetVisibleEnergyDeposition(step) / GeV;

  auto prestatus = prestep->GetStepStatus();

  int32_t tile_id = m_detector->module_id_for(volume);

  G4Track const* track = step->GetTrack();

  auto particle = track->GetParticleDefinition();
  bool geantino = (particle->GetPDGEncoding() == 0 && particle->GetParticleName().find("geantino") != std::string::npos);

  if ((prestatus == fPostStepDoItProc && poststatus == fGeomBoundary) || prestatus == fGeomBoundary || prestatus == fUndefined)
  {
    if (m_hit == nullptr)
      m_hit = new PHG4Hitv1();

    m_hit->set_scint_id(tile_id);

    m_hit->set_x(0, prestep->GetPosition().x() / cm);
    m_hit->set_y(0, prestep->GetPosition().y() / cm);
    m_hit->set_z(0, prestep->GetPosition().z() / cm);
    m_hit->set_t(0, prestep->GetGlobalTime() / nanosecond);

    m_hit->set_trkid(track->GetTrackID());

    PHG4TrackUserInfoV1* userinfo = dynamic_cast<PHG4TrackUserInfoV1*>(
        track->GetUserInformation());

    if (userinfo != nullptr)
    {
      m_hit->set_trkid(userinfo->GetUserTrackId());

      userinfo->GetShower()->add_g4hit_id(
          m_hit_container->GetID(), m_hit->get_hit_id());
    }

    m_hit->set_edep(0);
    m_hit->set_eion(0);
    m_hit->set_light_yield(0);
  }

  m_hit->set_edep(m_hit->get_edep() + deposit);
  m_hit->set_eion(m_hit->get_eion() + ionising);

  G4StepPoint* poststep = step->GetPostStepPoint();
  auto postpos = poststep->GetPosition();

  m_hit->set_light_yield(m_hit->get_light_yield() + light_yield * GetLightCorrection(postpos.x(), postpos.y()));

  poststatus = poststep->GetStepStatus();

  if (poststatus != fGeomBoundary && poststatus != fWorldBoundary && poststatus != fAtRestDoItProc && track->GetTrackStatus() != fStopAndKill)
    return true;

  if (m_hit->get_edep() <= 0 && !geantino)
  {
    m_hit->Reset();

    return true;
  }

  m_hit->set_x(1, poststep->GetPosition().x() / cm);
  m_hit->set_y(1, poststep->GetPosition().y() / cm);
  m_hit->set_z(1, poststep->GetPosition().z() / cm);
  m_hit->set_t(1, poststep->GetGlobalTime() / nanosecond);

  PHG4TrackUserInfoV1* userinfo = dynamic_cast<PHG4TrackUserInfoV1*>(
      track->GetUserInformation());

  if (userinfo != nullptr)
    userinfo->SetKeep(1);

  if (geantino)
  {
    m_hit->set_edep(-1.);
    m_hit->set_eion(-1.);
    m_hit->set_light_yield(-1.);
  }

  m_hit_container->AddHit(tile_id, m_hit);

  m_hit = nullptr;

  return true;
}

void PHG4EPDSteppingAction::SetInterfacePointers(PHCompositeNode* topNode)
{
  m_hit_container = findNode::getClass<PHG4HitContainer>(topNode, m_HitNodeName);

  m_SupportHitContainer = findNode::getClass<PHG4HitContainer>(topNode, m_SupportNodeName);
  // if we do not find the node it's messed up.
  if (!m_hit_container)
  {
    std::cout << "PHG4ZDCSteppingAction::SetTopNode - unable to find " << m_HitNodeName << std::endl;
    gSystem->Exit(1);
  }
  // this is perfectly fine if support hits are disabled
  if (!m_SupportHitContainer)
  {
    if (Verbosity() > 0)
    {
      std::cout << "PHG4ZDCSteppingAction::SetTopNode - unable to find " << m_SupportNodeName << std::endl;
    }
  }
}

void PHG4EPDSteppingAction::SetHitNodeName(const std::string& type, const std::string& name)
{
  if (type == "G4HIT")
  {
    m_HitNodeName = name;
    return;
  }
  else if (type == "G4HIT_SUPPORT")
  {
    m_SupportNodeName = name;
    return;
  }
  std::cout << "Invalid output hit node type " << type << std::endl;
  gSystem->Exit(1);
  return;
}
