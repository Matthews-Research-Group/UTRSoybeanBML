#include <vector>
#include <string>
#include "../framework/module.h"                  // for direct_module and update
#include "../framework/module_helper_functions.h"  // for get_ip, get_input
#include "thornley_nutrient_dynamics.h"  // for the thornley_nutrient_dynamics namespace
#include "thornley_utilization_calculator.h"

using thornley_nutrient_dynamics::generate_multi_organ_quantity_names;
using thornley_nutrient_dynamics::generate_pairwise_names;
using thornley_nutrient_dynamics::get_multi_organ_ips;
using thornley_nutrient_dynamics::get_multi_organ_ops;
using thornley_nutrient_dynamics::hill_coefficient;
using thornley_nutrient_dynamics::hill_reaction_rate;
using thornley_nutrient_dynamics::senescence_logistic_rate;
using thornley_nutrient_dynamics::organ;

thornley_utilization_calculator::thornley_utilization_calculator(
    std::vector<organ> const& organs,
    state_map const& input_quantities,
    state_map* output_quantities)
    :  // Define basic module properties by passing its name to its parent class
      direct_module(),

      // Store the organs
      organs(organs),

      // Get pointers to input quantities
      mass_ips(get_multi_organ_ips(input_quantities, organs, "")),
      structural_carbon_ips(get_multi_organ_ips(input_quantities, organs, "structural_carbon")),
      substrate_carbon_ips(get_multi_organ_ips(input_quantities, organs, "substrate_carbon")),

      // Get pointers to input parameters
      utilization_rate_constant_ips(get_multi_organ_ips(input_quantities, organs, "utilization_rate_constant")),
      utilization_km_ips(get_multi_organ_ips(input_quantities, organs, "utilization_km")),
      senescence_rate_max_ips(get_multi_organ_ips(input_quantities, organs, "senescence_rate_max")),
      senescence_alpha_ips(get_multi_organ_ips(input_quantities, organs, "senescence_alpha")),
      senescence_beta_ips(get_multi_organ_ips(input_quantities, organs, "senescence_beta")),

      // Get references to input parameters
      Pod_start_dvi(get_input(input_quantities, "Pod_start_dvi")),
      stop_growth_dvi(get_input(input_quantities, "stop_growth_dvi")),
      DVI(get_input(input_quantities, "DVI")), // any difference from {}?

      // Get pointers to output parameters
      utilization_rate_ops(get_multi_organ_ops(output_quantities, organs, "utilization_rate")),
      senescence_rate_ops(get_multi_organ_ops(output_quantities, organs, "senescence_rate"))
{
}

std::vector<std::string> thornley_utilization_calculator::get_inputs(std::vector<organ> const& organs)
{
    // List the quantity names that exist for each organ
    std::vector<std::string> quantities_for_each_organ = {
        "",                                  // (Mg / ha) 
        "structural_carbon",                 // mol / m^2
        "substrate_carbon",                  // mol / m^2
        "utilization_rate_constant",         // hr^-1
        "utilization_km",                    // [10^-4 mol/Mg]
        "senescence_rate_max",               // hr^-1
        "senescence_alpha",                  // dimensionless
        "senescence_beta"                    // [DVI]^-1

    };

    // Append the organ names as prefixes
    std::vector<std::string> inputs = generate_multi_organ_quantity_names(organs, quantities_for_each_organ);  
    inputs.push_back("Pod_start_dvi");
    inputs.push_back("stop_growth_dvi");
    inputs.push_back("DVI");  
    return inputs;
}

std::vector<std::string> thornley_utilization_calculator::get_outputs(std::vector<organ> const& organs)
{
    // List the quantity names that exist for each organ
    std::vector<std::string> quantities_for_each_organ = {
        "utilization_rate",  // mol / m^2 / hr
        "senescence_rate",   // mol / m^2 / hr
    };
    return generate_multi_organ_quantity_names(organs, quantities_for_each_organ);
}

void thornley_utilization_calculator::do_multi_organ_operation() const
{
    // Calculate quantities for each organ and update the relevant outputs.
    // Also add up the structural carbon values
    for (size_t i = 0; i < organs.size(); ++i) {
        // double total_C_per_m2 = *structural_carbon_ips[i] + *substrate_carbon_ips[i] ; // mol C / m^2 
        double structural_C_per_m2 = *structural_carbon_ips[i]; // mol C / m^2 
        double utilization_rate_per_m2 = structural_C_per_m2 * hill_reaction_rate(
            *substrate_carbon_ips[i] / *mass_ips[i], // [10^-4 mol/Mg]
            hill_coefficient,
            *utilization_rate_constant_ips[i],
            *utilization_km_ips[i]);  // mol / m2 / hr

        double senescence_rate_per_m2 = senescence_logistic_rate(
            structural_C_per_m2,
            DVI,
            *senescence_rate_max_ips[i],
            *senescence_alpha_ips[i],
            *senescence_beta_ips[i]);  // mol / m2 / hr
        
        if ((organs[i].name() == "Pod" && DVI < Pod_start_dvi) || DVI > stop_growth_dvi){
            utilization_rate_per_m2 = 0;
        }
        update(utilization_rate_ops[i], utilization_rate_per_m2);
        update(senescence_rate_ops[i], senescence_rate_per_m2);
    }
}
