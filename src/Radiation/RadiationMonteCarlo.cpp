// ----------------------------------------------------------------------------
//! \file RadiationMonteCarlo.cpp
//
//! \brief This class performs the Nonlinear Inverse Compton Scattering
//! on particles using a Monte-Carlo approach.
//
//! The implementation is adapted from the thesis results of M. Lobet
//! See http://www.theses.fr/2015BORD0361
//
// ----------------------------------------------------------------------------

#include "RadiationMonteCarlo.h"

#include <cstring>
#include <fstream>

#include <cmath>

// ---------------------------------------------------------------------------------------------------------------------
//! Constructor for RadiationMonteCarlo
//! Inherit from Radiation
// ---------------------------------------------------------------------------------------------------------------------
RadiationMonteCarlo::RadiationMonteCarlo( Params &params, Species *species, Random * rand  )
    : Radiation( params, species, rand )
{
    radiation_photon_sampling_ = species->radiation_photon_sampling_;
    radiation_photon_gamma_threshold_ = species->radiation_photon_gamma_threshold_;
    inv_radiation_photon_sampling_ = 1. / radiation_photon_sampling_;
}

// ---------------------------------------------------------------------------------------------------------------------
//! Destructor for RadiationMonteCarlo
// ---------------------------------------------------------------------------------------------------------------------
RadiationMonteCarlo::~RadiationMonteCarlo()
{
}

