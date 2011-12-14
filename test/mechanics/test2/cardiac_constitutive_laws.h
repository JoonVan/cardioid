#ifndef OOMPH_CARDIAC_CONSTITUTIVE_LAWS_HEADER
#define OOMPH_CARDIAC_CONSTITUTIVE_LAWS_HEADER

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

#include "cardiac_myofilaments.h"

namespace oomph
{
/// \short Anisotropic constitutive law
///
/// This class has pointer to the transformtation tensor,
/// which is changed at each integration point in
/// AnisotropicPVDEquations::action_before_sigma_calculation.
/// Transformation tensor is used to rotate g, G, and sigma when
/// this class calls
/// AnisotropicConstitutiveLaw::calculate_second_piola_kirchhoff_stress_in_anisotropic_law
class AnisotropicConstitutiveLaw : public oomph::ConstitutiveLaw
{
public:

  /// \short Calculate the contravariant 2nd Piola Kirchhoff
  /// stress tensor. Arguments are the
  /// covariant undeformed and deformed metric tensor and the
  /// matrix in which to return the stress tensor.
  /// Uses correct 3D invariants for 2D (plane strain) problems.
  ///
  /// This function rotates G first, then calls
  /// AnisotropicConstitutiveLaw::calculate_second_piola_kirchhoff_stress_in_anisotropic_law,
  /// and rotates sigma back to global coordinate system.
  virtual void calculate_second_piola_kirchhoff_stress(
    const oomph::DenseMatrix<double> &g, const oomph::DenseMatrix<double> &G,
    oomph::DenseMatrix<double> &sigma) {
    unsigned dim = G.nrow();
    oomph::DenseMatrix<double> G_rotated(dim, dim), sigma_rotated(dim, dim), tmp(dim, dim);

    mult_AT_B((*Transformation), G, tmp);
    mult_A_B(tmp, (*Transformation), G_rotated);
   
    // call overloaded function of the child class
    calculate_second_piola_kirchhoff_stress_in_anisotropic_law(g, G_rotated, sigma_rotated);
    
    mult_A_B((*Transformation), sigma_rotated, tmp);
    mult_A_BT(tmp, (*Transformation), sigma);
            
  }

  const DenseMatrix<double>* &transformation_pt() {
    return Transformation;
  }

protected:

  /// \short Calculate the contravariant 2nd Piola Kirchhoff
  /// stress tensor. Arguments are the
  /// covariant undeformed and deformed metric tensor and the
  /// matrix in which to return the stress tensor.
  /// Uses correct 3D invariants for 2D (plane strain) problems.
  ///
  /// This function is called with g and G in the fiber coordinate system.
  /// should be overloaded in the child class.
  virtual void calculate_second_piola_kirchhoff_stress_in_anisotropic_law(
    const oomph::DenseMatrix<double> &g,
    const oomph::DenseMatrix<double> &G,
    oomph::DenseMatrix<double> &sigma) = 0;
private:
  const DenseMatrix<double>* Transformation;

};


/// \short Fung-based constitutive law.
///
/// See  Usyk et al. "Computational model of three-dimensional cardiac electromechanics"
class UsykConstitutiveLaw : public AnisotropicConstitutiveLaw
{
public:
  /// \short Indices of constitutive law coefficients
  enum Coefficient {scaling_factor = 0, fiber_fiber = 1, sheet_sheet = 2, cross_sheet_cross_sheet = 3,
                    fiber_sheet = 4, fiber_cross_sheet = 5, sheet_cross_sheet = 6, penalty_factor = 7
                   };
  /// \short Indices of axes in the fiber coordinate system
  enum Axis_index {fiber = 0, sheet = 1, cross_sheet = 2};

  /// \short constructor
  UsykConstitutiveLaw()
    : AnisotropicConstitutiveLaw(),
      Coefficient_set((double*)(&Default_coefficient_set)) {
  }
  /// \short This function is used to allocate memory for coefficient set different from default.
  ///
  /// It allocates memory for coefficient array,
  /// which can be changed by UsykConstitutiveLaw::set_coefficient.
  void create_new_coefficient_set() {
#ifdef PARANOID

    if(Coefficient_set != (double*)Default_coefficient_set)
      throw OomphLibError(
        "Coefficient set is already created",
        "UsykConstitutiveLaw::create_new_coefficient_set()",
        OOMPH_EXCEPTION_LOCATION);

#endif
    Coefficient_set = new double[size_of_coefficient_set];
    memcpy(Coefficient_set, Default_coefficient_set, sizeof(double)*size_of_coefficient_set);
  }
  /// \short Set coefficient of Usyk's constitutive law
  /// See  Usyk et al. "Computational model of three-dimensional cardiac electromechanics"
  void set_coefficient(Coefficient coefficient, double value) {
#ifdef PARANOID

    if(Coefficient_set == (double*)Default_coefficient_set)
      throw OomphLibError(
        "Default coefficients can't be changed, use create_new_coefficient_set",
        "UsykConstitutiveLaw::set_coefficient()",
        OOMPH_EXCEPTION_LOCATION);

#endif
    Coefficient_set[coefficient] = value;
  }

