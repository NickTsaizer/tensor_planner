#import "Basic";
#import "Compiler";
#import "File_Utilities";
#import,dir "..";

#run,stallable copy_tensor_planner_runtime_library();

Agent :: struct { name: string; }
Item :: struct { name: string; }
Location :: struct { name: string; }

copy_tensor_planner_runtime_library :: () {
    options := get_build_options();
    output_path := options.output_path;
    if !output_path output_path = #filepath;

    source := tprint("%/../libtensor_planner.so", #filepath);
    dest := tprint("%/libtensor_planner.so", output_path);
    ok := copy_file(source, dest);
    assert(ok);
}

crafting_limits :: () -> TP_Limits {
    limits := default_limits();
    limits.max_objects     = 32;
    limits.max_facts       = 256;
    limits.max_goals       = 8;
    limits.max_candidates  = 512;
    limits.max_expansions  = 20000;
    limits.max_plan_length = 64;
    return limits;
}

Crafting :: #run planner_domain(
Type.[Agent, Item, Location],
Predicate.[
    predicate("at", Agent, Location),
    predicate("has", Item),
    predicate("connected", Location, Location),
    predicate("gather_at", Item, Location),
    predicate("home", Location),
    predicate("requires_tool", Item, Item),
    predicate("gather_bare_hands", Item),
    predicate("recipe2_a", Item, Item),
    predicate("recipe2_b", Item, Item),
    predicate("recipe3_a", Item, Item),
    predicate("recipe3_b", Item, Item),
    predicate("recipe3_c", Item, Item),
],
Action.[
    action(
    "move",
    Param.[param("agent", Agent), param("from", Location), param("to", Location)],
    Atom.[atom("at", "agent", "from"), atom("connected", "from", "to")],
    Atom.[atom("at", "agent", "from")],
    Atom.[atom("at", "agent", "to")]
),
action(
"gather_free",
Param.[param("agent", Agent), param("item", Item), param("where", Location)],
Atom.[
    atom("at", "agent", "where"),
    atom("gather_at", "item", "where"),
    atom("gather_bare_hands", "item"),
],
Atom.[],
Atom.[atom("has", "item")]
),
action(
"gather_tool",
Param.[
    param("agent", Agent),
    param("item", Item),
    param("where", Location),
    param("tool", Item),
],
Atom.[
    atom("at", "agent", "where"),
    atom("gather_at", "item", "where"),
    atom("requires_tool", "item", "tool"),
    atom("has", "tool"),
],
Atom.[],
Atom.[atom("has", "item")]
),
action(
"craft2",
Param.[
    param("agent", Agent),
    param("item", Item),
    param("where", Location),
    param("first", Item),
    param("second", Item),
],
Atom.[
    atom("at", "agent", "where"),
    atom("home", "where"),
    atom("recipe2_a", "item", "first"),
    atom("recipe2_b", "item", "second"),
    atom("has", "first"),
    atom("has", "second"),
],
Atom.[],
Atom.[atom("has", "item")]
),
action(
"craft3",
Param.[
    param("agent", Agent),
    param("item", Item),
    param("where", Location),
    param("first", Item),
    param("second", Item),
    param("third", Item),
],
Atom.[
    atom("at", "agent", "where"),
    atom("home", "where"),
    atom("recipe3_a", "item", "first"),
    atom("recipe3_b", "item", "second"),
    atom("recipe3_c", "item", "third"),
    atom("has", "first"),
    atom("has", "second"),
    atom("has", "third"),
],
Atom.[],
Atom.[atom("has", "item")]
),
],
#run crafting_limits()
);