// ---------------------------------------------------------------------------------------------------------------------
//! Overloading of the operator (): perform the Discontinuous radiation reaction
//! induced by the nonlinear inverse Compton scattering
//
//! \param particles   particle object containing the particle properties
//! \param photon_species species that will receive emitted photons
//! \param smpi        MPI properties
//! \param RadiationTables Cross-section data tables and useful functions
//                     for nonlinear inverse Compton scattering
//! \param istart      Index of the first particle
//! \param iend        Index of the last particle
//! \param ithread     Thread index
//! \param radiated_energy     overall energy radiated during the call to this method
// ---------------------------------------------------------------------------------------------------------------------
void RadiationMonteCarlo::operator()(
    Particles       &particles,
    Species         *photon_species,
    SmileiMPI       *smpi,
    RadiationTables &RadiationTables,
    double          &radiated_energy,
    int             istart,
    int             iend,
    int             ithread,
    int             ipart_ref)
{

    // _______________________________________________________________
    // Parameters

    std::vector<double> *Epart = &( smpi->dynamics_Epart[ithread] );
    std::vector<double> *Bpart = &( smpi->dynamics_Bpart[ithread] );
    //std::vector<double> *invgf = &(smpi->dynamics_invgf[ithread]);

    const int nparts = Epart->size()/3;
    const double *const __restrict__ Ex = &( ( *Epart )[0*nparts] );
    const double *const __restrict__ Ey = &( ( *Epart )[1*nparts] );
    const double *const __restrict__ Ez = &( ( *Epart )[2*nparts] );
    const double *const __restrict__ Bx = &( ( *Bpart )[0*nparts] );
    const double *const __restrict__ By = &( ( *Bpart )[1*nparts] );
    const double *const __restrict__ Bz = &( ( *Bpart )[2*nparts] );

    // 1 / mass^2
    const double one_over_mass_square = one_over_mass_*one_over_mass_;

    // Temporary quantum parameter
    double particle_chi;

    // Temporary Lorentz factor
    double gamma;

    // Radiated energy
    double cont_rad_energy;

    // Temporary double parameter
    double temp;

    // Momentum shortcut
    double *const __restrict__ momentum_x = particles.getPtrMomentum(0);
    double *const __restrict__ momentum_y = particles.getPtrMomentum(1);
    double *const __restrict__ momentum_z = particles.getPtrMomentum(2);

    // Position shortcut
    double *const __restrict__ position_x = particles.getPtrPosition( 0 );
    double *const __restrict__ position_y = nDim_ > 1 ? particles.getPtrPosition( 1 ) : nullptr;
    double *const __restrict__ position_z = nDim_ > 2 ? particles.getPtrPosition( 2 ) : nullptr;

    // Minimum value of chi for the radiation
    const double minimum_chi_continuous = RadiationTables.getMinimumChiContinuous();
    const double minimum_chi_discontinuous = RadiationTables.getMinimumChiDiscontinuous();

    // Charge shortcut
    const short *const __restrict__ charge = particles.getPtrCharge();

    // Weight shortcut
    const double *const __restrict__ weight = &( particles.weight( 0 ) );

    // Optical depth for the Monte-Carlo process
    double *const __restrict__ tau = &( particles.tau( 0 ) );

    // Optical depth for the Monte-Carlo process
    double *const __restrict__ chi = &( particles.chi(0));

    // _______________________________________________________________
    // Computation

    for( int ipart=istart ; ipart<iend; ipart++ ) {
        
        // charge / mass^2
        const double charge_over_mass_square = ( double )( charge[ipart] )*one_over_mass_square;

        // Time to emission
        double emission_time = 0;
        
        // time spent in the iteration
        double local_it_time = 0;
        
        // Number of Monte-Carlo iteration
        int mc_it_nb = 0;

        // Monte-Carlo Manager inside the time step
        while( ( local_it_time < dt_ )
                &&( mc_it_nb < max_monte_carlo_iterations_ ) ) {

            // Gamma
            gamma = std::sqrt( 1.0 + momentum_x[ipart]*momentum_x[ipart]
                          + momentum_y[ipart]*momentum_y[ipart]
                          + momentum_z[ipart]*momentum_z[ipart] );

            if( gamma==1. ){ // does not apply the MC routine for particles with 0 kinetic energy
                break;
            }

            // Computation of the Lorentz invariant quantum parameter
            particle_chi = Radiation::computeParticleChi( charge_over_mass_square,
                           momentum_x[ipart], momentum_y[ipart], momentum_z[ipart],
                           gamma,
                           Ex[ipart-ipart_ref], Ey[ipart-ipart_ref], Ez[ipart-ipart_ref],
                           Bx[ipart-ipart_ref], By[ipart-ipart_ref], Bz[ipart-ipart_ref] );

            // Update the quantum parameter in species
            // chi[ipart] = particle_chi;

            // Discontinuous emission: New emission
            // If tau[ipart] <= 0, this is a new emission
            // We also check that particle_chi > chipa_threshold,
            // else particle_chi is too low to induce a discontinuous emission
            if( ( particle_chi > minimum_chi_discontinuous )
                    && ( tau[ipart] <= epsilon_tau_ ) ) {
                // New final optical depth to reach for emision
                while( tau[ipart] <= epsilon_tau_ ) {
                    //tau[ipart] = -log( 1.-Rand::uniform() );
                    tau[ipart] = -log( 1.-rand_->uniform() );
                }

            }

            // Discontinuous emission: emission under progress
            // If epsilon_tau_ > 0
            if( tau[ipart] > epsilon_tau_ ) {

                // from the cross section
                temp = RadiationTables.computePhotonProductionYield( particle_chi, gamma );

                // Time to discontinuous emission
                // If this time is > the remaining iteration time,
                // we have a synchronization
                emission_time = std::min( tau[ipart]/temp, dt_ - local_it_time );

                // Update of the optical depth
                tau[ipart] -= temp*emission_time;

                // If the final optical depth is reached
                if( tau[ipart] <= epsilon_tau_ ) {

                    // Emission of a photon
                    // Radiated energy is incremented only if the macro-photon is not created
                    radiated_energy += RadiationMonteCarlo::photonEmission( ipart,
                                                         particle_chi, gamma,
                                                         position_x,
                                                         position_y,
                                                         position_z,
                                                         momentum_x,
                                                         momentum_y,
                                                         momentum_z,
                                                         weight,
                                                         photon_species,
                                                         RadiationTables );

                    // Optical depth becomes negative meaning
                    // that a new drawing is possible
                    // at the next Monte-Carlo iteration
                    tau[ipart] = -1.;
                }

                // Incrementation of the Monte-Carlo iteration counter
                mc_it_nb ++;
                // Update of the local time
                local_it_time += emission_time;

            }

            // Continuous emission
            // particle_chi needs to be below the discontinuous threshold
            // particle_chi needs to be above the continuous threshold
            // No discontiuous emission is in progress:
            // tau[ipart] <= epsilon_tau_
            else if( ( particle_chi <= minimum_chi_discontinuous )
                     && ( tau[ipart] <= epsilon_tau_ )
                     && ( particle_chi > minimum_chi_continuous )
                     && ( gamma > 1. ) ) {

                // Remaining time of the iteration
                emission_time = dt_ - local_it_time;

                // Radiated energy during emission_time
                cont_rad_energy =
                    RadiationTables.getRidgersCorrectedRadiatedEnergy( particle_chi,
                            emission_time );

                // Effect on the momentum
                temp = cont_rad_energy*gamma/( gamma*gamma-1. );
                momentum_x[ipart] -= temp*momentum_x[ipart];
                momentum_y[ipart] -= temp*momentum_y[ipart];
                momentum_z[ipart] -= temp*momentum_z[ipart];

                // Incrementation of the radiated energy cumulative parameter
                radiated_energy += weight[ipart]*( gamma - std::sqrt( 1.0
                                                    + momentum_x[ipart]*momentum_x[ipart]
                                                    + momentum_y[ipart]*momentum_y[ipart]
                                                    + momentum_z[ipart]*momentum_z[ipart] ) );

                // End for this particle
                local_it_time = dt_;
            }
            // No emission since particle_chi is too low
            else { // if (particle_chi < RadiationTables.getMinimumChiContinuous())
                local_it_time = dt_;
            }

        }

    }

    // ____________________________________________________
    // Update of the quantum parameter chi

    #pragma omp simd
    for( int ipart=istart ; ipart<iend; ipart++ ) {
        const double charge_over_mass_square = ( double )( charge[ipart] )*one_over_mass_square;

        // Gamma
        gamma = std::sqrt( 1.0 + momentum_x[ipart]*momentum_x[ipart]
                      + momentum_y[ipart]*momentum_y[ipart]
                      + momentum_z[ipart]*momentum_z[ipart] );

        // Computation of the Lorentz invariant quantum parameter
        chi[ipart] = Radiation::computeParticleChi( charge_over_mass_square,
                     momentum_x[ipart], momentum_y[ipart], momentum_z[ipart],
                     gamma,
                     Ex[ipart-ipart_ref], Ey[ipart-ipart_ref], Ez[ipart-ipart_ref],
                     Bx[ipart-ipart_ref], By[ipart-ipart_ref], Bz[ipart-ipart_ref] );

    }
}

