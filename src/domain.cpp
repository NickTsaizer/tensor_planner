#include "planner_internal.hpp"

namespace {

bool is_valid_slot(int8_t slot, uint8_t action_arity) {
  return slot >= 0 && slot < static_cast<int8_t>(action_arity);
}

bool is_valid_predicate_id(const TP_Domain &domain, int32_t predicate_id) {
  return predicate_id >= 0 && predicate_id < static_cast<int32_t>(domain.predicates.size());
}

}  // namespace

bool is_valid_limits(const TP_Limits &limits) {
  return limits.max_objects > 0 &&
    limits.max_facts > 0 &&
    limits.max_goals > 0 &&
    limits.max_candidates > 0 &&
    limits.max_expansions > 0 &&
    limits.max_plan_length > 0;
}

bool is_valid_predicate_def(const TP_Predicate_Def &predicate_def) {
  if (predicate_def.arity > TP_MAX_ARITY) {
    return false;
  }

  for (uint8_t index = 0; index < predicate_def.arity; ++index) {
    if (predicate_def.arg_types[index] < 0) {
      return false;
    }
  }

  return true;
}

bool is_valid_function_def(const TP_Function_Def &function_def) {
  if (function_def.arity > TP_MAX_ARITY) {
    return false;
  }

  for (uint8_t index = 0; index < function_def.arity; ++index) {
    if (function_def.arg_types[index] < 0) {
      return false;
    }
  }

  return true;
}

bool validate_action_schema(
  const TP_Domain &domain,
  uint8_t arity,
  const int32_t *arg_types,
  const TP_Action_Literal *preconditions,
  int32_t precondition_count,
  const TP_Action_Effect *effects,
  int32_t effect_count,
  const TP_Numeric_Precondition *numeric_preconditions,
  int32_t numeric_precondition_count,
  const TP_Numeric_Effect *numeric_effects,
  int32_t numeric_effect_count
) {
  if (arity > TP_MAX_PARAMS || arg_types == nullptr) {
    return false;
  }

  for (uint8_t index = 0; index < arity; ++index) {
    if (arg_types[index] < 0) {
      return false;
    }
  }

  if (precondition_count < 0 || effect_count < 0 || numeric_precondition_count < 0 ||
      numeric_effect_count < 0) {
    return false;
  }

  for (int32_t index = 0; index < precondition_count; ++index) {
    const TP_Action_Literal &literal = preconditions[index];
    if (!is_valid_predicate_id(domain, literal.predicate_id)) {
      return false;
    }

    const PredicateDef &predicate = domain.predicates[static_cast<std::size_t>(literal.predicate_id)];
    if (literal.arity != predicate.arity || literal.sign == 0) {
      return false;
    }

    for (uint8_t slot_index = 0; slot_index < literal.arity; ++slot_index) {
      if (!is_valid_slot(literal.slots[slot_index], arity)) {
        return false;
      }

      const int32_t slot_type = arg_types[static_cast<std::size_t>(literal.slots[slot_index])];
      if (slot_type != predicate.arg_types[slot_index]) {
        return false;
      }
    }
  }

  for (int32_t index = 0; index < effect_count; ++index) {
    const TP_Action_Effect &effect = effects[index];
    if (!is_valid_predicate_id(domain, effect.predicate_id)) {
      return false;
    }

    const PredicateDef &predicate = domain.predicates[static_cast<std::size_t>(effect.predicate_id)];
    if (effect.arity != predicate.arity) {
      return false;
    }

    if (effect.op != TP_EFFECT_ADD && effect.op != TP_EFFECT_DELETE) {
      return false;
    }

    for (uint8_t slot_index = 0; slot_index < effect.arity; ++slot_index) {
      if (!is_valid_slot(effect.slots[slot_index], arity)) {
        return false;
      }

      const int32_t slot_type = arg_types[static_cast<std::size_t>(effect.slots[slot_index])];
      if (slot_type != predicate.arg_types[slot_index]) {
        return false;
      }
    }
  }

  for (int32_t index = 0; index < numeric_precondition_count; ++index) {
    const TP_Numeric_Precondition &precondition = numeric_preconditions[index];
    if (precondition.function_id < 0 ||
        precondition.function_id >= static_cast<int32_t>(domain.functions.size())) {
      return false;
    }

    const FunctionDef &function = domain.functions[static_cast<std::size_t>(precondition.function_id)];
    if (precondition.arity != function.arity) {
      return false;
    }

    if (precondition.cmp_op < TP_NUM_CMP_LT || precondition.cmp_op > TP_NUM_CMP_GT) {
      return false;
    }

    for (uint8_t slot_index = 0; slot_index < precondition.arity; ++slot_index) {
      if (!is_valid_slot(precondition.slots[slot_index], arity)) {
        return false;
      }

      const int32_t slot_type = arg_types[static_cast<std::size_t>(precondition.slots[slot_index])];
      if (slot_type != function.arg_types[slot_index]) {
        return false;
      }
    }
  }

  for (int32_t index = 0; index < numeric_effect_count; ++index) {
    const TP_Numeric_Effect &effect = numeric_effects[index];
    if (effect.function_id < 0 || effect.function_id >= static_cast<int32_t>(domain.functions.size())) {
      return false;
    }

    const FunctionDef &function = domain.functions[static_cast<std::size_t>(effect.function_id)];
    if (effect.arity != function.arity) {
      return false;
    }

    if (effect.op < TP_NUM_EFFECT_SET || effect.op > TP_NUM_EFFECT_SUBTRACT) {
      return false;
    }

    for (uint8_t slot_index = 0; slot_index < effect.arity; ++slot_index) {
      if (!is_valid_slot(effect.slots[slot_index], arity)) {
        return false;
      }

      const int32_t slot_type = arg_types[static_cast<std::size_t>(effect.slots[slot_index])];
      if (slot_type != function.arg_types[slot_index]) {
        return false;
      }
    }
  }

  return true;
}

