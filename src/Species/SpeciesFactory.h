#ifndef SPECIESFACTORY_H
#define SPECIESFACTORY_H

#include "Species.h"
#include "Species_norm.h"
#include "Species_rrll.h"

#include "PusherFactory.h"
#include "IonizationFactory.h"
#include "PartBoundCond.h"

#include "Params.h"
#include "SmileiMPI.h"

#include "Tools.h"

class SpeciesFactory {
public:

    static std::vector<Species*> createVector(Params& params, SmileiMPI* smpi) {
        // this will be returned
        std::vector<Species*> retSpecies;
        
        // Define later ... needed for ionization
        Species *electron_species=NULL;
        
        // read from python namelist
        bool ok;
        for (unsigned int ispec = 0; ispec < (unsigned int) PyTools::nComponents("Species"); ispec++) {
            
            // Extract type of species dynamics from namelist
            std::string dynamics_type = "norm"; // default value
            if (!PyTools::extract("dynamics_type", dynamics_type ,"Species",ispec) )
                WARNING("For species #" << ispec << ", dynamics_type not defined: assumed = 'norm'.");
            if (dynamics_type!="norm" && dynamics_type!="rrll"){
                ERROR("dynamics_type different than norm not yet implemented");
            }
            
            // Create species object
            Species * thisSpecies=NULL;
            if (dynamics_type=="norm") {
                // Species with Boris dynamics
                thisSpecies = new Species_norm(params, smpi);
            } else if (dynamics_type=="rrll") {
                // Species with Boris dynamics + Radiation Back-Reaction (using the Landau-Lifshitz formula)
                thisSpecies = new Species_rrll(params, smpi);
            } else {
                ERROR("For species #" << ispec << ", dynamics_type must be either 'norm' or 'rrll'")
            }
            
            thisSpecies->dynamics_type = dynamics_type;
            thisSpecies->speciesNumber = ispec;
            
            // Extract various parameters from the namelist
            
            PyTools::extract("species_type",thisSpecies->species_type,"Species",ispec);
            if(thisSpecies->species_type.empty()) {
                ERROR("For species #" << ispec << ", parameter species_type required");
            }
            
            PyTools::extract("initPosition_type",thisSpecies->initPosition_type ,"Species",ispec);
            if (thisSpecies->initPosition_type.empty()) {
                ERROR("For species #" << ispec << " empty initPosition_type");
            } else if ( (thisSpecies->initPosition_type!="regular")&&(thisSpecies->initPosition_type!="random") ) {
                ERROR("For species #" << ispec << " bad definition of initPosition_type " << thisSpecies->initPosition_type);
            }
            
            PyTools::extract("initMomentum_type",thisSpecies->initMomentum_type ,"Species",ispec);
            if ( (thisSpecies->initMomentum_type=="mj") || (thisSpecies->initMomentum_type=="maxj") ) {
                thisSpecies->initMomentum_type="maxwell-juettner";
            }
            if (   (thisSpecies->initMomentum_type!="cold")
                && (thisSpecies->initMomentum_type!="maxwell-juettner")
                && (thisSpecies->initMomentum_type!="rectangular") ) {
                ERROR("For species #" << ispec << " bad definition of initMomentum_type");
            }
            
            thisSpecies->c_part_max = 1.0; // default value
            PyTools::extract("c_part_max",thisSpecies->c_part_max,"Species",ispec);
            
            if( !PyTools::extract("mass",thisSpecies->mass ,"Species",ispec) ) {
                ERROR("For species #" << ispec << ", mass not defined.");
            }
            
            thisSpecies->time_frozen = 0.0; // default value
            PyTools::extract("time_frozen",thisSpecies->time_frozen ,"Species",ispec);
            if (thisSpecies->time_frozen > 0 && thisSpecies->initMomentum_type!="cold") {
                WARNING("For species #" << ispec << " possible conflict between time-frozen & not cold initialization");
            }
            
            thisSpecies->radiating = false; // default value
            PyTools::extract("radiating",thisSpecies->radiating ,"Species",ispec);
            if (thisSpecies->dynamics_type=="rrll" && (!thisSpecies->radiating)) {
                WARNING("For species #" << ispec << ", dynamics_type='rrll' forcing radiating=True");
                thisSpecies->radiating=true;
            }
            
            if (!PyTools::extract("bc_part_type_west",thisSpecies->bc_part_type_west,"Species",ispec) )
                ERROR("For species #" << ispec << ", bc_part_type_west not defined");
            if (!PyTools::extract("bc_part_type_east",thisSpecies->bc_part_type_east,"Species",ispec) )
                ERROR("For species #" << ispec << ", bc_part_type_east not defined");
            
            if (params.nDim_particle>1) {
                if (!PyTools::extract("bc_part_type_south",thisSpecies->bc_part_type_south,"Species",ispec) )
                    ERROR("For species #" << ispec << ", bc_part_type_south not defined");
                if (!PyTools::extract("bc_part_type_north",thisSpecies->bc_part_type_north,"Species",ispec) )
                    ERROR("For species #" << ispec << ", bc_part_type_north not defined");
            }
            
            // for thermalizing BCs on particles check if thermT is correctly defined
            bool thermTisDefined=false;
            if ( (thisSpecies->bc_part_type_west=="thermalize") || (thisSpecies->bc_part_type_east=="thermalize") ){
                thermTisDefined=PyTools::extract("thermT",thisSpecies->thermT,"Species",ispec);
                if (!thermTisDefined) ERROR("thermT needs to be defined for species " <<ispec<< " due to x-BC thermalize");
            }
            if ( (params.nDim_particle==2) && (!thermTisDefined) &&
                (thisSpecies->bc_part_type_south=="thermalize" || thisSpecies->bc_part_type_north=="thermalize") ) {
                thermTisDefined=PyTools::extract("thermT",thisSpecies->thermT,"Species",ispec);
                if (!thermTisDefined) ERROR("thermT needs to be defined for species " <<ispec<< " due to y-BC thermalize");
            }
            if (thermTisDefined) {
                if (thisSpecies->thermT.size()==1) {
                    thisSpecies->thermT.resize(3);
                    for (unsigned int i=1; i<3;i++)
                        thisSpecies->thermT[i]=thisSpecies->thermT[0];
                }
            } else {
                thisSpecies->thermT.resize(3);
                for (unsigned int i=0; i<3;i++)
                    thisSpecies->thermT[i]=0.0;
            }
            
            
            thisSpecies->ionization_model = "none"; // default value
            PyTools::extract("ionization_model", thisSpecies->ionization_model, "Species",ispec);
            
            ok = PyTools::extract("atomic_number", thisSpecies->atomic_number, "Species",ispec);
            if( !ok && thisSpecies->ionization_model!="none" ) {
                ERROR("For species #" << ispec << ", `atomic_number` not found => required for the ionization model .");
            }
            
            if (thisSpecies->species_type=="electron") {
                if (electron_species) {
                    WARNING("Two species are named electron");
                } else {
                    electron_species=thisSpecies;
                }
            }
            
            
            // Species geometry
            // ----------------
            
            // Density
            bool ok1, ok2;
            PyObject *profile1, *profile2, *profile3;
            ok1 = PyTools::extract_pyProfile("nb_density"    , profile1, "Species", ispec);
            ok2 = PyTools::extract_pyProfile("charge_density", profile1, "Species", ispec);
            if(  ok1 &&  ok2 ) ERROR("For species #" << ispec << ", cannot define both `nb_density` and `charge_density`.");
            if( !ok1 && !ok2 ) ERROR("For species #" << ispec << ", must define `nb_density` or `charge_density`.");
            if( ok1 ) thisSpecies->densityProfileType = "nb";
            if( ok2 ) thisSpecies->densityProfileType = "charge";
            thisSpecies->densityProfile = new Profile(profile1, params.nDim_particle);
            
            // Number of particles per cell
            if( !PyTools::extract_pyProfile("n_part_per_cell", profile1, "Species", ispec))
                ERROR("For species #" << ispec << ", n_part_per_cell not found or not understood");
            thisSpecies->ppcProfile = new Profile(profile1, params.nDim_particle);
            
            // Charge
            if( !PyTools::extract_pyProfile("charge", profile1, "Species", ispec))
                ERROR("For species #" << ispec << ", charge not found or not understood");
            thisSpecies->chargeProfile = new Profile(profile1, params.nDim_particle);
            
            // Mean velocity
            PyTools::extract3Profiles("mean_velocity", ispec, profile1, profile2, profile3);
            thisSpecies->velocityProfile[0] = new Profile(profile1, params.nDim_particle);
            thisSpecies->velocityProfile[1] = new Profile(profile2, params.nDim_particle);
            thisSpecies->velocityProfile[2] = new Profile(profile3, params.nDim_particle);
            
            // Temperature
            PyTools::extract3Profiles("temperature", ispec, profile1, profile2, profile3);
            thisSpecies->temperatureProfile[0] = new Profile(profile1, params.nDim_particle);
            thisSpecies->temperatureProfile[1] = new Profile(profile2, params.nDim_particle);
            thisSpecies->temperatureProfile[2] = new Profile(profile3, params.nDim_particle);
            
            
            // CALCULATE USEFUL VALUES
            
            // here I save the dimension of the pb (to use in BoundaryConditionType.h)
            thisSpecies->nDim_fields = params.nDim_field;
            
            /*        double gamma=1.+thisSpecies->thermT[0]/thisSpecies->mass;
             
             for (unsigned int i=0; i<3; i++) {
             thisSpecies->thermalVelocity[i] = sqrt( 1.-1./gamma*gamma );
             thisSpecies->thermalMomentum[i] = gamma*thisSpecies->thermalVelocity[i];
             }
             
             double gamma=1.+thisSpecies->thermT[0]/thisSpecies->mass;
             */
            
            thisSpecies->thermalVelocity.resize(3);
            thisSpecies->thermalMomentum.resize(3);
            
            if (thermTisDefined) {
                WARNING("Using thermT[0] for species ispec=" << ispec << " in all directions");
                if (thisSpecies->thermalVelocity[0]>0.3) {
                    ERROR("for Species#"<<ispec<<" thermalising BCs require ThermT[0]="<<thisSpecies->thermT[0]<<"<<"<<thisSpecies->mass);
                }
                for (unsigned int i=0; i<3; i++) {
                    thisSpecies->thermalVelocity[i] = sqrt(2.*thisSpecies->thermT[0]/thisSpecies->mass);
                    thisSpecies->thermalMomentum[i] = thisSpecies->thermalVelocity[i];
                }
            }
            
            
            // Extract test Species flag
            thisSpecies->isTest = false; // default value
            PyTools::extract("isTest",thisSpecies->isTest ,"Species",ispec);
            thisSpecies->particles.isTestParticles = thisSpecies->isTest;
            
            // Verify they don't ionize
            if (thisSpecies->ionization_model!="none" && (!thisSpecies->isTest)) {
                ERROR("For species #" << ispec << ", disabled for now : test & ionized");
            }
            
            // Define the number of timesteps for dumping test particles
            thisSpecies->test_dump_every = 1;
            if (PyTools::extract("dump_every",thisSpecies->test_dump_every ,"Species",ispec)) {
                if (thisSpecies->test_dump_every>1 && !thisSpecies->isTest)
                    WARNING("For species #" << ispec << ", dump_every discarded because not test particles");
            }
            
            // Create the particles
            if (!params.restart) {
                // unsigned int npart_effective=0;
                
                // Create particles in a space starting at cell_index
                std::vector<double> cell_index(3,0);
                for (unsigned int i=0 ; i<params.nDim_field ; i++) {
                    if (params.cell_length[i]!=0)
                        cell_index[i] = smpi->getDomainLocalMin(i);
                }
                
                int starting_bin_idx = 0;
                // does a loop over all cells in the simulation
                // considering a 3d volume with size n_space[0]*n_space[1]*n_space[2]
                /*npart_effective = */
                thisSpecies->createParticles(params.n_space, cell_index, starting_bin_idx, params );
                
                //PMESSAGE( 1, smpi->getRank(),"Species "<< speciesNumber <<" # part "<< npart_effective );
            }
            
            // Communicate some stuff if this is a test species
            // Need to be placed after createParticles()
            if (thisSpecies->isTest) {
                int locNbrParticles = thisSpecies->getNbrOfParticles();
                int* allNbrParticles = new int[smpi->smilei_sz];
                MPI_Gather( &locNbrParticles, 1, MPI_INTEGER, allNbrParticles, 1, MPI_INTEGER, 0, MPI_COMM_WORLD );
                int nParticles(0);
                if (smpi->isMaster()) {
                    nParticles =  allNbrParticles[0];
                    for (int irk=1 ; irk<smpi->getSize() ; irk++){
                        allNbrParticles[irk] += nParticles;
                        nParticles = allNbrParticles[irk];
                    }
                    for (int irk=smpi->getSize()-1 ; irk>0 ; irk--){
                        allNbrParticles[irk] = allNbrParticles[irk-1];
                    }
                    allNbrParticles[0] = 0;
                }
                int offset(0);
                MPI_Scatter(allNbrParticles, 1 , MPI_INTEGER, &offset, 1, MPI_INTEGER, 0, MPI_COMM_WORLD );
                thisSpecies->particles.addIdOffsets(offset);
            }
            
            // assign the correct Pusher to Push
            thisSpecies->Push = PusherFactory::create(params, thisSpecies);
            
            // Assign the Ionization model (if needed) to Ionize
            //  Needs to be placed after createParticles() because requires the knowledge of max_charge
            thisSpecies->Ionize = IonizationFactory::create( params, thisSpecies);
            if (thisSpecies->Ionize) {
                DEBUG("Species " << thisSpecies->species_type << " can be ionized!");
            }
            
            // define limits for BC and functions applied and for domain decomposition
            thisSpecies->partBoundCond = new PartBoundCond( params, thisSpecies, smpi);
            
            // Put the newly created species in the vector of species
            retSpecies.push_back(thisSpecies);
            
            // Print info
            unsigned int nPart = thisSpecies->getNbrOfParticles();
            MPI_Reduce(smpi->isMaster()?MPI_IN_PLACE:&nPart, &nPart, 1, MPI_UNSIGNED, MPI_SUM, 0, MPI_COMM_WORLD);
            MESSAGE(1,"Species " << ispec << " (" << thisSpecies->species_type << ") created with " << nPart << " particles" );
        }
        
        // we cycle again to fix electron species for ionizable species
        for (unsigned int i=0; i<retSpecies.size(); i++) {
            if (retSpecies[i]->Ionize)  {
                if (electron_species) {
                    retSpecies[i]->electron_species=electron_species;
                    PMESSAGE(2,smpi->getRank(),"Added electron species to species " << retSpecies[i]->species_type);
                } else {
                    ERROR("Ionization needs a species called \"electron\" to be defined");
                }
            }
        }
        
        
        // Plasma related parameters
        // -------------------------
        MESSAGE("Plasma related parameters");
        MESSAGE(1,"n_species       : " << retSpecies.size());
        for ( unsigned int i=0 ; i<retSpecies.size() ; i++ ) {
            MESSAGE(1,"            type : "<< retSpecies[i]->species_type);
        }
        
        return retSpecies;
    }

};

#endif