// ---------------------------------------------------------------------------------------------------------------------
//! Perform the photon emission (creation of a super-photon
//! and slow down of the emitting particle)
//! \param ipart              particle index
//! \param particle_chi              particle quantum parameter
//! \param particle_gamma            particle gamma factor
//! \param position           particle position
//! \param momentum           particle momentum
//! \param RadiationTables    Cross-section data tables and useful functions
//                        for nonlinear inverse Compton scattering
// ---------------------------------------------------------------------------------------------------------------------
double RadiationMonteCarlo::photonEmission( int ipart,
        const double particle_chi,
        const double particle_gamma,
        const double *const __restrict__ position_x,
        const double *const __restrict__ position_y,
        const double *const __restrict__ position_z,
        double *const __restrict__ momentum_x,
        double *const __restrict__ momentum_y,
        double *const __restrict__ momentum_z,
        const double *const __restrict__ weight,
        Species *photon_species,
        RadiationTables &RadiationTables)
{
    // ____________________________________________________
    // Parameters
    double inv_old_norm_p;
    double radiated_energy = 0;
    //double new_norm_p;

    // Get the photon quantum parameter from the table xip
    // photon_chi = RadiationTables.computeRandomPhotonChi( particle_chi );
    const double photon_chi = RadiationTables.computeRandomPhotonChiWithInterpolation( particle_chi, rand_ );

    // compute the photon gamma factor
    double photon_gamma = photon_chi/particle_chi*( particle_gamma-1.0 );

    // ____________________________________________________
    // Creation of the new photon

    // ____________________________________________________
    // Update of the particle properties
    // direction d'emission // direction de l'electron (1/gamma << 1)
    // With momentum conservation

    inv_old_norm_p = photon_gamma/std::sqrt( particle_gamma*particle_gamma - 1.0 );
    momentum_x[ipart] -= momentum_x[ipart]*inv_old_norm_p;
    momentum_y[ipart] -= momentum_y[ipart]*inv_old_norm_p;
    momentum_z[ipart] -= momentum_z[ipart]*inv_old_norm_p;

    // With energy conservation
    /*inv_old_norm_p = 1./sqrt(particle_gamma*particle_gamma - 1.0);
    particle_gamma -= photon_gamma;
    new_norm_p = sqrt(particle_gamma*particle_gamma - 1.0);
    px *= new_norm_p * inv_old_norm_p;
    py *= new_norm_p * inv_old_norm_p;
    pz *= new_norm_p * inv_old_norm_p;*/

    // Creation of macro-photons if requested
    // Check that the photon_species is defined and the threshold on the energy
    if( photon_species
            && ( photon_gamma >= radiation_photon_gamma_threshold_ ) ) {
        /* ---------------------------------------------------------------------
        // First method: emission of a single photon

        // Creation of the new photon in the temporary array new_photons_
        new_photons_.createParticle();

        int idNew = new_photons_.size() - 1;

        for (int i=0; i<n_dimensions_; i++) {
            new_photons_.position(i,idNew)=position[i][ipart];
        }

        inv_old_norm_p = 1./sqrt(momentum_x[ipart]*momentum_x[ipart]
                                + momentum_y[ipart]*momentum_y[ipart]
                                + momentum_z[ipart]*momentum_z[ipart]);

        for (unsigned int i=0; i<3; i++) {
            new_photons_.momentum(i,idNew) =
            photon_gamma*momentum[i][ipart]*inv_old_norm_p;
        }

        new_photons_.weight(idNew)=weight[ipart];
        new_photons_.charge(idNew)=0;
        --------------------------------------------------------------------- */

        // Second method: emission of several photons for statistics following
        // the parameter radiation_photon_sampling_

        // Creation of new photons in the temporary array new_photons_
        new_photons_.createParticles( radiation_photon_sampling_ );

        // Final size
        int npart = new_photons_.size();

        // Inverse of the momentum norm
        inv_old_norm_p = 1./std::sqrt( momentum_x[ipart]*momentum_x[ipart]
                                  + momentum_y[ipart]*momentum_y[ipart]
                                  + momentum_z[ipart]*momentum_z[ipart] );

        // For all new photons...
        for( int idNew=npart-radiation_photon_sampling_; idNew<npart; idNew++ ) {


            new_photons_.position( 0, idNew )=position_x[ipart];
            if (nDim_>1) {
                new_photons_.position( 1, idNew )=position_y[ipart];
                if (nDim_>2) {
                    new_photons_.position( 2, idNew )=position_z[ipart];
                }
            }

            new_photons_.momentum( 0, idNew ) =
                photon_gamma*momentum_x[ipart]*inv_old_norm_p;
            new_photons_.momentum( 1, idNew ) =
                photon_gamma*momentum_y[ipart]*inv_old_norm_p;
            new_photons_.momentum( 2, idNew ) =
                photon_gamma*momentum_z[ipart]*inv_old_norm_p;


            new_photons_.weight( idNew )=weight[ipart]*inv_radiation_photon_sampling_;
            new_photons_.charge( idNew )=0;

            if( new_photons_.isQuantumParameter ) {
                new_photons_.chi( idNew ) = photon_chi;
            }

            if( new_photons_.isMonteCarlo ) {
                new_photons_.tau( idNew ) = -1.;
            }

        }

    }
    // Addition of the emitted energy in the cumulating parameter
    // for the scalar diagnostics
    else {
        photon_gamma = particle_gamma - std::sqrt( 1.0 + momentum_x[ipart]*momentum_x[ipart]
                                         + momentum_y[ipart]*momentum_y[ipart]
                                         + momentum_z[ipart]*momentum_z[ipart] );
        radiated_energy += weight[ipart]*photon_gamma;
    }

    return radiated_energy;
}
