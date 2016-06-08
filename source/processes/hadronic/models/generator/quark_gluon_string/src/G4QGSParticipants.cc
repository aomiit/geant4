#include "globals.hh"
#include "G4QGSParticipants.hh"
#include "G4LorentzVector.hh"
#include "G4Pair.hh"

// Class G4QGSParticipants 

// HPW Feb 1999
// Promoting model parameters from local variables class properties

G4QGSParticipants::G4QGSParticipants() : nCutMax(7),ThersholdParameter(0.45*GeV),
                                         QGSMThershold(3*GeV),theNucleonRadius(1.5*fermi),
                                         theDiffExcitaton(0.7*GeV, 250*MeV, 250*MeV)
{
}

G4QGSParticipants::G4QGSParticipants(const G4QGSParticipants &right)
: nCutMax(right.nCutMax),ThersholdParameter(right.ThersholdParameter),
  QGSMThershold(right.QGSMThershold),theNucleonRadius(right.theNucleonRadius)
{
}


G4QGSParticipants::~G4QGSParticipants()
{
}

void G4QGSParticipants::BuildInteractions(const G4ReactionProduct  &thePrimary) 
{
  G4VSplitableHadron* aProjectile = new G4QGSMSplitableHadron(thePrimary, TRUE); // @@@ check the TRUE
  G4PomeronCrossSection theProbability(thePrimary.GetDefinition()); // @@@ should be data member
  G4double outerRadius = theNucleus->GetOuterRadius();
  // Check reaction threshold 

  theNucleus->StartLoop();
  G4Nucleon * pNucleon = theNucleus->GetNextNucleon();
  G4LorentzVector aPrimaryMomentum(thePrimary.GetMomentum(), thePrimary.GetTotalEnergy());
  G4double s = (aPrimaryMomentum + pNucleon->Get4Momentum()).mag2();
  G4double ThresholdMass = thePrimary.GetMass() + pNucleon->GetDefinition()->GetPDGMass(); 
  ModelMode = SOFT;
  if (sqr(ThresholdMass + ThersholdParameter) > s)
  {
    G4Exception("Initial energy is too low. The 4-vectors of the input are inconsistant with the particle masses.");
  }
  if (sqr(ThresholdMass + QGSMThershold) > s) // thus only diffractive in cascade!
  {
    ModelMode = DIFFRACTIVE;
  }
 
  // first find the collisions HPW
  theInteractions.clearAndDestroy();
  G4int totalCuts = 0;
  G4double impactUsed = 0;
  while(theInteractions.entries() == 0)
  {
    // choose random impact parameter HPW
    G4Pair<G4double, G4double> theImpactParameter;
    theImpactParameter = theNucleus->ChooseImpactXandY(outerRadius+theNucleonRadius);
    G4double impactX = theImpactParameter.first; 
    G4double impactY = theImpactParameter.second;
    
    // loop over nuclei to find collissions HPW
    theNucleus->StartLoop();
    G4int nucleonCount = 0; // debug
    while( pNucleon = theNucleus->GetNextNucleon() )
    {
      if(totalCuts>nCutMax) break;
       nucleonCount++; // debug
      // Needs to be moved to Probability class @@@
      G4double s = (aPrimaryMomentum + pNucleon->Get4Momentum()).mag2();
      G4double Distance2 = sqr(impactX - pNucleon->GetPosition().x()) +
                           sqr(impactY - pNucleon->GetPosition().y());
      G4double Probability = theProbability.GetInelasticProbability(s, Distance2);  
      // test for inelastic collision
      G4double rndNumber = G4UniformRand();
//      ModelMode = DIFFRACTIVE;
      if (Probability > rndNumber)
      {
//--DEBUG--        cout << "DEBUG p="<< Probability<<" r="<<rndNumber<<" d="<<sqrt(Distance2)<<G4endl;
        G4QGSMSplitableHadron* aTarget = new G4QGSMSplitableHadron(*pNucleon);
        theTargets.insert(aTarget);
 	pNucleon->Hit(aTarget);
        if ((theProbability.GetDiffractiveProbability(s, Distance2)/Probability > G4UniformRand() 
             &&(ModelMode==SOFT)) || (ModelMode==DIFFRACTIVE ))
	{ 
	  // diffractive interaction occurs
	  if(IsSingleDiffractive())
	  {
	    theSingleDiffExcitation.ExciteParticipants(aProjectile, aTarget);
	  }
	  else
	  {
	    theDiffExcitaton.ExciteParticipants(aProjectile, aTarget);
	  }
          G4InteractionContent * aInteraction = new G4InteractionContent(aProjectile);
          aInteraction->SetTarget(aTarget); 
          theInteractions.insert(aInteraction);
          totalCuts += 1;
	}
	else
	{
	  // nondiffractive soft interaction occurs
	  // sample nCut+1 (cut Pomerons) pairs of strings can be produced
          G4int nCut;
          G4double * running = new G4double[nCutMax];
          running[0] = 0;
          for(nCut = 0; nCut < nCutMax; nCut++)
          {
	    running[nCut] = theProbability.GetCutPomeronProbability(s, Distance2, nCut + 1);
            if(nCut!=0) running[nCut] += running[nCut-1];
          }
	  G4double random = running[nCutMax-1]*G4UniformRand();
          for(nCut = 0; nCut < nCutMax; nCut++)
          {
	     if(running[nCut] > random) break; 
          }
          delete [] running;
            nCut = 0;
          aTarget->IncrementCollisionCount(nCut+1);
          aProjectile->IncrementCollisionCount(nCut+1);
          G4InteractionContent * aInteraction = new G4InteractionContent(aProjectile);
          aInteraction->SetTarget(aTarget);
          aInteraction->SetNumberOfSoftCollisions(nCut+1);
          theInteractions.insert(aInteraction);
          totalCuts += nCut+1;
          impactUsed=Distance2;
       }
      }
    }
//--DEBUG--  cout << G4endl<<"NUCLEONCOUNT "<<nucleonCount<<G4endl;
  }
//--DEBUG--  cout << G4endl<<"CUTDEBUG "<< totalCuts <<G4endl;
//--DEBUG--  cout << "Impact Parameter used = "<<impactUsed<<G4endl;
  // now build the parton pairs. HPW
  SplitHadrons();
  
  // soft collisions first HPW, ordering is vital
  PerformSoftCollisions();
   
  // the rest is diffractive HPW
  PerformDiffractiveCollisions();
  
  // clean-up, if necessary
  theInteractions.clearAndDestroy();
  theTargets.clearAndDestroy();
  delete aProjectile;
}

