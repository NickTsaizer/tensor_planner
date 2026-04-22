#include "../include/tensor_planner.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

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

struct CapraIds {
  int32_t fabrizio = 0;
  int32_t lupo = 1;
  int32_t capra = 2;
  int32_t barca = 3;
  int32_t cavolo = 4;
  std::vector<int32_t> locations;
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

void add_goal_at_item(TP_State *state, int32_t object_id, int32_t location_id) {
  const int32_t args[2] = {object_id, location_id};
  require_status(tp_state_add_goal_fact(state, PRED_AT_ITEM, 2, args), "add goal fact");
}

void add_goal_at_agent(TP_State *state, int32_t object_id, int32_t location_id) {
  const int32_t args[2] = {object_id, location_id};
  require_status(tp_state_add_goal_fact(state, PRED_AT_AGENT, 2, args), "add goal fact");
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

CapraIds make_ids(int location_count) {
  CapraIds ids {};
  ids.fabrizio = 0;
  ids.lupo = 1;
  ids.capra = 2;
  ids.barca = 3;
  ids.cavolo = 4;
  for (int index = 0; index < location_count; ++index) {
    ids.locations.push_back(5 + index);
  }
  return ids;
}

TP_State *build_problem_state(TP_Domain *domain, const CapraIds &ids, int location_count) {
  std::vector<int32_t> object_types;
  object_types.reserve(5 + location_count);
  object_types.push_back(TYPE_AGENT);
  object_types.push_back(TYPE_ITEM);
  object_types.push_back(TYPE_ITEM);
  object_types.push_back(TYPE_VEHICLE);
  object_types.push_back(TYPE_ITEM);
  for (int index = 0; index < location_count; ++index) {
    object_types.push_back(TYPE_LOCATION);
  }

  TP_State *state = tp_state_create(domain, static_cast<int32_t>(object_types.size()), object_types.data());
  require_true(state != nullptr, "tp_state_create failed");

  for (const int32_t location : ids.locations) {
    add_unary_fact(state, PRED_RIVER_BANK, location);
  }
  add_unary_fact(state, PRED_GOAT, ids.capra);
  add_unary_fact(state, PRED_FARMER, ids.fabrizio);
  add_unary_fact(state, PRED_WOLF, ids.lupo);
  add_unary_fact(state, PRED_CABBAGE, ids.cavolo);
  add_unary_fact(state, PRED_BOAT, ids.barca);

  for (int index = 0; index < location_count - 1; ++index) {
    add_binary_fact(state, PRED_RIVER, ids.locations[index], ids.locations[index + 1]);
    add_binary_fact(state, PRED_RIVER, ids.locations[index + 1], ids.locations[index]);
  }

  const int32_t left_bank = ids.locations.front();
  const int32_t right_bank = ids.locations.back();
  add_binary_fact(state, PRED_AT_ITEM, ids.capra, left_bank);
  add_binary_fact(state, PRED_AT_AGENT, ids.fabrizio, left_bank);
  add_binary_fact(state, PRED_AT_ITEM, ids.lupo, left_bank);
  add_binary_fact(state, PRED_AT_ITEM, ids.cavolo, left_bank);
  add_binary_fact(state, PRED_AT_VEHICLE, ids.barca, left_bank);

  add_goal_at_item(state, ids.lupo, right_bank);
  add_goal_at_item(state, ids.capra, right_bank);
  add_goal_at_item(state, ids.cavolo, right_bank);
  add_goal_at_agent(state, ids.fabrizio, right_bank);
  return state;
}

struct RunResult {
  TP_Status status = TP_STATUS_UNSUPPORTED;
  int64_t time_us = 0;
  TP_Solve_Result solve {};
};

RunResult run_mode(TP_Solver *solver, TP_State *state, TP_Solver_Mode mode, bool use_scorer) {
  require_status(tp_solver_set_mode(solver, mode), "set solver mode");
  if (use_scorer) {
    require_status(tp_solver_use_tensor_baseline_scorer(solver), "use baseline scorer");
  } else {
    require_status(tp_solver_set_scorer(solver, nullptr, nullptr), "clear scorer");
  }

  RunResult result {};
  const auto start = std::chrono::steady_clock::now();
  if (mode == TP_SOLVER_MODE_OPTIMAL_DEBUG) {
    result.status = tp_solver_solve_optimal_debug(solver, state, &result.solve);
  } else {
    result.status = tp_solver_solve(solver, state, &result.solve);
  }
  const auto end = std::chrono::steady_clock::now();
  result.time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  return result;
}

void print_summary_row(const char *label, const RunResult &result) {
  std::cout << label
            << " status=" << result.status
            << " solved=" << (result.solve.solved ? "yes" : "no")
            << " time_us=" << result.time_us
            << " plan_length=" << result.solve.plan_length
            << " expansions=" << result.solve.expansions
            << " generated=" << result.solve.generated
            << " scorer_calls=" << result.solve.scorer_calls
            << '\n';
}

void write_csv_row(
  std::ofstream *stream,
  int location_count,
  const char *mode,
  const RunResult &result
) {
  *stream << location_count << ',' << mode << ',' << result.status << ','
          << (result.solve.solved ? 1 : 0) << ',' << result.time_us << ','
          << result.solve.plan_length << ',' << result.solve.expansions << ','
          << result.solve.generated << ',' << result.solve.scorer_calls << '\n';
}

}  // namespace

int main() {
  const TP_Limits limits {
    .max_objects = 64,
    .max_facts = 512,
    .max_goals = 16,
    .max_candidates = 1024,
    .max_expansions = 50000,
    .max_plan_length = 128,
  };

  const char *csv_path = "benchmark_results.csv";
  std::ofstream csv(csv_path, std::ios::trunc);
  require_true(csv.is_open(), "failed to open benchmark_results.csv");

  const char *header = "banks,mode,status,solved,time_us,plan_length,expansions,generated,scorer_calls\n";
  std::cout << header;
  csv << header;

  for (const int location_count : {2, 3, 4, 5}) {
    TP_Domain *domain = tp_domain_create(&limits);
    require_true(domain != nullptr, "tp_domain_create failed");
    build_domain(domain);
    const CapraIds ids = make_ids(location_count);
    TP_State *state = build_problem_state(domain, ids, location_count);
    TP_Solver *solver = tp_solver_create(domain);
    require_true(solver != nullptr, "tp_solver_create failed");

    RunResult greedy = run_mode(solver, state, TP_SOLVER_MODE_PURE_GREEDY, false);
    RunResult guided = run_mode(solver, state, TP_SOLVER_MODE_GUIDED_MIXED, true);
    RunResult symbolic = run_mode(solver, state, TP_SOLVER_MODE_SYMBOLIC_ASTAR, false);
    RunResult optimal = run_mode(solver, state, TP_SOLVER_MODE_OPTIMAL_DEBUG, false);

    write_csv_row(&csv, location_count, "greedy", greedy);
    write_csv_row(&csv, location_count, "guided", guided);
    write_csv_row(&csv, location_count, "symbolic", symbolic);
    write_csv_row(&csv, location_count, "optimal", optimal);

    std::cout << location_count << ",greedy," << greedy.status << "," << (greedy.solve.solved ? 1 : 0) << ","
              << greedy.time_us << "," << greedy.solve.plan_length << "," << greedy.solve.expansions << ","
              << greedy.solve.generated << "," << greedy.solve.scorer_calls << '\n';
    std::cout << location_count << ",guided," << guided.status << "," << (guided.solve.solved ? 1 : 0) << ","
              << guided.time_us << "," << guided.solve.plan_length << "," << guided.solve.expansions << ","
              << guided.solve.generated << "," << guided.solve.scorer_calls << '\n';
    std::cout << location_count << ",symbolic," << symbolic.status << "," << (symbolic.solve.solved ? 1 : 0) << ","
              << symbolic.time_us << "," << symbolic.solve.plan_length << "," << symbolic.solve.expansions << ","
              << symbolic.solve.generated << "," << symbolic.solve.scorer_calls << '\n';
    std::cout << location_count << ",optimal," << optimal.status << "," << (optimal.solve.solved ? 1 : 0) << ","
              << optimal.time_us << "," << optimal.solve.plan_length << "," << optimal.solve.expansions << ","
              << optimal.solve.generated << "," << optimal.solve.scorer_calls << '\n';

    tp_solve_result_dispose(&greedy.solve);
    tp_solve_result_dispose(&guided.solve);
    tp_solve_result_dispose(&symbolic.solve);
    tp_solve_result_dispose(&optimal.solve);
    tp_solver_destroy(solver);
    tp_state_destroy(state);
    tp_domain_destroy(domain);
  }

  std::cout << "Wrote benchmark CSV: " << csv_path << '\n';

  return 0;
}