TP_Status export_schema_tensors_impl(
  const TP_Domain &domain,
  TP_Schema_Tensors *out_tensors
) {
  if (out_tensors == nullptr) {
    return TP_STATUS_INVALID_ARGUMENT;
  }

  zero_schema_tensors(out_tensors);

  const int32_t predicate_count = static_cast<int32_t>(domain.predicates.size());
  const int32_t function_count = static_cast<int32_t>(domain.functions.size());
  const int32_t action_count = static_cast<int32_t>(domain.actions.size());

  int32_t max_preconditions = 0;
  int32_t max_effects = 0;
  int32_t max_num_preconditions = 0;
  int32_t max_num_effects = 0;
  for (const ActionSchema &action : domain.actions) {
    max_preconditions = std::max(max_preconditions, static_cast<int32_t>(action.preconditions.size()));
    max_effects = std::max(max_effects, static_cast<int32_t>(action.effects.size()));
    max_num_preconditions = std::max(
      max_num_preconditions,
      static_cast<int32_t>(action.numeric_preconditions.size())
    );
    max_num_effects = std::max(max_num_effects, static_cast<int32_t>(action.numeric_effects.size()));
  }

  out_tensors->predicate_count = predicate_count;
  out_tensors->function_count = function_count;
  out_tensors->action_count = action_count;
  out_tensors->max_preconditions = max_preconditions;
  out_tensors->max_effects = max_effects;
  out_tensors->max_num_preconditions = max_num_preconditions;
  out_tensors->max_num_effects = max_num_effects;
  out_tensors->pred_arity_count = predicate_count;
  out_tensors->pred_arg_type_count = predicate_count * TP_MAX_ARITY;
  out_tensors->func_arity_count = function_count;
  out_tensors->func_arg_type_count = function_count * TP_MAX_ARITY;
  out_tensors->action_arity_count = action_count;
  out_tensors->action_arg_type_count = action_count * TP_MAX_PARAMS;
  out_tensors->pre_pred_id_count = action_count * max_preconditions;
  out_tensors->pre_sign_count = action_count * max_preconditions;
  out_tensors->pre_slot_count = action_count * max_preconditions * TP_MAX_ARITY;
  out_tensors->eff_pred_id_count = action_count * max_effects;
  out_tensors->eff_op_count = action_count * max_effects;
  out_tensors->eff_slot_count = action_count * max_effects * TP_MAX_ARITY;
  out_tensors->num_pre_func_id_count = action_count * max_num_preconditions;
  out_tensors->num_pre_cmp_count = action_count * max_num_preconditions;
  out_tensors->num_pre_slot_count = action_count * max_num_preconditions * TP_MAX_ARITY;
  out_tensors->num_pre_value_count = action_count * max_num_preconditions;
  out_tensors->num_eff_func_id_count = action_count * max_num_effects;
  out_tensors->num_eff_op_count = action_count * max_num_effects;
  out_tensors->num_eff_slot_count = action_count * max_num_effects * TP_MAX_ARITY;
  out_tensors->num_eff_value_count = action_count * max_num_effects;

  out_tensors->pred_arity = static_cast<uint8_t *>(std::calloc(out_tensors->pred_arity_count, sizeof(uint8_t)));
  out_tensors->pred_arg_type = static_cast<int32_t *>(std::calloc(out_tensors->pred_arg_type_count, sizeof(int32_t)));
  out_tensors->func_arity = static_cast<uint8_t *>(std::calloc(out_tensors->func_arity_count, sizeof(uint8_t)));
  out_tensors->func_arg_type = static_cast<int32_t *>(std::calloc(out_tensors->func_arg_type_count, sizeof(int32_t)));
  out_tensors->action_arity = static_cast<uint8_t *>(std::calloc(out_tensors->action_arity_count, sizeof(uint8_t)));
  out_tensors->action_arg_type = static_cast<int32_t *>(std::calloc(out_tensors->action_arg_type_count, sizeof(int32_t)));
  out_tensors->pre_pred_id = static_cast<int16_t *>(std::calloc(out_tensors->pre_pred_id_count, sizeof(int16_t)));
  out_tensors->pre_sign = static_cast<int8_t *>(std::calloc(out_tensors->pre_sign_count, sizeof(int8_t)));
  out_tensors->pre_slot = static_cast<int8_t *>(std::calloc(out_tensors->pre_slot_count, sizeof(int8_t)));
  out_tensors->eff_pred_id = static_cast<int16_t *>(std::calloc(out_tensors->eff_pred_id_count, sizeof(int16_t)));
  out_tensors->eff_op = static_cast<int8_t *>(std::calloc(out_tensors->eff_op_count, sizeof(int8_t)));
  out_tensors->eff_slot = static_cast<int8_t *>(std::calloc(out_tensors->eff_slot_count, sizeof(int8_t)));
  out_tensors->num_pre_func_id = static_cast<int16_t *>(std::calloc(out_tensors->num_pre_func_id_count, sizeof(int16_t)));
  out_tensors->num_pre_cmp = static_cast<int8_t *>(std::calloc(out_tensors->num_pre_cmp_count, sizeof(int8_t)));
  out_tensors->num_pre_slot = static_cast<int8_t *>(std::calloc(out_tensors->num_pre_slot_count, sizeof(int8_t)));
  out_tensors->num_pre_value = static_cast<float *>(std::calloc(out_tensors->num_pre_value_count, sizeof(float)));
  out_tensors->num_eff_func_id = static_cast<int16_t *>(std::calloc(out_tensors->num_eff_func_id_count, sizeof(int16_t)));
  out_tensors->num_eff_op = static_cast<int8_t *>(std::calloc(out_tensors->num_eff_op_count, sizeof(int8_t)));
  out_tensors->num_eff_slot = static_cast<int8_t *>(std::calloc(out_tensors->num_eff_slot_count, sizeof(int8_t)));
  out_tensors->num_eff_value = static_cast<float *>(std::calloc(out_tensors->num_eff_value_count, sizeof(float)));

  for (int32_t predicate_index = 0; predicate_index < predicate_count; ++predicate_index) {
    const PredicateDef &predicate = domain.predicates[static_cast<std::size_t>(predicate_index)];
    out_tensors->pred_arity[predicate_index] = predicate.arity;
    for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
      const int32_t offset = predicate_index * TP_MAX_ARITY + arg_index;
      out_tensors->pred_arg_type[offset] = predicate.arg_types[static_cast<std::size_t>(arg_index)];
    }
  }

  for (int32_t function_index = 0; function_index < function_count; ++function_index) {
    const FunctionDef &function = domain.functions[static_cast<std::size_t>(function_index)];
    out_tensors->func_arity[function_index] = function.arity;
    for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
      const int32_t offset = function_index * TP_MAX_ARITY + arg_index;
      out_tensors->func_arg_type[offset] = function.arg_types[static_cast<std::size_t>(arg_index)];
    }
  }

  for (int32_t action_index = 0; action_index < action_count; ++action_index) {
    const ActionSchema &action = domain.actions[static_cast<std::size_t>(action_index)];
    out_tensors->action_arity[action_index] = action.arity;

    for (int32_t arg_index = 0; arg_index < TP_MAX_PARAMS; ++arg_index) {
      const int32_t offset = action_index * TP_MAX_PARAMS + arg_index;
      out_tensors->action_arg_type[offset] = action.arg_types[static_cast<std::size_t>(arg_index)];
    }

    for (int32_t pre_index = 0; pre_index < static_cast<int32_t>(action.preconditions.size()); ++pre_index) {
      const ActionLiteral &precondition = action.preconditions[static_cast<std::size_t>(pre_index)];
      const int32_t row = action_index * max_preconditions + pre_index;
      out_tensors->pre_pred_id[row] = static_cast<int16_t>(precondition.predicate_id);
      out_tensors->pre_sign[row] = precondition.sign;
      for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
        const int32_t offset = row * TP_MAX_ARITY + arg_index;
        out_tensors->pre_slot[offset] = precondition.slots[static_cast<std::size_t>(arg_index)];
      }
    }

    for (int32_t eff_index = 0; eff_index < static_cast<int32_t>(action.effects.size()); ++eff_index) {
      const ActionEffect &effect = action.effects[static_cast<std::size_t>(eff_index)];
      const int32_t row = action_index * max_effects + eff_index;
      out_tensors->eff_pred_id[row] = static_cast<int16_t>(effect.predicate_id);
      out_tensors->eff_op[row] = static_cast<int8_t>(effect.op);
      for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
        const int32_t offset = row * TP_MAX_ARITY + arg_index;
        out_tensors->eff_slot[offset] = effect.slots[static_cast<std::size_t>(arg_index)];
      }
    }

    for (int32_t num_pre_index = 0;
         num_pre_index < static_cast<int32_t>(action.numeric_preconditions.size());
         ++num_pre_index) {
      const NumericPrecondition &precondition =
        action.numeric_preconditions[static_cast<std::size_t>(num_pre_index)];
      const int32_t row = action_index * max_num_preconditions + num_pre_index;
      out_tensors->num_pre_func_id[row] = static_cast<int16_t>(precondition.function_id);
      out_tensors->num_pre_cmp[row] = static_cast<int8_t>(precondition.cmp_op);
      out_tensors->num_pre_value[row] = precondition.rhs_value;
      for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
        const int32_t offset = row * TP_MAX_ARITY + arg_index;
        out_tensors->num_pre_slot[offset] = precondition.slots[static_cast<std::size_t>(arg_index)];
      }
    }

    for (int32_t num_eff_index = 0;
         num_eff_index < static_cast<int32_t>(action.numeric_effects.size());
         ++num_eff_index) {
      const NumericEffect &effect = action.numeric_effects[static_cast<std::size_t>(num_eff_index)];
      const int32_t row = action_index * max_num_effects + num_eff_index;
      out_tensors->num_eff_func_id[row] = static_cast<int16_t>(effect.function_id);
      out_tensors->num_eff_op[row] = static_cast<int8_t>(effect.op);
      out_tensors->num_eff_value[row] = effect.rhs_value;
      for (int32_t arg_index = 0; arg_index < TP_MAX_ARITY; ++arg_index) {
        const int32_t offset = row * TP_MAX_ARITY + arg_index;
        out_tensors->num_eff_slot[offset] = effect.slots[static_cast<std::size_t>(arg_index)];
      }
    }
  }

  return TP_STATUS_OK;
}

void zero_schema_tensors(TP_Schema_Tensors *tensors) {
  if (tensors != nullptr) {
    std::memset(tensors, 0, sizeof(TP_Schema_Tensors));
  }
}