  /// \short Calculate the derivatives of the contravariant
  /// 2nd Piola Kirchhoff stress tensor with respect to the deformed metric
  /// tensor. Arguments are the
  /// covariant undeformed and deformed metric tensor, the current value of
  /// the stress tensor and the
  /// rank four tensor in which to return the derivatives of the stress tensor
  /// The default implementation uses finite differences, but can be
  /// overloaded for constitutive laws in which an analytic formulation
  /// is possible.
  /// If the boolean flag symmetrize_tensor is false, only the
  /// "upper  triangular" entries of the tensor will be filled in. This is
  /// a useful efficiency when using the derivatives in Jacobian calculations.
  ///
  /// This function is not finished yet. Currently, it calls the function of the parent class.
  void calculate_d_second_piola_kirchhoff_stress_dG(
    const oomph::DenseMatrix<double> &g,
    const oomph::DenseMatrix<double> &G,
    const oomph::DenseMatrix<double> &sigma,
    oomph::RankFourTensor<double> &d_sigma_dG,
    const bool &symmetrize_tensor);

  /// \short Pure virtual function in which the user must declare if the constitutive
  /// equation requires an incompressible formulation in which the volume constraint is
  /// enforced explicitly. Used as a sanity check in PARANOID mode.
  ///
  /// We use penalty law, rather than mix formulation so this funciton returns false.
  bool requires_incompressibility_constraint() {
    return false;
  }
  /// \short Destructor
  virtual ~UsykConstitutiveLaw() {
    if(Coefficient_set != (double*)Default_coefficient_set)
      delete [] Coefficient_set;
  }


  /// \short Calculate the contravariant 2nd Piola Kirchhoff
  /// stress tensor. Arguments are the
  /// covariant undeformed and deformed metric tensor and the
  /// matrix in which to return the stress tensor.
  /// Uses correct 3D invariants for 2D (plane strain) problems.
  ///
  /// This function is called from AnisotropicConstitutiveLaw::calculate_second_piola_kirchhoff_stress,
  /// which rotates G first, then call this function, and rotates sigma back to global
  /// coordinate system.
  void calculate_second_piola_kirchhoff_stress_in_anisotropic_law(
    const oomph::DenseMatrix<double> &g, const oomph::DenseMatrix<double> &G,
    oomph::DenseMatrix<double> &sigma);



private:

  static const unsigned size_of_coefficient_set = 8;
  static const unsigned size_of_upper_tensor_triangle = 6;


  /// \short Coefficient set from Usyk et al.
  /// "Computational model of three-dimensional cardiac electromechanics"
  static double Default_coefficient_set[size_of_coefficient_set];
  static double data_for_derivative_calculation[size_of_upper_tensor_triangle];
  static double data_for_derivative_calculation_adj[size_of_upper_tensor_triangle];
  double* Coefficient_set;

};

/// \short Interface for set_active_model
///
/// This class should be a parent of any active constitutive law.
/// We have to change the pointer to different instances of active model in
/// one instance of active constitutive law of the finite element
/// when we calculate active stress at different gaussian points. So we do dynamic_cast
/// of ConstitutiveLaw to InterfaceToConstitutiveLawActiveComponent
/// and call InterfaceToConstitutiveLawActiveComponent::set_active_model
/// from ActiveTPVDElement::action_before_sigma_calculation.
class InterfaceToConstitutiveLawActiveComponent
{
public:
  /// \short Setting active model for this instance of constitutive law.
  void set_active_model(ActiveModel* model) {
    Active_model = model;
  }
  /// \short Pointer to current active model
  ActiveModel* &active_model() {
    return Active_model;
  }
private:
  ActiveModel* Active_model;
};



/// \short constitutive law with active model
template <class CONSTITUTIVELAW>
class ActiveConstitutiveLaw : public CONSTITUTIVELAW,
  public InterfaceToConstitutiveLawActiveComponent
{
public:

  ActiveConstitutiveLaw(): CONSTITUTIVELAW(), InterfaceToConstitutiveLawActiveComponent() {}
  /*
    void calculate_d_second_piola_kirchhoff_stress_dG(
        const oomph::DenseMatrix<double> &g,
        const oomph::DenseMatrix<double> &G,
        const oomph::DenseMatrix<double> &sigma,
        oomph::RankFourTensor<double> &d_sigma_dG,
        const bool &symmetrize_tensor) {
        T::ConstitutiveLaw::calculate_d_second_piola_kirchhoff_stress_dG(g, G,
                sigma,
                d_sigma_dG,
                symmetrize_tensor);

        double stretch_ratio=sqrt(G(0,0));
        double d_stretch_ratio = 0.5/stretch_ratio;
        double d_factor = 1.66*d_stretch_ratio;
        d_sigma_dG(0,0,0,0)+=d_factor*Active_tension;
    }
  */
  /// \short Constructor
  virtual ~ActiveConstitutiveLaw() {

  }

  /// \short Calculate the contravariant 2nd Piola Kirchhoff
  /// stress tensor. Arguments are the
  /// covariant undeformed and deformed metric tensor and the
  /// matrix in which to return the stress tensor.
  /// Uses correct 3D invariants for 2D (plane strain) problems.
  ///
  /// This function adds active stress to the components of total stress (sigma) calculated
  /// from parent class (template parameter CONSTITUTIVELAW).
  void calculate_second_piola_kirchhoff_stress(
    const oomph::DenseMatrix<double> &g, const oomph::DenseMatrix<double> &G,
    oomph::DenseMatrix<double> &sigma) {

    CONSTITUTIVELAW::calculate_second_piola_kirchhoff_stress(g, G, sigma);
    active_model()->add_active_stress(g, G, sigma, this->transformation_pt());

  }

};

}

#endif

