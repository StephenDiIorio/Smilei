
#ifndef DIAGNOSTICFIELDS3D_H
#define DIAGNOSTICFIELDS3D_H

#include <string>
#include <vector>

#include "DiagnosticFields.h"
#include "Tools.h"

class DiagnosticFields3D : public DiagnosticFields
{
public:
    DiagnosticFields3D( Params &params, SmileiMPI *smpi, VectorPatch &vecPatches, int, OpenPMDparams & );
    ~DiagnosticFields3D();
    
    void setFileSplitting( SmileiMPI *smpi, VectorPatch &vecPatches ) override;
    
    //! Copy patch field to current "data" buffer
    void getField( Patch *patch, unsigned int ) override;
    
    H5Write writeField( H5Write*, std::string, int ) override;
    
private:

    std::vector<unsigned int> buffer_skip_x, buffer_skip_y, buffer_skip_z;

};

#endif
