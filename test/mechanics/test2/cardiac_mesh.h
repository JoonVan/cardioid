#ifndef OOMPH_CARDIAC_MESH_HEADER
#define OOMPH_CARDIAC_MESH_HEADER


#include "cardiac_elements.h"
#include "cardiac_nodes.h"

// Config header generated by autoconfig
#ifdef HAVE_CONFIG_H
#include <oomph-lib-config.h>
#endif

//OOMPH-LIB includes
#include <constitutive.h>
#include <generic.h>
#include <solid.h>

#include <algorithm>
#include <string.h>

#include <meshes/tetgen_mesh.h>

namespace oomph
{
/// \short This solid mesh class initializes integration point data in the elelements.
class SolidMeshWithDataAtIntegrationPoints: public SolidMesh
{
protected:
  /// \short Initialization of integration point data in the elelements.
  ///
  /// This function should be called from constructors of child classes.
  void initialize_data_at_integration_points() {
    unsigned long n = nelement();

    for(unsigned long ii = 0; ii < n; ii++) {

      DataAtIntegrationPoints* finite_element = dynamic_cast<DataAtIntegrationPoints*>(element_pt(ii));

      if(finite_element)
        finite_element->initialize_data_at_integration_points();

      after_initialization_data_at_integration_points(element_pt(ii));
    }
  }

protected:
  /// \short Action after data initialization in integration points for element.
  virtual void after_initialization_data_at_integration_points(GeneralisedElement* const) = 0;
};

/// \short solid mesh with anisotropic material properties
class AnisotropicSolidMesh : public SolidMeshWithDataAtIntegrationPoints
{
public:

  /// \short temporary replacement for function, which assigns fiber orientation at
  /// the nodes of the mesh. Currently this funciton assigns the same orientation at all nodes.
  void assign_transformation_at_nodes() {
    unsigned long n = nnode();

    for(unsigned ii = 0; ii < n; ii++) {
      AnisotropicSolidNode* node = static_cast<AnisotropicSolidNode*>(node_pt(ii));
      unsigned nposition_type = node->nposition_type();

      for(unsigned jj = 0; jj < nposition_type; jj++) {
        double* transformation = node->transformation_component(jj);
        transformation[0] = 2.0;
        transformation[1] = 0.0;
        transformation[2] = 0.0;
        transformation[3] = 1.0;
        transformation[4] = 0.0;
        transformation[5] = 3.0;
      }
    }
  }

protected:
  /// \short Action after data initialization in integration points for element.
  ///
  /// We precalculate transformation tensors in integration points of anisotropic elemenent
  void after_initialization_data_at_integration_points(GeneralisedElement* const finite_element) {

    AnisotropicFiniteElement* fe = dynamic_cast<AnisotropicFiniteElement*>(finite_element);

    if(fe)
      fe->precalculate_transformation_tensor_at_gaussian_points();
  }
};



/// \brief Class that describes a mesh with anisotropic elements
template<class ELEMENT>
class AnisotropicTetMesh : public virtual TetgenMesh<ELEMENT>,
  public virtual AnisotropicSolidMesh
{

public:
  /// \short Constructor
  ///
  /// Set lagrangian nodal coordinates, boundary elements info,
  /// transformation tensors at the nodes,
  /// initialize data at integration points
  AnisotropicTetMesh(const std::string& node_file_name,
                     const std::string& element_file_name,
                     const std::string& face_file_name,
                     TimeStepper* time_stepper_pt =
                       &Mesh::Default_TimeStepper) :
    TetgenMesh<ELEMENT>(node_file_name, element_file_name,
                        face_file_name, time_stepper_pt) {
    //Assign the Lagrangian coordinates
    set_lagrangian_nodal_coordinates();

    // Find elements next to boundaries
    setup_boundary_element_info();

    assign_transformation_at_nodes();
    initialize_data_at_integration_points();
  }

  virtual ~AnisotropicTetMesh() { }

};


}

#endif

