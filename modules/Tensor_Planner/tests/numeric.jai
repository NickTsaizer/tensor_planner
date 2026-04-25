#import "Basic";
#import "Compiler";
#import "File_Utilities";
#import,dir "..";

#run,stallable copy_tensor_planner_runtime_library();

Agent :: struct { name: string; }
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

Numeric_Domain :: #run planner_domain(
Type.[Agent, Location],
Predicate.[
    predicate("at", Agent, Location),
    predicate("connected", Location, Location),
],
Function.[
    function("energy", Agent),
],
Action.[
    action(
    "move",
    Param.[param("agent", Agent), param("from", Location), param("to", Location)],
    Atom.[atom("at", "agent", "from"), atom("connected", "from", "to")],
    Numeric_Precondition.[numeric("energy", "agent") >= 1.0],
    Atom.[atom("at", "agent", "from")],
    Atom.[atom("at", "agent", "to")],
    Numeric_Effect.[decrease(numeric("energy", "agent"), 1.0)]
),
],
default_limits()
);

main :: () {
    planner := planner_create(Numeric_Domain);
    defer planner_destroy(*planner);

    agent := Agent.{"agent"};
    home := Location.{"home"};
    middle := Location.{"middle"};
    target := Location.{"target"};

    state := state_begin(Numeric_Domain);
    agent_ref := object(*state, *agent, agent.name);
    home_ref := object(*state, *home, home.name);
    middle_ref := object(*state, *middle, middle.name);
    target_ref := object(*state, *target, target.name);

    fact(*state, "at", agent_ref, home_ref);
    edge(*state, "connected", home_ref, middle_ref);
    edge(*state, "connected", middle_ref, target_ref);
    value(*state, "energy", 2.0, agent_ref);
    goal(*state, "at", agent_ref, target_ref);

    result := solve(*planner, *state);
    defer solve_result_destroy(*result);

    assert(result.raw.solved);
    assert(result.raw.plan_length == 2);

    first := plan_step(result, 0);
    assert(step_is(first, "move"));
    assert(step_arg(first, "from", Location) == *home);
    assert(step_arg(first, "to", Location) == *middle);

    second := plan_step(result, 1);
    assert(step_is(second, "move"));
    assert(step_arg(second, "from", Location) == *middle);
    assert(step_arg(second, "to", Location) == *target);

    log("Tensor Planner Jai numeric planning check passed.");
}
