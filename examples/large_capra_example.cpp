#include "../include/tensor_planner.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

enum ObjectId : int32_t {
  OBJ_FABRIZIO = 0,
  OBJ_LUPO = 1,
  OBJ_CAPRA = 2,
  OBJ_LEFT_BANK = 3,
  OBJ_MIDDLE_BANK = 4,
  OBJ_RIGHT_BANK = 5,
  OBJ_BARCA = 6,
  OBJ_CAVOLO = 7,
};

enum TypeId : int32_t {
  TYPE_AGENT = 0,
  TYPE_ITEM = 1,
  TYPE_LOCATION = 2,
  TYPE_VEHICLE = 3,
};

enum PredicateId : int32_t {
  PRED_GOAT = 0,
  PRED_FARMER = 1,
  PRED_WOLF = 2,
  PRED_CABBAGE = 3,
  PRED_BOAT = 4,
  PRED_AT_AGENT = 5,
  PRED_AT_ITEM = 6,
  PRED_AT_VEHICLE = 7,
  PRED_ON_AGENT = 8,
  PRED_ON_ITEM = 9,
  PRED_RIVER_BANK = 10,
  PRED_RIVER = 11,
};

enum SchemaId : int32_t {
  SCHEMA_BOARD_GOAT = 0,
  SCHEMA_BOARD_WOLF_NO_GOAT = 1,
  SCHEMA_BOARD_WOLF_NO_CABBAGE = 2,
  SCHEMA_BOARD_CABBAGE_NO_GOAT = 3,
  SCHEMA_BOARD_CABBAGE_NO_WOLF = 4,
  SCHEMA_BOARD_ALONE_NO_GOAT = 5,
  SCHEMA_BOARD_ALONE_NO_WOLF_AND_CABBAGE = 6,
  SCHEMA_ROWL = 7,
  SCHEMA_UNBOARD = 8,
  SCHEMA_UNBOARD_ALONE = 9,
};

void require_status(TP_Status status, const char *message) {
  if (status != TP_STATUS_OK) {
    std::cerr << message << " failed with status " << status << '\n';
    std::exit(1);
  }
}