main :: () {
    planner := planner_create(Crafting);
    defer planner_destroy(*planner);

    agent := Agent.{"agent"};
    home := Location.{"home"};
    forest := Location.{"forest"};
    river := Location.{"river"};
    cave := Location.{"cave"};
    deep_cave := Location.{"deep_cave"};

    flint := Item.{"flint"};
    fiber := Item.{"fiber"};
    flint_axe := Item.{"flint_axe"};
    wood := Item.{"wood"};
    stick := Item.{"stick"};
    stone := Item.{"stone"};
    stone_pickaxe := Item.{"stone_pickaxe"};
    iron_ore := Item.{"iron_ore"};
    iron_pickaxe := Item.{"iron_pickaxe"};
    diamond := Item.{"diamond"};
    diamond_pickaxe := Item.{"diamond_pickaxe"};

    state := state_begin(Crafting);
    agent_ref := object(*state, *agent, agent.name);
    home_ref := object(*state, *home, home.name);
    forest_ref := object(*state, *forest, forest.name);
    river_ref := object(*state, *river, river.name);
    cave_ref := object(*state, *cave, cave.name);
    deep_cave_ref := object(*state, *deep_cave, deep_cave.name);
    flint_ref := object(*state, *flint, flint.name);
    fiber_ref := object(*state, *fiber, fiber.name);
    flint_axe_ref := object(*state, *flint_axe, flint_axe.name);
    wood_ref := object(*state, *wood, wood.name);
    stick_ref := object(*state, *stick, stick.name);
    stone_ref := object(*state, *stone, stone.name);
    stone_pickaxe_ref := object(*state, *stone_pickaxe, stone_pickaxe.name);
    iron_ore_ref := object(*state, *iron_ore, iron_ore.name);
    iron_pickaxe_ref := object(*state, *iron_pickaxe, iron_pickaxe.name);
    diamond_ref := object(*state, *diamond, diamond.name);
    diamond_pickaxe_ref := object(*state, *diamond_pickaxe, diamond_pickaxe.name);

    fact(*state, "at", agent_ref, home_ref);
    fact(*state, "home", home_ref);
    edge(*state, "connected", home_ref, forest_ref);
    edge(*state, "connected", forest_ref, river_ref);
    edge(*state, "connected", forest_ref, cave_ref);
    edge(*state, "connected", cave_ref, deep_cave_ref);

    fact(*state, "gather_at", flint_ref, river_ref);
    fact(*state, "gather_at", fiber_ref, forest_ref);
    fact(*state, "gather_at", wood_ref, forest_ref);
    fact(*state, "gather_at", stone_ref, cave_ref);
    fact(*state, "gather_at", iron_ore_ref, cave_ref);
    fact(*state, "gather_at", diamond_ref, deep_cave_ref);
    fact(*state, "gather_bare_hands", flint_ref);
    fact(*state, "gather_bare_hands", fiber_ref);
    fact(*state, "requires_tool", wood_ref, flint_axe_ref);
    fact(*state, "requires_tool", stone_ref, flint_axe_ref);
    fact(*state, "requires_tool", iron_ore_ref, stone_pickaxe_ref);
    fact(*state, "requires_tool", diamond_ref, iron_pickaxe_ref);

    fact(*state, "recipe2_a", flint_axe_ref, flint_ref);
    fact(*state, "recipe2_b", flint_axe_ref, fiber_ref);
    fact(*state, "recipe2_a", stick_ref, wood_ref);
    fact(*state, "recipe2_b", stick_ref, fiber_ref);
    fact(*state, "recipe2_a", stone_pickaxe_ref, stone_ref);
    fact(*state, "recipe2_b", stone_pickaxe_ref, stick_ref);
    fact(*state, "recipe2_a", iron_pickaxe_ref, iron_ore_ref);
    fact(*state, "recipe2_b", iron_pickaxe_ref, stick_ref);
    fact(*state, "recipe3_a", diamond_pickaxe_ref, diamond_ref);
    fact(*state, "recipe3_b", diamond_pickaxe_ref, stick_ref);
    fact(*state, "recipe3_c", diamond_pickaxe_ref, iron_pickaxe_ref);
    goal(*state, "has", diamond_pickaxe_ref);

    result := solve(*planner, *state);
    defer solve_result_destroy(*result);

    assert(result.raw.solved);
    assert(result.raw.plan_length > 0);

    last := plan_step(result, result.raw.plan_length - 1);
    assert(step_is(last, "craft3"));
    final_item := step_arg(last, "item", Item);
    assert(final_item == *diamond_pickaxe);

    assert(plan_contains_craft(result, *flint_axe));
    assert(plan_contains_craft(result, *stone_pickaxe));
    assert(plan_contains_craft(result, *iron_pickaxe));

    log("Tensor Planner Jai typed-object crafting example passed.");
    log_plan(result);
}

plan_contains_craft :: (result: Solve_Result, item: *Item) -> bool {
    if result.raw.plan_length == 0 return false;
    for 0..result.raw.plan_length - 1 {
        step := plan_step(result, cast(s32) it);
        if step_is(step, "craft2") {
            made := step_arg(step, "item", Item);
            if made == item return true;
        }
    }
    return false;
}
