#include <vector>
#include <string>
#include <unordered_map>
#include "../framework/module.h"                  // for differential_module and update
#include "../framework/module_helper_functions.h"  // for get_ip
#include "../framework/constants.h"                // for getting physical constants
#include "thornley_nutrient_dynamics.h"  // for the thornley_nutrient_dynamics namespace
#include "thornley_biomass_calculator.h"
// #include <Rcpp.h> // for debugging

// using namespace Rcpp; // for debugging
using thornley_nutrient_dynamics::generate_multi_organ_quantity_names;
using thornley_nutrient_dynamics::get_multi_organ_ips;
using thornley_nutrient_dynamics::get_multi_organ_ops;
using thornley_nutrient_dynamics::organ;
using thornley_nutrient_dynamics::transport_link;

thornley_biomass_calculator::thornley_biomass_calculator(
    std::vector<organ> const& organs,
    std::vector<transport_link> const& organ_links,
    state_map const& input_quantities,
    state_map* output_quantities)
    :  // Define basic module properties by passing its name to its parent class
      direct_module(),

      // Store the organ names
      organs(organs),
      // Store the organ links
      organ_links(organ_links),

      // Get pointers to organ input quantities
      substrate_carbon_source_rate_ips(get_external_substrate_ips(input_quantities, organs)),
      utilization_rate_ips(get_multi_organ_ips(input_quantities, organs, "utilization_rate")),
      senescence_rate_ips(get_multi_organ_ips(input_quantities, organs, "senescence_rate")),
      // Get pointers to organ link input quantities  
      substrate_transport_ips(get_ip(input_quantities, generate_pairwise_names("substrate_transport", organ_links))),

      // Get pointers to input parameters
      respiration_factor_ips(get_multi_organ_ips(input_quantities, organs, "respiration_factor")),
      senescence_reuse_factor_ips(get_multi_organ_ips(input_quantities, organs, "senescence_reuse_factor")),
      
      // Get reference to input parameters
      stop_growth_dvi(get_input(input_quantities, "stop_growth_dvi")),
      DVI(get_input(input_quantities, "DVI")), // any difference from {}?

      // Get pointers to output parameters
      total_C_change_per_m2_ops(get_multi_organ_ops(output_quantities, organs, "total_C_change_per_m2"))
{
}

std::vector<std::string> thornley_biomass_calculator::get_inputs(
    std::vector<organ> const& organs,
    std::vector<transport_link> const& organ_links)
{
    // Add the external substrate sources
    std::vector<std::string> inputs = get_external_substrate_quantity_names(organs);  // mol / m^2

    // List the quantity names that are guaranteed to exist for each organ
    std::vector<std::string> quantities_for_each_organ = {
        "utilization_rate",         // mol / m2
        "senescence_rate",          // mol / m2
        "respiration_factor",       // mol / m2
        "senescence_reuse_factor"   // dimensionless
    };

    // Append the organ names as prefixes
    std::vector<std::string> multi_organ_quantity_names = generate_multi_organ_quantity_names(organs, quantities_for_each_organ);
    inputs.insert(inputs.end(), multi_organ_quantity_names.begin(), multi_organ_quantity_names.end());
    
    // Append the link names as prefixes
    std::vector<std::string> link_inputs = generate_pairwise_names("substrate_transport", organ_links);
    
    // Append the link-quantity names to the organ-quantity names
    inputs.insert(inputs.end(), link_inputs.begin(), link_inputs.end()); 
    inputs.push_back("stop_growth_dvi");
    inputs.push_back("DVI");
    return inputs;
}

std::vector<std::string> thornley_biomass_calculator::get_outputs(std::vector<organ> const& organs)
{
    // List the quantity names that exist for each organ
    std::vector<std::string> quantities_for_each_organ = {
        "total_C_change_per_m2"    // Mg / ha
    };
    // Append the organ names as prefixes
    return generate_multi_organ_quantity_names(organs, quantities_for_each_organ);
}

void thornley_biomass_calculator::do_multi_organ_operation() const
{
    // Initialize a library of total_C_change_per_m2 for each organ
    std::unordered_map<std::string, double> total_C_change_per_m2;
    
    // Calculate changes from utilization
    for (size_t i = 0; i < organs.size(); ++i) {
        total_C_change_per_m2[organs[i].name()] = - *utilization_rate_ips[i] *
                                                 *respiration_factor_ips[i] - 
                                                 *senescence_rate_ips[i] * 
                                                 (1 - *senescence_reuse_factor_ips[i]);

        // if the organ is a substrate C source, add the rate to the change
        if (substrate_carbon_source_rate_ips[i] && DVI < stop_growth_dvi) {
            // refer to thornley_utilization.cpp comments for how 0.6 is calculated
            double const cf = 0.6 / physical_constants::molar_mass_of_glucose;   // (mol C / m^2) / (Mg glucose / hr)
            total_C_change_per_m2[organs[i].name()] += *substrate_carbon_source_rate_ips[i] * cf;  // mol C / Mg Source Organ / hr
            // Rcout << "substrate_carbon_source_rate_ips is " <<  *substrate_carbon_source_rate_ips[i] * cf << std::endl;
        }
    }
    
    // Calculate changes from transport
    for (size_t i = 0; i < organ_links.size(); ++i) {
        total_C_change_per_m2[organ_links[i].first.name()] -= *substrate_transport_ips[i];
        total_C_change_per_m2[organ_links[i].second.name()] += *substrate_transport_ips[i];
    }

    // Update _ops
    for (size_t i = 0; i < organs.size(); ++i) {
        update(total_C_change_per_m2_ops[i], total_C_change_per_m2[organs[i].name()]);
    }
}
