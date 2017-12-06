#include "CUDADiffusion.hh"
#include "DiffusionUtils.hh"
#include "SymmetricTensor.hh"
#include <vector>
#include <map>
//#include <iostream>

//update is called before calc
//dVm is combined with Vm from reaction without any rearrangement
//
//Hence
//'update' must rearrange the dVmBlock
//activate map_dVm

using namespace std;

#define CONTAINS(container, item) (container.find(item) != container.end())

typedef double Real;

extern "C"
void call_cuda_kernels(const Real *VmRaw, Real *dVmRaw, const Real *sigmaRaw, int nx, int ny, int nz, Real *dVmOut, const int *lookup,int nCells);

CUDADiffusion::CUDADiffusion(const Anatomy& anatomy, int simLoopType)
: anatomy_(anatomy), simLoopType_(simLoopType),
  localGrid_(DiffusionUtils::findBoundingBox(anatomy, false))
{
   //move data into position
   nx_ = localGrid_.nx();
   ny_ = localGrid_.ny();
   nz_ = localGrid_.nz();
   VmBlock_.setup(vector<double>(nx_*ny_*nz_));
   dVmBlock_.setup(vector<double>(nx_*ny_*nz_));
   cellLookup_.setup(vector<int>(anatomy.size()));

   nLocal_ = anatomy.nLocal();
   nRemote_ = anatomy.nRemote();
   nCells_ = anatomy.size();
   
   //initialize the block index as well.
   //cellFromRed_.setup(vector<int>(anatomy.size()));
   //blockFromRed_.setup(vector<int>(anatomy.size()));
   //vector<int>& blockFromRedVec(blockFromRed_.modifyOnHost());
   //vector<int>& cellFromRedVec(cellFromRed_.modifyOnHost());
   map<int,int> cellFromBlock;
   vector<int>& cellLookupVec(cellLookup_.modifyOnHost());
//   vector<int>& blockLookupVec(blockLookup_.modifyOnHost());

   for (int icell=0; icell<nCells_; icell++)
   {
      //index to coordinate 
      Tuple ll = localGrid_.localTuple(anatomy.globalTuple(icell));
      int iblock = ll.z() + nz_ * ( ll.y() + ny_ * ll.x() );
      cellFromBlock[iblock] = icell;
      cellLookupVec[icell] = iblock;
   }

   const int offsets[3] = {ny_*nz_, nz_ , 1};
   const double areas[3] =
      {
         anatomy.dy()*anatomy.dz(),
         anatomy.dx()*anatomy.dz(),
         anatomy.dx()*anatomy.dy()
      };
   const double disc[3] = {anatomy.dx(), anatomy.dy(), anatomy.dz()};
   //initialize the array of diffusion coefficients
   sigmaFaceNormal_.setup(vector<double>(nx_*ny_*nz_*9));
   vector<double>& sigmaFaceNormalVec(sigmaFaceNormal_.modifyOnHost());

   //initialize the array of diffusion coefficients
   int sigmaNx=nx_-1;
   int sigmaNy=ny_-1;
   int sigmaNz=nz_-1;
   sigmaFaceNormal_.setup(vector<double>(sigmaNx*sigmaNy*sigmaNz*9));
   vector<double>& sigmaFaceNormalVec(sigmaFaceNormal_.modifyOnHost());
   for (int iz=0; iz<nz_-1; iz++)
   {
      for (int iy=0; iy<ny_-1; iy++)
      {
         for (int ix=0; ix<nx_-1; ix++)
         {
            //int iblock = ix+1 + nx_*(iy+1 + ny_*(iz+1));
            //int isigma = ix + sigmaNx*(iy + sigmaNy*iz);

            int iblock = iz+1 + nz_*(iy+1 + nx_*(ix+1));
            int isigma = iz + sigmaNz*(iy + sigmaNy*ix);

            if (CONTAINS(cellFromBlock, iblock))
            {
               int icell = cellFromBlock[iblock];
               const SymmetricTensor& sss(anatomy.conductivity(icell));
               double thisSigma[3][3] = {{sss.a11, sss.a12, sss.a13},
                                         {sss.a12, sss.a22, sss.a23},
                                         {sss.a13, sss.a23, sss.a33}};
               for (int idim=0; idim<3; idim++)
               {
                  int otherBlock = iblock-offsets[idim];
                  if (CONTAINS(cellFromBlock, otherBlock))
                  {
                     for (int jdim=0; jdim<3; jdim++)
                     {
                        double sigmaValue = thisSigma[idim][jdim]*areas[idim]/disc[jdim];
                        if (idim != jdim)
                        {
                           bool canComputeGradient=
                                 CONTAINS(cellFromBlock, iblock              -offsets[jdim])
                              && CONTAINS(cellFromBlock, iblock              +offsets[jdim])
                              && CONTAINS(cellFromBlock, iblock-offsets[idim]-offsets[jdim])
                              && CONTAINS(cellFromBlock, iblock-offsets[idim]+offsets[jdim])
                              ;
                           if (! canComputeGradient)
                           {
                              sigmaValue = 0;
                           }
                        }
                        sigmaFaceNormalVec[isigma + sigmaNx*sigmaNy*sigmaNz*(jdim +3*idim)] = sigmaValue;
                     }
                  }
                  else
                  {
                     for (int jdim=0; jdim<3; jdim++)
                     {
                        sigmaFaceNormalVec[jdim +3*(idim + 3*isigma)] = 0;
                     }
                  }
               }
            }
            else
            {
               for (int idim=0; idim<3; idim++)
               {
                  for (int jdim=0; jdim<3; jdim++)
                  {
                     sigmaFaceNormalVec[jdim +3*(idim + 3*isigma)] = 0;
                  }
               }
            }
         }
      }
   }
}

