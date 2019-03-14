
#ifndef SYNCCARTESIANPATCHAM_H
#define SYNCCARTESIANPATCHAM_H

class Domain;
class VectorPatch;
class Patch;
class Params;
class SmileiMPI;
class Timers;
class Field;
class ElectroMagn;
class ElectroMagnAM;

class SyncCartesianPatchAM
{
public :

    static void patchedToCartesian( VectorPatch &vecPatches, Domain &domain, Params &params, SmileiMPI *smpi, Timers &timers, int itime, unsigned int imode );
    static void cartesianToPatches( Domain &domain, VectorPatch &vecPatches, Params &params, SmileiMPI *smpi, Timers &timers, int itime, unsigned int imode );

    static void sendPatchedToCartesian( ElectroMagnAM* localfields, unsigned int hindex, int send_to_global_patch_rank, SmileiMPI * smpi, Patch *patch, Params& params, unsigned int imode );
    static void finalize_sendPatchedToCartesian( ElectroMagnAM* localfields, unsigned int hindex, int send_to_global_patch_rank, SmileiMPI* smpi, Patch *patch, Params& params, unsigned int imode );
    static void recvPatchedToCartesian( ElectroMagnAM* globalfields, unsigned int hindex, int local_patch_rank, VectorPatch& vecPatches, Params &params, SmileiMPI* smpi, Domain& domain, unsigned int imode );

    static void recvCartesianToPatches( ElectroMagn* localfields, unsigned int hindex, int recv_from_global_patch_rank, SmileiMPI* smpi, Patch* patch, unsigned int imode );
    static void finalize_recvCartesianToPatches( ElectroMagn* localfields, unsigned int hindex, int recv_from_global_patch_rank, SmileiMPI* smpi, Patch* patch, unsigned int imode );
    static void sendCartesianToPatches( ElectroMagn* globalfields, unsigned int hindex, int local_patch_rank, VectorPatch& vecPatches, Params &params, SmileiMPI* smpi, Domain& domain, unsigned int imode );

    static void patchedToCartesian_MW( VectorPatch& vecPatches, Domain& domain, Params &params, SmileiMPI* smpi );
    static void sendPatchedToCartesian_MW( ElectroMagn* localfields, unsigned int hindex, int send_to_global_patch_rank, SmileiMPI * smpi, Patch *patch, Params& params );
    static void finalize_sendPatchedToCartesian_MW( ElectroMagn* localfields, unsigned int hindex, int send_to_global_patch_rank, SmileiMPI* smpi, Patch *patch, Params& params );
    static void recvPatchedToCartesian_MW( ElectroMagn* globalfields, unsigned int hindex, int local_patch_rank, VectorPatch& vecPatches, Params &params, SmileiMPI* smpi, Domain& domain );

};

#endif
