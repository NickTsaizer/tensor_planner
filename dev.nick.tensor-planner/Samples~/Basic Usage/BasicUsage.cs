using TensorPlanner;
using UnityEngine;

public sealed class TensorPlannerBasicUsage : MonoBehaviour
{
    private sealed class Character
    {
        public Character(string name) { Name = name; }
        public string Name { get; private set; }
    }

    private sealed class Location
    {
        public Location(string name) { Name = name; }
        public string Name { get; private set; }
    }

    private void Start()
    {
        Character player = new Character("player");
        Location home = new Location("home");
        Location forest = new Location("forest");

        using (Planner planner = new Planner())
        {
            Predicate at = planner.Predicate<Character, Location>("at");
            Predicate connected = planner.Predicate<Location, Location>("connected");

            PlannerAction move = planner.Action("move")
                .Param<Character>("who")
                .Param<Location>("from")
                .Param<Location>("to")
                .Require(at.Create("who", "from"))
                .Require(connected.Create("from", "to"))
                .Removes(at.Create("who", "from"))
                .Adds(at.Create("who", "to"))
                .Commit();

            StateBuilder state = planner.State()
                .Object(player)
                .Object(home)
                .Object(forest)
                .Fact(at.Create(player, home))
                .Edge(connected, home, forest)
                .Goal(at.Create(player, forest));

            SolveResult result = planner.Solve(state);
            if (result.Solved && result.Steps.Count > 0 && result.Steps[0].Is(move))
            {
                Debug.Log(result.Steps[0].ToString());
            }
        }
    }
}