void CUDADiffusion::updateLocalVoltage(const double* VmLocal)
{
   vector<double>& VmBlockVec(VmBlock_.modifyOnDevice());
   double* VmBlockVecRaw=&VmBlockVec[0];
   const vector<int>& cellLookupVec(cellLookup_.readOnDevice());
   const int* lookupRaw=&cellLookupVec[0];
   #pragma omp target teams distribute parallel for
   for (int icell=0; icell<nLocal_; icell++)
   {
      VmBlockVecRaw[lookupRaw[icell]] = VmLocal[icell];
   }
}

void CUDADiffusion::updateRemoteVoltage(const double* VmRemote)
{
   vector<double>& VmBlockVec(VmBlock_.modifyOnDevice());
   double* VmBlockVecRaw=&VmBlockVec[0];
   const vector<int>& cellLookupVec(cellLookup_.readOnDevice());
   const int* lookupRaw=&cellLookupVec[0];
   #pragma omp target teams distribute parallel for
   for (int icell=nLocal_; icell<nCells_; icell++)
   {
      VmBlockVecRaw[lookupRaw[icell]] = VmRemote[icell];
   }
}

void CUDADiffusion::calc(VectorDouble32& dVm){
   actualCalc(*this, dVm);
}

void actualCalc(CUDADiffusion& self, VectorDouble32& dVm)
{
   int self_nLocal_ = self.nLocal_;
   int self_nCells_ = self.nCells_;
   int nx = self.nx_;
   int ny = self.ny_;
   int nz = self.nz_;
   double* dVmRaw=&dVm[0];
   if (self.simLoopType_ == 0) // it is a hard coded of enum LoopType {omp, pdr}  
   {
      #pragma omp target teams distribute parallel for firstprivate(self_nLocal_)
      for (int icell=0; icell<self_nLocal_; icell++)
      {
         dVmRaw[icell] = 0;
      }
   }
   
   const vector<double>& dVmBlockVec(self.dVmBlock_.modifyOnDevice());
   const vector<double>& VmBlockVec(self.VmBlock_.readOnDevice());
   const vector<double>& sigmaFaceNormalVec(self.sigmaFaceNormal_.readOnDevice());
   const vector<int>& cellLookupVec(self.cellLookup_.readOnDevice());
   
   const double* VmRaw=&VmBlockVec[0];
   //double* dVmRaw=&dVmBlocVec[0];
   const double* sigmaRaw=&sigmaFaceNormalVec[0];
   const int*    lookupRaw=&cellLookupVec[0];
   double* dVmOut=&dVm[0];
//   cout << "call cuda kernel" << endl;
   
#if _OPENMP >= 201511
   #pragma omp target data use_device_ptr(VmRaw, dVmRaw, sigmaRaw, dVmOut, lookupRaw)
#endif
   {
      call_cuda_kernels(VmRaw,dVmRaw,sigmaRaw,nx,ny,nz,dVmOut,lookupRaw,self_nCells_);
   }
   //diff_6face_v1<<<dim3(10,10,10),dim3(32,32,1)>>>(VmRaw,dVmRaw,sigmaRaw,sigmaRaw+3*nx*ny*nz,sigmaRaw+6*nx*ny*nz,nx,ny,nz);
   //map_dVm<<<self_nCells_>>>(dVmRaw,dVmOut,lookupRaw);

}

unsigned* CUDADiffusion::blockIndex() {return NULL;}
double* CUDADiffusion::VmBlock() {return NULL;}
double* CUDADiffusion::dVmBlock() {return NULL;}


