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
  OBJ_RIGHT_BANK = 4,
  OBJ_BARCA = 5,
  OBJ_CAVOLO = 6,
};

enum PredicateId : int32_t {
  PRED_GOAT = 0,
  PRED_FARMER = 1,
  PRED_WOLF = 2,
  PRED_CABBAGE = 3,
  PRED_BOAT = 4,
  PRED_AT = 5,
  PRED_ON = 6,
  PRED_RIVER_BANK = 7,
  PRED_RIVER = 8,
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
  require_status(tp_state_add_goal_fact(state, PRED_AT, 2, args), "add goal fact");
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

std::string schema_name(int32_t schema_id) {
  switch (schema_id) {
    case SCHEMA_BOARD_GOAT:
      return "board_goat";
    case SCHEMA_BOARD_WOLF_NO_GOAT:
      return "board_wolf_no_goat";
    case SCHEMA_BOARD_WOLF_NO_CABBAGE:
      return "board_wolf_no_cabbage";
    case SCHEMA_BOARD_CABBAGE_NO_GOAT:
      return "board_cabbage_no_goat";
    case SCHEMA_BOARD_CABBAGE_NO_WOLF:
      return "board_cabbage_no_wolf";
    case SCHEMA_BOARD_ALONE_NO_GOAT:
      return "board_alone_no_goat";
    case SCHEMA_BOARD_ALONE_NO_WOLF_AND_CABBAGE:
      return "board_alone_no_wolf_and_cabbage";
    case SCHEMA_ROWL:
      return "rowl";
    case SCHEMA_UNBOARD:
      return "unboard";
    case SCHEMA_UNBOARD_ALONE:
      return "unboard_alone";
    default:
      return "unknown";
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
      return schema_name(action.schema_id);
  }
}

int32_t add_predicate(TP_Domain *domain, uint8_t arity) {
  TP_Predicate_Def predicate {.arity = arity, .arg_types = {0, 0, 0, 0}};
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

}  // namespace

int main() {
  const TP_Limits limits {
    .max_objects = 16,
    .max_facts = 128,
    .max_goals = 8,
    .max_candidates = 256,
    .max_expansions = 4096,
    .max_plan_length = 32,
  };

  TP_Domain *domain = tp_domain_create(&limits);
  require_true(domain != nullptr, "tp_domain_create failed");

  for (int index = 0; index <= PRED_RIVER; ++index) {
    add_predicate(domain, index == PRED_AT || index == PRED_ON || index == PRED_RIVER ? 2 : 1);
  }

  const int32_t type0_6[6] = {0, 0, 0, 0, 0, 0};
  const int32_t type0_4[4] = {0, 0, 0, 0};

  add_action(
    domain,
    4,
    type0_4,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_BOARD_GOAT
  );

  add_action(
    domain,
    6,
    type0_6,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_WOLF, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_CABBAGE, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = -1, .arity = 2, .slots = {4, 3, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_BOARD_WOLF_NO_GOAT
  );

  add_action(
    domain,
    6,
    type0_6,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_WOLF, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_CABBAGE, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = -1, .arity = 2, .slots = {5, 3, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_BOARD_WOLF_NO_CABBAGE
  );

  add_action(
    domain,
    6,
    type0_6,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_CABBAGE, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_WOLF, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = -1, .arity = 2, .slots = {4, 3, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_BOARD_CABBAGE_NO_GOAT
  );

  add_action(
    domain,
    6,
    type0_6,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_CABBAGE, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_WOLF, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
      {.predicate_id = PRED_AT, .sign = -1, .arity = 2, .slots = {5, 3, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_BOARD_CABBAGE_NO_WOLF
  );

  add_action(
    domain,
    4,
    type0_4,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_AT, .sign = -1, .arity = 2, .slots = {3, 2, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 1, 0, 0}},
    },
    SCHEMA_BOARD_ALONE_NO_GOAT
  );

  add_action(
    domain,
    6,
    type0_6,
    {
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_WOLF, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_CABBAGE, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_AT, .sign = -1, .arity = 2, .slots = {3, 2, 0, 0}},
      {.predicate_id = PRED_AT, .sign = -1, .arity = 2, .slots = {4, 2, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 1, 0, 0}},
    },
    SCHEMA_BOARD_ALONE_NO_WOLF_AND_CABBAGE
  );

  add_action(
    domain,
    4,
    type0_4,
    {
      {.predicate_id = PRED_ON, .sign = 1, .arity = 2, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_RIVER, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_AT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
    },
    SCHEMA_ROWL
  );

  add_action(
    domain,
    4,
    type0_4,
    {
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {2, 3, 0, 0}},
      {.predicate_id = PRED_ON, .sign = 1, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_FARMER, .sign = -1, .arity = 1, .slots = {1, 0, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 3, 0, 0}},
      {.predicate_id = PRED_AT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {1, 3, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {1, 2, 0, 0}},
    },
    SCHEMA_UNBOARD
  );

  add_action(
    domain,
    6,
    type0_6,
    {
      {.predicate_id = PRED_FARMER, .sign = 1, .arity = 1, .slots = {0, 0, 0, 0}},
      {.predicate_id = PRED_BOAT, .sign = 1, .arity = 1, .slots = {1, 0, 0, 0}},
      {.predicate_id = PRED_RIVER_BANK, .sign = 1, .arity = 1, .slots = {2, 0, 0, 0}},
      {.predicate_id = PRED_GOAT, .sign = 1, .arity = 1, .slots = {3, 0, 0, 0}},
      {.predicate_id = PRED_WOLF, .sign = 1, .arity = 1, .slots = {4, 0, 0, 0}},
      {.predicate_id = PRED_CABBAGE, .sign = 1, .arity = 1, .slots = {5, 0, 0, 0}},
      {.predicate_id = PRED_ON, .sign = 1, .arity = 2, .slots = {0, 1, 0, 0}},
      {.predicate_id = PRED_AT, .sign = 1, .arity = 2, .slots = {1, 2, 0, 0}},
      {.predicate_id = PRED_ON, .sign = -1, .arity = 2, .slots = {3, 1, 0, 0}},
      {.predicate_id = PRED_ON, .sign = -1, .arity = 2, .slots = {4, 1, 0, 0}},
      {.predicate_id = PRED_ON, .sign = -1, .arity = 2, .slots = {5, 1, 0, 0}},
    },
    {
      {.predicate_id = PRED_AT, .op = TP_EFFECT_ADD, .arity = 2, .slots = {0, 2, 0, 0}},
      {.predicate_id = PRED_ON, .op = TP_EFFECT_DELETE, .arity = 2, .slots = {0, 1, 0, 0}},
    },
    SCHEMA_UNBOARD_ALONE
  );

  const int32_t object_types[7] = {0, 0, 0, 0, 0, 0, 0};
  TP_State *state = tp_state_create(domain, 7, object_types);
  require_true(state != nullptr, "tp_state_create failed");

  add_unary_fact(state, PRED_RIVER_BANK, OBJ_RIGHT_BANK);
  add_unary_fact(state, PRED_RIVER_BANK, OBJ_LEFT_BANK);
  add_unary_fact(state, PRED_GOAT, OBJ_CAPRA);
  add_unary_fact(state, PRED_FARMER, OBJ_FABRIZIO);
  add_unary_fact(state, PRED_WOLF, OBJ_LUPO);
  add_unary_fact(state, PRED_CABBAGE, OBJ_CAVOLO);
  add_unary_fact(state, PRED_BOAT, OBJ_BARCA);
  add_binary_fact(state, PRED_RIVER, OBJ_RIGHT_BANK, OBJ_LEFT_BANK);
  add_binary_fact(state, PRED_RIVER, OBJ_LEFT_BANK, OBJ_RIGHT_BANK);
  add_binary_fact(state, PRED_AT, OBJ_CAPRA, OBJ_LEFT_BANK);
  add_binary_fact(state, PRED_AT, OBJ_FABRIZIO, OBJ_LEFT_BANK);
  add_binary_fact(state, PRED_AT, OBJ_LUPO, OBJ_LEFT_BANK);
  add_binary_fact(state, PRED_AT, OBJ_CAVOLO, OBJ_LEFT_BANK);
  add_binary_fact(state, PRED_AT, OBJ_BARCA, OBJ_LEFT_BANK);

  add_goal_at(state, OBJ_LUPO, OBJ_RIGHT_BANK);
  add_goal_at(state, OBJ_CAPRA, OBJ_RIGHT_BANK);
  add_goal_at(state, OBJ_CAVOLO, OBJ_RIGHT_BANK);
  add_goal_at(state, OBJ_FABRIZIO, OBJ_RIGHT_BANK);

  TP_Candidate_Action_List candidates {};
  require_status(tp_state_generate_candidates(domain, state, 8, &candidates), "generate candidates");

  std::cout << "Capra candidates after pruning: " << candidates.count << "\n";
  for (int32_t index = 0; index < candidates.count; ++index) {
    std::cout << "candidate " << (index + 1) << ": " << readable_action(candidates.actions[index]) << "\n";
  }

  TP_Solver *solver = tp_solver_create(domain);
  require_true(solver != nullptr, "tp_solver_create failed");

  TP_Solve_Result result {};
  const auto solve_start = std::chrono::steady_clock::now();
  const TP_Status solve_status = tp_solver_solve(solver, state, &result);
  const auto solve_end = std::chrono::steady_clock::now();
  const auto solve_us = std::chrono::duration_cast<std::chrono::microseconds>(solve_end - solve_start);

  std::cout << "Solve status: " << solve_status << "\n";
  std::cout << "Solved: " << (result.solved ? "yes" : "no") << "\n";
  std::cout << "Solve time (us): " << solve_us.count() << "\n";
  std::cout << "Solve time (ms): " << (static_cast<double>(solve_us.count()) / 1000.0) << "\n";
  std::cout << "Candidate generation calls: " << result.candidate_generation_calls << "\n";
  std::cout << "Candidate generation time (us): " << result.candidate_generation_time_us << "\n";
  std::cout << "Index rebuilds: " << result.index_rebuilds << "\n";
  std::cout << "Plan length: " << result.plan_length << "\n";
  std::cout << "Search expansions: " << result.expansions << "\n";
  std::cout << "Generated candidates: " << result.generated << "\n";
  std::cout << "Readable plan:\n";
  for (int32_t index = 0; index < result.plan_length; ++index) {
    std::cout << "  " << (index + 1) << ". " << readable_action(result.plan_actions[index]) << "\n";
  }

  TP_Solve_Result optimal_result {};
  const auto optimal_start = std::chrono::steady_clock::now();
  const TP_Status optimal_status = tp_solver_solve_optimal_debug(solver, state, &optimal_result);
  const auto optimal_end = std::chrono::steady_clock::now();
  const auto optimal_us = std::chrono::duration_cast<std::chrono::microseconds>(optimal_end - optimal_start);

  std::cout << "Optimal debug status: " << optimal_status << "\n";
  std::cout << "Optimal debug solved: " << (optimal_result.solved ? "yes" : "no") << "\n";
  std::cout << "Optimal debug time (us): " << optimal_us.count() << "\n";
  std::cout << "Optimal debug plan length: " << optimal_result.plan_length << "\n";
  std::cout << "Optimal debug expansions: " << optimal_result.expansions << "\n";
  std::cout << "Optimal debug generated candidates: " << optimal_result.generated << "\n";
  std::cout << "Optimal debug readable plan:\n";
  for (int32_t index = 0; index < optimal_result.plan_length; ++index) {
    std::cout << "  " << (index + 1) << ". " << readable_action(optimal_result.plan_actions[index]) << "\n";
  }

  tp_solve_result_dispose(&result);
  tp_solve_result_dispose(&optimal_result);
  tp_solver_destroy(solver);
  tp_candidate_action_list_dispose(&candidates);
  tp_state_destroy(state);
  tp_domain_destroy(domain);
  return solve_status == TP_STATUS_OK ? 0 : 1;
}
