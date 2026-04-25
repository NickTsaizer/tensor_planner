#scope_export;

#if OS == .WINDOWS {
    tensor_planner_lib :: #library "tensor_planner";
} else #if OS == .MACOS {
    tensor_planner_lib :: #library "libtensor_planner";
} else #if OS == .LINUX {
    tensor_planner_lib :: #library "libtensor_planner";
}
