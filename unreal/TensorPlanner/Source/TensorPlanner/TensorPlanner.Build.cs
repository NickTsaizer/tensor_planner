using System.IO;
using UnrealBuildTool;

public class TensorPlanner : ModuleRules
{
    public TensorPlanner(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = true;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AIModule"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "GameplayTasks",
            "NavigationSystem"
        });

        string PluginDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
        string ThirdPartyDir = Path.Combine(PluginDir, "Source", "ThirdParty", "TensorPlanner");
        string IncludeDir = Path.Combine(ThirdPartyDir, "include");
        string BinariesDir = Path.Combine(PluginDir, "Binaries", "ThirdParty", "TensorPlanner");
        string LibraryDir = Path.Combine(ThirdPartyDir, "lib");

        PublicIncludePaths.Add(IncludeDir);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string PlatformLibraryDir = Path.Combine(LibraryDir, "Win64");
            string RuntimeDll = Path.Combine(BinariesDir, "Win64", "tensor_planner.dll");
            string[] ImportLibraryCandidates =
            {
                Path.Combine(PlatformLibraryDir, "tensor_planner.lib"),
                Path.Combine(PlatformLibraryDir, "libtensor_planner.dll.a")
            };

            foreach (string Candidate in ImportLibraryCandidates)
            {
                if (File.Exists(Candidate))
                {
                    PublicAdditionalLibraries.Add(Candidate);
                    break;
                }
            }

            PublicDelayLoadDLLs.Add("tensor_planner.dll");
            RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/TensorPlanner/Win64/tensor_planner.dll", RuntimeDll);
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string SharedObject = Path.Combine(LibraryDir, "Linux", "libtensor_planner.so");

            PublicAdditionalLibraries.Add(SharedObject);
            RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/TensorPlanner/Linux/libtensor_planner.so", SharedObject);
        }
    }
}
