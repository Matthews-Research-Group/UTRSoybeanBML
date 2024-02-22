#include <vector>
#include <string>
#include "../framework/module.h"                  // for differential_module and update
#include "../framework/module_helper_functions.h"  // for get_ip
#include "../framework/constants.h"                // for getting physical constants
#include "thornley_nutrient_dynamics.h"  // for the thornley_nutrient_dynamics namespace
#include "thornley_biomass.h"

using thornley_nutrient_dynamics::generate_multi_organ_quantity_names;
using thornley_nutrient_dynamics::get_multi_organ_ips;
using thornley_nutrient_dynamics::get_multi_organ_ops;
using thornley_nutrient_dynamics::organ;

thornley_biomass::thornley_biomass(
    std::vector<organ> const& organs,
    state_map const& input_quantities,
    state_map* output_quantities)
    :  // Define basic module properties by passing its name to its parent class
      differential_module(),

      // Store the organ names
      organs(organs),

      // Get pointers to input parameters
      total_C_change_per_m2_ips(get_multi_organ_ips(input_quantities, organs, "total_C_change_per_m2")),
      carbon_to_mass_factor_ips(get_multi_organ_ips(input_quantities, organs, "carbon_to_mass_factor")),

      // Get pointers to output parameters
      mass_ops(get_multi_organ_ops(output_quantities, organs, ""))
{
}

std::vector<std::string> thornley_biomass::get_inputs(std::vector<organ> const& organs)
{
    // List the quantity names that are guaranteed to exist for each organ
    std::vector<std::string> quantities_for_each_organ = {
        "total_C_change_per_m2",    // mol / m2
        "carbon_to_mass_factor"     // (Mg / ha) / (mol / m^2)
    };

    // Append the organ names as prefixes
    return generate_multi_organ_quantity_names(organs, quantities_for_each_organ);
}

std::vector<std::string> thornley_biomass::get_outputs(std::vector<organ> const& organs)
{
    // List the quantity names that exist for each organ
    std::vector<std::string> quantities_for_each_organ = {
        ""    // Mg / ha
    };
    // Append the organ names as prefixes
    return generate_multi_organ_quantity_names(organs, quantities_for_each_organ);
}

void thornley_biomass::do_multi_organ_operation() const
{
    for (size_t i = 0; i < organs.size(); ++i) {
        double organ_mass_change = *total_C_change_per_m2_ips[i] * (*carbon_to_mass_factor_ips[i]);  // Mg Dry Mass / ha
        update(mass_ops[i], organ_mass_change);
    }
}
