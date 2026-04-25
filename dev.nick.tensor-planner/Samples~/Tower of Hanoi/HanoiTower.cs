using TensorPlanner;
using UnityEngine;

public sealed class TensorPlannerHanoiTower : MonoBehaviour
{
    private sealed class Support
    {
        public Support(string name) { Name = name; }
        public string Name { get; private set; }
        public override string ToString() { return Name; }
    }

    private void Start()
    {
        Support leftPeg = new Support("left peg");
        Support middlePeg = new Support("middle peg");
        Support rightPeg = new Support("right peg");
        Support smallDisc = new Support("small disc");
        Support mediumDisc = new Support("medium disc");
        Support largeDisc = new Support("large disc");

        using (Planner planner = new Planner())
        {
            Predicate on = planner.Predicate<Support, Support>("on");
            Predicate clear = planner.Predicate<Support>("clear");
            Predicate smaller = planner.Predicate<Support, Support>("smaller");

            PlannerAction move = planner.Action("move")
                .Param<Support>("disc")
                .Param<Support>("from")
                .Param<Support>("to")
                .Require(on.Create("disc", "from"))
                .Require(clear.Create("disc"))
                .Require(clear.Create("to"))
                .Require(smaller.Create("disc", "to"))
                .Removes(on.Create("disc", "from"))
                .Removes(clear.Create("to"))
                .Adds(on.Create("disc", "to"))
                .Adds(clear.Create("from"))
                .Commit();

            StateBuilder state = planner.State()
                .Object(leftPeg)
                .Object(middlePeg)
                .Object(rightPeg)
                .Object(smallDisc)
                .Object(mediumDisc)
                .Object(largeDisc)
                .Fact(on.Create(smallDisc, mediumDisc))
                .Fact(on.Create(mediumDisc, largeDisc))
                .Fact(on.Create(largeDisc, leftPeg))
                .Fact(clear.Create(smallDisc))
                .Fact(clear.Create(middlePeg))
                .Fact(clear.Create(rightPeg))
                .Fact(smaller.Create(smallDisc, mediumDisc))
                .Fact(smaller.Create(smallDisc, largeDisc))
                .Fact(smaller.Create(mediumDisc, largeDisc))
                .Fact(smaller.Create(smallDisc, leftPeg))
                .Fact(smaller.Create(smallDisc, middlePeg))
                .Fact(smaller.Create(smallDisc, rightPeg))
                .Fact(smaller.Create(mediumDisc, leftPeg))
                .Fact(smaller.Create(mediumDisc, middlePeg))
                .Fact(smaller.Create(mediumDisc, rightPeg))
                .Fact(smaller.Create(largeDisc, leftPeg))
                .Fact(smaller.Create(largeDisc, middlePeg))
                .Fact(smaller.Create(largeDisc, rightPeg))
                .Goal(on.Create(largeDisc, rightPeg))
                .Goal(on.Create(mediumDisc, largeDisc))
                .Goal(on.Create(smallDisc, mediumDisc));

            SolveResult result = planner.Solve(state);
            if (!result.Solved)
            {
                Debug.LogWarning("Tower of Hanoi was not solved. Status: " + result.Status);
                return;
            }

            Debug.Log("Tower of Hanoi solved in " + result.Steps.Count + " moves.");
            for (int index = 0; index < result.Steps.Count; index++)
            {
                Debug.Log((index + 1) + ". " + FormatMove(result.Steps[index], move));
            }
        }
    }

    private static string FormatMove(PlanStep step, PlannerAction move)
    {
        if (!step.Is(move))
        {
            return step.ToString();
        }

        Support disc = step.Arg<Support>("disc");
        Support from = step.Arg<Support>("from");
        Support to = step.Arg<Support>("to");
        return "move " + disc + " off " + from + " onto " + to;
    }
}