void G4QGSParticipants::PerformDiffractiveCollisions()
{
  // remove the "G4PartonPair::PROJECTILE", etc., which are not necessary. @@@
  G4int i;
  for(i = 0; i < theInteractions.length(); i++) 
  {
    G4InteractionContent* anIniteraction = theInteractions.at(i);
    G4VSplitableHadron* aProjectile = anIniteraction->GetProjectile();
    G4Parton* aParton = aProjectile->GetNextParton();
    G4PartonPair * aPartonPair;
    // projectile first HPW
    if (aParton)
    {
      aPartonPair = new G4PartonPair(aParton, aProjectile->GetNextAntiParton(), 
                                     G4PartonPair::DIFFRACTIVE, 
                                     G4PartonPair::PROJECTILE);
      thePartonPairs.insert(aPartonPair);
    }
    // then target HPW
    G4VSplitableHadron* aTarget = anIniteraction->GetTarget();
    aParton = aTarget->GetNextParton();
    if (aParton)
    {
      aPartonPair = new G4PartonPair(aParton, aTarget->GetNextAntiParton(), 
                                     G4PartonPair::DIFFRACTIVE, 
                                     G4PartonPair::TARGET);
      thePartonPairs.insert(aPartonPair);
    }
  }
}

void G4QGSParticipants::PerformSoftCollisions()
{
  G4int i;
  for(i = 0; i < theInteractions.length(); i++)   
  {
    G4InteractionContent* anIniteraction = theInteractions.at(i);
    G4PartonPair * aPair = NULL;
    if (anIniteraction->GetNumberOfSoftCollisions())
    { 
      G4VSplitableHadron* pProjectile = anIniteraction->GetProjectile();
      G4VSplitableHadron* pTarget     = anIniteraction->GetTarget();
      for (G4int j = 0; j < anIniteraction->GetNumberOfSoftCollisions(); j++)
      {
        aPair = new G4PartonPair(pTarget->GetNextParton(), pProjectile->GetNextAntiParton(), 
                                 G4PartonPair::SOFT, G4PartonPair::TARGET);
        thePartonPairs.insert(aPair);
        aPair = new G4PartonPair(pProjectile->GetNextParton(), pTarget->GetNextAntiParton(), 
                                 G4PartonPair::SOFT, G4PartonPair::PROJECTILE);
        thePartonPairs.insert(aPair);
      }  
      delete theInteractions.removeAt(i--);
    }
  }
}