void require_true(bool value, const char *message) {
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void add_unary_fact(TP_State *state, int32_t predicate_id, int32_t object_id) {
  const int32_t args[1] = {object_id};
  require_status(tp_state_add_fact(state, predicate_id, 1, args), "add unary fact");
}

void add_binary_fact(TP_State *state, int32_t predicate_id, int32_t arg0, int32_t arg1) {
  const int32_t args[2] = {arg0, arg1};
  require_status(tp_state_add_fact(state, predicate_id, 2, args), "add binary fact");
}

void add_goal_at(TP_State *state, int32_t object_id, int32_t location_id) {
  const int32_t args[2] = {object_id, location_id};
  if (object_id == OBJ_FABRIZIO) {
    require_status(tp_state_add_goal_fact(state, PRED_AT_AGENT, 2, args), "add goal fact");
  } else {
    require_status(tp_state_add_goal_fact(state, PRED_AT_ITEM, 2, args), "add goal fact");
  }
}

std::string object_name(int32_t object_id) {
  switch (object_id) {
    case OBJ_FABRIZIO:
      return "fabrizio";
    case OBJ_LUPO:
      return "lupo";
    case OBJ_CAPRA:
      return "capra";
    case OBJ_LEFT_BANK:
      return "left_bank";
    case OBJ_MIDDLE_BANK:
      return "middle_bank";
    case OBJ_RIGHT_BANK:
      return "right_bank";
    case OBJ_BARCA:
      return "barca";
    case OBJ_CAVOLO:
      return "cavolo";
    default:
      return std::to_string(object_id);
  }
}

std::string readable_action(const TP_Candidate_Action &action) {
  switch (action.schema_id) {
    case SCHEMA_BOARD_GOAT:
      return "board_goat(fabrizio, capra, " + object_name(action.args[3]) + ")";
    case SCHEMA_BOARD_WOLF_NO_GOAT:
    case SCHEMA_BOARD_WOLF_NO_CABBAGE:
      return "board_wolf(fabrizio, lupo, " + object_name(action.args[3]) + ")";
    case SCHEMA_BOARD_CABBAGE_NO_GOAT:
    case SCHEMA_BOARD_CABBAGE_NO_WOLF:
      return "board_cabbage(fabrizio, cavolo, " + object_name(action.args[3]) + ")";
    case SCHEMA_BOARD_ALONE_NO_GOAT:
    case SCHEMA_BOARD_ALONE_NO_WOLF_AND_CABBAGE:
      return "board_alone(fabrizio, " + object_name(action.args[2]) + ")";
    case SCHEMA_ROWL:
      return "rowl(barca, " + object_name(action.args[1]) + " -> " + object_name(action.args[2]) + ")";
    case SCHEMA_UNBOARD:
      return "unboard(fabrizio, " + object_name(action.args[1]) + ", " + object_name(action.args[3]) + ")";
    case SCHEMA_UNBOARD_ALONE:
      return "unboard_alone(fabrizio, " + object_name(action.args[2]) + ")";
    default:
      return "unknown";
  }
}

int32_t add_predicate(TP_Domain *domain, uint8_t arity, std::initializer_list<int32_t> arg_types) {
  TP_Predicate_Def predicate {.arity = arity, .arg_types = {0, 0, 0, 0}};
  int index = 0;
  for (const int32_t type_id : arg_types) {
    predicate.arg_types[index++] = type_id;
  }
  int32_t predicate_id = -1;
  require_status(tp_domain_add_predicate(domain, &predicate, &predicate_id), "add predicate");
  return predicate_id;
}

void add_action(
  TP_Domain *domain,
  uint8_t arity,
  const int32_t *arg_types,
  const std::vector<TP_Action_Literal> &preconditions,
  const std::vector<TP_Action_Effect> &effects,
  int32_t expected_schema_id
) {
  int32_t schema_id = -1;
  require_status(
    tp_domain_add_action_schema(
      domain,
      arity,
      arg_types,
      preconditions.data(),
      static_cast<int32_t>(preconditions.size()),
      effects.data(),
      static_cast<int32_t>(effects.size()),
      nullptr,
      0,
      nullptr,
      0,
      &schema_id
    ),
    "add action schema"
  );
  require_true(schema_id == expected_schema_id, "unexpected schema id");
}

void build_domain(TP_Domain *domain) {
  add_predicate(domain, 1, {TYPE_ITEM});
  add_predicate(domain, 1, {TYPE_AGENT});
  add_predicate(domain, 1, {TYPE_ITEM});
  add_predicate(domain, 1, {TYPE_ITEM});
  add_predicate(domain, 1, {TYPE_VEHICLE});
  add_predicate(domain, 2, {TYPE_AGENT, TYPE_LOCATION});
  add_predicate(domain, 2, {TYPE_ITEM, TYPE_LOCATION});
  add_predicate(domain, 2, {TYPE_VEHICLE, TYPE_LOCATION});
  add_predicate(domain, 2, {TYPE_AGENT, TYPE_VEHICLE});
  add_predicate(domain, 2, {TYPE_ITEM, TYPE_VEHICLE});
  add_predicate(domain, 1, {TYPE_LOCATION});
  add_predicate(domain, 2, {TYPE_LOCATION, TYPE_LOCATION});

  const int32_t board_goat_types[4] = {TYPE_AGENT, TYPE_ITEM, TYPE_VEHICLE, TYPE_LOCATION};
  const int32_t board_carry_types[6] = {TYPE_AGENT, TYPE_ITEM, TYPE_VEHICLE, TYPE_LOCATION, TYPE_ITEM, TYPE_ITEM};
  const int32_t board_alone_safe_types[4] = {TYPE_AGENT, TYPE_VEHICLE, TYPE_LOCATION, TYPE_ITEM};
  const int32_t board_alone_types[6] = {TYPE_AGENT, TYPE_VEHICLE, TYPE_LOCATION, TYPE_ITEM, TYPE_ITEM, TYPE_ITEM};
  const int32_t row_types[4] = {TYPE_VEHICLE, TYPE_LOCATION, TYPE_LOCATION, TYPE_AGENT};
  const int32_t unboard_types[4] = {TYPE_AGENT, TYPE_ITEM, TYPE_VEHICLE, TYPE_LOCATION};
  const int32_t unboard_alone_types[6] = {TYPE_AGENT, TYPE_VEHICLE, TYPE_LOCATION, TYPE_ITEM, TYPE_ITEM, TYPE_ITEM};

  add_action(domain, 4, board_goat_types,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_AT_VEHICLE, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_ON_AGENT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON_ITEM, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_BOARD_GOAT);

  add_action(domain, 6, board_carry_types,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_WOLF, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_CABBAGE, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_AT_VEHICLE, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .sign = -1, .arity = 2, .slots = {4, 3, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_ON_AGENT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON_ITEM, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_BOARD_WOLF_NO_GOAT);

  add_action(domain, 6, board_carry_types,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_WOLF, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_CABBAGE, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_AT_VEHICLE, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .sign = -1, .arity = 2, .slots = {5, 3, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_ON_AGENT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON_ITEM, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_BOARD_WOLF_NO_CABBAGE);

  add_action(domain, 6, board_carry_types,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_CABBAGE, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_WOLF, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_AT_VEHICLE, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .sign = -1, .arity = 2, .slots = {4, 3, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_ON_AGENT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON_ITEM, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_BOARD_CABBAGE_NO_GOAT);

  add_action(domain, 6, board_carry_types,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_CABBAGE, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_WOLF, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_AT_VEHICLE, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .sign = -1, .arity = 2, .slots = {5, 3, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_ON_AGENT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON_ITEM, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_BOARD_CABBAGE_NO_WOLF);

  add_action(domain, 4, board_alone_safe_types,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_AT_VEHICLE, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .sign = -1, .arity = 2, .slots = {3, 2, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON_AGENT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 1, 0, 0}},
    },
    SCHEMA_BOARD_ALONE_NO_GOAT);

  add_action(domain, 6, board_alone_types,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_WOLF, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_CABBAGE, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_AT_AGENT, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_AT_VEHICLE, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .sign = -1, .arity = 2, .slots = {3, 2, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .sign = -1, .arity = 2, .slots = {4, 2, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON_AGENT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 1, 0, 0}},
    },
    SCHEMA_BOARD_ALONE_NO_WOLF_AND_CABBAGE);

  add_action(domain, 4, row_types,
    {
      {.predicate_id = PRED_ON_AGENT, .sign = 1, .arity = 2, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_AT_VEHICLE, .sign = 1, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_RIVER, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_VEHICLE, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_AT_VEHICLE, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
    },
    SCHEMA_ROWL);

  add_action(domain, 4, unboard_types,
    {
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_AT_VEHICLE, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
      {.predicate_id = PRED_ON_AGENT, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON_ITEM, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT_ITEM, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_ON_AGENT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON_ITEM, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_UNBOARD);

  add_action(domain, 6, unboard_alone_types,
    {
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_WOLF, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_CABBAGE, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_ON_AGENT, .sign = 1, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_AT_VEHICLE, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_ON_ITEM, .sign = -1, .arity = 2, .slots = {3, 1, 0, 0}},
      {.predicate_id = PRED_ON_ITEM, .sign = -1, .arity = 2, .slots = {4, 1, 0, 0}},
      {.predicate_id = PRED_ON_ITEM, .sign = -1, .arity = 2, .slots = {5, 1, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT_AGENT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON_AGENT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 1, 0, 0}},
    },
    SCHEMA_UNBOARD_ALONE);
}

}  // namespace

int main() {
  const TP_Limits limits {
    .max_objects = 24,
    .max_facts = 256,
    .max_goals = 8,
    .max_candidates = 512,
    .max_expansions = 20000,
    .max_plan_length = 80,
  };

  TP_Domain *domain = tp_domain_create(&limits);
  require_true(domain != nullptr, "tp_domain_create failed");
  build_domain(domain);

  const int32_t object_types[8] = {
    TYPE_AGENT, TYPE_ITEM, TYPE_ITEM, TYPE_LOCATION,
    TYPE_LOCATION, TYPE_LOCATION, TYPE_VEHICLE, TYPE_ITEM,
  };
  TP_State *state = tp_state_create(domain, 8, object_types);
  require_true(state != nullptr, "tp_state_create failed");

  add_unary_fact(state, PRED_RIVER_BANK, OBJ_LEFT_BANK);
  add_unary_fact(state, PRED_RIVER_BANK, OBJ_MIDDLE_BANK);
  add_unary_fact(state, PRED_RIVER_BANK, OBJ_RIGHT_BANK);
  add_unary_fact(state, PRED_GOAT, OBJ_CAPRA);
  add_unary_fact(state, PRED_FARMER, OBJ_FABRIZIO);
  add_unary_fact(state, PRED_WOLF, OBJ_LUPO);
  add_unary_fact(state, PRED_CABBAGE, OBJ_CAVOLO);
  add_unary_fact(state, PRED_BOAT, OBJ_BARCA);

  add_binary_fact(state, PRED_RIVER, OBJ_LEFT_BANK, OBJ_MIDDLE_BANK);
  add_binary_fact(state, PRED_RIVER, OBJ_MIDDLE_BANK, OBJ_LEFT_BANK);
  add_binary_fact(state, PRED_RIVER, OBJ_MIDDLE_BANK, OBJ_RIGHT_BANK);
  add_binary_fact(state, PRED_RIVER, OBJ_RIGHT_BANK, OBJ_MIDDLE_BANK);

  add_binary_fact(state, PRED_AT_ITEM, OBJ_CAPRA, OBJ_LEFT_BANK);
  add_binary_fact(state, PRED_AT_AGENT, OBJ_FABRIZIO, OBJ_LEFT_BANK);
  add_binary_fact(state, PRED_AT_ITEM, OBJ_LUPO, OBJ_LEFT_BANK);
  add_binary_fact(state, PRED_AT_ITEM, OBJ_CAVOLO, OBJ_LEFT_BANK);
  add_binary_fact(state, PRED_AT_VEHICLE, OBJ_BARCA, OBJ_LEFT_BANK);

  add_goal_at(state, OBJ_LUPO, OBJ_RIGHT_BANK);
  add_goal_at(state, OBJ_CAPRA, OBJ_RIGHT_BANK);
  add_goal_at(state, OBJ_CAVOLO, OBJ_RIGHT_BANK);
  add_goal_at(state, OBJ_FABRIZIO, OBJ_RIGHT_BANK);

  TP_Solver *solver = tp_solver_create(domain);
  require_true(solver != nullptr, "tp_solver_create failed");

  TP_Action_Graph graph {};
  require_status(tp_state_export_action_graph(domain, state, 64, &graph), "export action graph");
  std::cout << "Large Capra graph nodes: objects=" << graph.object_node_count
            << " facts=" << graph.fact_node_count
            << " actions=" << graph.action_node_count
            << " total=" << graph.total_node_count << "\n";
  std::cout << "Large Capra graph edges: " << graph.edge_count << "\n";
  tp_action_graph_dispose(&graph);

  TP_Solve_Result greedy {};
  require_status(tp_solver_set_mode(solver, TP_SOLVER_MODE_PURE_GREEDY), "set pure greedy mode");
  const auto greedy_start = std::chrono::steady_clock::now();
  const TP_Status greedy_status = tp_solver_solve(solver, state, &greedy);
  const auto greedy_end = std::chrono::steady_clock::now();
  const auto greedy_us = std::chrono::duration_cast<std::chrono::microseconds>(greedy_end - greedy_start);

  require_status(tp_solver_use_tensor_baseline_scorer(solver), "set scorer");

  TP_Solve_Result guided {};
  require_status(tp_solver_set_mode(solver, TP_SOLVER_MODE_GUIDED_MIXED), "set guided mode");
  const auto guided_start = std::chrono::steady_clock::now();
  const TP_Status guided_status = tp_solver_solve(solver, state, &guided);
  const auto guided_end = std::chrono::steady_clock::now();
  const auto guided_us = std::chrono::duration_cast<std::chrono::microseconds>(guided_end - guided_start);

  TP_Solve_Result optimal {};
  require_status(tp_solver_set_mode(solver, TP_SOLVER_MODE_SYMBOLIC_ASTAR), "set symbolic astar mode");
  TP_Solve_Result symbolic {};
  const auto symbolic_start = std::chrono::steady_clock::now();
  const TP_Status symbolic_status = tp_solver_solve(solver, state, &symbolic);
  const auto symbolic_end = std::chrono::steady_clock::now();
  const auto symbolic_us = std::chrono::duration_cast<std::chrono::microseconds>(symbolic_end - symbolic_start);

  require_status(tp_solver_set_mode(solver, TP_SOLVER_MODE_OPTIMAL_DEBUG), "set optimal debug mode");
  const auto optimal_start = std::chrono::steady_clock::now();
  const TP_Status optimal_status = tp_solver_solve_optimal_debug(solver, state, &optimal);
  const auto optimal_end = std::chrono::steady_clock::now();
  const auto optimal_us = std::chrono::duration_cast<std::chrono::microseconds>(optimal_end - optimal_start);

  std::cout << "Large Capra greedy status: " << greedy_status << "\n";
  std::cout << "Large Capra greedy solved: " << (greedy.solved ? "yes" : "no") << "\n";
  std::cout << "Large Capra greedy time (us): " << greedy_us.count() << "\n";
  std::cout << "Large Capra greedy plan length: " << greedy.plan_length << "\n";
  std::cout << "Large Capra greedy expansions: " << greedy.expansions << "\n";
  std::cout << "Large Capra greedy generated candidates: " << greedy.generated << "\n";
  std::cout << "Large Capra greedy readable plan:\n";
  for (int32_t index = 0; index < greedy.plan_length; ++index) {
    std::cout << "  " << (index + 1) << ". " << readable_action(greedy.plan_actions[index]) << "\n";
  }

  std::cout << "Large Capra guided status: " << guided_status << "\n";
  std::cout << "Large Capra guided solved: " << (guided.solved ? "yes" : "no") << "\n";
  std::cout << "Large Capra guided time (us): " << guided_us.count() << "\n";
  std::cout << "Large Capra guided plan length: " << guided.plan_length << "\n";
  std::cout << "Large Capra guided expansions: " << guided.expansions << "\n";
  std::cout << "Large Capra guided generated candidates: " << guided.generated << "\n";
  std::cout << "Large Capra guided scorer calls: " << guided.scorer_calls << "\n";
  std::cout << "Large Capra guided scorer export time (us): " << guided.scorer_export_time_us << "\n";
  std::cout << "Large Capra guided scorer callback time (us): " << guided.scorer_time_us << "\n";
  std::cout << "Large Capra guided scorer sort time (us): " << guided.scorer_sort_time_us << "\n";
  std::cout << "Large Capra guided readable plan:\n";
  for (int32_t index = 0; index < guided.plan_length; ++index) {
    std::cout << "  " << (index + 1) << ". " << readable_action(guided.plan_actions[index]) << "\n";
  }

  std::cout << "Large Capra symbolic status: " << symbolic_status << "\n";
  std::cout << "Large Capra symbolic solved: " << (symbolic.solved ? "yes" : "no") << "\n";
  std::cout << "Large Capra symbolic time (us): " << symbolic_us.count() << "\n";
  std::cout << "Large Capra symbolic plan length: " << symbolic.plan_length << "\n";
  std::cout << "Large Capra symbolic expansions: " << symbolic.expansions << "\n";
  std::cout << "Large Capra symbolic generated candidates: " << symbolic.generated << "\n";
  std::cout << "Large Capra symbolic readable plan:\n";
  for (int32_t index = 0; index < symbolic.plan_length; ++index) {
    std::cout << "  " << (index + 1) << ". " << readable_action(symbolic.plan_actions[index]) << "\n";
  }

  std::cout << "Large Capra optimal status: " << optimal_status << "\n";
  std::cout << "Large Capra optimal solved: " << (optimal.solved ? "yes" : "no") << "\n";
  std::cout << "Large Capra optimal time (us): " << optimal_us.count() << "\n";
  std::cout << "Large Capra optimal plan length: " << optimal.plan_length << "\n";
  std::cout << "Large Capra optimal expansions: " << optimal.expansions << "\n";
  std::cout << "Large Capra optimal generated candidates: " << optimal.generated << "\n";
  std::cout << "Large Capra optimal readable plan:\n";
  for (int32_t index = 0; index < optimal.plan_length; ++index) {
    std::cout << "  " << (index + 1) << ". " << readable_action(optimal.plan_actions[index]) << "\n";
  }

  tp_solve_result_dispose(&greedy);
  tp_solve_result_dispose(&guided);
  tp_solve_result_dispose(&symbolic);
  tp_solve_result_dispose(&optimal);
  tp_solver_destroy(solver);
  tp_state_destroy(state);
  tp_domain_destroy(domain);
  return greedy_status == TP_STATUS_OK && guided_status == TP_STATUS_OK &&
      symbolic_status == TP_STATUS_OK && optimal_status == TP_STATUS_OK ? 0 : 1;
}
